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

#define SHA1_SIZE 20 // 20 bytes for the application ID
#define PRIVATE_KEY_SIZE 21 // secp160r1 requires 21 bytes for the private key
#define PUBLIC_KEY_SIZE 40 // secp160r1 requires 40 bytes for the public key
#define CREDENTIAL_ID_SIZE 16 // 128 bits for the credential ID
#define EEPROM_MAX_ENTRIES 17 // 1024 bytes / 57 ~ 17 (partie entière)

volatile uint8_t bouton_etat = 1;         // État du bouton (1 = relâché, 0 = appuyé)
volatile uint8_t bouton_compteur = 0;     // Compteur pour détecter la stabilité de l'état
volatile uint8_t pressed_button = 0;  

struct ring_buffer rx_buffer;   // Buffer de réception global
uint8_t buffer_data[BUFFER_SIZE]; // Tableau pour les données du buffer

typedef struct {
    uint8_t app_id[SHA1_SIZE];
    uint8_t credential_id[CREDENTIAL_ID_SIZE];
    uint8_t private_key[PRIVATE_KEY_SIZE];
} Credential;

Credential EEMEM eeprom_data[EEPROM_MAX_ENTRIES] ; // Stockage des données dans l'EEPROM
uint8_t EEMEM nb_credentials = 0 ; // Nombre d'entrées dans l'EEPROM


void config(void){
    // Initialisation des broches
    DDRD |= (1 << LED_PIN);    // Configurer PD4 comme sortie pour la led
    DDRD &= ~(1 << BUTTON_PIN);   // Configurer PD2 comme entrée pour le bouton
    PORTD |= (1 << BUTTON_PIN);   // Activer la résistance pull-up interne pour le bouton
}

int ask_for_approval(void) {
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

void debounce(void) {
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

void UART_init(uint32_t ubrr){
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
    }
}

// Interruption pour la réception UART
ISR(USART_RX_vect) {
    uint8_t data = UDR0;             // Lire la donnée reçue
    ring_buffer__push(&rx_buffer, data);  // Stocker dans le buffer
}

void UART_getc(void){
    uint8_t data;
    // Attendre qu'une donnée soit disponible dans le buffer
    while (ring_buffer__pop(&rx_buffer, &data));
    UART_handle_command(data);
}

void UART_handle_command(uint8_t data){
	if (data == COMMAND_MAKE_CREDENTIAL){ // Message MakeCredential
        UART_handle_make_credential();
	} else if (data == COMMAND_GET_ASSERTION) { // Message GetAssertion
        UART_handle_get_assertion();
	} else if (data == COMMAND_LIST_CREDENTIALS){ // Message ListCredentials
        UART_handle_list_credentials();
    } else if (data == COMMAND_RESET){ // Message Reset
        UART_handle_reset();
    } else {
        // Si la commande est inconnue, envoyer une erreur
        UART_putc(STATUS_ERR_COMMAND_UNKNOWN);
	}
}

// --------------------------------- MakeCredential ---------------------------------

void UART_handle_make_credential(void){
    uint8_t app_id[SHA1_SIZE]; // Tableau pour stocker l'empreinte
    uint8_t temp;

    for (int i = 0; i < 20; i++) {
        if (ring_buffer__pop(&rx_buffer, &temp)) {
            // Si un octet manque, envoyer une erreur et réinitialiser le tableau
            UART_putc(STATUS_ERR_BAD_PARAMETER);
        }
        app_id[i] = temp; // Stocker l'octet dans le tableau
    }
    // Appeler la fonction pour générer de nouvelles clés avec app_id
    gen_new_keys(app_id);
}

void store_in_eeprom(uint8_t *app_id, uint8_t *credential_id, uint8_t *private_key, uint8_t *public_key) {
    Credential current_entry;
    int i = 0;

    if (eeprom_read_byte(&nb_credentials) == EEPROM_MAX_ENTRIES) {
        // Si l'EEPROM est pleine, envoyer une erreur
        UART_putc(STATUS_ERR_STORAGE_FULL);
        return;
    }

    while (i < eeprom_read_byte(&nb_credentials)){
        eeprom_read_block(&current_entry, &eeprom_data[i], sizeof(Credential));

        // Si l'entrée correspond à app_id ou est vide
        if (current_entry.app_id == app_id){
            break;
        }
        i++;
    }
    eeprom_read_block(&current_entry, &eeprom_data[i], sizeof(Credential));
    if (current_entry.app_id != app_id){
        memcpy(current_entry.app_id, app_id, SHA1_SIZE);
        // Incrémenter le nombre d'entrées
        eeprom_update_byte(&nb_credentials, eeprom_read_byte(&nb_credentials) + 1);
    }
    memcpy(current_entry.credential_id, credential_id, CREDENTIAL_ID_SIZE);
    memcpy(current_entry.private_key, private_key, PRIVATE_KEY_SIZE);
    eeprom_update_block(&current_entry, &eeprom_data[i], sizeof(Credential));

    // Envoyer un message de confirmation
    UART_putc(STATUS_OK);
    send_pattern((const char*)credential_id, CREDENTIAL_ID_SIZE); // Envoyer credential_id
    send_pattern((const char*)public_key, PUBLIC_KEY_SIZE);       // Envoyer public_key
}

void gen_new_keys(uint8_t *app_id){
   if (!ask_for_approval()) {
        // Si l'utilisateur ne valide pas dans les 10 secondes, envoyer une erreur
        UART_putc(STATUS_ERR_APROVAL);
        return;
    }
    uint8_t private_key[PRIVATE_KEY_SIZE];
    uint8_t public_key[PUBLIC_KEY_SIZE];
    uint8_t credential_id[CREDENTIAL_ID_SIZE];

    // Select the curve secp160r1
    uECC_Curve curve = uECC_secp160r1();

    if (!uECC_make_key(public_key, private_key, curve)) {
        // Si la génération des clés a échoué, envoyer une erreur
        UART_putc(STATUS_ERR_CRYPTO_FAILED);
        return;
    }
    // Generate credential_id (16 random bytes)
    uECC_RNG_Function rng_function = uECC_get_rng();
    rng_function(credential_id, CREDENTIAL_ID_SIZE);

    // Stocker les clés dans l'EEPROM
    store_in_eeprom(app_id, credential_id, private_key, public_key);
}

// --------------------------------- GetAssertion ---------------------------------

void sign_data(uint8_t *app_id, uint8_t* client_data){
    Credential current_entry;
    int i = 0;

    if (!ask_for_approval()) {
        // Si l'utilisateur ne valide pas dans les 10 secondes, envoyer une erreur
        UART_putc(STATUS_ERR_APROVAL);
        return;
    }

    while (i < EEPROM_MAX_ENTRIES){
        eeprom_read_block(&current_entry, &eeprom_data[i], sizeof(Credential));
        // Si l'entrée correspond à app_id
        if (current_entry.app_id == app_id){
            uECC_Curve curve = uECC_secp160r1();
            uint8_t signature[40];
            if (!uECC_sign(current_entry.private_key,
                    client_data,
                    SHA1_SIZE,
                    signature,
                    curve)){
                // Si la signature a échoué, envoyer une erreur
                UART_putc(STATUS_ERR_CRYPTO_FAILED);
                return;
            }
            // Envoyer un message de confirmation
            UART_putc(STATUS_OK);
            send_pattern((const char*)current_entry.credential_id, CREDENTIAL_ID_SIZE); // Envoyer credential_id
            send_pattern((const char*)signature, PUBLIC_KEY_SIZE);       // Envoyer signature
            return;
        }
        i++;
    }
    // Si l'entrée n'est pas trouvée, envoyer une erreur
    UART_putc(STATUS_ERR_NOT_FOUND);
}

void UART_handle_get_assertion(void){
    uint8_t app_id[SHA1_SIZE]; // Tableau pour stocker l'empreinte
    uint8_t client_data[SHA1_SIZE]; // Tableau pour stocker le hachage
    uint8_t temp;

    for (int i = 0; i < SHA1_SIZE; i++) {
        if (ring_buffer__pop(&rx_buffer, &temp)) {
            // Si un octet manque, envoyer une erreur
            UART_putc(STATUS_ERR_BAD_PARAMETER);
        }
        app_id[i] = temp; // Stocker l'octet dans le tableau
    }
    for (int i = 0; i < SHA1_SIZE; i++) {
        if (ring_buffer__pop(&rx_buffer, &temp)) {
            // Si un octet manque, envoyer une erreur
            UART_putc(STATUS_ERR_BAD_PARAMETER);
        }
        client_data[i] = temp; // Stocker l'octet dans le tableau
    }

    // Appeler la fonction pour signer les données
    sign_data(app_id, client_data);
}

// --------------------------------- ListCredentials ---------------------------------

void UART_handle_list_credentials(void){
    UART_putc(STATUS_OK);
    UART_putc(eeprom_read_byte(&nb_credentials));
    uint8_t i = 0;
    Credential current_entry;
    while (i < nb_credentials){
        eeprom_read_block(&current_entry, &eeprom_data[i], sizeof(Credential));
        send_pattern((const char*)current_entry.credential_id, CREDENTIAL_ID_SIZE); // Envoyer credential_id
        send_pattern((const char*)current_entry.app_id, SHA1_SIZE); // Envoyer app_id
        i++;
    }
}

// --------------------------------- Reset ---------------------------------

void UART_handle_reset(void){
    if (!ask_for_approval()) {
        // Si l'utilisateur ne valide pas dans les 10 secondes, envoyer une erreur
        UART_putc(STATUS_ERR_APROVAL);
        return;
    }
    int mem_len = eeprom_read_byte(&nb_credentials) * sizeof(Credential);
    for (int i = 0; i < mem_len; i++){
        eeprom_write_byte(&((uint8_t*)eeprom_data)[i], 0);
    }
    eeprom_write_byte(&nb_credentials, 0);
    // Envoyer un message de confirmation
    UART_putc(STATUS_OK);
}



int main(void){

    uECC_set_rng(RNG_Function);
    config();
    UART_init(MYUBRR);

    while(1){
        UART_getc();
    }
	return 0;
}
