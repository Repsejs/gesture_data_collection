#include "led.h"

void led_init(void) {
    rcu_periph_clock_enable(RCU_GPIOB);
    gpio_init(GPIOB, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_0 | GPIO_PIN_1);
}

void led_on_B0(void) {
    gpio_bit_write(GPIOB, GPIO_PIN_0, 1);
}

void led_off_B0(void) {
    gpio_bit_write(GPIOB, GPIO_PIN_0, 0);
}

void led_on_B1(void) {
    gpio_bit_write(GPIOB, GPIO_PIN_1, 1);
}

void led_off_B1(void) {
    gpio_bit_write(GPIOB, GPIO_PIN_1, 0);
}

void led_set_B1(int on_off) {
    gpio_bit_write(GPIOB, GPIO_PIN_1, on_off);
}
