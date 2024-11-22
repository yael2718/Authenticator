#include "uart.h"

#define FOSC 1843200 // Clock Speed
#define BAUD 9600 // Baud rate
#define MYUBRR FOSC/16/BAUD-1
#define BUFFER_SIZE 64

// MakeCredentialError messages
#define STATUS_ERR_BAD_PARAMETER 0x01
#define STATUS_ERR_APROVAL 0x02
#define STATUS_ERR_CRYPTO_FAILED 0x03
#define STATUS_ERR_STORAGE_FULL 0x04

struct ring_buffer rx_buffer;   // Buffer de réception global
uint8_t buffer_data[BUFFER_SIZE]; // Tableau pour les données du buffer

// Initialisation UART
void UART_init(uint32_t ubrr){
    /* Définir le débit en bauds */
    UBRR0H = (unsigned char)(ubrr>>8);
    UBRR0L = (unsigned char)ubrr;
    /* Activer la réception et la transmission */
    UCSR0B = (1<<RXEN0)|(1<<TXEN0);
    // Activer l'interruption "Réception complète du USART" (USART_RXC)
    UCSR0B |= (1<<RXCIE0);

    // Équivalent à UCSR0C = (1<<UCSZ01) | (1<<UCSZ00) //
    // On configure UCSZ00 et UCSZ01 à 1 afin d'obtenir des données sur 8 bits.
    // On ne configure pas UPM0 et UPM1 pour ne pas activer les bits de parité.
    UCSR0C = (3<<UCSZ00);

    // Initialisation du ring buffer
    ring_buffer__init(&rx_buffer, buffer_data, BUFFER_SIZE);
    
    // Activer globalement les interruptions
    sei();
}

// Envoi d'un caractère via UART
void UART_putc(uint8_t data) {
    // Boucle jusqu'à ce que le registre UDR0 soit prêt pour une nouvelle donnée
    while (!(UCSR0A & (1 << UDRE0))) {
        // Attente active (polling)
    }
    // Envoie de la donnée
    UDR0 = data;
}

// Envoi du motif via UART
void send_pattern(const char* pattern, uint8_t length) {
    for (uint8_t i = 0; i < length; i++) {
		UART_putc(pattern[i]);
        // TODO
    }
}

// Interruption pour la réception UART
ISR(USART_RX_vect) {
    uint8_t data = UDR0;             // Lire la donnée reçue
    ring_buffer__push(&rx_buffer, data);  // Stocker dans le buffer
}

uint8_t UART_getc(void){
    uint8_t data;
    // Attendre qu'une donnée soit disponible dans le buffer
    while (!ring_buffer__pop(&rx_buffer, &data));
    UART_handle_command(data);
}

void UART_handle_command(data){
	if (data == 1){ // Message MakeCredential
	} else if (data == 2) { // Message GetAssertion
	} else { // Message invalide
	}
}

int main(void){
	return 0;
}
