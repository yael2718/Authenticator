# Projet Authenticator

## Compilation et téléversement

Make upload\
Si sur MAC, modifier dans le Makefile la ligne 3 pour y renseigner le bon port, et faire\
Make upload MAC=1

## Fonctionnalités implémentées

- **Génération de clés** : Utilisation de la bibliothèque `uECC` pour la génération de paires de clés publiques/privées.
- **Stockage en EEPROM** : Les credentials et les clés sont stockés dans la mémoire EEPROM pour assurer la persistance des données entre les redémarrages du système.
- **Authentification via UART** : Commandes permettant de créer de nouveaux credentials, de récupérer des assertions et de lister les credentials stockés.
- **Reset de l'EEPROM** : Effacement de toutes les données stockées après validation par l'utilisateur.

## Choix techniques

### micro-ecc
Nous avons décidé d'utiliser la branche static de la librairie micro-ecc car elle répondait à nos besoin et qu'elle permettait de faire plus de pré calcul à la compilatio, et donc de réduire le temps de calcul à l'exécution.

### eeprom
On stocke un utilisateur dans l'eeprom grâce à la structure Credential, qui comporte 3 champs : 
- **app_id** : l'app id sur 20 octets (taille d'un hash de sha1).
- **credential_id** : le credential id sur 17 octets.
- **private_key** : la clé privée sur 21 octets.


## Difficultés rencontrées

### Génération de nombres aléatoires
Nous avons commencé par vouloir utiliser le Mersenne Twister (http://www.math.sci.hiroshima-u.ac.jp/m-mat/MT/MT2002/emt19937ar.html), un générateur de nombre pseudo aléatoire pas cryptographique, mais apportant un niveau de sécurité suffisant (en comparaison de la courbe secp160r1), et surtout très performant.\
Cependant la présence du fichier faisait bugger la fonction make_key de uECC, on a donc du faire sans.
Nous avons donc utilisé la fonction rand() de stdlib, que nous avons initialisé avec une seed générée à partir de l'ADC.\
On a réalisé plusieurs essais, et c'était en mettant le prescaler à 128 (ADPS2,1,0 = 1) qu'on avait des valeurs les plus éloignées les unes des autres.

### BaudRate
En utilisant la même méthode qu'au TP précèdent pour set le baudRate, on avait des bugs car il était trop élevé. Nous avons donc utilisé la librairie util/setbaud.h .

### APP_ID
Le client envoie toujours la même app_id, donc on ne peut pas teset plusieurs applications à la fois.//A redire

### ring_buffer
Nous avons commencé par utiliser la librairie ring_buffer pour communiquer en uart. Mais nous n'arrivions pas à la fiare marcher, puis finalement nous nous sommes rendus compte qu'il n'y en avait pas besoin car l'utilisateur n'envoie pas beaucoup de commandes dans un intervalle court.

## Tests réalisés

register nom\
login nom\
logout\
//A redire