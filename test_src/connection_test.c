#include <stdio.h>
#include <unistd.h>
#include "../src/config.h"
#include "../src/network.h"

#include "../src/mumble_pb.h"



void send_version_packet(network_manager *nm);
void send_auth_packet(network_manager *nm);
void send_ping_packet(network_manager *nm, time_t t);
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
	nm_post_egress(nm, p);
} /*}}}*/

int main(int argc, char **argv) {

	if (argc != 2) {
		fprintf(stderr, "needs config file argument\n");
		return 1;
	}

	p_pool packet_pool = {0};
	config cfg;

	if (!load_config_from_file(&cfg, argv[1])) {
		fprintf(stderr, "config file load error\n");
		return 1;
	}
	print_config_to_stdout(&cfg);

	network_manager nm = {0};

	nm_init(&nm, &cfg.network, &packet_pool);

	if (!nm_connect(&nm)) {
		fprintf(stderr, "connection failed, exiting\n");
		return 73;
	}

	fprintf(stdout, "connected\n");

	send_version_packet(&nm);
	send_auth_packet(&nm);

	time_t last_time = 0;
	for (;;) {
		//printf("mainloop\n");
		bool activity = false;
		time_t current_time = time(NULL);
		if (current_time >= last_time + 10) {
			last_time = current_time;
			send_ping_packet(&nm, current_time);
		}

		//printf("trying to read\n");
		if (nm_read(&nm)) {
			activity = true;
			packet *p = nm_get_ingress(&nm);
			if (p != NULL) {
				printf("INGRESS packet of type %04" PRIu16 " and length %04" PRIu32 "\n",
				       p->raw.type,
				       p->raw.len);
				nm_recycle(&nm, p);
			}
		}

		//printf("trying to write\n");
		if (nm_write(&nm)) {
			activity = true;
		}

		if (!activity) {
			//printf("sleeping\n");
			usleep(1250);
		}

	}
}
