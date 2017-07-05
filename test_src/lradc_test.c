#include "../src/lradc.h"
#include <unistd.h>
#include <stdio.h>

int main(void) {
	lradc l;
	lradc_setup(&l);

	for (;;) {
		printf("%" PRIu32 "\n", lradc_value(&l));
		sleep(1);
	}
}
