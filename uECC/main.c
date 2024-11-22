#include "uECC.h"

int main(){
    uECC_RNG_Function a = uECC_get_rng();
    uECC_set_rng(a);
    uECC_secp160r1
    int return_value = uECC_make_key(uint8_t *public_key, uint8_t *private_key, uECC_Curve curve);
    if(return_value == 0){
        printf("erreur lors de la génération de clef");
    }
    return 0;
}