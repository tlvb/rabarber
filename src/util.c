#include "util.h"
#include <stdio.h>

void setpos(uint16_t row, uint16_t col) {
	printf("\x1b[%u;%uH", row, col);
}

void printwave(int row, int col, const int16_t *wave, size_t n) {
	n = n < PRINTWAVEHEIGHT ? n : PRINTWAVEHEIGHT;
	for (uint8_t i=0; i<n>>2; ++i) {
		setpos(row+i,col);
		printwaveline(wave[i*4], wave[i*4+1], wave[i*4+2], wave[i*4+3]);
	}
	printf("\n");

}
void printwaveline(int16_t s0, int16_t s1, int16_t s2, int16_t s3) { /*{{{*/
	int16_t v0 = s0/PRINTWAVEDIVISOR;
	int16_t v1 = s1/PRINTWAVEDIVISOR;
	int16_t v2 = s2/PRINTWAVEDIVISOR;
	int16_t v3 = s3/PRINTWAVEDIVISOR;
	v0 = v0 < -PRINTWAVEWIDTH ? -PRINTWAVEWIDTH : v0;
	v1 = v1 < -PRINTWAVEWIDTH ? -PRINTWAVEWIDTH : v1;
	v2 = v2 < -PRINTWAVEWIDTH ? -PRINTWAVEWIDTH : v2;
	v3 = v3 < -PRINTWAVEWIDTH ? -PRINTWAVEWIDTH : v3;
	v0 = v0 > PRINTWAVEWIDTH ? PRINTWAVEWIDTH : v0;
	v1 = v1 > PRINTWAVEWIDTH ? PRINTWAVEWIDTH : v1;
	v2 = v2 > PRINTWAVEWIDTH ? PRINTWAVEWIDTH : v2;
	v3 = v3 > PRINTWAVEWIDTH ? PRINTWAVEWIDTH : v3;
	if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0) {
		for (int i=PRINTWAVEWIDTH; i>=0; --i) {
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
		for (int i=0; i<PRINTWAVEWIDTH; ++i) {
			putchar(' ');
		}
	}
	putchar('|');
	if (v0 > 0 || v1 > 0 || v2 > 0 || v3 > 0) {
		for (int i=0; i<PRINTWAVEWIDTH; ++i) {
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
		for (int i=0; i<PRINTWAVEWIDTH; ++i) {
			putchar(' ');
		}
	}
} /*}}}*/
