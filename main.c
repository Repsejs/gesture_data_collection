#include "gd32vf103.h"
#include "usb_serial_if.h"
#include "usb_delay.h"
#include <stdlib.h>
#include <string.h>
#include "systick.h"
#include "gd32v_tf_card_if.h"
#include "ff.h"
#include "gd32v_mpu6500_if.h"
#include "dma.h"
#include "mpu6500_registers.h"
#include "mpu6500_driver.h"
#include "queue.h"
#include "tf_card.h"
#include "led.h"
#include "timer_config.h"
#include "string_utils.h"
#include "hardware_init.h"
#include "eclicw.h"

#define BUFFER_SIZE 512
#define NUMBER_OF_GESTURES 3
#define MEASURMENTS_OF_EACH_GESTURE 100
#define MEASURMENTS_PER_GESTURE 100

char header[] = "X-acc;Y-acc;Z-acc;label\n";
char labels[NUMBER_OF_GESTURES][10] = {
    "idle",
    "waving",
    "sliding"
};

volatile int16_t last_Sample_buffer[3] = {0};
uint32_t measurments_taken = 0;
char current_label[10] = "idle";
uint8_t* pDatabuffer = (uint8_t*)&dataBuffer;

int main(void) {
    char usb_data_buffer[512] = {'\0'};
    char usb_read_buf[80] = {'\0'};

    configure_usb_serial();
    usb_delay_1ms(1);

    while (!usb_serial_available()) {
        usb_delay_1ms(100);
    }
    printf("\r\nready\r\n");

    init_q();
    led_init();
    usb_delay_1ms(1);

    FATFS fs;
    volatile FRESULT fr;
    FIL fil;
    UINT bw = 0;

    set_fattime(1980, 1, 1, 0, 0, 0);
    delay_1ms(100);

    fr = f_mount(&fs, "test", 1);

    delay_1ms(400);
    printf("initiating MPU\r\n");
    init_mpu();
    printf("MPU initiated\r\n");

    rcu_periph_clock_enable(RCU_DMA0);
    delay_1ms(20);

    char write_to_sd[2560] = {'\0'};
    int blink = 0;
    int onOff = 0;

    fr = f_mount(&fs, "test", 1);
    f_sync(&fil);

    fr = f_open(&fil, "TDATA.CSV", FA_WRITE | FA_CREATE_ALWAYS);
    clear_queues();
    f_sync(&fil);

    printf("running\r\n");
    int start_measurment = 0;
    enqueue_string(header);

    for (int gesture = 0; gesture < NUMBER_OF_GESTURES; gesture++) {
        timer_interrupt_disable(TIMER1, TIMER_INT_CH0);
        printf("start %s\r\n", labels[gesture]);
        fflush(0);
        delay_1ms(5000);
        start_measurment = 1;

        for (int repetition = 0; repetition < MEASURMENTS_OF_EACH_GESTURE; repetition++) {
            snprintf(current_label, sizeof(current_label), "%s", labels[gesture]);
            printf("Collecting data for gesture: %s, repetition %d\r\n", current_label, repetition + 1);

            if (start_measurment == 1) {
                timer_interrupt_config();
                config_clic_irqs();
                eclic_global_interrupt_enable();
            }
            start_measurment = 0;

            while (measurments_taken < MEASURMENTS_PER_GESTURE * (gesture * MEASURMENTS_OF_EACH_GESTURE + repetition + 1)) {
                blink++;
                if (blink == 1000000) {
                    blink = 0;
                    if (onOff) {
                        led_on_B0();
                        onOff = 0;
                    } else {
                        led_off_B0();
                        onOff = 1;
                    }
                }

                if (queue_str_len() > 5) {
                    memset(write_to_sd, '\0', sizeof(write_to_sd));
                    printf("x=%d, y=%d, z=%d\r\n", last_Sample_buffer[0], last_Sample_buffer[1], last_Sample_buffer[2]);
                    fflush(0);
                    if (dequeue_string(write_to_sd, 5)) {
                        f_write(&fil, write_to_sd, strlen(write_to_sd), &bw);
                        f_sync(&fil);
                    }
                }
            }
        }
    }

    printf("Emptying queue\r\n");
    timer_interrupt_disable(TIMER1, TIMER_INT_CH0);
    write_to_sd[0] = '\0';

    while (empty_string_queue(write_to_sd, 5)) {
        f_write(&fil, write_to_sd, strlen(write_to_sd), &bw);
        f_sync(&fil);
        write_to_sd[0] = '\0';
    }

    f_close(&fil);
    printf("done\r\n");
    fflush(0);
}


void TIMER1_IRQHandler(void) {
    led_set_B1(measurments_taken % 2 == 0);
    char buf[64] = {'\0'};
    measurments_taken++;
    mpu_vector_t vec;

    if (SET == timer_interrupt_flag_get(TIMER1, TIMER_INT_CH0)) {
        mpu6500_getAccel(&vec);
        last_Sample_buffer[0] = (int16_t)vec.x;
        last_Sample_buffer[1] = (int16_t)vec.y;
        last_Sample_buffer[2] = (int16_t)vec.z;

        append_int_to_string(buf, last_Sample_buffer[0]);
        append_int_to_string(buf, last_Sample_buffer[1]);
        append_int_to_string(buf, last_Sample_buffer[2]);

        strncat(buf, current_label, sizeof(buf) - strlen(buf) - 1);
        strcat(buf, "\n");

        enqueue_string(buf);

        timer_interrupt_flag_clear(TIMER1, TIMER_INT_CH0);
    }
}
