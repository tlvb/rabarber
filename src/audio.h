#pragma once
#include <inttypes.h>
#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include "sll_meta.h"
#include "config.h"
#include "varint.h"
#include "packet_common.h"


// incoming voice packet targets
#define NORMAL 0
#define CHANNEL_WHISPER 1
#define USER_WHISPER 2
#define LOOPBACK 31

typedef struct keyed_ap_buffer {
	SLL_LINK(keyed_ap_buffer);
	int64_t key;
	bool prebuffering;
	p_list buffer;
	OpusDecoder *decoder;
	bool drain;
} keyed_ap_buffer;

SLL_DECLS(kab, keyed_ap_buffer, kab_list);
SLL_ITER_DECLS(kab, keyed_ap_buffer, kab_list, kab_iter);
SLL_POOL_DECLS(kab, keyed_ap_buffer, kab_list, kab_pool);

typedef struct {
	const audio_config *cfg;
	p_pool *packet_pool;
	struct {
		kab_pool buffer_pool;
		kab_list buffer_list;

		bool mixbuf_is_dirty;
		int32_t *mixbuf;
		int16_t *pcmbuf;
		size_t   pcmri;

	} play;
	struct {
		bool    recording;
		int16_t *pcmbuf;
		size_t   pcmwi;
		OpusEncoder *encoder;

		uint8_t target;
		int64_t seq;
		bool    islast;
	} cap;
	struct {
		snd_pcm_t *output;
		snd_pcm_t *input;
	} alsa;
} audio_manager;


bool am_setup(audio_manager *am, const audio_config *ac, p_pool *pool);
bool setup_alsa_output(audio_manager *am);
bool setup_alsa_input(audio_manager *am);
bool shutdown_alsa_output(audio_manager *am);
void kab_free(keyed_ap_buffer *kab);

void route_for_playback(audio_manager *am, packet *p);
bool decode_and_mix(audio_manager *am);
bool write_alsa_output(audio_manager *am);
bool get_alsa_input(audio_manager *am);

void fprint_audio_packet(FILE *f, const packet *p);
bool interpret_contents(packet *ap, bool local);

packet *build_opus_packet_from_captured_data(audio_manager *am);
void dissect_outgoing_opus_packet(const packet *p);

void start_recording(audio_manager *am);
void end_recording(audio_manager *am);
