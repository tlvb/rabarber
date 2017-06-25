#pragma once
#include <inttypes.h>
#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include "sll_meta.h"
#include "config.h"

typedef struct audio_packet {
	SLL_LINK(audio_packet);
	uint8_t *data;
	size_t dsz;
	union {
		struct {
			int64_t timestamp;
		} ping;
		struct {
			uint8_t target;
			int64_t sid;
			int64_t seq;
			struct {
				uint8_t *data;
				size_t len;
				bool islast;
			} opus;
		} audio;
	};
} audio_packet;

SLL_DECLS(ap, audio_packet, ap_list);
SLL_POOL_DECLS(ap, audio_packet, ap_list, ap_pool);

typedef struct keyed_ap_buffer {
	SLL_LINK(keyed_ap_buffer);
	int64_t key;
	bool prebuffering;
	ap_list buffer;
	OpusDecoder *decoder;
	bool drain;
} keyed_ap_buffer;

SLL_DECLS(kab, keyed_ap_buffer, kab_list);
SLL_ITER_DECLS(kab, keyed_ap_buffer, kab_list, kab_iter);
SLL_POOL_DECLS(kab, keyed_ap_buffer, kab_list, kab_pool);

typedef struct {
	const audio_config *cfg;
	ap_pool audio_pool;
	struct {
		kab_pool buffer_pool;
		kab_list buffer_list;

		bool mixbuf_is_dirty;
		int32_t *mixbuf;
		int16_t *pcmbuf;
		size_t   pcmri;

	} out;
	struct {
		int tmp;
	} in;
	struct {
		snd_pcm_t *output;
		snd_pcm_t *input;
	} alsa;
} audio_manager;

bool am_setup(audio_manager *am, const audio_config *ac);
bool setup_alsa_output(audio_manager *am);
bool shutdown_alsa_output(audio_manager *am);
void ap_free(audio_packet *ap);
void kab_free(keyed_ap_buffer *kab);

void ap_post(audio_manager *am, audio_packet *ap);
void decode_and_mix(audio_manager *am);
bool write_alsa_output(audio_manager *am);
