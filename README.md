# Authenticator

## MakeCredential

## GetAssertion

## ListCredentials

## Reset

## Compiler le projet
Make upload
Si sur MAC, modifier dans le Makefile la ligne 3 pour y renseigner le bon port, et faire 
Make upload MAC=1

## Source d'aléatoire
Nous avons essayé d'utiliser la fonction de génération de nombres pseudo aléatoire "Mersenne Twister" car bien que pas sécurisé cryptogrpahiquement, elle reste assez résistante et surtout très peu coûteuse, mais nous n'avons pas réussi à la faire fonctionner avec la bibliothèque uECC, cela générait des erreurs avec la fonction make_key que nous ne comprenions pas. Nous avons alors utilisé la fonction de génération de nombre pseudo aléatoires de la librairie stdlib rand(), que nous avons initialisé en utilisant l'ADC.