# Rocket Space Invaders

Rocket Space Invaders est un projet réalisé dans le cadre de notre 3e année de [licence informatique](https://licenceinfo.fr) à l'[Université de Reims Champagne-Ardenne](https://www.univ-reims.fr), qui consiste à créer un jeu multi-joueurs en réseau et multi-threadé.


![forthebadge](https://forthebadge.com/images/badges/built-with-love.svg)
![forthebadge](https://forthebadge.com/images/badges/made-with-c.svg)


## Détails exhaustifs des fonctionnalités et explications techniques
Le rapport couvre de manière détaillée l'ensemble des aspects du projet. Vous y trouverez une description approfondie de l'éditeur, incluant son interface, les outils disponibles, ainsi que la gestion des structures et des fichiers binaires. Les sections dédiées au client et au serveur expliquent leur fonctionnement respectif, en passant par leur lancement, les interfaces, les règles du jeu, ainsi que les structures et fonctions associées. Les mécanismes de communication entre le client et le serveur sont également abordés, avec des explications sur la création et la gestion des parties, et une analyse des protocoles de communication utilisés (UDP, TCP).

Chaque étape, de la création des cartes (map) à l'organisation des échanges réseau, est rigoureusement expliquée afin de fournir une vision complète du projet. Pour une analyse plus approfondie et une vue d'ensemble des fonctionnalités techniques, vous pouvez consulter le rapport dans son intégralité ici : [Rapport](Rapport_DRV_SNT.pdf).


## Prérequis
- Être sous [Linux](https://www.linux.org) ou sur [WSL](https://learn.microsoft.com/fr-fr/windows/wsl/install)

- Installer la [bibliothèque ncurses](https://manpages.debian.org/stretch/ncurses-doc/ncurses.3ncurses.en.html) grace à la commande suivante (*sous Ubuntu*)
    ```bash
    sudo apt-get install libncurses-dev
    ```

## Compilation

Vous devez vous positionner dans le dossier `Code` et faire la commande suivante dans le terminal.
```bash
make
```

## Lancement des programmes

Une fois le projet compilé, en étant dans le dossier `Code`, vous pouvez lancer le programme de votre choix.
- Le programme *`editeur.c`* correspond à l'éditeur, il permet de créer/modifier des maps.
- Le programme *`serveur.c`* correspond au serveur, il permet de gérer les parties et doit être lancer __avant les clients__.
- Le programme *`client.c`* correspond au client, il doit être lancé après que le serveur soit actif pour que le joueur puisse jouer.
    <br/><br/>

    ### Éditeur
    ```bash
    ./bin/editeur NOM_FICHIER
    ```
    Avec NOM_FICHIER qui est le nom du fichier binaire contenant le map (Exemple : `./bin/editeur map1.bin`)

    ### Serveur
    ```bash
    ./bin/serveur
    ```


    ### Client
    ```bash
    ./bin/client IP_SERVEUR NUMERO_PORT
    ```
    Avec `IP_SERVEUR` qui est l'adresse IP du serveur et `NUMERO_PORT` qui est le numéro de port de la socket UDP.

    Pour tester directement : 
    ```bash
    ./bin/client 127.0.0.1 12345
    ```

## Éditeur
![gif1](gifs/editeur.gif)

## Jeu
**Mourir :**

![gif2](gifs/game1.gif)

**Utilisation des bombes :**

![gif3](gifs/game2.gif)

**Winner :**

![gif4](gifs/endgame.gif)

## Contact
- DARVILLE Killian (killian.darville@etudiant.univ-reims.fr)
- SINET Théo (theo.sinet@etudiant.univ-reims.fr)


## Licence
Copyright © 2023, [Rocket Industries]().