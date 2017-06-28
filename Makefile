vpath %.h src/
vpath %.c src/
vpath %.c test_src/

CFLAGS += -g
CFLAGS += -Werror -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wcast-qual -Wunreachable-code
CFLAGS += -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -pie
CFLAGS += -Wl,-z,relro -Wl,-z,now

debug_cflags = -DVERBOSE_SSL_IO -DVERBOSE_NETWORK

#CFLAGS += $(debug_cflags)

test_binaries = playback_test capture_test capture_and_playback_test connection_test network_and_audio_test
libraries = opus asound crypto ssl protobuf-c
test_libraries = m


headers = audio.h config.h config_core.h mumble_pb.h network.h packet_common.h sll_meta.h ssl_io.h varint.h
objs = audio.o config.o config_core.o varint.o packet_common.o network.o ssl_io.o mumble_pb.o
test_objs = util.o
libparams = $(addprefix -l,$(libraries))
test_libparams = $(addprefix -l,$(test_libraries))

.PHONY: test_binaries
test_binaries: $(test_binaries)

network_and_audio_test: network_and_audio_test.o $(objs) $(test_objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

connection_test: connection_test.o $(objs) $(test_objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

capture_and_playback_test: capture_and_playback_test.o $(objs) $(test_objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

capture_test: capture_test.o $(objs) $(test_objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

playback_test: playback_test.o $(objs) $(test_objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

%.o: %.c $(headers)
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm *.o $(test_binaries) || true
