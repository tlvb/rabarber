vpath %.h src/
vpath %.c src/
vpath %.c test_src/

CFLAGS += -g -O0
CFLAGS += -Werror -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-align -Wstrict-prototypes -Wcast-qual -Wunreachable-code
CFLAGS += -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -pie
CFLAGS += -Wl,-z,relro -Wl,-z,now

libraries = opus asound m
objs = audio.o config.o config_core.o varint.o
libparams = $(addprefix -l,$(libraries))

.PHONY: playback_test

playback_test: playback_test.o $(objs)
	$(CC) $(CFLAGS) -o $@ $^ $(libparams)

config.o: config.c config.h config_core.h
	$(CC) $(CFLAGS) -c -o $@ $<

audio.o: audio.c audio.h sll_meta.h config.h
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm *.o || true
