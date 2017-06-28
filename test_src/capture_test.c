#include "../src/audio.h"
#include "../src/util.h"
#include <math.h>
#include <opus/opus.h>

int main(void) {

	audio_config ac;
	ac.output_device = "default";
	ac.input_device = "default";
	ac.fs_Hz = 48000;
	ac.bitrate_bps = 20000;
	ac.packetlen_us = 20000;
	ac.prebuffer_amount = 2;
	ac.output_latency_us = 2000;

	ac.packetlen_samples = ac.fs_Hz * ac.packetlen_us / 1000000;

	audio_manager am = {0};

	p_pool packet_pool = {0};
	am_setup(&am, &ac, &packet_pool);

	printf("CAPTURE\n");
	bool caughtsomething = false;
	for (;;) {
		setpos(1,1);

		caughtsomething = get_alsa_input(&am);
		//printf("%08zu\n", am.cap.pcmwi);

		packet *ap = build_opus_packet_from_captured_data(&am);
		if (ap != NULL) {
			interpret_contents(ap, true);
			printf("post interp:  "); fprint_audio_packet(stdout, ap);
			p_preturn(&packet_pool, ap);
			ap = NULL;
		}






		if (!caughtsomething && ap == NULL) {
		//	printf("sleeping\n");
			usleep(ac.packetlen_us/2);
		}
	}
}
