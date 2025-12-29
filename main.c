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

// Set to 1 to use SD card, 0 to output data over serial
#define USE_SDCARD 0

#define NUMBER_OF_GESTURES 3
#define MEASUREMENTS_OF_EACH_GESTURE 100
#define MEASUREMENTS_PER_GESTURE 100
#define CSV_FILENAME "TDATA.CSV"

// Serial protocol markers
#define SERIAL_CONFIG_START "<<<CONFIG>>>\r\n"
#define SERIAL_CONFIG_END "<<<CONFIG_END>>>\r\n"
#define SERIAL_READY "<<<READY>>>\r\n"
#define SERIAL_DATA_START "<<<DATA>>>\r\n"
#define SERIAL_DATA_END "<<<DATA_END>>>\r\n"
#define SERIAL_DONE "<<<DONE>>>\r\n"

static const char* labels[NUMBER_OF_GESTURES] = {"idle", "waving", "sliding"};
#if USE_SDCARD
static char header[] = "X-acc;Y-acc;Z-acc;label\n";
#else
static char header[] = "seq;X-acc;Y-acc;Z-acc;label\n";
#endif

volatile int16_t last_sample_buffer[3] = {0};
volatile uint32_t measurements_taken = 0;
volatile uint8_t measure_enabled = 0;
volatile uint32_t seq_number = 0;
char current_label[10] = "idle";

// Buffer for one repetition's samples (enables retransmission)
typedef struct {
    uint32_t seq;
    int16_t x;
    int16_t y;
    int16_t z;
} sample_t;
static sample_t sample_buffer[MEASUREMENTS_PER_GESTURE];

#if USE_SDCARD
static FATFS fs;
static FIL fil;
#endif
static char write_buffer[2560];
static int timer_started = 0;

static void print_msg(const char* msg) {
#if USE_SDCARD
    printf("%s", msg);
    usb_serial_flush();
    delay_1ms(200);
#else
    (void)msg; // Silent in serial protocol mode
#endif
}

#if !USE_SDCARD
static void send_config(void) {
    printf(SERIAL_CONFIG_START);
    usb_serial_flush();

    // Send gesture labels
    printf("gestures:");
    for (int i = 0; i < NUMBER_OF_GESTURES; i++) {
        printf("%s", labels[i]);
        if (i < NUMBER_OF_GESTURES - 1) printf(",");
    }
    printf("\r\n");
    usb_serial_flush();

    // Send parameters
    printf("repetitions:%d\r\n", MEASUREMENTS_OF_EACH_GESTURE);
    printf("samples:%d\r\n", MEASUREMENTS_PER_GESTURE);
    usb_serial_flush();

    // Send header format
    printf("header:%s", header);
    usb_serial_flush();

    printf(SERIAL_CONFIG_END);
    usb_serial_flush();
    delay_1ms(100);
}

static void wait_for_start_command(void) {
    uint8_t input_buf[64];
    while (read_usb_serial(input_buf) == 0) {
        usb_delay_1ms(50);
    }
}

static void send_ready(void) {
    printf(SERIAL_READY);
    usb_serial_flush();
}

static void send_done(void) {
    printf(SERIAL_DONE);
    usb_serial_flush();
}
#endif

static void wait_for_usb(void) {
    while (!usb_serial_available()) {
        usb_delay_1ms(1000);
    }
}

static void wait_for_enter(void) {
    uint8_t input_buf[64];
    while (read_usb_serial(input_buf) == 0) {
        usb_delay_1ms(50);
    }
}

#if USE_SDCARD
static void init_sdcard(void) {
    set_fattime(1980, 1, 1, 0, 0, 0);
    delay_1ms(100);
    f_mount(&fs, "test", 1);
    rcu_periph_clock_enable(RCU_DMA0);
    delay_1ms(20);
}
#endif

static void open_data_output(void) {
#if USE_SDCARD
    f_mount(&fs, "test", 1);
    f_open(&fil, CSV_FILENAME, FA_WRITE | FA_CREATE_ALWAYS);
    clear_queues();
    enqueue_string(header);
#else
    clear_queues();
    send_config();
#endif
}

static void write_queued_data(void) {
    if (queue_str_len() > 5) {
        memset(write_buffer, '\0', sizeof(write_buffer));
        if (dequeue_string(write_buffer, 5)) {
#if USE_SDCARD
            UINT bw;
            f_write(&fil, write_buffer, strlen(write_buffer), &bw);
            f_sync(&fil);
#else
            printf("%s", write_buffer);
            usb_serial_flush();
#endif
        }
    }
}

static void flush_remaining_data(void) {
    write_buffer[0] = '\0';
    while (empty_string_queue(write_buffer, 5)) {
#if USE_SDCARD
        UINT bw;
        f_write(&fil, write_buffer, strlen(write_buffer), &bw);
        f_sync(&fil);
#else
        printf("%s", write_buffer);
        usb_serial_flush();
#endif
        write_buffer[0] = '\0';
    }
}

static void close_data_output(void) {
#if USE_SDCARD
    f_close(&fil);
#else
    send_done();
#endif
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

#if !USE_SDCARD
    // Ensure all serial data is flushed before status messages
    usb_serial_flush();
    delay_1ms(50);
#endif
}

static void drain_queue(void) {
    while (queue_str_len() > 0) {
        memset(write_buffer, '\0', sizeof(write_buffer));
        if (dequeue_string(write_buffer, 1)) {
#if USE_SDCARD
            UINT bw;
            f_write(&fil, write_buffer, strlen(write_buffer), &bw);
            f_sync(&fil);
#else
            printf("%s", write_buffer);
            usb_serial_flush();
#endif
        }
    }
}

#if !USE_SDCARD
// Send all samples from buffer over serial
static void send_sample_buffer(uint32_t count) {
    char buf[64];
    for (uint32_t i = 0; i < count; i++) {
        buf[0] = '\0';
        append_int_to_string(buf, (int)sample_buffer[i].seq);
        append_int_to_string(buf, sample_buffer[i].x);
        append_int_to_string(buf, sample_buffer[i].y);
        append_int_to_string(buf, sample_buffer[i].z);
        strncat(buf, current_label, sizeof(buf) - strlen(buf) - 1);
        strcat(buf, "\r\n");
        printf("%s", buf);
        // Flush every 10 samples to prevent buffer overflow
        if ((i + 1) % 10 == 0) {
            usb_serial_flush();
        }
    }
    usb_serial_flush();
}

// Wait for ACK/NACK, returns 1 for ACK, 0 for NACK
static int wait_for_ack(void) {
    uint8_t ack_buf[64];
    while (1) {
        if (read_usb_serial(ack_buf) > 0) {
            if (strstr((char*)ack_buf, "ACK") != NULL) {
                return 1;
            } else if (strstr((char*)ack_buf, "NACK") != NULL) {
                return 0;
            }
        }
        usb_delay_1ms(10);
    }
}
#endif

static void run_gesture(int gesture_idx) {
    snprintf(current_label, sizeof(current_label), "%s", labels[gesture_idx]);

#if USE_SDCARD
    // SD card mode: interactive with status messages
    printf("\r\n=== Prepare for gesture: %s ===\r\n", labels[gesture_idx]);
    usb_serial_flush();
    delay_1ms(200);

    printf("Press ENTER to start...\r\n");
    usb_serial_flush();
    wait_for_enter();

    printf("Starting in 3 seconds...\r\n");
    usb_serial_flush();
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

#else
    // Serial protocol mode: Python controls UI, per-repetition ACK with retry
    seq_number = 0;  // Reset sequence counter for this gesture

    send_ready();
    wait_for_start_command();

    for (int rep = 0; rep < MEASUREMENTS_OF_EACH_GESTURE; rep++) {
        int retry_count = 0;
        const int max_retries = 3;
        int need_collect = 1;  // Only collect on first attempt

        while (retry_count <= max_retries) {
            if (need_collect) {
                // Collect samples into buffer
                collect_samples();
                need_collect = 0;
            }

            // Send repetition data
            printf("<<<REP:%d>>>\r\n", rep + 1);
            usb_serial_flush();
            delay_1ms(20);

            printf(SERIAL_DATA_START);
            usb_serial_flush();

            send_sample_buffer(MEASUREMENTS_PER_GESTURE);

            delay_1ms(50);
            printf(SERIAL_DATA_END);
            usb_serial_flush();
            delay_1ms(20);

            // Send count and wait for ACK
            printf("<<<COUNT:%d>>>\r\n", MEASUREMENTS_PER_GESTURE);
            usb_serial_flush();

            if (wait_for_ack()) {
                break;  // ACK received, move to next repetition
            }

            // NACK received, retry from buffer (same data)
            retry_count++;
            if (retry_count <= max_retries) {
                printf("<<<RETRY:%d>>>\r\n", retry_count);
                usb_serial_flush();
                delay_1ms(100);
            }
        }
    }

    // Signal gesture complete
    printf("<<<GESTURE_DONE>>>\r\n");
    usb_serial_flush();
#endif
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

#if USE_SDCARD
    init_sdcard();
#endif

    delay_1ms(400);
    print_msg("initiating MPU\r\n");
    init_mpu();
    print_msg("MPU initiated\r\n");

    open_data_output();
    print_msg("running\r\n");

    for (int gesture = 0; gesture < NUMBER_OF_GESTURES; gesture++) {
        run_gesture(gesture);
    }

    print_msg("Emptying queue\r\n");
    flush_remaining_data();
    close_data_output();
    print_msg("done\r\n");

    while (1) {}
}

void TIMER1_IRQHandler(void) {
    if (SET == timer_interrupt_flag_get(TIMER1, TIMER_INT_CH0)) {
        timer_interrupt_flag_clear(TIMER1, TIMER_INT_CH0);

        if (measure_enabled && measurements_taken < MEASUREMENTS_PER_GESTURE) {
            mpu_vector_t vec;
            uint32_t idx = measurements_taken;

            seq_number++;
            led_set_B1(measurements_taken % 2 == 0);
            mpu6500_getAccel(&vec);

#if USE_SDCARD
            // SD card mode: use queue for streaming (no seq number needed)
            char buf[64] = {'\0'};
            last_sample_buffer[0] = (int16_t)vec.x;
            last_sample_buffer[1] = (int16_t)vec.y;
            last_sample_buffer[2] = (int16_t)vec.z;

            append_int_to_string(buf, last_sample_buffer[0]);
            append_int_to_string(buf, last_sample_buffer[1]);
            append_int_to_string(buf, last_sample_buffer[2]);

            strncat(buf, current_label, sizeof(buf) - strlen(buf) - 1);
            strcat(buf, "\r\n");
            enqueue_string(buf);
#else
            // Serial mode: store in buffer for later transmission (enables retry)
            sample_buffer[idx].seq = seq_number;
            sample_buffer[idx].x = (int16_t)vec.x;
            sample_buffer[idx].y = (int16_t)vec.y;
            sample_buffer[idx].z = (int16_t)vec.z;
#endif
            measurements_taken++;
        }
    }
}
