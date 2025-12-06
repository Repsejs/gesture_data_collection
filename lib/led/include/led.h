#ifndef LED_H
#define LED_H

#include "gd32vf103.h"

void led_init(void);
void led_on_B0(void);
void led_off_B0(void);
void led_on_B1(void);
void led_off_B1(void);
void led_set_B1(int on_off);

#endif
