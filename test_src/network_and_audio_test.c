#include <stdio.h>
#include <unistd.h>
#include "../src/config.h"
#include "../src/network.h"
#include "../src/audio.h"

#include "../src/mumble_pb.h"



void send_version_packet(network_manager *nm);
void send_auth_packet(network_manager *nm);
void send_ping_packet(network_manager *nm, time_t t);
void handle_ingress_packet(network_manager *nm, audio_manager *am, packet *p);
void send_version_packet(network_manager *nm) { /*{{{*/
	MumbleProto__Version ver = MUMBLE_PROTO__VERSION__INIT;
	ver.has_version = 1;
	ver.version = 0x00010204;
	ver.release = "debug";
	ver.os = "linux";
	ver.os_version = "whatever";

	size_t payload_len = mumble_proto__version__get_packed_size(&ver);
	size_t packet_len = payload_len + 6;

	packet *p = nm_from_pool(nm, packet_len);

	mumble_proto__version__pack(&ver, p->raw.data);

	p->raw.type = 0;
	p->raw.len = payload_len;
	cast_raw_packet_header(p);
	nm_post_egress(nm, p);

}/*}}}*/
void send_auth_packet(network_manager *nm) { /*{{{*/
	MumbleProto__Authenticate auth = MUMBLE_PROTO__AUTHENTICATE__INIT;
	auth.username = nm->cfg->user_name;
	auth.password = nm->cfg->server_password;
	auth.n_tokens = 0; // TODO implement access tokens
	auth.has_opus = 1;
	auth.opus = 1;

	size_t payload_len = mumble_proto__authenticate__get_packed_size(&auth);
	size_t packet_len = payload_len + 6;

	packet *p = nm_from_pool(nm, packet_len);

	mumble_proto__authenticate__pack(&auth, p->raw.data);

	p->raw.type = 2;
	p->raw.len = payload_len;
	cast_raw_packet_header(p);
	nm_post_egress(nm, p);

}/*}}}*/
void send_ping_packet(network_manager *nm, time_t t) { /*{{{*/
	MumbleProto__Ping ping = MUMBLE_PROTO__PING__INIT;
	ping.has_timestamp = 1;
	ping.timestamp = t;

	size_t payload_len = mumble_proto__ping__get_packed_size(&ping);
	size_t packet_len = payload_len + 6;

	packet *p = nm_from_pool(nm, packet_len);

	mumble_proto__ping__pack(&ping, p->raw.data);

	p->raw.type = 3;
	p->raw.len = payload_len;
	cast_raw_packet_header(p);

	printf("sending ping packet\n");
	nm_post_egress(nm, p);
} /*}}}*/
void handle_ingress_packet(network_manager *nm, audio_manager *am, packet *p) { /*{{{*/
	switch (p->raw.type) {
		case  0:
			printf("ingress VERSION packet\n");
			nm_recycle(nm, p);
			break;
		case  1:
			interpret_contents(p, false);
			route_for_playback(am, p);
			break;
		case  2:
			printf("ingress AUTHENTICATE packet\n");
			nm_recycle(nm, p);
			break;
		case  3:
			printf("ingress PING packet\n");
			nm_recycle(nm, p);
			break;
		case  4:
			printf("ingress REJECT packet\n");
			nm_recycle(nm, p);
			break;
		case  5:
			printf("ingress SERVERSYNC packet\n");
			nm_recycle(nm, p);
			break;
		case  6:
			printf("ingress CHANNELREMOVE packet\n");
			nm_recycle(nm, p);
			break;
		case  7:
			printf("ingress CHANNELSTATE packet\n");
			nm_recycle(nm, p);
			break;
		case  8:
			printf("ingress USERREMOVE packet\n");
			nm_recycle(nm, p);
			break;
		case  9:
			printf("ingress USERSTATE packet\n");
			nm_recycle(nm, p);
			break;
		case 10:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 11:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 12:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 13:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 14:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 15:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 16:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 17:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 18:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 19:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 20:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 21:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 22:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 23:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 24:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		case 25:
			printf("ingress BANLIST packet\n");
			nm_recycle(nm, p);
			break;
		default:
			printf("ingress PACKET OF UNKNOWN TYPE: %" PRIu32 "\n", p->raw.type);
			nm_recycle(nm, p);
			break;
	}
}/*}}}*/

int main(int argc, char **argv) {

	if (argc != 4) {
		fprintf(stderr, "needs <host:port> <user_name> and <server_password>\n");
		return 1;
	}
	for (int i=0; i<argc; ++i) {
		printf("%d: %s\n", i, argv[i]);
	}

	p_pool packet_pool = {0};
	config cfg;

	cfg.network.server_hostport = argv[1];
	cfg.network.server_password = argv[2];
	cfg.network.user_name       = argv[3];

	cfg.network.client_cert = "cert/client_cert.pem";
	cfg.network.client_key  = "cert/client_key.pem";
	cfg.network.server_cert = "cert/server_cert.pem";

	cfg.audio.output_device = "default";
	cfg.audio.input_device = "default";
	cfg.audio.fs_Hz = 48000;
	cfg.audio.bitrate_bps = 40000;
	cfg.audio.packetlen_us = 20000;
	cfg.audio.prebuffer_amount = 8;
	cfg.audio.output_latency_us = 2000;
	cfg.audio.input_latency_us = 2000;
	cfg.audio.packetlen_samples = cfg.audio.fs_Hz * cfg.audio.packetlen_us / 1000000;

	network_manager nm = {0};

	nm_init(&nm, &cfg.network, &packet_pool);

	if (!nm_connect(&nm)) {
		fprintf(stderr, "connection failed, exiting\n");
		return 73;
	}

	fprintf(stdout, "connected\n");

	audio_manager am = {0};
	am_setup(&am, &cfg.audio, &packet_pool);

	send_version_packet(&nm);
	send_auth_packet(&nm);

	time_t last_time = 0;
	for (;;) {

		bool captured    = false;
		bool encoded     = false;
		bool transmitted = false;
		bool received    = false;
		bool decoded     = false;
		bool played      = false;

		// capture
		captured = get_alsa_input(&am);

		// encode
		packet *ap = build_opus_packet_from_captured_data(&am);
		if (ap != NULL) {
			if (ap->raw.type == 1) {
				nm_post_egress(&nm, ap);
			}
			else {
				printf("!!!! %u\n", ap->type);
				nm_recycle(&nm, ap);
			}

			encoded = true;
		}

		// transmit
		transmitted = nm_write(&nm);

		// receive
		if (nm_read(&nm)) {
			received = true;
			packet *p = nm_get_ingress(&nm);
			if (p != NULL) {
				handle_ingress_packet(&nm, &am, p);
			}
		}

		// decode
		decoded = decode_and_mix(&am);

		// play
		played  = write_alsa_output(&am);

		time_t current_time = time(NULL);
		if (current_time >= last_time + 10) {
			last_time = current_time;
			send_ping_packet(&nm, current_time);
		}

		bool activity = captured || encoded || transmitted || received || decoded || played;

		if (!activity) {
			usleep(1250);
		}


	}
}
