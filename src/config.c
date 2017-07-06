#include "config.h"
#include "config_core.h"
static const config_mapping audio_config_map[] = {
	{ "input_device",          's', offsetof(audio_config, input_device),          .sm=NULL },
	{ "output_device",         's', offsetof(audio_config, output_device),         .sm=NULL },
	{ "mixer_device",          's', offsetof(audio_config, mixer_device),          .sm=NULL },
	{ "output_volume_control", 's', offsetof(audio_config, output_volume_control), .sm=NULL },
	{ "volume_control_steps",  'u', offsetof(audio_config, volume_steps),          .sm=NULL },
	{ "sample_frequency_Hz",   'u', offsetof(audio_config, fs_Hz),                 .sm=NULL },
	{ "bitrate_bps",           'u', offsetof(audio_config, bitrate_bps),           .sm=NULL },
	{ "packetlen_us",          'u', offsetof(audio_config, packetlen_us),          .sm=NULL },
	{ "prebuffer_amount",      'u', offsetof(audio_config, prebuffer_amount),      .sm=NULL },
	{ "output_latency_us",     'u', offsetof(audio_config, output_latency_us),     .sm=NULL },
	{ "input_latency_us",      'u', offsetof(audio_config, input_latency_us),      .sm=NULL },
	{  NULL,                    0,  0,                                             .sm=NULL }};

static const config_mapping certificate_config_map[] = {
	{ "client_cert", 's', offsetof(cert_config,client_cert), .sm=NULL },
	{ "client_key",  's', offsetof(cert_config,client_key),  .sm=NULL },
	{ "server_cert", 's', offsetof(cert_config,server_cert), .sm=NULL },
	{  NULL,          0,  0,                                 .sm=NULL }};
static const config_mapping network_config_map[] = {
	{ "server_host:port", 's', offsetof(network_config,server_hostport),  .sm=NULL                   },
	{ "server_password",  's', offsetof(network_config,server_password),  .sm=NULL                   },
	{ "user_name",        's', offsetof(network_config,user_name),        .sm=NULL                   },
	{ "use_certificates", 'b', offsetof(network_config,use_certificates), .sm=NULL                   },
	{ "certificates",     '+', offsetof(network_config,certificates),     .sm=certificate_config_map },
	{  NULL,               0,  0,                                         .sm=NULL                   }};
static const config_mapping config_map[] = {
	{ "network", '+', offsetof(config,network), .sm=network_config_map },
	{ "audio",   '+', offsetof(config,audio),   .sm=audio_config_map   },
	{  NULL,      0,  0,                        .sm=NULL               }};

bool load_config_from_file(config *c, const char* filename) {
	FILE *f = fopen(filename, "r");
	bool ret = inflate_config(c, config_map, f);
	if (ret == true) {
		c->audio.packetlen_samples = c->audio.fs_Hz * c->audio.packetlen_us / 1000000;
	}
	return ret;
}

void print_config_to_stdout(const config *c) {
	deflate_config(stdout, c, config_map);
}

