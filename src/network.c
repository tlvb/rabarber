#include "network.h"
#include <string.h>

void nm_init(network_manager *nm, const network_config *cfg, p_pool *packet_pool) { /*{{{*/
	nm->cfg = cfg;
	nm->packet_pool = packet_pool;
	sio_init();
	sio_con_init(&nm->connection);
	sio_con_set_certs(&nm->connection,
	                  nm->cfg->client_cert,
	                  nm->cfg->client_key,
	                  nm->cfg->server_cert);
	VPNETWORK("client cert: %s\nclient key:  %s\nserver cert: %s\n",
                  nm->cfg->client_cert,
                  nm->cfg->client_key,
                  nm->cfg->server_cert);
	p_lclear(&nm->ingress.queue);
	nm->ingress.headerwi = 0;
	nm->ingress.current  = NULL;

	p_lclear(&nm->egress.queue);
	nm->egress.current = NULL;
} /*}}}*/
bool nm_connect(network_manager *nm) { /*{{{*/
	assert(nm != NULL);
	return sio_con_connect(&nm->connection, nm->cfg->server_hostport);
} /*}}}*/
bool nm_read(network_manager *nm) { /*{{{*/
	if (nm->ingress.current == NULL) {
		int m = nm->ingress.headerwi;
		int n = sio_con_read(&nm->connection,
		                     nm->ingress.header + m,
		                     6-m);
		if (n > 0) {
			VPNETWORK("read %d header bytes\n", n);
			nm->ingress.headerwi += n;
			if (nm->ingress.headerwi == 6) {
				nm->ingress.headerwi = 0;
				uint16_t type = (nm->ingress.header[0] <<  8)
				              | (nm->ingress.header[1]      );
				uint32_t len  = (nm->ingress.header[2] << 24)
				              | (nm->ingress.header[3] << 16)
				              | (nm->ingress.header[4] <<  8)
				              | (nm->ingress.header[5]      );
				nm->ingress.current = get_packet(nm->packet_pool, len);
				assert(nm->ingress.current != NULL);
				memcpy(nm->ingress.current->data, nm->ingress.header, 6);
				nm->ingress.current->raw.type = type;
				if (len == 0) {
					p_lpushback(&nm->ingress.queue, nm->ingress.current);
					nm->ingress.current = NULL;
				}
			}
			return true;
		}
		else if (n == 0) {
			//VPNETWORK("could not read anything\n");
			return false;
		}
		else {
			NETWORK_FATAL_IF( n<0, "tried to read up to %d header bytes but con_read returned %d\n", 6-m, n);
		}
	}
	else {
		// while the packet is filled with data the raw len and data pointers serve as counters for where
		// and how much data is written, once no more data is to be written, they are reset to point at
		// the beginning of the data and contain the length of the packet
		int m = nm->ingress.current->raw.len;
		int n = sio_con_read(&nm->connection,
		                     nm->ingress.current->raw.data,
		                     m);
		if (n > 0) {
			VPNETWORK("read %d payload bytes\n", n);
			nm->ingress.current->raw.data += n;
			nm->ingress.current->raw.len  -= n;
			if (nm->ingress.current->raw.len == 0) {
				// reset the length and data pointers
				nm->ingress.current->raw.len = nm->ingress.current->raw.data - nm->ingress.current->data;
				nm->ingress.current->raw.data = nm->ingress.current->data;
				p_lpushback(&nm->ingress.queue, nm->ingress.current);
				nm->ingress.current  = NULL;
			}
		}
		else if (n == 0) {
			//VPNETWORK("could not read anything\n");
			return false;
		}
		else {
			NETWORK_FATAL_IF( n<0, "tried to read up to %d payload bytes but con_read returned %d\n", m, n);
		}
	}
	return false;
}/*}}}*/
bool nm_write(network_manager *nm) { /*{{{*/
	if (nm->egress.current == NULL) {
		//VPNETWORK("getting a new packet to write\n");
		nm->egress.current = p_lpopfront(&nm->egress.queue);
	}
	if (nm->egress.current == NULL) {
		//VPNETWORK("nothing to write\n");
		return false;
	}
	int n = sio_con_write(&nm->connection,
	                      nm->egress.current->raw.data,
	                      nm->egress.current->raw.len);
	if (n > 0) {
		VPNETWORK("wrote %d egress bytes\n", n);
		nm->egress.current->raw.data += n;
		nm->egress.current->raw.len  -= n;
		if (nm->egress.current->raw.len == 0) {
			nm_recycle(nm, nm->egress.current);
			nm->egress.current = NULL;
		}
		return true;
	}
	else if (n == 0) {
		//VPNETWORK("could not write anything\n");
		return false;
	}
	else {
		VPNETWORK("negative write return: %d\n", n);
		exit(1);
	}
	return false;
}/*}}}*/
packet *nm_from_pool(network_manager *nm, size_t size) { /*{{{*/
	return get_packet(nm->packet_pool, size);
}/*}}}*/
void nm_post_egress(network_manager *nm, packet *p) { /*{{{*/
	assert(nm != NULL);
	p_lpushback(&nm->egress.queue, p);
}/*}}}*/
packet *nm_get_ingress(network_manager *nm) { /*{{{*/
	assert(nm != NULL);
	return p_lpopfront(&nm->ingress.queue);
}/*}}}*/
void nm_recycle(network_manager *nm, packet *p) { /*{{{*/
	assert(nm != NULL);
	p_preturn(nm->packet_pool, p);
}/*}}}*/
