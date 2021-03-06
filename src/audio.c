#include "audio.h"
#include <string.h>
#include <math.h>
#include "util.h"

#define AUDIO_FAIL_IF(cond, ...) if (cond) { fprintf(stderr, "(EE) AUDIO: " __VA_ARGS__); return false; }
#define AUDIO_WARN_IF(cond, ...) if (cond) { fprintf(stderr, "(WW) AUDIO: " __VA_ARGS__); }
#ifdef VERBOSE_AUDIO
#define AUDIO_VPRINTF(...) fprintf(stderr, "(DD) AUDIO " __VA_ARGS__)
#else
#define AUDIO_VPRINTF(...)
#endif

// max size of outgoing opus audio packet minus the encoded payload data
// packet header      2
// packet len         4
// packet type+target 1
// sequence number    9 varint
// payload len        9 varint, although in reality never > 0x3fff
#define AP_OUT_STATICS_UB (2+4+1+9+6)

SLL_DEFS(kab, keyed_ap_buffer, kab_list, kab_free);
SLL_ITER_DEFS(kab, keyed_ap_buffer, kab_list, kab_iter);
SLL_POOL_DEFS(kab, keyed_ap_buffer, kab_list, kab_pool);

#ifdef VERBOSE_AUDIO
static uint16_t capture_eagain_counter = 0;
#endif

bool am_setup(audio_manager *am, const audio_config *ac, p_pool *pool) { /*{{{*/
	am->cfg = ac;
	am->packet_pool = pool;
	kab_pclear(&am->play.buffer_pool);
	kab_lclear(&am->play.buffer_list);
	am->play.mixbuf_is_dirty = false;
	am->play.mixbuf = calloc(am->cfg->packetlen_samples, sizeof(int32_t));
	am->play.pcmbuf = calloc(am->cfg->packetlen_samples, sizeof(int16_t));
	am->play.pcmri = 0;
	am->cap.recording = false;
	am->cap.pcmbuf = calloc(am->cfg->packetlen_samples, sizeof(int16_t));
	am->cap.pcmwi = 0;

	int e;
	am->cap.encoder = opus_encoder_create(am->cfg->fs_Hz, 1, OPUS_APPLICATION_VOIP, &e);
	assert(e == OPUS_OK);
	opus_encoder_ctl(am->cap.encoder, OPUS_SET_BITRATE(am->cfg->bitrate_bps));
	opus_encoder_ctl(am->cap.encoder, OPUS_SET_INBAND_FEC(1));
	return setup_alsa_output(am) && setup_alsa_input(am) && setup_alsa_mixer(am);;
}/*}}}*/
bool setup_alsa_output(audio_manager *am) { /*{{{*/
	AUDIO_FAIL_IF( 0 != snd_pcm_open(&am->alsa.output, am->cfg->output_device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK),
	              "could not open output device %s\n", am->cfg->output_device);
	AUDIO_FAIL_IF( 0 != snd_pcm_set_params(am->alsa.output, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 1, am->cfg->fs_Hz, 1, am->cfg->output_latency_us),
	              "could not configure output device %s\n", am->cfg->output_device);
	return true;
}/*}}}*/
bool setup_alsa_input(audio_manager *am) { /*{{{*/
	AUDIO_FAIL_IF( 0 != snd_pcm_open(&am->alsa.input, am->cfg->input_device, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK),
	              "could not open input device %s\n", am->cfg->input_device);
	AUDIO_FAIL_IF( 0 != snd_pcm_set_params(am->alsa.input, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 1, am->cfg->fs_Hz, 1, am->cfg->input_latency_us),
	              "could not configure input device %s\n", am->cfg->input_device);
	return true;
} /*}}}*/
bool setup_alsa_mixer(audio_manager *am) { /*{{{*/
	AUDIO_FAIL_IF( 0 != snd_mixer_open(&am->alsa.mixer, 0),
	              "could not open mixer\n");
	AUDIO_FAIL_IF( 0 != snd_mixer_attach(am->alsa.mixer, am->cfg->mixer_device),
	              "could not attach mixer handle\n");
	AUDIO_FAIL_IF( 0 != snd_mixer_selem_register(am->alsa.mixer, NULL, NULL),
	              "could not register mixer\n");
	AUDIO_FAIL_IF( 0 != snd_mixer_load(am->alsa.mixer),
	              "could not load mixer elements\n");
	snd_mixer_selem_id_t *sid;
	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, am->cfg->output_volume_control);
	am->alsa.volume_control = snd_mixer_find_selem(am->alsa.mixer, sid);
	AUDIO_FAIL_IF( am->alsa.volume_control == NULL, "could not find volume control\n");
	long tmp = 0;
	snd_mixer_selem_get_playback_volume_range(am->alsa.volume_control, &am->alsa.volmin, &am->alsa.volmax);
	AUDIO_FAIL_IF( 0 != snd_mixer_selem_get_playback_volume(am->alsa.volume_control, 0, &tmp),
	              "could not get volume\n");
	printf("volume (%ld-%ld): %ld\n", am->alsa.volmin, am->alsa.volmax, tmp);

	uint32_t delta = am->alsa.volmax - am->alsa.volmin;
	if (delta < am->cfg->volume_steps) {
		am->alsa.volume = tmp;
	}
	else {
		am->alsa.volume = (tmp - am->alsa.volmin) * am->cfg->volume_steps / delta;
	}

	return true;
} /*}}}*/
bool shutdown_alsa_output(audio_manager *am) { /*{{{*/
	fprintf(stderr, "%p\n", am);
	return true;
}/*}}}*/

void kab_free(keyed_ap_buffer *kab) { /*{{{*/
	while (p_lsize(&kab->buffer) > 0) {
		packet *ap = p_lpopfront(&kab->buffer);
		p_free(ap);
	}
	free(kab);
} /*}}}*/
void route_for_playback(audio_manager *am, packet *ap) { /*{{{*/
	keyed_ap_buffer *kab = NULL;
	for (kab_iter kit=SLL_ISTART(&am->play.buffer_list); !kab_iisend(&kit); kab_inext(&kit)) {
		kab = kab_iget(&kit);
		if (kab->key == ap->opus.sid) {
			p_lpushback(&kab->buffer, ap);
			return;
		}
	}
	bool isnew = false;
	kab = kab_pgetm(&am->play.buffer_pool, &isnew);

	kab->key = ap->opus.sid;
	kab->prebuffering = true;
	kab->drain = ap->opus.islast;
	if (isnew) {
		kab_lnclear(kab);
		p_lclear(&kab->buffer);
		int e;
		kab->decoder = opus_decoder_create(am->cfg->fs_Hz, 1, &e);
		assert(e == OPUS_OK);
	}
	else {
		opus_decoder_ctl(kab->decoder, OPUS_RESET_STATE);
	}
	p_lpushback(&kab->buffer, ap);
	kab_lpushback(&am->play.buffer_list, kab);
} /*}}}*/
bool decode_and_mix(audio_manager *am) { /*{{{*/
	keyed_ap_buffer *kab = NULL;
	if (am->play.mixbuf_is_dirty) {
		bzero(am->play.mixbuf, am->cfg->packetlen_samples*sizeof(int32_t));
		am->play.mixbuf_is_dirty = false;
	}
	if (am->play.pcmri != am->cfg->packetlen_samples) {
		// this signifies that not all data has been loaded into alsa yet
		// so we cannot yet write anything to the buffer
		return false;
	}
	//printf("b\n");
	bool decoded = false;
	for (kab_iter kit=SLL_ISTART(&am->play.buffer_list); !kab_iisend(&kit); kab_inext(&kit)) {
		kab = kab_iget(&kit);
		//printf("c %" PRIi64 "\n", kab->key);
		if ((!kab->prebuffering) || (p_lsize(&kab->buffer) > am->cfg->prebuffer_amount)) {
			kab->prebuffering = false;
			packet *ap = p_lpopfront(&kab->buffer);
			if (ap != NULL) {
				decoded = true;
				int n = opus_decode(kab->decoder, ap->opus.data, ap->opus.len, am->play.pcmbuf, am->cfg->packetlen_samples, 0);
				p_preturn(am->packet_pool, ap);

				AUDIO_WARN_IF(n <= 0, "opus packet decoding error for from sid %" PRIi64 "\n", kab->key);

				for(size_t i=0; i<(size_t)n; ++i) {
					am->play.mixbuf[i] += am->play.pcmbuf[i];
				}
				am->play.mixbuf_is_dirty = true;
			}
			if (p_lsize(&kab->buffer) == 0 && kab->drain) {
				kab_ipop(&kit);
				kab_preturn(&am->play.buffer_pool, kab);
			}
		}
	}
	bool tone = false;
	for (size_t i=0; i<2; ++i) {
		if (am->play.tone[i].t > 0) {
			tone = true;
			for (size_t j=0; j<am->cfg->packetlen_samples; ++j) {
				am->play.mixbuf[j] += (int16_t) ((INT16_MAX>>1)*sinf(((float)am->play.tone[i].i)*((float)am->play.tone[i].f)*6.28318530717959f/((float)am->cfg->fs_Hz)));
				++am->play.tone[i].i;
			}
			// approximate time-keeping
			uint32_t n_ms = am->cfg->fs_Hz / 1000;
			if (am->play.tone[i].t > n_ms) {
				am->play.tone[i].t -= n_ms;
			}
			else {
				am->play.tone[i].t = 0;
			}
			if (am->play.tone[i].t == 0) {
				am->play.tone[i].f = 0;
			}
			break;
		}
	}
	// clip
	if (decoded || tone) {

#ifdef PRINTWAVE
		printwave(PRINTWAVEHEIGHT/4+1,1, am->play.pcmbuf, am->cfg->packetlen_samples);
#endif

		for (size_t i=0; i<am->cfg->packetlen_samples; ++i) {
			if (am->play.mixbuf[i] > INT16_MAX) {
				am->play.pcmbuf[i] = INT16_MAX;
			}
			else if (am->play.mixbuf[i] < INT16_MIN) {
				am->play.pcmbuf[i] = INT16_MIN;
			}
			else {
				am->play.pcmbuf[i] = (int16_t)am->play.mixbuf[i];
			}
		}
		am->play.pcmri = 0;
		return true;
	}
	return false;
} /*}}}*/
bool write_alsa_output(audio_manager *am) { /*{{{*/
	if (am->play.pcmri >= am->cfg->packetlen_samples) { return false; }
	snd_pcm_sframes_t n = snd_pcm_writei(am->alsa.output, am->play.pcmbuf+am->play.pcmri, am->cfg->packetlen_samples-am->play.pcmri);
	if (n == -EAGAIN) {
		return false;
	}
	if (n < 0) {
		n = snd_pcm_recover(am->alsa.output, n, 0);
	}
	if (n < 0) {
		//fprintf(stderr, "snd_pcm_writei failed %s\n", snd_strerror(n));
		return true;
	}
	if (n >= 0) {
		am->play.pcmri += n;
	}
	return true;
}/*}}}*/
bool get_alsa_input(audio_manager *am) { /*{{{*/
	assert(am != NULL);
	if (!am->cap.recording) { return false; }
	if (am->cap.pcmwi == am->cfg->packetlen_samples) { return false; }
	snd_pcm_sframes_t n = snd_pcm_readi(am->alsa.input, am->cap.pcmbuf+am->cap.pcmwi, am->cfg->packetlen_samples-am->cap.pcmwi);
#ifdef VERBOSE_AUDIO
	if (capture_eagain_counter >= 0x0400 || (capture_eagain_counter > 0 && n != -EAGAIN)) {
		AUDIO_VPRINTF("eagain count=%u\n", capture_eagain_counter);
		capture_eagain_counter = 0;
	}
#endif
	if (n == -EAGAIN) {
#ifdef VERBOSE_AUDIO
		++capture_eagain_counter;
#endif
		return false;
	}
	if (n < 0) {
		n = snd_pcm_recover(am->alsa.input, n, 0);
		snd_pcm_reset(am->alsa.input);
		snd_pcm_prepare(am->alsa.input);
		snd_pcm_start(am->alsa.input);
	}
	if (n < 0) {
		AUDIO_VPRINTF("snd_pcm_readi failed %s\n", snd_strerror(n));
		return true;
	}
	if (n >= 0) {
		am->cap.pcmwi += n;
	}
	return true;
} /*}}}*/
void fprint_audio_packet(FILE *f, const packet *ap) { /*{{{*/
	assert(ap != NULL);

	if (ap->type == PING) {
		fprintf(f, "[ping, ts=%016"PRIx64"]\n", ap->ping.timestamp);
	}
	else if (ap->type == OPUS) {
		fprintf(f,
		        "[opus, target=%02x, sid=%016" PRIx64 ", seq=%016" PRIx64 ", dlen=%04" PRIu16 ", last?=%1c]\n",
		        ap->opus.target,
		        ap->opus.sid,
		        ap->opus.seq,
		        ap->opus.len,
		        ap->opus.islast?'y':'n');
	}
	else {
		fprintf(f,
		        "[UNKNOWN PACKET TYPE] %02x\n",
		        ap->type);
	}
} /*}}}*/
bool interpret_contents(packet *ap, bool local) { /*{{{*/
	assert(ap != NULL);
	assert(ap->data != NULL);

	uint8_t *dptr = ap->raw.data+6;
	size_t   left = ap->raw.len-6;

	ap->type = dptr[0] & 0xe0; // don't increment dptr immediately, we may want to extract target as well

	if (ap->type == PING) {
		left -= 1; dptr += 1;
		varint_decode(&ap->ping.timestamp, dptr, left);
		return true;
	}
	else if (ap->type == OPUS) {
		ap->opus.target = dptr[0] & 0x1f;
		left -= 1; dptr += 1;

		size_t delta = 0;

		if (local) {
			ap->opus.sid = 0x2320c0ffee15dea7;
		}
		else {
			delta = varint_decode(&ap->opus.sid, dptr, left);
			left -= delta; dptr += delta;
		}

		delta = varint_decode(&ap->opus.seq, dptr, left);
		left -= delta; dptr += delta;

		if (ap->type == OPUS) {
			int64_t tmp = 0;
			delta = varint_decode(&tmp, dptr, left);
			left -= delta; dptr += delta;

			ap->opus.len = tmp & 0x1fff;
			ap->opus.islast = (tmp & 0x2000) != 0;
			ap->opus.data = dptr;

			return true;
		}
	}
	else {
		AUDIO_FAIL_IF(ap->type != PING && ap->type != OPUS, "packet type is not implemented: 0x%02u\n", ap->type);
	}
	return false;
}/*}}}*/
packet *build_opus_packet_from_captured_data(audio_manager *am) { /*{{{*/

	if (am->cap.pcmwi < am->cfg->packetlen_samples) { return NULL; }
	am->cap.pcmwi = 0;

#ifdef PRINTWAVE
	printwave(1, 1, am->cap.pcmbuf, am->cfg->packetlen_samples);
#endif


	const size_t enc_len_est = am->cfg->packetlen_us * am->cfg->bitrate_bps / 8000000;
	packet *ap = get_packet(am->packet_pool, AP_OUT_STATICS_UB + enc_len_est*3/2);

	uint8_t metadata[AP_OUT_STATICS_UB];

	assert(ap != NULL);
	assert(ap->data != NULL);

	ap->type = OPUS;
	ap->opus.target = am->cap.target;
	ap->opus.seq    = am->cap.seq;
	am->cap.seq += 2;
	ap->opus.islast = am->cap.islast;
	if (am->cap.islast) {
		snd_pcm_drop(am->alsa.input);
		am->cap.recording = false;
	}

	uint16_t m = 2+4;

	metadata[m] = ap->type | (ap->opus.target & 0x1f);
	++m;

	m += varint_encode(&metadata[m], 9, ap->opus.seq);

	uint16_t n = ap->opus.len = opus_encode(am->cap.encoder, am->cap.pcmbuf, am->cfg->packetlen_samples, ap->data+AP_OUT_STATICS_UB, (ap->dsz-AP_OUT_STATICS_UB)&0x1fff);

	m += varint_encode(&metadata[m], 9, (ap->opus.islast ? 0x2000 : 0x0000) | n);

	ap->raw.data = ap->data+AP_OUT_STATICS_UB-m;
	ap->raw.len  = m + n;
	uint32_t plen = ap->raw.len-6;
	ap->raw.type = 1;

	metadata[0] = (ap->raw.type >> 8) & 0xff;
	metadata[1] = (ap->raw.type     ) & 0xff;
	metadata[2] = (plen >> 24) & 0xff;
	metadata[3] = (plen >> 16) & 0xff;
	metadata[4] = (plen >>  8) & 0xff;
	metadata[5] = (plen      ) & 0xff;
	for (uint16_t i=0; i<m; ++i) {
		ap->data[AP_OUT_STATICS_UB-m+i] = metadata[i];
	}

	return ap;
} /*}}}*/

void dissect_outgoing_opus_packet(const packet *p) { /*{{{*/
	const uint8_t *dp = p->raw.data;

	uint16_t type = ((dp[0] >> 8) & 0xff) | dp[1];
	uint32_t len  = ((dp[2] << 24) & 0xff) | ((dp[3] << 16) & 0xff) | ((dp[4] << 8) & 0xff) | ((dp[5] & 0xff));
	uint8_t  typetarget = dp[6];

	uint8_t atype = typetarget & 0xe0;
	uint8_t target = typetarget & 0x1f;

	size_t d1 = 0, d2 = 0;

	int64_t seqno = 0; d1 = varint_decode(&seqno, &dp[7], 9);

	int64_t lth   = 0; d2 = varint_decode(&lth, &dp[7+d1], 9);

	bool last = (lth & 0x2000) != 0;

	size_t dplen = lth & 0x1fff;
	size_t mplen = len - (7+d1+d2);

	printf("[H| T:%" PRIu16 " L:%" PRIu32 " |P| at:%x tg:%x sn:%" PRIi64 " l:%u dpl:%zu cpl:%zu]\n",
	       type,
	       len,
	       atype,
	       target,
	       seqno,
	       last,
	       dplen,
	       mplen);
} /*}}}*/

void start_recording(audio_manager *am) { /*{{{*/
	am->cap.islast = false;
	am->cap.recording = true;
	am->cap.pcmwi = 0;
	opus_encoder_ctl(am->cap.encoder, OPUS_RESET_STATE);
	snd_pcm_reset(am->alsa.input);
	snd_pcm_prepare(am->alsa.input);
	snd_pcm_start(am->alsa.input);
}/*}}}*/
void end_recording(audio_manager *am) { /*{{{*/
	am->cap.islast = true;
}/*}}}*/

void increase_volume(audio_manager *am) { /*{{{*/
	uint32_t delta = am->alsa.volmax - am->alsa.volmin;
	if (delta < am->cfg->volume_steps) {
		am->alsa.volume = am->alsa.volume >= am->alsa.volmax ? am->alsa.volmax : am->alsa.volume + 1;
	}
	else {
		am->alsa.volume = am->alsa.volume >= (long)am->cfg->volume_steps ? (long)am->cfg->volume_steps : am->alsa.volume + 1;
		snd_mixer_selem_set_playback_volume_all(am->alsa.volume_control,
		                                        am->alsa.volume*delta/am->cfg->volume_steps+am->alsa.volmin);
	}
	printf("%ld\n", am->alsa.volume);
}/*}}}*/
void decrease_volume(audio_manager *am) { /*{{{*/
	uint32_t delta = am->alsa.volmax - am->alsa.volmin;
	if (delta < am->cfg->volume_steps) {
		am->alsa.volume = am->alsa.volume <= am->alsa.volmin ? am->alsa.volmin : am->alsa.volume - 1;
	}
	else {
		am->alsa.volume = am->alsa.volume <= 0 ? 0 : am->alsa.volume - 1;
		snd_mixer_selem_set_playback_volume_all(am->alsa.volume_control,
		                                        am->alsa.volume*delta/am->cfg->volume_steps+am->alsa.volmin);
	}
	printf("%ld\n", am->alsa.volume);
}/*}}}*/

void play_2tone(audio_manager *am, uint32_t f0, uint32_t t0, uint32_t f1, uint32_t t1) { /*{{{*/
	am->play.tone[0].f = f0;
	am->play.tone[0].t = t0;
	am->play.tone[0].i = 0;
	am->play.tone[1].f = f1;
	am->play.tone[1].t = t1;
	am->play.tone[1].i = 0;
} /*}}}*/
