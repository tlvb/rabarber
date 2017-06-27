#pragma once

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/select.h>
#include <sys/time.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <assert.h>
#include <stdbool.h>

#ifdef VERBOSE_SSL_IO
#       define VPSSLIO(...) do { fprintf(stderr, "(DD) SSL_IO " __VA_ARGS__); } while (0);
#else
#       define VPSSLIO(...) {}
#endif

#define SSL_FATAL_IF(cond, ...) if (cond) { \
	fprintf(stderr, "(FATAL) SSL_IO:\n%s +%d\n", __FILE__, __LINE__); \
	fprintf(stderr, __VA_ARGS__); \
	exit(2); \
}
#define SSL_FAIL_IF(cond, ...)  if (cond) { fprintf(stderr, "(EE) SSL_IO: " __VA_ARGS__); return false; }

typedef struct {
	SSL_CTX *ctx;
	BIO *bio;
	int fd;
} sio_con_t;

void sio_init(void);
void sio_con_init(sio_con_t *con);
void sio_con_set_certs(sio_con_t *con, const char *ccert, const char *ckey, const char *scert);
bool sio_con_connect(sio_con_t *con, const char *remote);
int sio_con_read(sio_con_t *con, void *buf, int n);
int sio_con_write(sio_con_t *con, const void *buf, int n);
void sio_con_close_free(sio_con_t *con);

