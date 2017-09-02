vpath %.h src/
vpath %.c src/
vpath %.c test_src/
vpath %.dot doc/

CFLAGS += -g
CFLAGS += -std=gnu11
CFLAGS += -Werror -Wall -Wextra -Wshadow -Wpointer-arith -Wstrict-prototypes -Wcast-qual -Wunreachable-code
#CFLAGS += -Wcast-align # causes errors with protobuf on arm, unsure to what extent it is Bad
CFLAGS += -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -pie
CFLAGS += -Wl,-z,relro -Wl,-z,now

#CFLAGS += -DVERBOSE_SSL_IO -DVERBOSE_NETWORK
#CFLAGS += -DVERBOSE_AUDIO

#CFLAGS += -DPRINTWAVE -DPRINTWAVEDIVISOR=3000 -DPRINTWAVEWIDTH=8 -DPRINTWAVEHEIGHT=16

documentation = rabarber_capture.pdf rabarber_playback.pdf
test_binaries = playback_test capture_test capture_and_playback_test connection_test network_and_audio_test lradc_test network_and_audio_and_tone_test
libraries = opus asound crypto ssl protobuf-c
test_libraries = m


headers = audio.h config.h config_core.h mumble_pb.h network.h packet_common.h sll_meta.h ssl_io.h varint.h
objs = audio.o config.o config_core.o varint.o packet_common.o network.o ssl_io.o mumble_pb.o
objs += util.o lradc.o
libparams = $(addprefix -l,$(libraries))
test_libparams = $(addprefix -l,$(test_libraries))

.PHONY: all test_binaries doc
all: test_binaries doc
test_binaries: $(test_binaries)
documentation: $(documentation)

network_and_audio_and_tone_test: network_and_audio_and_tone_test.o $(objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

lradc_test: lradc_test.o $(objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

network_and_audio_test: network_and_audio_test.o $(objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

connection_test: connection_test.o $(objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

capture_and_playback_test: capture_and_playback_test.o $(objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

capture_test: capture_test.o $(objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

playback_test: playback_test.o $(objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

%.o: %.c $(headers)
	$(CC) $(CFLAGS) -c -o $@ $<

%.pdf: %.dot
	dot -Tpdf -o $@ $^

.PHONY: clean
clean:
	rm *.o $(test_binaries) $(documentation) || true
