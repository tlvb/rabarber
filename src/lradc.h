#pragma once
#include <inttypes.h>
#include <stdbool.h>

#define LRADC_ADDR  0x01C22800
#define LRADC_INDEX 0x00000204

#define LRADC_REC      1
#define LRADC_VOLDOWN  2
#define LRADC_VOLUP    3
#define LRADC_NOKEY    0

#define lradc_value(lradc_) (*(lradc_)->ptr)

typedef struct {
	int fd;
	void *page;
	bool pressed;
	volatile uint32_t *ptr;
} lradc;


bool lradc_setup(lradc *l);
uint8_t lradc_key(lradc *l);

