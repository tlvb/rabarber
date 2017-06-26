#include "util.h"
#include <stdio.h>

void setpos(uint8_t row, uint8_t col) {
	printf("\x1b[%u;%uH", row, col);
}
void printwave(int16_t s0, int16_t s1, int16_t s2, int16_t s3) { /*{{{*/
	int16_t v0 = s0/500;
	int16_t v1 = s1/500;
	int16_t v2 = s2/500;
	int16_t v3 = s3/500;
	if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0) {
		for (int i=63; i>=0; --i) {
			if (v0 < -i || v1 < -i) {
				if (v2 < -i || v3 < -i) {
					putchar('|');
				}
				else {
					putchar('\'');
				}
			}
			else {
				if (v2 < -i || v3 < -i) {
					putchar(',');
				}
				else {
					putchar(' ');
				}
			}
		}
	}
	else {
		printf("                                                                ");
	}
	putchar('|');
	if (v0 > 0 || v1 > 0 || v2 > 0 || v3 > 0) {
		for (int i=0; i<64; ++i) {
			if (v0 > i || v1 > i) {
				if (v2 > i || v3 > i) {
					putchar('|');
				}
				else {
					putchar('\'');
				}
			}
			else {
				if (v2 > i || v3 > i) {
					putchar(',');
				}
				else {
					putchar(' ');
				}
			}
		}
	}
	else {
		printf("                                                                ");
	}
} /*}}}*/
