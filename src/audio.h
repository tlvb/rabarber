#pragma once
#include <inttypes.h>
#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include "sll_meta.h"
#include "config.h"
#include "varint.h"

// audio packet types
#define PING 32
#define OPUS 128

// incoming voice packet targets
#define NORMAL 0
#define CHANNEL_WHISPER 1
#define USER_WHISPER 2
#define LOOPBACK 31

typedef struct audio_packet {
	SLL_LINK(audio_packet);
	uint8_t *data;
	size_t   dsz;
	uint8_t  type;
	union {
		struct {
			int64_t timestamp;
		} ping;
		struct {
			uint8_t  target;
			int64_t sid;
			int64_t seq;
			struct {
				uint8_t *data;
				int16_t len; // protocol specifies max len of 0x1fff;
				bool islast;
			} opus;
		} audio;
		struct {
			uint8_t *data;
			uint16_t len;
		} raw;
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

	} play;
	struct {
		int16_t *pcmbuf;
		size_t   pcmwi;
		OpusEncoder *encoder;

		uint8_t target;
		int64_t sid;
		int64_t seq;
		bool    islast;
	} cap;
	struct {
		snd_pcm_t *output;
		snd_pcm_t *input;
	} alsa;
} audio_manager;

bool am_setup(audio_manager *am, const audio_config *ac);
bool setup_alsa_output(audio_manager *am);
bool setup_alsa_input(audio_manager *am);
bool shutdown_alsa_output(audio_manager *am);
void ap_free(audio_packet *ap);
void kab_free(keyed_ap_buffer *kab);

void ap_post(audio_manager *am, audio_packet *ap);
bool decode_and_mix(audio_manager *am);
bool write_alsa_output(audio_manager *am);
bool get_alsa_input(audio_manager *am);

void fprint_audio_packet(FILE *f, const audio_packet *ap);
bool interpret_contents(audio_packet *ap, bool local);

audio_packet *build_opus_packet_from_captured_data(audio_manager *am);
