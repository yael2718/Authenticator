# Détection du port selon le système d'exploitation
ifeq ($(MAC), 1)
    PORT := /dev/tty.usbmodem101
else
    PORT := /dev/ttyACM0
endif

# Options du compilateur
WARNINGS := -Wall -Wextra -pedantic -Wshadow -Wpointer-arith -Wcast-align \
            -Wwrite-strings \
            -Wnested-externs -Winline -Wno-long-long \
            -Wstrict-prototypes -Wno-array-bounds

CFLAGS := -Os -DF_CPU=16000000UL -mmcu=atmega328p $(WARNINGS)


# Chemins des fichiers sources
SRC := uart.c
ECC_SRC := $(wildcard ecc/*.c)

# Fichiers objets générés
OBJ := $(SRC:.c=.o) $(ECC_SRC:.c=.o)

# Cible principale
all: program.hex

# Génération du fichier .hex à partir du fichier .elf
program.hex: program.elf
	avr-objcopy -O ihex $^ $@

# Génération du fichier .elf à partir des fichiers objets
program.elf: $(OBJ)
	avr-gcc $(CFLAGS) -o $@ $^ 
	rm -f $(OBJ) # Supprimer les fichiers objets après génération

# Compilation des fichiers sources en fichiers objets
%.o: %.c
	avr-gcc $(CFLAGS) -c $< -o $@

# Téléversement sur la carte
upload: program.hex
	avrdude -v -patmega328p -carduino -P$(PORT) -b115200 -D -Uflash:w:$<

# Nettoyage des fichiers générés
clean:
	rm -f *.o *.elf *.hex ecc/*.o
