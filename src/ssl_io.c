#include "ssl_io.h"
void sio_init(void) { /*{{{*/
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	ERR_load_crypto_strings();
	ERR_load_SSL_strings();
	SSL_library_init();
	OpenSSL_add_all_algorithms();
} /*}}}*/
void sio_con_init(sio_con_t *con) { /*{{{*/
	con->ctx = SSL_CTX_new(SSLv23_client_method());
	SSL_FATAL_IF(con->ctx == NULL, "could not create ssl ctx\n");
} /*}}}*/
void sio_con_set_certs(sio_con_t *con, const char *ccert, const char *ckey, const char *scert) { /*{{{*/
	if (ccert != NULL && ckey != NULL) {
		SSL_FATAL_IF(1 != SSL_CTX_use_certificate_file(con->ctx, ccert, SSL_FILETYPE_PEM), "could not load certificate from \"%s\"\n", ccert);
		SSL_FATAL_IF(1 != SSL_CTX_use_PrivateKey_file(con->ctx, ckey, SSL_FILETYPE_PEM),   "could not load private key from \"%s\"\n", ckey);
	}
	if (scert != NULL) {
		SSL_FATAL_IF(1 != SSL_CTX_load_verify_locations(con->ctx, scert, NULL),  "could not load private key from \"%s\"\n", scert);
	}
} /*}}}*/
bool sio_con_connect(sio_con_t *con, const char *remote) { /*{{{*/
	con->bio = BIO_new_ssl_connect(con->ctx);
	BIO_set_nbio(con->bio, 1);

	BIO_set_conn_hostname(con->bio, remote);

	/* since we are using non-blocking sockets we need to do more than just
	 * check the return value of BIO_do_connect in order to know if the
	 * connection was successful or not
	 */
	VPSSLIO("connecting to remote \"%s\"\n", remote);
	if (BIO_do_connect(con->bio) <= 0) {
		BIO_get_fd(con->bio, &con->fd);
		SSL_FAIL_IF(errno != EINPROGRESS, "connection error\n");
		fd_set set;
		FD_ZERO(&set);
		FD_SET(con->fd, &set);
		struct timeval tv = { 30, 0 };
		SSL_FAIL_IF(1 != select(con->fd+1, NULL, &set, NULL, &tv), "connection timed out during setup\n");
		socklen_t l = sizeof(int);
		getsockopt(con->fd, SOL_SOCKET, SO_ERROR, &errno, &l);
		SSL_FAIL_IF(errno != 0, "connection error\n");
	}

	/* now shake hands for as long as it takes to become friends
	 */
	VPSSLIO("initiating ssl handshake\n");
	if (BIO_do_handshake(con->bio) <= 0) {
		while (BIO_retry_type(con->bio) != 0) {
			fd_set rset;
			fd_set wset;
			FD_ZERO(&rset);
			FD_ZERO(&wset);
			if (BIO_should_read(con->bio) != 0) {
				FD_SET(con->fd, &rset);
			}
			else if (BIO_should_write(con->bio) != 0) {
				FD_SET(con->fd, &wset);
			}
			struct timeval tv = { 30, 0 };
			SSL_FAIL_IF(0 == select(con->fd+1, &wset, &rset, NULL, &tv), "connection timed out during handshake\n");
			BIO_do_handshake(con->bio);
		}
	}
	VPSSLIO("connection setup all done\n");
	return true;
} /*}}}*/
int sio_con_read(sio_con_t *con, void *buf, int n) { /*{{{*/
	int ret = BIO_read(con->bio, buf, n);
	VPSSLIO("reading up to %d bytes returned %d and should retry is %d\n", n, ret, BIO_should_retry(con->bio));
	/*
	if (ret > 0) {
		printf("INGRESS [");
		for (size_t i=0; i<(size_t)ret; ++i) {
			printf("%02x", ((const uint8_t*)buf)[i]);
		}
		printf("]\n");
	}
	*/
	if (ret <= 0) {
		if (BIO_should_retry(con->bio)) {
			return 0;
		}
		else {
			return -1;
		}
	}
	return ret;
} /*}}}*/
int sio_con_write(sio_con_t *con, const void *buf, int n) { /*{{{*/
	int ret = BIO_write(con->bio, buf, n);
	VPSSLIO("writing up to %d bytes returned %d and should retry is %d\n", n, ret, BIO_should_retry(con->bio));
	/*
	if (ret > 0) {
		printf("EGRESS [");
		for (size_t i=0; i<(size_t)ret; ++i) {
			printf("%02x", ((const uint8_t*)buf)[i]);
		}
		printf("]\n");
	}
	*/
	if (ret <= 0) {
		if (BIO_should_retry(con->bio)) {
			return 0;
		}
		else {
			return -1;
		}
	}
	return ret;
} /*}}}*/
void sio_con_close_free(sio_con_t *con) { /*{{{*/
	BIO_free_all(con->bio);
	con->bio = NULL;
	SSL_CTX_free(con->ctx);
	con->ctx = NULL;
	con->fd = 0;
} /*}}}*/
