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
	fprintf(stderr, "ap->sid: %" PRId64 "\n", ap->audio.sid);
	fprintf(stderr, "bl->n: %zu\n", kab_lsize(&am->out.buffer_list));
	keyed_ap_buffer *kab = NULL;
	for (kab_iter kit=SLL_ISTART(&am->out.buffer_list); !kab_iisend(&kit); kab_inext(&kit)) {
		kab = kab_iget(&kit);
		fprintf(stderr, "%p, %ld\n", kab, kab->key);
		if (kab->key == ap->audio.sid) {
			ap_lpushback(&kab->buffer, ap);
			return;
		}
	}
	fprintf(stderr, "found no matching buffer\n");
	bool isnew = false;
	kab = kab_pgetm(&am->out.buffer_pool, &isnew);

	kab->key = ap->audio.sid;
	kab->prebuffering = true;
	kab->drain = ap->audio.opus.islast;
	if (isnew) {
		fprintf(stderr, "created a new buffer\n");
		kab_lnclear(kab);
		ap_lclear(&kab->buffer);
		int e;
		kab->decoder = opus_decoder_create(am->cfg->fs_Hz, 1, &e);
		assert(e == OPUS_OK);
	}
	else {
		fprintf(stderr, "reused an old buffer\n");
		opus_decoder_ctl(kab->decoder, OPUS_RESET_STATE);
	}
	ap_lpushback(&kab->buffer, ap);
	kab_lpushback(&am->out.buffer_list, kab);
} /*}}}*/
void decode_and_mix(audio_manager *am) { /*{{{*/
	keyed_ap_buffer *kab = NULL;
	if (am->out.mixbuf_is_dirty) {
		bzero(am->out.mixbuf, am->cfg->packetlen_samples*sizeof(int32_t));
		am->out.mixbuf_is_dirty = false;
	}
	if (am->out.pcmri != am->cfg->packetlen_samples) {
		// this signifies that not all data has been loaded into alsa yet
		// so we cannot yet write anything to the buffer
		return;
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
	}
} /*}}}*/
bool write_alsa_output(audio_manager *am) { /*{{{*/
	if (am->out.pcmri >= am->cfg->packetlen_samples) { return false; }
	snd_pcm_sframes_t n = snd_pcm_avail(am->alsa.output);
	printf("alsa is ready to receive %ld frames\n", n);
	n = snd_pcm_writei(am->alsa.output, am->out.pcmbuf+am->out.pcmri, am->cfg->packetlen_samples-am->out.pcmri);
	if (n < 0) {
		n = snd_pcm_recover(am->alsa.output, n, 0);
	}
	if (n < 0) {
		fprintf(stderr, "snd_pcm_writei failed %s\n", snd_strerror(n));
	}
	if (n >= 0) {
		printf("wrote %ld frames\n", n);
		am->out.pcmri += n;
	}















	return true;
//	snd_pcm_state_t state = snd_pcm_state(am->alsa.output);
//	if (state != SND_PCM_STATE_PREPARED && state != SND_PCM_STATE_RUNNING) {
//		if (state == SND_PCM_STATE_SUSPENDED) {
//			snd_pcm_resume(am->alsa.output);
//		}
//		else {
//			snd_pcm_prepare(am->alsa.output);
//		}
//	}
//	printf("there are %zu samples to play\n", am->cfg->packetlen_samples-am->out.pcmri);
//
//
//
//
//
//
//
//
//
//	//printwave2(am->out.pcmbuf+am->out.pcmri, am->cfg->packetlen_samples-am->out.pcmri);
//	snd_pcm_sframes_t ret = 0;
//	do {
//		ret = snd_pcm_writei(am->alsa.output, am->out.pcmbuf+am->out.pcmri, am->cfg->packetlen_samples-am->out.pcmri);
//		if (ret >=0 && (size_t)ret < am->cfg->packetlen_samples-am->out.pcmri) {
//			am->out.pcmri += ret;
//			ret = -EAGAIN;
//		}
//	} while (ret == -EAGAIN);
//	if (ret >= 0) {
//		am->out.pcmri += ret;
//	}
//	else {
//		switch (ret) {
//			case -EAGAIN:
//				fprintf(stderr, "eagain: %s\n", snd_strerror(ret));
//				return true;
//			case -EBADFD:
//				fprintf(stderr, "ebadfd: %s\n", snd_strerror(ret));
//				ret = snd_pcm_prepare(am->alsa.output);
//				if (ret < 0) {
//					fprintf(stderr, "audio playback error: %s\n", snd_strerror(ret));
//				}
//				break;
//			case -EPIPE:
//			//case -ESTRPIPE:
//				fprintf(stderr, "epipe or estrpipe: %s\n", snd_strerror(ret));
//				ret = snd_pcm_recover(am->alsa.output, ret, 0);
//				while (ret < 0) {
//					fprintf(stderr, "audio playback error: %s\n", snd_strerror(ret));
//					ret = snd_pcm_prepare(am->alsa.output);
//				}
//				break;
//			default:
//				fprintf(stderr, "uncaught audio playback error: %s\n", snd_strerror(ret));
//				break;
//		}
//		return false;
//	}
//	return true;
}/*}}}*/
