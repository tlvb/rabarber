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
		fprintf(stderr, "config load error\n");
		return 1;
	}
	print_config_to_stdout(&cfg);
	printf("packetlen_samples calculated to %u\n", cfg.audio.packetlen_samples);

	audio_manager am = {0};

	p_pool packet_pool = {0};
	am_setup(&am, &cfg.audio, &packet_pool);

	printf("CAPTURE\n");
	bool caughtsomething = false;
	for (;;) {
		caughtsomething = get_alsa_input(&am);

		packet *ap = build_opus_packet_from_captured_data(&am);
		if (ap != NULL) {
			interpret_contents(ap, true);
			//printf("post interp:  "); fprint_audio_packet(stdout, ap);
			p_preturn(&packet_pool, ap);
			ap = NULL;
		}

		if (!caughtsomething && ap == NULL) {
			usleep(cfg.audio.packetlen_us/2);
		}
	}
}
