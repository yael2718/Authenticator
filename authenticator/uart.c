#include "uart.h"

#define LED_PIN PD6
#define BUTTON_PIN PD2

#define FOSC 1843200 // Clock Speed
#define F_CPU 16000000UL
#define BAUD 115200 // Baud rate
#define USE_2X 1

// MakeCredentialError messages
#define COMMAND_LIST_CREDENTIALS 0
#define COMMAND_MAKE_CREDENTIAL 1
#define COMMAND_GET_ASSERTION 2
#define COMMAND_RESET 3
#define STATUS_OK 0
#define STATUS_ERR_COMMAND_UNKNOWN 1
#define STATUS_ERR_CRYPTO_FAILED 2
#define STATUS_ERR_BAD_PARAMETER 3
#define STATUS_ERR_NOT_FOUND 4
#define STATUS_ERR_STORAGE_FULL 5
#define STATUS_ERR_APROVAL 6

#define SHA1_SIZE 20 // 20 bytes for the application ID
#define PRIVATE_KEY_SIZE 21 // secp160r1 requires 21 bytes for the private key
#define PUBLIC_KEY_SIZE 40 // secp160r1 requires 40 bytes for the public key
#define CREDENTIAL_ID_SIZE 16 // 128 bits for the credential ID
#define EEPROM_MAX_ENTRIES 17 // 1024 bytes / 57 ~ 17 (partie entière)

volatile uint8_t bouton_etat = 1;         // État du bouton (1 = relâché, 0 = appuyé)
volatile uint8_t bouton_compteur = 0;     // Compteur pour détecter la stabilité de l'état
volatile uint8_t pressed_button = 0;  

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

void UART_init() {
    // Calcul de UBRR
    #include <util/setbaud.h>

    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;
    #if USE_2X
    UCSR0A |= (1 << U2X0); // Double vitesse
    #else
    UCSR0A &= ~(1 << U2X0); // Mode normal
    #endif

    UCSR0B = (1 << RXEN0) | (1 << TXEN0);   // Activation de la réception et de la transmission
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // Format de trame : 8 bits de données, 1 bit de stop
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

uint8_t UART_getc(void){
    uint8_t data;
    while (!(UCSR0A & (1 << RXC0)));    // Attente jusqu'à réception d'un caractère
    data = UDR0;
    return data;                     // Retourne le caractère reçu
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

    for (int i = 0; i < 20; i++) {
        app_id[i] = UART_getc(); // Stocker l'octet dans le tableau
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
    uint8_t private_key[PRIVATE_KEY_SIZE] = {0};
    uint8_t public_key[PUBLIC_KEY_SIZE] = {0};
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

    for (int i = 0; i < SHA1_SIZE; i++) {
        app_id[i] = UART_getc(); // Stocker l'octet dans le tableau
    }
    for (int i = 0; i < SHA1_SIZE; i++) {
        client_data[i] = UART_getc(); // Stocker l'octet dans le tableau
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

void UART_handle_reset(void) {
    if (!ask_for_approval()) {
        UART_putc(STATUS_ERR_APROVAL);
        return;
    }

    size_t eeprom_size = EEPROM_MAX_ENTRIES * sizeof(Credential);

    uint8_t zero_buffer[eeprom_size];
    memset(zero_buffer, 0, eeprom_size);

    eeprom_write_block(zero_buffer, eeprom_data, eeprom_size);

    eeprom_write_byte(&nb_credentials, 0);

    UART_putc(STATUS_OK);
}




int main(void){

    uECC_set_rng(RNG_Function);
    config();
    UART_init();

    while(1){
        uint8_t command = UART_getc();
        UART_handle_command(command);
    }
	return 0;
}
