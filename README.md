# Projet Authenticator

## Fonctionnalités implémentées

- **Génération de clés** : Utilisation de la bibliothèque `uECC` pour la génération de paires de clés publiques/privées.
- **Stockage en EEPROM** : Les credentials et les clés sont stockés dans la mémoire EEPROM pour assurer la persistance des données entre les redémarrages du système.
- **Authentification via UART** : Commandes permettant de créer de nouveaux credentials, de récupérer des assertions et de lister les credentials stockés.
- **Reset de l'EEPROM** : Effacement de toutes les données stockées après validation par l'utilisateur.

## Difficultés rencontrées

Parler de Mersenne Twister

## Tests réalisés

register nom
login nom
logout

## Compilation et téléversement

Make upload
Si sur MAC, modifier dans le Makefile la ligne 3 pour y renseigner le bon port, et faire 
Make upload MAC=1

## Conclusion

Ce projet implémente un authentificateur sécurisé utilisant des techniques de cryptographie modernes. La gestion de l'EEPROM et la communication UART sont des aspects essentiels du système, garantissant une interaction fluide avec l'utilisateur tout en assurant la sécurité des données.

---

Pour plus d'informations sur la configuration des outils ou des étapes de développement, n'hésitez pas à consulter la documentation Arduino et la documentation de la bibliothèque `uECC`.
