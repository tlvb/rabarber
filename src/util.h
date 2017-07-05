#pragma once
#include <inttypes.h>
#include <stddef.h>

#ifndef PRINTWAVEWIDTH
#define PRINTWAVEWIDTH 5
#endif
#ifndef PRINTWAVEDIVISOR
#define PRINTWAVEDIVISOR 1000
#endif
#ifndef PRINTWAVEHEIGHT
#define PRINTWAVEHEIGHT 4
#endif

void setpos(uint16_t row, uint16_t col);
void printwave(int row, int col, const int16_t *wave, size_t n);
void printwaveline(int16_t s0, int16_t s1, int16_t s2, int16_t s3);
