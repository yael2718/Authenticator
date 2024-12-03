#ifndef UART_H
#define UART_H

#include <avr/io.h>
#include <avr/interrupt.h>
#include "ring_buffer.h"
#include <stddef.h>
#include <stdio.h>

void UART_init(uint32_t ubrr);
uint8_t UART_getc(void);
void UART_putc(uint8_t data);
void UART_handle_command(uint8_t data);

void gen_new_keys(uint32_t app_id);

#endif