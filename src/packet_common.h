#pragma once

#include "sll_meta.h"
#include <inttypes.h>

#define RAW 0

// audio packet types
#define PING 32
#define OPUS 128

typedef struct packet {
	SLL_LINK(packet);
	uint8_t *data;
	size_t   dsz;
	uint8_t  type;
	union {
		struct {
			uint16_t  type;
			uint32_t  len;
			uint8_t  *data;
		} raw;
		struct {
			int64_t timestamp;
		} ping;
		struct {
			uint8_t  target;
			int64_t  sid;
			int64_t  seq;
			uint8_t *data;
			int16_t  len; // protocol specifies max len of 0x1fff;
			bool     islast;
		} opus;
	};
} packet;

SLL_DECLS(p, packet, p_list);
SLL_POOL_DECLS(p, packet, p_list, p_pool);
void p_free(packet *p);
packet *get_packet(p_pool *pool, size_t min_size);
void cast_raw_packet_header(packet *p);
