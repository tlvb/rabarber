#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "lradc.h"
bool lradc_setup(lradc *l) { /*{{{*/
	uint32_t page_size = sysconf(_SC_PAGESIZE);

	uint32_t page_addr = LRADC_ADDR & (~(page_size-1));

	l->fd = open("/dev/mem", O_RDONLY|O_SYNC);
	if (l->fd < 1) {
		return false;
	}

	l->page = mmap(NULL, page_size, PROT_READ, MAP_SHARED, l->fd, page_addr);

	l->ptr = ((volatile uint32_t*)l->page) + LRADC_INDEX;
	l->pressed = true;
	return true;
} /*}}}*/
uint8_t lradc_key(lradc *l) { /*{{{*/
	uint8_t v = lradc_value(l);
	if (v > 20) {
		l->pressed = false;
	}
	else if (!l->pressed) {
		l->pressed = true;
		if (v < 3) {
			return LRADC_REC;
		}
		else if (v < 8) {
			return LRADC_VOLUP;
		}
		else if (v < 14) {
			return LRADC_VOLDOWN;
		}
	}
	return LRADC_NOKEY;
} /*}}}*/
