#pragma once
#include "packet_common.h"
#include "ssl_io.h"
#include "config.h"

#define NETWORK_FATAL_IF(cond, ...) if (cond) { \
	fprintf(stderr, "(FATAL) NETWORK:\n%s +%d\n", __FILE__, __LINE__); \
	fprintf(stderr, __VA_ARGS__); \
	exit(2); \
}

#ifdef VERBOSE_NETWORK
#       define VPNETWORK(...) do { fprintf(stderr, "(DD) NETWORK " __VA_ARGS__); } while (0);
#else
#       define VPNETWORK(...) {}
#endif

typedef struct {

	const network_config *cfg;
	p_pool *packet_pool;
	sio_con_t connection;

	struct {
		p_list queue;
		uint8_t header[8];
		uint8_t headerwi;
		packet *current;
	} ingress;

	struct {
		p_list queue;
		packet *current;
	} egress;

} network_manager;


void nm_init(network_manager *nm, const network_config *cfg, p_pool *packet_pool);
bool nm_connect(network_manager *nm);
bool nm_read(network_manager *nm);
bool nm_write(network_manager *nm);

packet *nm_from_pool(network_manager *nm, size_t size);
void nm_post_egress(network_manager *nm, packet *p);
packet *nm_get_ingress(network_manager *nm);
void nm_recycle(network_manager *nm, packet *p);
