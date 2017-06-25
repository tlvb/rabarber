#include "../src/audio.h"
#include <math.h>
#include <opus/opus.h>

void printwave(int16_t s);
void printwave(int16_t s) { /*{{{*/
	int16_t v = s/2000;
	if (v < 0) {
		for(int i=0; i<16+v; ++i) {
			printf(" ");
		}
		for(int i=0; i<-v; ++i) {
			printf("#");
		}
	}
	else {
		for (int i=0; i<16; ++i) {
			printf(" ");
		}
	}
	printf("|");
	if (v > 0) {
		for(int i=0; i<v; ++i) {
			printf("#");
		}
		for(int i=0; i<16-v; ++i) {
			printf(" ");
		}
	}
	else {
		for (int i=0; i<16; ++i) {
			printf(" ");
		}
	}
} /*}}}*/
int main(void) {

	audio_config ac;
	ac.output_device = "default";
	ac.fs_Hz = 48000;
	ac.bitrate_bps = 20000;
	ac.packetlen_us = 20000;
	ac.prebuffer_amount = 2;
	ac.output_latency_us = 2000;

	ac.packetlen_samples = ac.fs_Hz * ac.packetlen_us / 1000000;

	audio_manager am = {0};

	am_setup(&am, &ac);


	int16_t sine[ac.packetlen_samples];
	int16_t noise[ac.packetlen_samples];

	double t=0;

	int e;
	OpusEncoder *sineoe = opus_encoder_create(ac.fs_Hz, 1, OPUS_APPLICATION_VOIP, &e);
	assert(e==OPUS_OK);
	opus_encoder_ctl(sineoe, OPUS_SET_BITRATE(ac.bitrate_bps));
	OpusEncoder *noiseoe = opus_encoder_create(ac.fs_Hz, 1, OPUS_APPLICATION_VOIP, &e);
	assert(e==OPUS_OK);
	opus_encoder_ctl(noiseoe, OPUS_SET_BITRATE(ac.bitrate_bps));

	bool needsdata = true;
	bool decoded = true;
	bool played = true;

	printf("PLAYBACK\n");
	for (;;) {
		printf("needs:%u decoded:%u played:%u\n", needsdata, decoded, played);

		if (needsdata) {
			audio_packet *sinepacket  = ap_pget(&am.audio_pool);
			if (sinepacket->audio.opus.data == NULL) {
				sinepacket->audio.opus.data = malloc(100*sizeof(uint8_t));
			}
			for (size_t i=0; i<ac.packetlen_samples; ++i) {
				sine[i] = INT16_MAX * sin(M_PI*2*t*440/ac.fs_Hz);
				t+=1;
			}
			sinepacket->type = OPUS;
			sinepacket->audio.opus.len = opus_encode(sineoe, sine, ac.packetlen_samples, sinepacket->audio.opus.data, 100);
			sinepacket->audio.opus.islast = false;
			sinepacket->audio.sid = 1;

			ap_post(&am, sinepacket);

			audio_packet *noisepacket = ap_pget(&am.audio_pool);
			if (noisepacket->audio.opus.data == NULL) {
				noisepacket->audio.opus.data = malloc(100*sizeof(uint8_t));
			}
			for (size_t i=0; i<ac.packetlen_samples; ++i) {
				noise[i] = (int16_t) (rand()&0xffff);
			}
			noisepacket->type = OPUS;
			noisepacket->audio.opus.len = opus_encode(noiseoe, noise, ac.packetlen_samples, noisepacket->audio.opus.data, 100);
			noisepacket->audio.opus.islast = false;
			noisepacket->audio.sid = 2;
			ap_post(&am, noisepacket);
		}

		/* debugging only, we are breaking encapsulation levels here */
		needsdata = kab_lsize(&am.out.buffer_list) == 0;
		for (kab_iter kit = SLL_ISTART(&am.out.buffer_list); !kab_iisend(&kit); kab_inext(&kit)) {
			keyed_ap_buffer *kab = kab_iget(&kit);
			printf("buffer %" PRIu16 " size: %zu\n", kab->key, ap_lsize(&kab->buffer));
			if (ap_lsize(&kab->buffer) < 4) {
				needsdata = true;
			}
		}

		//fprint_audio_packet(stderr, noisepacket);
		//fprint_audio_packet(stderr, sinepacket);

		decoded = decode_and_mix(&am);
		played = write_alsa_output(&am);

		if (!decoded && !played) {
			printf("sleeping %uus\n", ac.packetlen_us/2);
			usleep(ac.packetlen_us/2);
		}
	}
}
