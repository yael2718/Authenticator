# Projet Authenticator

## Description du projet

Le projet **Authenticator** est un système embarqué développé pour un dispositif d'authentification sécurisé. Ce système utilise un microcontrôleur Arduino, et implémente une série de fonctionnalités cryptographiques et de gestion de credentials (identifiants de clés).

Les principales fonctionnalités incluent :
- Génération de clés privées et publiques à l'aide de la courbe elliptique secp160r1.
- Stockage sécurisé des clés et des identifiants dans la mémoire EEPROM.
- Authentification des utilisateurs via un challenge et une réponse.
- Commandes via UART pour gérer les credentials (ajout, suppression, et récupération).
- Fonction de reset pour effacer toutes les données stockées.

## Fonctionnalités implémentées

- **Génération de clés** : Utilisation de la bibliothèque `uECC` pour la génération de paires de clés publiques/privées et le stockage dans l'EEPROM.
- **Stockage en EEPROM** : Les credentials sont stockés dans la mémoire EEPROM pour assurer la persistance des données entre les redémarrages du système.
- **Authentification via UART** : Commandes permettant de créer de nouveaux credentials, de récupérer des assertions et de lister les credentials stockés.
- **Reset de l'EEPROM** : Effacement de toutes les données stockées après validation par l'utilisateur.

## Difficultés rencontrées

- **Gestion de la mémoire EEPROM** : La taille limitée de l'EEPROM a nécessité de bien optimiser le stockage des données, notamment en réduisant la taille des données stockées et en utilisant une gestion efficace des entrées.
- **Problèmes de synchronisation** : Des problèmes de synchronisation de la communication UART ont été rencontrés, notamment lors de l'envoi des données de manière séquentielle. L'utilisation de buffers et de commandes d'interruption a permis de résoudre ces problèmes.
- **Implémentation des algorithmes cryptographiques** : L'intégration de la bibliothèque `uECC` pour la génération de clés et la signature des données a été un défi, notamment dû à la complexité des calculs en ressources limitées sur un microcontrôleur.

## Tests réalisés

- **Tests unitaires sur la génération de clés** : Vérification que les clés générées correspondent aux standards cryptographiques attendus.
- **Tests de stockage EEPROM** : Tests effectués pour s'assurer que les données sont correctement stockées et récupérées de la mémoire EEPROM.
- **Tests de communication UART** : Vérification que les données envoyées et reçues via UART sont correctement traitées, y compris les différentes commandes (MakeCredential, GetAssertion, ListCredentials).
- **Test de réinitialisation EEPROM** : Tests pour vérifier que toutes les données sont effacées de l'EEPROM après un reset du système.

## Compilation et téléversement

### Prérequis

- **Arduino IDE** : Utilisez l'IDE Arduino pour compiler et téléverser le code sur la carte Arduino.
- **Bibliothèque `uECC`** : Téléchargez et ajoutez la bibliothèque `uECC` pour la gestion des courbes elliptiques.
- **Microcontrôleur** : Ce projet a été testé sur une carte **Arduino Uno** (Atmega328p).

### Étapes pour compiler et téléverser

1. **Ouvrir le projet dans l'IDE Arduino** :
   - Ouvrir l'IDE Arduino et charger le fichier `*.ino` ou `*.cpp` associé au projet.

2. **Configurer la carte et le port** :
   - Sélectionnez la carte Arduino dans l'IDE via le menu `Outils > Type de carte > Arduino Uno`.
   - Sélectionnez le port série approprié dans le menu `Outils > Port`.

3. **Compiler le programme** :
   - Cliquez sur le bouton "Vérifier" dans l'IDE Arduino pour compiler le code.

4. **Téléverser le programme sur la carte Arduino** :
   - Une fois la compilation réussie, cliquez sur le bouton "Téléverser" pour envoyer le code à la carte Arduino.

5. **Vérification** :
   - Une fois le code téléversé, ouvrez le moniteur série dans l'IDE Arduino pour voir les messages de débogage ou les résultats de l'exécution.

## Conclusion

Ce projet implémente un authentificateur sécurisé utilisant des techniques de cryptographie modernes. La gestion de l'EEPROM et la communication UART sont des aspects essentiels du système, garantissant une interaction fluide avec l'utilisateur tout en assurant la sécurité des données.

---

Pour plus d'informations sur la configuration des outils ou des étapes de développement, n'hésitez pas à consulter la documentation Arduino et la documentation de la bibliothèque `uECC`.
