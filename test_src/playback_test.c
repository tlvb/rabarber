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
	ac.prebuffer_amount = 4;
	ac.input_latency_us = 200000;

	ac.packetlen_samples = ac.fs_Hz * ac.packetlen_us / 1000000;

	audio_manager am = {0};

	am_setup(&am, &ac);


	int16_t sine[ac.packetlen_samples];
	int16_t noise[ac.packetlen_samples];

	// samples/s = 48000
	// periods/s = 480
	// sample/period = (samples/s) / (periods/s)
	//               = 100


	double t=0;

	int e;
	OpusEncoder *sineoe = opus_encoder_create(ac.fs_Hz, 1, OPUS_APPLICATION_VOIP, &e);
	assert(e==OPUS_OK);
	opus_encoder_ctl(sineoe, OPUS_SET_BITRATE(ac.bitrate_bps));
	OpusEncoder *noiseoe = opus_encoder_create(ac.fs_Hz, 1, OPUS_APPLICATION_VOIP, &e);
	assert(e==OPUS_OK);
	opus_encoder_ctl(noiseoe, OPUS_SET_BITRATE(ac.bitrate_bps));


	printf("PLAYBACK\n");
	for (;;) {

		usleep(9*ac.packetlen_us/10);

		audio_packet *sinepacket  = ap_pget(&am.audio_pool);
		if (sinepacket->audio.opus.data == NULL) {
			sinepacket->audio.opus.data = malloc(100*sizeof(uint8_t));
		}
		for (size_t i=0; i<ac.packetlen_samples; ++i) {
			sine[i] = INT16_MAX * sin(M_PI*2*t*440/ac.fs_Hz);
			t+=1;
		}
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
		noisepacket->audio.opus.len = opus_encode(noiseoe, noise, ac.packetlen_samples, noisepacket->audio.opus.data, 100);
		sinepacket->audio.opus.islast = false;
		noisepacket->audio.sid = 2;
		ap_post(&am, noisepacket);

		for (int i=0; i<50; ++i) {
			printwave(noise[i]); printf(" || ");
			printwave(sine[i]); printf("\n");
		}

		printf("encoded n:%zu s:%zu bytes\n", noisepacket->audio.opus.len, sinepacket->audio.opus.len);
		decode_and_mix(&am);
		write_alsa_output(&am);


	}




}
