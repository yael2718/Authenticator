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

volatile uint8_t state_button = 1;         // État du bouton (1 = relâché, 0 = appuyé)
volatile uint8_t count_button = 0;     // Compteur pour détecter la stabilité de l'état
volatile uint8_t pressed_button = 0;  

typedef struct {
    uint8_t app_id[SHA1_SIZE];
    uint8_t credential_id[CREDENTIAL_ID_SIZE];
    uint8_t private_key[PRIVATE_KEY_SIZE];
} Credential;

Credential EEMEM eeprom_data[EEPROM_MAX_ENTRIES] ; // Stockage des données dans l'EEPROM
uint8_t EEMEM nb_credentials = 0 ; // Nombre d'entrées dans l'EEPROM

/**
 * @brief Configure le générateur pseudo-aléatoire, initialise les broches de la LED et du bouton, et configure l'UART.
 * 
 * @param Aucun.
 * @return Aucun.
 */
void config(void){

    // Initialisation de la seed pour le générateur pseudo-aléatoire
    srand(init_seed());
    uECC_set_rng(avr_rng);

    // Initialisation des broches
    DDRD |= (1 << LED_PIN);    // Configurer PD4 comme sortie pour la led
    DDRD &= ~(1 << BUTTON_PIN);   // Configurer PD2 comme entrée pour le bouton
    PORTD |= (1 << BUTTON_PIN);   // Activer la résistance pull-up interne pour le bouton

    // Initialisation de l'UART
    UART_init();
}

//--------------------------------- Random ---------------------------------
uint16_t read_adc() {
    // Configuration de l'ADC :
    ADMUX = (1 << REFS0);  // Référence AVcc.
    ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    //(1<<ADEN) pour activer l'ADC. (1<<ADSC) pour activer la conversion. (1<<ADPS0:2) set le division factor à 128 pour avoir une fréquence de 125kHz.

    // Attendre la fin de la conversion : le bit ADSC est set à 0 à la fin de la conversion.
    while (ADCSRA & (1 << ADSC));

    return ADC;
}

/**
 * @brief Initialise le générateur pseudo-aléatoire avec une graine obtenue à partir de l'ADC.
 * 
 * @param Aucun.
 * @return int : La graine générée.
 */
int init_seed() {
    uint32_t seed = 0;
    for (int i = 0; i < 4; i++) {
        seed ^= (read_adc() & 0xFF) << (i<<3);  // Combine les 8 bits significatifs
    }
    return seed;
}

/**
 * @brief Génère un nombre pseudo-aléatoire pour un nombre spécifié d'octets.
 * 
 * @param dest Pointeur où les octets aléatoires seront stockés.
 * @param size Nombre d'octets à générer.
 * @return int : 1 pour le succès.
 */
int avr_rng(uint8_t *dest, unsigned size) {
    for (unsigned i = 0; i < size; i++) {
        dest[i] = (uint8_t)(rand() & 0xFF); // Générer un octet pseudo-aléatoire
    }
    return 1; // Succès
}

// --------------------------------- Button methods ---------------------------------

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

    if (current_state != state_button) {       // Si l'état a changé
        count_button++;                             // Incrémenter le compteur
        if (count_button >= 4) {                    // Si l'état est stable pendant 4 cycles
            state_button = current_state;      // Mettre à jour l'état du bouton
            if (state_button == 0) {           // Si le bouton est stable à l'état bas
                pressed_button = 1;             // Signaler un appui validé
            }
            count_button = 0;                       // Réinitialiser le compteur
        }
    } else {
        count_button = 0;                           // Si l'état est constant, réinitialiser
    }
}

// --------------------------------- UART methods ---------------------------------

/**
 * @brief Initialise l'UART avec les paramètres spécifiés dans les macros BAUD et F_CPU.
 * 
 * @param Aucun.
 * @return Aucun.
 */
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

/**
 * @brief Envoie un octet via l'UART.
 * 
 * @param data L'octet à envoyer.
 * @return Aucun.
 */
void UART_putc(uint8_t data) {
    // Boucle jusqu'à ce que le registre UDR0 soit prêt pour une nouvelle donnée
    while (!(UCSR0A & (1 << UDRE0))) {
        // Attente active (polling)
    }
    // Envoie de la donnée
    UDR0 = data;
}

/**
 * @brief Envoie une séquence de données (motif) via l'UART.
 * 
 * @param pattern Pointeur vers les données à envoyer.
 * @param length Nombre d'octets à envoyer.
 * @return Aucun.
 */
void send_pattern(const char* pattern, uint8_t length) {
    for (uint8_t i = 0; i < length; i++) {
		UART_putc(pattern[i]);
    }
}

/**
 * @brief Reçoit un octet via l'UART.
 * 
 * @param Aucun.
 * @return uint8_t - L'octet reçu.
 */
uint8_t UART_getc(void){
    uint8_t data;
    while (!(UCSR0A & (1 << RXC0)));    // Attente jusqu'à réception d'un caractère
    data = UDR0;
    return data;                     // Retourne le caractère reçu
}

/**
 * @brief Traite une commande reçue via l'UART.
 * 
 * @param data Commande reçue (un octet).
 * @return Aucun.
 */
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

/**
 * @brief Stocke les informations dans l'EEPROM, incluant app_id, credential_id, private_key, et public_key.
 * 
 * @param app_id Pointeur vers l'empreinte de l'application (SHA1).
 * @param credential_id Pointeur vers le credential_id (16 octets).
 * @param private_key Pointeur vers la clé privée.
 * @param public_key Pointeur vers la clé publique.
 * @return Aucun.
 */
void store_in_eeprom(uint8_t *app_id, uint8_t *credential_id, uint8_t *private_key, uint8_t *public_key) {
    Credential current_entry;
    int i = 0;
    uint8_t nb = eeprom_read_byte(&nb_credentials);
    uint8_t app_id_found = 0;

    if (nb == EEPROM_MAX_ENTRIES) {
        // Si l'EEPROM est pleine, envoyer une erreur
        UART_putc(STATUS_ERR_STORAGE_FULL);
        return;
    }

    while (i < nb){
        eeprom_read_block(&current_entry, &eeprom_data[i], sizeof(Credential));
        // Si l'entrée correspond à app_id ou est vide
        if (memcmp(current_entry.app_id, app_id, SHA1_SIZE) == 0) {
            app_id_found = 1;
            break;
        }
        i++;
    }

    eeprom_read_block(&current_entry, &eeprom_data[i], sizeof(Credential));
    if (app_id_found == 0){
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

/**
 * @brief Génère une paire de clés (privée/publique) et un credential_id, puis les stocke dans l'EEPROM.
 * 
 * @param app_id Pointeur vers l'empreinte de l'application (SHA1).
 * @return Aucun.
 */
void gen_new_keys(uint8_t *app_id){
   if (!ask_for_approval()) {
        // Si l'utilisateur ne valide pas dans les 10 secondes, envoyer une erreur
        UART_putc(STATUS_ERR_APROVAL);
        return;
    }
    uint8_t private_key[PRIVATE_KEY_SIZE];
    uint8_t public_key[PUBLIC_KEY_SIZE];
    uint8_t credential_id[CREDENTIAL_ID_SIZE];

    if (!uECC_make_key(public_key, private_key)) {
        // Si la génération des clés a échoué, envoyer une erreur
        UART_putc(STATUS_ERR_CRYPTO_FAILED);
        return;
    }

    for(int i=0; i<CREDENTIAL_ID_SIZE; i++){
            credential_id[i] = app_id[i];
    }

    // Stocker les clés dans l'EEPROM
    store_in_eeprom(app_id, credential_id, private_key, public_key);
}

/**
 * @brief Gère la commande MakeCredential pour générer une nouvelle clé.
 *        Lit les données de l'UART et appelle la fonction `gen_new_keys`.
 * 
 * @param Aucun.
 * @return Aucun.
 */
void UART_handle_make_credential(void){
    uint8_t app_id[SHA1_SIZE]; // Tableau pour stocker l'empreinte

    for (int i = 0; i < SHA1_SIZE; i++) {
        app_id[i] = UART_getc(); // Stocker l'octet dans le tableau
    }
    // Appeler la fonction pour générer de nouvelles clés avec app_id
    gen_new_keys(app_id);
}

// --------------------------------- GetAssertion ---------------------------------

/**
 * @brief Signe les données client_data en utilisant la clé privée correspondant à app_id.
 * 
 * @param app_id Pointeur vers l'empreinte de l'application (SHA1).
 * @param client_data Pointeur vers les données client à signer (SHA1).
 * @return Aucun.
 */
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
        if (memcmp(current_entry.app_id, app_id, SHA1_SIZE) == 0){
            uint8_t signature[40];
            if (!uECC_sign(current_entry.private_key,
                    client_data,
                    signature)){
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

/**
 * @brief Gère la commande GetAssertion pour signer des données client.
 *        Lit app_id et client_data via l'UART, puis appelle la fonction `sign_data`.
 * 
 * @param Aucun.
 * @return Aucun.
 */
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

/**
 * @brief Liste les credentials stockés dans l'EEPROM, incluant app_id et credential_id.
 *        Envoie les informations via l'UART.
 * 
 * @param Aucun.
 * @return Aucun.
 */
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

/**
 * @brief Réinitialise l'EEPROM en supprimant tous les credentials et réinitialise le compteur nb_credentials.
 * 
 * @param Aucun.
 * @return Aucun.
 */
void UART_handle_reset(void) {
    if (!ask_for_approval()) {
        UART_putc(STATUS_ERR_APROVAL);
        return;
    }
    uint8_t nb = eeprom_read_byte(&nb_credentials);
    Credential empty_entry = {0};

    // Réinitialiser chaque entrée
    for (uint8_t i = 0; i < nb; i++) {
        eeprom_write_block(&empty_entry, &eeprom_data[i], sizeof(Credential));
    }

    // Réinitialiser le compteur
    eeprom_write_byte(&nb_credentials, 0);

    UART_putc(STATUS_OK);
}

// --------------------------------- Main ---------------------------------

/**
 * @brief Fonction principale qui configure les périphériques et exécute une boucle infinie pour traiter les commandes.
 * 
 * @param Aucun.
 * @return int : Code de sortie (0 si tout se passe bien).
 */
int main(void){
    config();
    while(1){
        uint8_t command = UART_getc();
        UART_handle_command(command);
    }
	return 0;
}
