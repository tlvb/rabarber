vpath %.h src/
vpath %.c src/
vpath %.c test_src/

CFLAGS += -g
CFLAGS += -Werror -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wcast-qual -Wunreachable-code
CFLAGS += -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -pie
CFLAGS += -Wl,-z,relro -Wl,-z,now

test_binaries = playback_test capture_test capture_and_playback_test
libraries = opus asound
test_libraries = m
objs = audio.o config.o config_core.o varint.o packet_common.o
test_objs = util.o
libparams = $(addprefix -l,$(libraries))
test_libparams = $(addprefix -l,$(test_libraries))

.PHONY: test_binaries
test_binaries: $(test_binaries)

capture_and_playback_test: capture_and_playback_test.o $(objs) $(test_objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

capture_test: capture_test.o $(objs) $(test_objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

playback_test: playback_test.o $(objs) $(test_objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams) $(test_libparams)

config.o: config.c config.h config_core.h
	$(CC) $(CFLAGS) -c -o $@ $<

audio.o: audio.c audio.h sll_meta.h config.h
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm *.o $(test_binaries) || true
