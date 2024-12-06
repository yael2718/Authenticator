#include "uart.h"

#define LED_PIN PD6
#define BUTTON_PIN PD2

#define F_CPU 16000000UL // Microcontroller frequency
#define BAUD 115200 // Baud rate
#define USE_2X 1

// Command identifiers
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
#define STATUS_ERR_APPROVAL 6

#define SHA1_SIZE 20 // 20 bytes for the application ID
#define PRIVATE_KEY_SIZE 21 // secp160r1 requires 21 bytes for the private key
#define PUBLIC_KEY_SIZE 40 // secp160r1 requires 40 bytes for the public key
#define CREDENTIAL_ID_SIZE 16 // 128 bits for the credential ID
#define EEPROM_MAX_ENTRIES 17 // Maximum entries that fit in 1024 bytes ~ 1024/(SHA1_SIZE+PRIVATE_KEY_SIZE+CREDENTIAL_ID_SIZE)

volatile uint8_t state_button = 1;    // Button state (1 = released, 0 = pressed)
volatile uint8_t count_button = 0;    // Counter for debounce stability
volatile uint8_t pressed_button = 0;  // Flag for a confirmed button press  

/**
 * @brief Structure representing a data entry for the authenticator.
 * 
 * This structure is used to store information associated with:
 * - an application identifier (`app_id`),
 * - a credential identifier (`credential_id`),
 * - and a private key (`private_key`).
 * 
 * It is saved in EEPROM memory for persistent storage.
 * 
 * Fields:
 * - app_id: SHA-1 hash associated with the application (20 bytes).
 * - credential_id: Unique identifier generated for the key pair (16 bytes).
 * - private_key: Private key used for signing data (21 bytes for secp160r1).
 */
typedef struct {
    uint8_t app_id[SHA1_SIZE];
    uint8_t credential_id[CREDENTIAL_ID_SIZE];
    uint8_t private_key[PRIVATE_KEY_SIZE];
} Credential;

Credential EEMEM eeprom_data[EEPROM_MAX_ENTRIES] ; // Persistent storage in EEPROM
uint8_t EEMEM nb_credentials = 0 ; // Number of credentials in EEPROM

//--------------------------------- Setup ---------------------------------

/**
 * @brief Configures the RNG, initializes pins for LED and button, and sets up UART.
 * 
 * @param None.
 * @return None.
 */
void config(void) {
    // Initialize seed for the pseudo-random number generator
    srand(random_seed());
    uECC_set_rng(avr_rng);

    // Initialize GPIO pins
    DDRD |= (1 << LED_PIN);    // Configure PD6 as output for the LED
    DDRD &= ~(1 << BUTTON_PIN);   // Configure PD2 as input for the button
    PORTD |= (1 << BUTTON_PIN);   // Enable the internal pull-up resistor for the button

    // Initialize UART
    UART_init();
}


//--------------------------------- Random ---------------------------------

/**
 * @brief Reads an analog value using the ADC (Analog-to-Digital Converter).
 * 
 * @param None.
 * @return uint16_t : The result of the ADC conversion.
 */
uint16_t read_adc() {
    // Configure ADC
    ADMUX = (1 << REFS0);  // Use AVcc as the reference voltage
    ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    // (1 << ADEN) enables the ADC. (1 << ADSC) starts the conversion.
    // (1 << ADPS0:2) sets the prescaler to 128 for a 125 kHz ADC clock.

    // Wait for the conversion to complete (ADSC is cleared upon completion)
    while (ADCSRA & (1 << ADSC));

    return ADC; // Return the ADC result
}

/**
 * @brief Generates a random seed based on ADC readings.
 * 
 * @param None.
 * @return int : The generated seed value.
 */
int random_seed() {
    uint32_t seed = 0;
    for (int i = 0; i < 4; i++) {
        seed ^= (read_adc() & 0xFF) << (i << 3); // Combine the 8 significant bits
    }
    return seed;
}

/**
 * @brief Generates pseudo-random bytes and stores them in the specified buffer.
 * 
 * @param dest Pointer to the buffer where random bytes will be stored.
 * @param size Number of bytes to generate.
 * @return int : Always returns 1 (success).
 */
int avr_rng(uint8_t *dest, unsigned size) {
    for (unsigned i = 0; i < size; i++) {
        dest[i] = (uint8_t)(rand() & 0xFF); // Generate a pseudo-random byte
    }
    return 1; // Success
}

// --------------------------------- Button methods ---------------------------------

/**
 * @brief Debounces a button to ensure stable state transitions.
 * 
 * @param None.
 * @return None.
 */
void debounce(void) {
    uint8_t current_state = PIND & (1 << BUTTON_PIN); // Read the current button state (PD2)

    if (current_state != state_button) { // If the state has changed
        count_button++;                  // Increment the counter
        if (count_button >= 4) {         // If stable for 4 cycles
            state_button = current_state; // Update the button state
            if (state_button == 0) {      // If the button is confirmed pressed
                pressed_button = 1;       // Flag the press
            }
            count_button = 0;            // Reset the counter
        }
    } else {
        count_button = 0; // Reset the counter if the state is constant
    }
}

/**
 * @brief Waits for user approval through a button press within 10 seconds.
 *        Flashes the LED while waiting for input.
 * 
 * @param None.
 * @return int : 1 if the button is pressed, 0 otherwise.
 */
int ask_for_approval(void) {
    for (int i = 0; i < 10; i++) {
        PORTD ^= (1 << LED_PIN); // Turn the LED on for 0.5 seconds
        for (int j = 0; j < 33; j++) { // 500 ms / 15 ms = ~33 iterations
            debounce();
            if (pressed_button) {
                pressed_button = 0;
                PORTD ^= (1 << LED_PIN); // Turn off the LED
                return 1; // Approval obtained
            }
            _delay_ms(15);
        }
        PORTD ^= (1 << LED_PIN); // Turn the LED off for 0.5 seconds
        for (int j = 0; j < 33; j++) {
            debounce();
            if (pressed_button) {
                pressed_button = 0;
                return 1; // Approval obtained
            }
            _delay_ms(15);
        }
    }
    return 0; // Timeout without approval
}

// --------------------------------- UART methods ---------------------------------

/**
 * @brief Initializes the UART with the specified parameters (BAUD, F_CPU).
 * 
 * @param None.
 * @return None.
 */
void UART_init() {
    #include <util/setbaud.h> // Include setbaud.h for UBRR calculation macros

    UBRR0H = UBRRH_VALUE; // Set the high byte of the baud rate
    UBRR0L = UBRRL_VALUE; // Set the low byte of the baud rate
    #if USE_2X
    UCSR0A |= (1 << U2X0); // Enable double-speed mode
    #else
    UCSR0A &= ~(1 << U2X0); // Use normal speed mode
    #endif

    UCSR0B = (1 << RXEN0) | (1 << TXEN0);   // Enable UART receiver and transmitter
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // Configure 8 data bits, 1 stop bit
}

/**
 * @brief Sends a single byte of data over UART.
 * 
 * @param data The byte to be transmitted.
 * @return None.
 */
void UART_putc(uint8_t data) {
    while (!(UCSR0A & (1 << UDRE0))) {
        // Wait until the data register is ready for transmission
    }
    UDR0 = data; // Send the byte
}

/**
 * @brief Sends a sequence of bytes (a pattern) over UART.
 * 
 * @param pattern Pointer to the data sequence to send.
 * @param length Number of bytes to send.
 * @return None.
 */
void send_pattern(const char* pattern, uint8_t length) {
    for (uint8_t i = 0; i < length; i++) {
        UART_putc(pattern[i]);
    }
}

/**
 * @brief Receives a single byte of data from UART.
 * 
 * @param None.
 * @return uint8_t - The received byte.
 */
uint8_t UART_getc(void) {
    while (!(UCSR0A & (1 << RXC0))) {
        // Wait until data is available
    }
    return UDR0; // Return the received byte
}

/**
 * @brief Handles commands received via UART by executing the appropriate action.
 * 
 * @param data The received command byte.
 * @return None.
 */
void UART_handle_command(uint8_t data) {
    switch (data) {
        case COMMAND_MAKE_CREDENTIAL:
            UART_handle_make_credential();
            break;
        case COMMAND_GET_ASSERTION:
            UART_handle_get_assertion();
            break;
        case COMMAND_LIST_CREDENTIALS:
            UART_handle_list_credentials();
            break;
        case COMMAND_RESET:
            UART_handle_reset();
            break;
        default:
            UART_putc(STATUS_ERR_COMMAND_UNKNOWN); // Send error for unknown command
    }
}

// --------------------------------- MakeCredential ---------------------------------

/**
 * @brief Stores the given key and credential information in EEPROM.
 * 
 * @param app_id Pointer to the application ID (20-byte SHA1 hash).
 * @param credential_id Pointer to the credential ID (16 bytes).
 * @param private_key Pointer to the private key (21 bytes).
 * @param public_key Pointer to the public key (40 bytes).
 * @return None.
 */
void store_in_eeprom(uint8_t *app_id, uint8_t *credential_id, uint8_t *private_key, uint8_t *public_key) {
    Credential current_entry;
    uint8_t nb = eeprom_read_byte(&nb_credentials);
    uint8_t app_id_found = 0;

    // Check if the EEPROM is full
    if (nb == EEPROM_MAX_ENTRIES) {
        UART_putc(STATUS_ERR_STORAGE_FULL); // EEPROM is full
        return;
    }

    // Stops when finding an empty entry or the app_id
    for (uint8_t i = 0; i < nb; i++) {
        eeprom_read_block(&current_entry, &eeprom_data[i], sizeof(Credential));
        if (memcmp(current_entry.app_id, app_id, SHA1_SIZE) == 0) {
            app_id_found = 1; // App ID already exists
            break;
        }
    }

    // Store the new entry in EEPROM
    if (!app_id_found) {
        memcpy(current_entry.app_id, app_id, SHA1_SIZE); // Store new app ID
        eeprom_update_byte(&nb_credentials, nb + 1);     // Increment credential count
    }
    memcpy(current_entry.credential_id, credential_id, CREDENTIAL_ID_SIZE);
    memcpy(current_entry.private_key, private_key, PRIVATE_KEY_SIZE);
    eeprom_update_block(&current_entry, &eeprom_data[nb], sizeof(Credential));

    // Send confirmation message
    UART_putc(STATUS_OK);
    send_pattern((const char*)credential_id, CREDENTIAL_ID_SIZE);
    send_pattern((const char*)public_key, PUBLIC_KEY_SIZE);
}
/**
 * @brief Generates a new key pair, associates it with an app ID, and stores the data in EEPROM.
 * 
 * @param app_id Pointer to the application ID (20-byte SHA1 hash).
 * @return None.
 */
void gen_new_keys(uint8_t *app_id) {
    if (!ask_for_approval()) {
        UART_putc(STATUS_ERR_APPROVAL); // Approval not granted
        return;
    }

    uint8_t private_key[PRIVATE_KEY_SIZE];
    uint8_t public_key[PUBLIC_KEY_SIZE];
    uint8_t credential_id[CREDENTIAL_ID_SIZE];

    if (!uECC_make_key(public_key, private_key)) {
        UART_putc(STATUS_ERR_CRYPTO_FAILED); // Key generation failed
        return;
    }

    for (int i = 0; i < CREDENTIAL_ID_SIZE; i++) {
        credential_id[i] = app_id[i]; // Generate credential ID based on app ID
    }

    store_in_eeprom(app_id, credential_id, private_key, public_key);
}
/**
 * @brief Handles the MakeCredential command by generating a new key pair and storing it.
 * 
 * @param None.
 * @return None.
 */
void UART_handle_make_credential(void) {
    uint8_t app_id[SHA1_SIZE]; // Buffer to store the application ID

    for (int i = 0; i < SHA1_SIZE; i++) {
        app_id[i] = UART_getc(); // Read the application ID from UART
    }
    gen_new_keys(app_id); // Generate and store new keys
}

// --------------------------------- GetAssertion ---------------------------------

/**
 * @brief Signs client data using the private key associated with the given app ID.
 * 
 * @param app_id Pointer to the application ID (20-byte SHA1 hash).
 * @param client_data Pointer to the client data to sign (20 bytes).
 * @return None.
 */
void sign_data(uint8_t *app_id, uint8_t *client_data) {
    Credential current_entry;
    uint8_t nb = eeprom_read_byte(&nb_credentials);

    if (!ask_for_approval()) {
        // If the user does not approve, return an error
        UART_putc(STATUS_ERR_APPROVAL);
        return;
    }

    for (uint8_t i = 0; i < nb; i++) {
        eeprom_read_block(&current_entry, &eeprom_data[i], sizeof(Credential));

        // Check if the app ID matches the current entry
        if (memcmp(current_entry.app_id, app_id, SHA1_SIZE) == 0) {

            // Sign the client data using the private key
            uint8_t signature[40];
            if (!uECC_sign(current_entry.private_key, client_data, signature)) {
                UART_putc(STATUS_ERR_CRYPTO_FAILED); // Signing failed
                return;
            }
            
            // Send the signed data over UART
            UART_putc(STATUS_OK);
            send_pattern((const char*)current_entry.credential_id, CREDENTIAL_ID_SIZE);
            send_pattern((const char*)signature, PUBLIC_KEY_SIZE);
            return;
        }
    }

    UART_putc(STATUS_ERR_NOT_FOUND); // App ID not found
}

/**
 * @brief Handles the GetAssertion command by signing client data with a private key.
 * 
 * @param None.
 * @return None.
 */
void UART_handle_get_assertion(void) {
    uint8_t app_id[SHA1_SIZE];
    uint8_t client_data[SHA1_SIZE];

    for (int i = 0; i < SHA1_SIZE; i++) {
        app_id[i] = UART_getc(); // Read application ID from UART
    }
    for (int i = 0; i < SHA1_SIZE; i++) {
        client_data[i] = UART_getc(); // Read client data from UART
    }

    sign_data(app_id, client_data);
}


// --------------------------------- ListCredentials ---------------------------------

/**
 * @brief Lists the credentials stored in EEPROM, including `app_id` and `credential_id`.
 *        Sends the credentials information over UART.
 * 
 * @param None.
 * @return None.
 */
void UART_handle_list_credentials(void) {
    Credential current_entry;
    uint8_t nb = eeprom_read_byte(&nb_credentials);
    uint8_t i = 0;

    UART_putc(STATUS_OK); // Indicate success
    UART_putc(nb); // Send the number of stored credentials

    // Iterate through the stored credentials
    while (i < nb) {
        eeprom_read_block(&current_entry, &eeprom_data[i], sizeof(Credential));
        send_pattern((const char*)current_entry.credential_id, CREDENTIAL_ID_SIZE); // Send credential_id
        send_pattern((const char*)current_entry.app_id, SHA1_SIZE); // Send app_id
        i++;
    }
}

// --------------------------------- Reset ---------------------------------

/**
 * @brief Resets the EEPROM by clearing all stored credentials and resetting the counter (`nb_credentials`).
 *        Requires user approval before executing the reset.
 * 
 * @param None.
 * @return None.
 */
void UART_handle_reset(void) {
    if (!ask_for_approval()) {
        UART_putc(STATUS_ERR_APPROVAL); // Approval not granted
        return;
    }

    uint8_t nb = eeprom_read_byte(&nb_credentials); // Read the current number of credentials
    Credential empty_entry = {0}; // Define an empty credential structure

    // Overwrite each stored entry with the empty structure
    for (uint8_t i = 0; i < nb; i++) {
        eeprom_write_block(&empty_entry, &eeprom_data[i], sizeof(Credential));
    }

    // Reset the counter
    eeprom_write_byte(&nb_credentials, 0);

    UART_putc(STATUS_OK); // Indicate success
}


// --------------------------------- Main ---------------------------------

/**
 * @brief Main function that configures peripherals and executes an infinite loop 
 *        to handle commands received via UART.
 * 
 * @param None.
 * @return int : Returns 0 if the program executes without errors.
 */
int main(void) {
    config(); // Initialize peripherals and configuration

    while (1) {
        uint8_t command = UART_getc(); // Wait for a command via UART
        UART_handle_command(command); // Process the received command
    }
    return 0;
}
