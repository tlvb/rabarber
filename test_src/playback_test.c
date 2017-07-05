#include "../src/audio.h"
#include "../src/util.h"
#include <math.h>
#include <opus/opus.h>

int main(int argc, char **argv) {

	if (argc != 2) {
		fprintf(stderr, "needs config file parameter\n");
		return 1;
	}

	config cfg;

	if (!load_config_from_file(&cfg, argv[1])) {
		fprintf(stderr, "config file load error\n");
		return 1;
	}
	print_config_to_stdout(&cfg);
	printf("packetlen_samples calculated to %u\n", cfg.audio.packetlen_samples);

	audio_manager am = {0};

	p_pool packet_pool = {0};
	am_setup(&am, &cfg.audio, &packet_pool);


	int16_t sine[cfg.audio.packetlen_samples];
	int16_t noise[cfg.audio.packetlen_samples];

	double t=0;

	int e;
	OpusEncoder *sineoe = opus_encoder_create(cfg.audio.fs_Hz, 1, OPUS_APPLICATION_VOIP, &e);
	assert(e==OPUS_OK);
	opus_encoder_ctl(sineoe, OPUS_SET_BITRATE(cfg.audio.bitrate_bps));
	OpusEncoder *noiseoe = opus_encoder_create(cfg.audio.fs_Hz, 1, OPUS_APPLICATION_VOIP, &e);
	assert(e==OPUS_OK);
	opus_encoder_ctl(noiseoe, OPUS_SET_BITRATE(cfg.audio.bitrate_bps));

	bool needsdata = true;
	bool decoded = true;
	bool played = true;

	printf("PLAYBACK\n");
	for (;;) {

		if (needsdata) {
			packet *sinepacket  = p_pget(am.packet_pool);
			if (sinepacket->opus.data == NULL) {
				sinepacket->opus.data = malloc(100*sizeof(uint8_t));
			}
			for (size_t i=0; i<cfg.audio.packetlen_samples; ++i) {
				sine[i] = INT16_MAX * sin(M_PI*2*t*440/cfg.audio.fs_Hz);
				t+=1;
			}
			sinepacket->type = OPUS;
			sinepacket->opus.len = opus_encode(sineoe, sine, cfg.audio.packetlen_samples, sinepacket->opus.data, 100);
			sinepacket->opus.islast = false;
			sinepacket->opus.sid = 1;

			route_for_playback(&am, sinepacket);

			packet *noisepacket = p_pget(am.packet_pool);
			if (noisepacket->opus.data == NULL) {
				noisepacket->opus.data = malloc(100*sizeof(uint8_t));
			}
			for (size_t i=0; i<cfg.audio.packetlen_samples; ++i) {
				noise[i] = (int16_t) (rand()&0xffff);
			}
			noisepacket->type = OPUS;
			noisepacket->opus.len = opus_encode(noiseoe, noise, cfg.audio.packetlen_samples, noisepacket->opus.data, 100);
			noisepacket->opus.islast = false;
			noisepacket->opus.sid = 2;
			route_for_playback(&am, noisepacket);

			fprint_audio_packet(stderr, noisepacket);
			fprint_audio_packet(stderr, sinepacket);


		}
		printf("needs:%u decoded:%u played:%u\n", needsdata, decoded, played);

		/* debugging only, we are breaking encapsulation levels here */
		needsdata = kab_lsize(&am.play.buffer_list) <= cfg.audio.prebuffer_amount;
		for (kab_iter kit = SLL_ISTART(&am.play.buffer_list); !kab_iisend(&kit); kab_inext(&kit)) {
			keyed_ap_buffer *kab = kab_iget(&kit);
			printf("buffer %" PRIi64 " size: %zu\n", kab->key, p_lsize(&kab->buffer));
			if (p_lsize(&kab->buffer) < 4) {
				needsdata = true;
			}
		}


		decoded = decode_and_mix(&am);
		played = write_alsa_output(&am);

		if (!decoded && !played) {
			printf("sleeping %uus\n", cfg.audio.packetlen_us/2);
			usleep(cfg.audio.packetlen_us/2);
		}
	}
}
