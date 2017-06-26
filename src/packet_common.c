#include "packet_common.h"

SLL_DEFS(p, packet, p_list, p_free);
SLL_POOL_DEFS(p, packet, p_list, p_pool);

void p_free(packet *p) { /*{{{*/
	assert(p != NULL);
	if (p->data != NULL) {
		free(p->data);
	}
	free(p);
} /*}}}*/
packet *get_packet(p_pool *pool, size_t min_size) { /*{{{*/
	packet *p = p_pget(pool);
	p->type = RAW;
	assert(p != NULL);
	if (p->dsz < min_size) {
		size_t target_size = min_size>256?min_size:256;
		p->data = realloc(p->data, target_size);
		assert(p->data != NULL);
		p->dsz = target_size;
	}
	return p;
} /*}}}*/
