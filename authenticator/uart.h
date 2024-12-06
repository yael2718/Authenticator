#ifndef UART_H
#define UART_H

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include "ecc/uECC.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

void config(void);
void UART_init(void);
uint8_t UART_getc(void);
void UART_putc(uint8_t data);
void UART_handle_command(uint8_t data);
void UART_handle_make_credential(void);
void UART_handle_get_assertion(void);
void UART_handle_list_credentials(void);
void UART_handle_reset(void);

int ask_for_approval(void);
void debounce(void);
void gen_new_keys(uint8_t *app_id);
void sign_data(uint8_t *app_id, uint8_t* client_data);
void send_pattern(const char* pattern, uint8_t length);
void store_in_eeprom(uint8_t *app_id, uint8_t *credential_id, uint8_t *private_key, uint8_t *public_key);

int avr_rng(uint8_t *dest, unsigned size);
#endif