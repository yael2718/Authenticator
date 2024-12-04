#ifndef UART_H
#define UART_H

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "ring_buffer.h"
#include "../uECC/uECC.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

void config();
void UART_init(uint32_t ubrr);
uint8_t UART_getc(void);
void UART_putc(uint8_t data);
void UART_handle_command(uint8_t data);
void UART_handle_make_credential(void);
void UART_handle_get_assertion(void);
void UART_handle_list_credentials(void);
void UART_handle_reset(void);

int ask_for_approval();
void debounce();
void gen_new_keys(uint8_t *app_id);
void store_in_eeprom(uint8_t *app_id, uint8_t *credential_id, uint8_t *private_key, uint8_t *public_key);
#endif