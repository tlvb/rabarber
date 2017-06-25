typedef struct {
	char     *input_device;
	char     *output_device;
	uint32_t  fs_Hz;
	uint32_t  bitrate_bps;
	uint32_t  packetlen_us;
	uint32_t  prebuffer_amount;
	uint32_t  output_latency_us;
	uint32_t  input_latency_us;

	// derived values, not read from config
	uint32_t  packetlen_samples;

} audio_config;

typedef struct {
	audio_config audio;
} config;
