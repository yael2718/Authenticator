#include "uart.h"

#define LED_PIN PD4
#define BUTTON_PIN PD2 

#define FOSC 1843200 // Clock Speed
#define BAUD 9600 // Baud rate
#define MYUBRR FOSC/16/BAUD-1
#define BUFFER_SIZE 64

// MakeCredentialError messages
#define COMMAND_LIST_CREDENTIALS 0x00
#define COMMAND_MAKE_CREDENTIAL 0x01
#define COMMAND_GET_ASSERTION 0x02
#define COMMAND_RESET 0x03
#define STATUS_OK 0x00
#define STATUS_ERR_COMMAND_UNKNOWN 0x01
#define STATUS_ERR_CRYPTO_FAILED 0x02
#define STATUS_ERR_BAD_PARAMETER 0x03
#define STATUS_ERR_NOT_FOUND 0x04
#define STATUS_ERR_STORAGE_FULL 0x05
#define STATUS_ERR_APROVAL 0x06

#define PRIVATE_KEY_SIZE 21 // secp160r1 requires 21 bytes for the private key
#define PUBLIC_KEY_SIZE 40 // secp160r1 requires 40 bytes for the public key

volatile uint8_t bouton_etat = 1;         // État du bouton (1 = relâché, 0 = appuyé)
volatile uint8_t bouton_compteur = 0;     // Compteur pour détecter la stabilité de l'état
volatile uint8_t pressed_button = 0;  

struct ring_buffer rx_buffer;   // Buffer de réception global
uint8_t buffer_data[BUFFER_SIZE]; // Tableau pour les données du buffer

void config(){
    // Initialisation des broches
    DDRD |= (1 << LED_PIN);    // Configurer PD4 comme sortie pour la led
    DDRD &= ~(1 << BUTTON_PIN);   // Configurer PD2 comme entrée pour le bouton
    PORTD |= (1 << BUTTON_PIN);   // Activer la résistance pull-up interne pour le bouton
}

void ask_for_approval() {
    for(int i = 0; i < 10; i++){
        PORTD ^= (1 << LED_PIN);     //  allumer la led pendant 0.5 seconde
        for(int j = 0; j < 33 ; j++){   //  j va jusqu'à 33 car 500/15 = 33
            debounce();
            if(pressed_button){
                pressed_button = 0;
                PORTD ^= (1 << LED_PIN);    //  eteindre la led
                return 1;                   //  confirmation obtenue
            }
            _delay_ms(15);
        }
        PORTD ^= (1 << LED_PIN);     //  eteindre la led pendant 0.5 seconde
        for(int j = 0; j < 33 ; j++){
            debounce();
            if(pressed_button){
                pressed_button = 0;   //  
                return 1;            //  confirmation obtenue
            }
            _delay_ms(15);
        }
    }
    return 0; 
}

void debounce() {
    uint8_t current_state = PIND & (1 << BUTTON_PIN); // Lire l'état actuel du bouton (PD2)

    if (current_state != bouton_etat) {       // Si l'état a changé
        bouton_compteur++;                             // Incrémenter le compteur
        if (bouton_compteur >= 4) {                    // Si l'état est stable pendant 4 cycles
            bouton_etat = current_state;      // Mettre à jour l'état du bouton
            if (bouton_etat == 0) {           // Si le bouton est stable à l'état bas
                pressed_button = 1;             // Signaler un appui validé
            }
            bouton_compteur = 0;                       // Réinitialiser le compteur
        }
    } else {
        bouton_compteur = 0;                           // Si l'état est constant, réinitialiser
    }
}

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
	if (data == COMMAND_MAKE_CREDENTIAL){ // Message MakeCredential
        UART_handle_make_credential();
	} else if (data == COMMAND_GET_ASSERTION) { // Message GetAssertion
        uint8_t app_id[20]; // Tableau pour stocker l'empreinte
        uint8_t client_data[20]; // Tableau pour stocker les données du client
        uint8_t temp;

	} else { // Message invalide

	}
}

void UART_handle_make_credential(){
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
}

void gen_new_keys(uint32_t app_id){
   if (!ask_for_approval()) {
        // Si l'utilisateur ne valide pas dans les 10 secondes, envoyer une erreur
        UART_putc(STATUS_ERR_APPROVAL);
        memset(app_id, 0, sizeof(app_id)); // Réinitialiser le tableau
        return;
    }
    uint8_t private_key[PRIVATE_KEY_SIZE];
    uint8_t public_key[PUBLIC_KEY_SIZE];

    // Select the curve secp160r1
    uECC_Curve curve = uECC_secp160r1();

    if (!uECC_make_key(public_key, private_key, curve)) {
        // Si la génération des clés a échoué, envoyer une erreur
        UART_putc(STATUS_ERR_CRYPTO_FAILED);
        memset(app_id, 0, sizeof(app_id)); // Réinitialiser le tableau
        return;
    }

    /*
    L’Authenticator génère également un identifiant unique associé à cette paire, appelé
    credential_id, de longueur 128 bits. Cet identifiant ne doit pas être lié au nombre de paires de
    clés stockées par l’Authenticator.
    L’Authenticator sauvegarde de façon non volatile l’association entre l’empreinte de app_id,
    credential_id et la clé privée générée.
    Si la mémoire non volatile est saturée et qu’il n’est pas possible d’ajouter la nouvelle entrée,
    l’Authenticator envoie un message MakeCredentialError avec le code
    STATUS_ERR_STORAGE_FULL.
    Si une entrée existe déjà pour l’empreinte de app_id, ses valeurs doivent être remplacées par la
    nouvelle entrée.
    Si aucune erreur ne s’est produite, l’Authenticator envoie un message MakeCredentialResponse
    contenant :
    • credential_id (16 octets)
    • la clé publique générée (40 octets)
    */


}

int main(void){
	return 0;
}
