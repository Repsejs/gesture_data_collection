#include "hardware_init.h"
#include "gd32v_mpu6500_if.h"
#include "systick.h"

void init_mpu(void) {
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_I2C0);
    gpio_init(GPIOB, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_6 | GPIO_PIN_7);

    mpu6500_install(I2C0);

    delay_1ms(100);
}
