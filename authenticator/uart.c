#include "uart.h"

#define FOSC 1843200 // Clock Speed
#define BAUD 9600 // Baud rate
#define MYUBRR FOSC/16/BAUD-1
#define BUFFER_SIZE 64

// MakeCredentialError messages
#define STATUS_ERR_BAD_PARAMETER 0x0A
#define STATUS_ERR_APROVAL 0x0B
#define STATUS_ERR_CRYPTO_FAILED 0x0C
#define STATUS_ERR_STORAGE_FULL 0x0D

struct ring_buffer rx_buffer;   // Buffer de réception global
uint8_t buffer_data[BUFFER_SIZE]; // Tableau pour les données du buffer

void UART__init(uint32_t ubrr){
    /* Set baud rate */
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;
    /* Enable receiver and transmitter */
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);
    // Set frame format: 8 data bits, 1 stop bit
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
    while (ring_buffer__pop(&rx_buffer, &data));
    UART_handle_command(data);
}

void UART_handle_command(data){
	if (data == 1){ // Message MakeCredential
        uint8_t app_id[20]; // Tableau pour stocker l'empreinte
        uint8_t temp;

        for (int i = 0; i < 20; i++) {
            if (!ring_buffer__pop(&rx_buffer, &temp)) {
                // Si un octet manque, envoyer une erreur et réinitialiser le tableau
                UART_putc(STATUS_ERR_BAD_PARAMETER);
                memset(app_id, 0, sizeof(app_id)); // Réinitialiser le tableau
            }
            app_id[i] = temp; // Stocker l'octet dans le tableau
        }

        // Appeler la fonction pour générer de nouvelles clés avec app_id
        gen_new_keys(app_id);

	} else if (data == 2) { // Message GetAssertion
        uint8_t app_id[20]; // Tableau pour stocker l'empreinte
        uint8_t client_data[20]; // Tableau pour stocker les données du client
        uint8_t temp;

	} else { // Message invalide
	}
}

void gen_new_keys(uint32_t app_id){
    // TODO
    /*
    - Faire clignoter la led jusqu'a ce que l'utilisateur presse le bouton
    --> s'il le presse, generer une paire de clés, stocker et envoyer la pk via uart
    --> si au bout de 10 secondes pas pressé de bouton, envoi message STATUS_ERR_APROVAL
    */
}

int main(void){
	return 0;
}
