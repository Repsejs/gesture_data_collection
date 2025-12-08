#pragma once

#include "cdc_acm_core.h"
#include <stdio.h>
#include <stdlib.h>

extern uint8_t packet_sent, packet_receive;
extern uint32_t receive_length;
extern usb_core_driver USB_OTG_dev;

void configure_usb_serial(void);
int usb_serial_available(void);
size_t read_usb_serial(uint8_t* data);
void set_usb_clock_96m_hxtal(void);
void usb_serial_flush(void);

ssize_t _write(int fd, const void* ptr, size_t len);
