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
	p->raw.type = 0xffff;
	p->raw.len  = min_size;
	assert(p != NULL);
	if (p->dsz < min_size) {
		size_t target_size = min_size>256?min_size:256;
		p->data = realloc(p->data, target_size);
		assert(p->data != NULL);
		p->dsz = target_size;
	}
	p->raw.data = p->data + 6;
	return p;
} /*}}}*/
void cast_raw_packet_header(packet *p) { /*{{{*/
	assert(p->data+6 <= p->raw.data);
	*(p->raw.data-6)= (p->raw.type >>  8) & 0xff;
	*(p->raw.data-5)= (p->raw.type      ) & 0xff;
	*(p->raw.data-4)= (p->raw.len  >> 24) & 0xff;
	*(p->raw.data-3)= (p->raw.len  >> 16) & 0xff;
	*(p->raw.data-2)= (p->raw.len  >>  8) & 0xff;
	*(p->raw.data-1)= (p->raw.len       ) & 0xff;
	p->raw.data -= 6;
	p->raw.len += 6;
} /*}}}*/
