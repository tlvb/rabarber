#include "audio.h"
#include <string.h>
#include <math.h>

#define AUDIO_FAIL_IF(cond, ...) if (cond) { fprintf(stderr, "(EE) AUDIO: " __VA_ARGS__); return false; }

SLL_DEFS(ap, audio_packet, ap_list, ap_free);
SLL_POOL_DEFS(ap, audio_packet, ap_list, ap_pool);

SLL_DEFS(kab, keyed_ap_buffer, kab_list, kab_free);
SLL_ITER_DEFS(kab, keyed_ap_buffer, kab_list, kab_iter);
SLL_POOL_DEFS(kab, keyed_ap_buffer, kab_list, kab_pool);

bool am_setup(audio_manager *am, const audio_config *ac) { /*{{{*/
	am->cfg = ac;
	ap_pclear(&am->audio_pool);
	kab_pclear(&am->out.buffer_pool);
	kab_lclear(&am->out.buffer_list);
	am->out.mixbuf_is_dirty = false;
	am->out.mixbuf = calloc(am->cfg->packetlen_samples, sizeof(int32_t));
	am->out.pcmbuf = calloc(am->cfg->packetlen_samples, sizeof(int16_t));
	return setup_alsa_output(am);
}/*}}}*/
bool setup_alsa_output(audio_manager *am) { /*{{{*/
	snd_pcm_open(&am->alsa.output, am->cfg->output_device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	snd_pcm_set_params(am->alsa.output, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, 1, am->cfg->fs_Hz, 1, am->cfg->input_latency_us);
	return true;
}/*}}}*/
bool shutdown_alsa_output(audio_manager *am) { /*{{{*/
	fprintf(stderr, "%p\n", am);
	return true;
}/*}}}*/

void ap_free(audio_packet *ap) { /*{{{*/
	assert(ap != NULL);
	if (ap->data != NULL) {
		// free(ap->data);
	}
	free(ap);
} /*}}}*/
void kab_free(keyed_ap_buffer *kab) { /*{{{*/
	while (ap_lsize(&kab->buffer) > 0) {
		audio_packet *ap = ap_lpopfront(&kab->buffer);
		ap_free(ap);
	}
	free(kab);
} /*}}}*/
audio_packet *get_audio_packet(audio_manager *am, size_t min_size) { /*{{{*/
	audio_packet *ap = ap_pget(&am->audio_pool);
	assert(ap != NULL);
	if (ap->dsz < min_size) {
		size_t target_size = min_size>256?min_size:256;
		ap->data = realloc(ap->data, target_size);
		assert(ap->data != NULL);
		ap->dsz = target_size;
	}
	return ap;
} /*}}}*/
void ap_post(audio_manager *am, audio_packet *ap) { /*{{{*/
	keyed_ap_buffer *kab = NULL;
	for (kab_iter kit=SLL_ISTART(&am->out.buffer_list); !kab_iisend(&kit); kab_inext(&kit)) {
		kab = kab_iget(&kit);
		if (kab->key == ap->audio.sid) {
			ap_lpushback(&kab->buffer, ap);
			return;
		}
	}
	bool isnew = false;
	kab = kab_pgetm(&am->out.buffer_pool, &isnew);

	kab->key = ap->audio.sid;
	kab->prebuffering = true;
	kab->drain = ap->audio.opus.islast;
	if (isnew) {
		kab_lnclear(kab);
		ap_lclear(&kab->buffer);
		int e;
		kab->decoder = opus_decoder_create(am->cfg->fs_Hz, 1, &e);
		assert(e == OPUS_OK);
	}
	else {
		opus_decoder_ctl(kab->decoder, OPUS_RESET_STATE);
	}
	ap_lpushback(&kab->buffer, ap);
	kab_lpushback(&am->out.buffer_list, kab);
} /*}}}*/
bool decode_and_mix(audio_manager *am) { /*{{{*/
	keyed_ap_buffer *kab = NULL;
	if (am->out.mixbuf_is_dirty) {
		bzero(am->out.mixbuf, am->cfg->packetlen_samples*sizeof(int32_t));
		am->out.mixbuf_is_dirty = false;
	}
	if (am->out.pcmri != am->cfg->packetlen_samples) {
		// this signifies that not all data has been loaded into alsa yet
		// so we cannot yet write anything to the buffer
		return false;
	}
	bool decoded = false;
	for (kab_iter kit=SLL_ISTART(&am->out.buffer_list); !kab_iisend(&kit); kab_inext(&kit)) {
		kab = kab_iget(&kit);
		if ((!kab->prebuffering) || (ap_lsize(&kab->buffer) > am->cfg->prebuffer_amount)) {
			kab->prebuffering = false;
			audio_packet *ap = ap_lpopfront(&kab->buffer);
			if (ap != NULL) {
				decoded = true;
				int n = opus_decode(kab->decoder, ap->audio.opus.data, ap->audio.opus.len, am->out.pcmbuf, am->cfg->packetlen_samples, 0);
				ap_preturn(&am->audio_pool, ap);
				assert(n > 0);

				for(size_t i=0; i<(size_t)n; ++i) {
					am->out.mixbuf[i] += am->out.pcmbuf[i];
				}
				am->out.mixbuf_is_dirty = true;
			}
			if (ap_lsize(&kab->buffer) == 0 && kab->drain) {
				kab_ipop(&kit);
				kab_preturn(&am->out.buffer_pool, kab);
			}
		}
	}
	// clip
	if (decoded) {
		for (size_t i=0; i<am->cfg->packetlen_samples; ++i) {
			if (am->out.mixbuf[i] > INT16_MAX) {
				am->out.pcmbuf[i] = INT16_MAX;
			}
			else if (am->out.mixbuf[i] < INT16_MIN) {
				am->out.pcmbuf[i] = INT16_MIN;
			}
			else {
				am->out.pcmbuf[i] = (int16_t)am->out.mixbuf[i];
			}
		}
		am->out.pcmri = 0;
		return true;
	}
	return false;
} /*}}}*/
bool write_alsa_output(audio_manager *am) { /*{{{*/
	if (am->out.pcmri >= am->cfg->packetlen_samples) { return false; }
	snd_pcm_sframes_t n = snd_pcm_avail(am->alsa.output);
	n = snd_pcm_writei(am->alsa.output, am->out.pcmbuf+am->out.pcmri, am->cfg->packetlen_samples-am->out.pcmri);
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
		am->out.pcmri += n;
	}
	return true;
}/*}}}*/

void fprint_audio_packet(FILE *f, const audio_packet *ap) { /*{{{*/
	assert(ap != NULL);
	if (ap->type == PING) {
		fprintf(f, "[ping, ts=%016"PRIx64"]\n", ap->ping.timestamp);
	}
	else if (ap->type == OPUS) {
		fprintf(f,
		        "[opus, target=%02x, sid=%016" PRIx16 ", seq=%016" PRIx16 ", dlen=%04" PRIu16 ", last?=%1c]\n",
		        ap->audio.target,
		        ap->audio.sid,
		        ap->audio.seq,
		        ap->audio.opus.len,
		        ap->audio.opus.islast?'y':'n');
	}
} /*}}}*/
bool interpret_contents(audio_packet *ap) { /*{{{*/
	assert(ap != NULL);
	assert(ap->data != NULL);
	ap->type = ap->data[0] & 0xe0;
	int64_t tmp;
	if (ap->type == PING) {
		varint_decode(&ap->ping.timestamp, ap->data+1, ap->dsz-1);
		return true;
	}
	else {
		ap->audio.target = ap->data[0] & 0x1f;

		uint8_t *dptr = ap->data + 1;
		size_t left = ap->dsz - 1;

		size_t delta = varint_decode(&tmp, dptr, left);
		ap->audio.sid = tmp & 0xffff;
		left -= delta;
		dptr += delta;

		delta = varint_decode(&tmp, dptr, left);
		ap->audio.seq = tmp & 0xffff;
		left -= delta;
		dptr += delta;

		if (ap->type == OPUS) {
			tmp = 0;
			delta = varint_decode(&tmp, dptr, left);
			left -= delta;
			dptr += delta;

			ap->audio.opus.len = tmp & 0x1fff;
			ap->audio.opus.islast = (tmp & 0x2000) != 0;
			ap->audio.opus.data = dptr;

			return true;
		}
	}
	return false;
}/*}}}*/
