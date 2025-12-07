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

#define NUMBER_OF_GESTURES 3
#define MEASUREMENTS_OF_EACH_GESTURE 5
#define MEASUREMENTS_PER_GESTURE 100
#define CSV_FILENAME "TDATA.CSV"

static const char* labels[NUMBER_OF_GESTURES] = {"idle", "waving", "sliding"};
static char header[] = "X-acc;Y-acc;Z-acc;label\n";

volatile int16_t last_sample_buffer[3] = {0};
volatile uint32_t measurements_taken = 0;
volatile uint8_t measure_enabled = 0;
char current_label[10] = "idle";

static FATFS fs;
static FIL fil;
static char write_to_sd[2560];
static int timer_started = 0;

static void print_msg(const char* msg) {
    printf("%s", msg);
    usb_serial_flush();
    delay_1ms(200);
}

static void wait_for_usb(void) {
    while (!usb_serial_available()) {
        usb_delay_1ms(100);
    }
}

static void wait_for_enter(void) {
    uint8_t input_buf[64];
    while (read_usb_serial(input_buf) == 0) {
        usb_delay_1ms(50);
    }
}

static void init_sdcard(void) {
    set_fattime(1980, 1, 1, 0, 0, 0);
    delay_1ms(100);
    f_mount(&fs, "test", 1);
    rcu_periph_clock_enable(RCU_DMA0);
    delay_1ms(20);
}

static void open_csv_file(void) {
    f_mount(&fs, "test", 1);
    f_sync(&fil);
    f_open(&fil, CSV_FILENAME, FA_WRITE | FA_CREATE_ALWAYS);
    clear_queues();
    f_sync(&fil);
    enqueue_string(header);
}

static void write_queued_data(void) {
    UINT bw;
    if (queue_str_len() > 5) {
        memset(write_to_sd, '\0', sizeof(write_to_sd));
        if (dequeue_string(write_to_sd, 5)) {
            f_write(&fil, write_to_sd, strlen(write_to_sd), &bw);
            f_sync(&fil);
        }
    }
}

static void flush_remaining_data(void) {
    UINT bw;
    write_to_sd[0] = '\0';
    while (empty_string_queue(write_to_sd, 5)) {
        f_write(&fil, write_to_sd, strlen(write_to_sd), &bw);
        f_sync(&fil);
        write_to_sd[0] = '\0';
    }
}

static void start_timer_if_needed(void) {
    if (!timer_started) {
        timer_interrupt_config();
        config_clic_irqs();
        eclic_global_interrupt_enable();
        timer_started = 1;
    }
}

static void collect_samples(void) {
    int blink = 0;
    int on_off = 0;

    start_timer_if_needed();
    measurements_taken = 0;
    measure_enabled = 1;

    while (measurements_taken < MEASUREMENTS_PER_GESTURE) {
        blink++;
        if (blink == 1000000) {
            blink = 0;
            on_off = !on_off;
            if (on_off) led_on_B0();
            else led_off_B0();
        }
        write_queued_data();
    }

    measure_enabled = 0;
    led_off_B1();
}

static void run_gesture(int gesture_idx) {
    snprintf(current_label, sizeof(current_label), "%s", labels[gesture_idx]);

    printf("\r\n=== Prepare for gesture: %s ===\r\n", labels[gesture_idx]);
    usb_serial_flush();
    delay_1ms(200);

    print_msg("Press ENTER to start...\r\n");
    usb_delay_1ms(5000);
    wait_for_enter();

    print_msg("Starting in 3 seconds...\r\n");
    delay_1ms(3000);

    for (int rep = 0; rep < MEASUREMENTS_OF_EACH_GESTURE; rep++) {
        printf("Gesture: %s - Repetition %d/%d\r\n",
               current_label, rep + 1, MEASUREMENTS_OF_EACH_GESTURE);
        usb_serial_flush();
        delay_1ms(100);

        collect_samples();
    }

    printf("Completed gesture: %s\r\n", labels[gesture_idx]);
    usb_serial_flush();
}

int main(void) {
    configure_usb_serial();
    usb_delay_1ms(1);
    wait_for_usb();

    print_msg("\r\nready\r\n");
    delay_1ms(300);

    init_q();
    led_init();
    usb_delay_1ms(1);

    init_sdcard();

    delay_1ms(400);
    print_msg("initiating MPU\r\n");
    init_mpu();
    print_msg("MPU initiated\r\n");

    open_csv_file();
    print_msg("running\r\n");

    for (int gesture = 0; gesture < NUMBER_OF_GESTURES; gesture++) {
        run_gesture(gesture);
    }

    print_msg("Emptying queue\r\n");
    flush_remaining_data();
    f_close(&fil);
    print_msg("done\r\n");

    while (1) {}
}

void TIMER1_IRQHandler(void) {
    if (SET == timer_interrupt_flag_get(TIMER1, TIMER_INT_CH0)) {
        timer_interrupt_flag_clear(TIMER1, TIMER_INT_CH0);

        if (measure_enabled) {
            char buf[64] = {'\0'};
            mpu_vector_t vec;

            measurements_taken++;
            led_set_B1(measurements_taken % 2 == 0);
            mpu6500_getAccel(&vec);
            last_sample_buffer[0] = (int16_t)vec.x;
            last_sample_buffer[1] = (int16_t)vec.y;
            last_sample_buffer[2] = (int16_t)vec.z;

            append_int_to_string(buf, last_sample_buffer[0]);
            append_int_to_string(buf, last_sample_buffer[1]);
            append_int_to_string(buf, last_sample_buffer[2]);

            strncat(buf, current_label, sizeof(buf) - strlen(buf) - 1);
            strcat(buf, "\n");

            enqueue_string(buf);
        }
    }
}
