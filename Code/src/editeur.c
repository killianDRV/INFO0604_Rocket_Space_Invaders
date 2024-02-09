#include <ncurses.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "functions.h"

#define TABLE_MAX_SIZE  10
#define NB_LIGNE        20
#define NB_COLONNE       60

#define COULEUR_RED_BLACK 1
#define COULEUR_MAGENTA_BLACK 2
#define COULEUR_GREEN_BLACK 3
#define COULEUR_YELLOW_BLACK 4
#define COULEUR_BLUE_BLACK 5
#define COULEUR_CYAN_BLACK 6

#define DELETE_SIMPLE 0
#define BLOCK   1
#define LADDER  2
#define TRAP    3
#define GATE    4
#define KEY     5
#define DOOR    6
#define EXIT    7
#define START   8
#define ROBOT   9
#define PROBE   10
#define LIFE    11
#define BOMB    12

// Structure qui représente les entrées de la table d'adressage
typedef struct {
    int numeroMap;      // ID de la map
    off_t position;     // Position de la map dans le fichier
    off_t taille;       // Taille de la map dans le fichier
} adresse_t;

// Structure qui représente la table d'adressage
typedef struct{
    adresse_t tableAdressage[TABLE_MAX_SIZE];   // Contient les entrées de la table d'adressage
    int prochaine_table_existe;                 // Permet de savoir si une prochaine table d'adressage existe
}adresse_complete_t;

// Structure qui représente une case de la map
typedef struct {
    int type;           // Type de la case (BLOCK, LADDER, TRAP ...)
    int couleur;        // Couleur de l'élément
    int numero;         // Numéro de l'élément
} case_t;

// Structure qui représente la map
typedef struct {
    case_t matrice[NB_LIGNE][NB_COLONNE];    // Contient toute les cases d'un level
} map_t;

// Palette pour les couleurs
void palette() {
	init_pair(COULEUR_RED_BLACK, COLOR_RED, COLOR_BLACK);
    init_pair(COULEUR_MAGENTA_BLACK, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(COULEUR_GREEN_BLACK, COLOR_GREEN, COLOR_BLACK);
    init_pair(COULEUR_YELLOW_BLACK, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COULEUR_BLUE_BLACK, COLOR_BLUE, COLOR_BLACK);
    init_pair(COULEUR_CYAN_BLACK, COLOR_CYAN, COLOR_BLACK);
}

// Initialise les boutons
void initialisationBoutons(char** boutons)
{
    boutons[0] = "Delete";
    boutons[1] = "Block";
    boutons[2] = "Ladder";
    boutons[3] = "Trap";
    boutons[4] = "Gate";
    boutons[5] = "Key";
    boutons[6] = "Door";
    boutons[7] = "Exit";
    boutons[8] = "Start";
    boutons[9] = "Robot";
    boutons[10] = "Probe";
    boutons[11] = "Life";
    boutons[12] = "Bomb";
    boutons[13] = "DELETE";
}

// Permet d'afficher les boutons dans la fenêtre tools
void affichageBoutons(WINDOW* tools, char** boutons)
{
    wattron(tools,COLOR_PAIR(COULEUR_RED_BLACK));
    for(int i=0; i<13; i++)
        mvwprintw(tools, i+1, 3, "%s", boutons[i]);
    wattroff(tools, COLOR_PAIR(COULEUR_RED_BLACK));
    mvwaddch(tools, 6, 9, '^');
}

// Permet de changer la couleur du bouton séléctionner
void changerBouton(WINDOW* tools, char** boutons, int* boutonSelectionne, int nouveauBouton)
{
    if(*boutonSelectionne!=-1) // permet de remettre en rouge (non sélectionné) un bouton lorsqu'un autre bouton à précédemment été sélectionné
    {
        if(*boutonSelectionne==13){mvwaddch(tools, 19, 1, ' ');}
        else{mvwaddch(tools, *boutonSelectionne+1, 1, ' ');}
        
        wattron(tools, COLOR_PAIR(COULEUR_RED_BLACK));

        if(*boutonSelectionne==13){mvwprintw(tools, 19, 4, "%s", boutons[*boutonSelectionne]);}
        else{mvwprintw(tools, *boutonSelectionne+1, 3, "%s", boutons[*boutonSelectionne]);}
        wattroff(tools, COLOR_PAIR(COULEUR_RED_BLACK));
    }

    // Changement de couleur + ajout du symbole au bouton sélectionné
    *boutonSelectionne = nouveauBouton;

    if(*boutonSelectionne==13){
        mvwprintw(tools, 19, 1, ">  %s", boutons[*boutonSelectionne]);
    }else{
        mvwprintw(tools, *boutonSelectionne+1, 1, "> %s", boutons[*boutonSelectionne]);
    }
    
    wrefresh(tools);
}

// Fonction qui permet de supprimer les informations d'un bouton du menu tools dans le menu "Informations"
void supprimeTextToolsInfo(WINDOW* informations)
{
    for(int i=2; i<4; i++)
    {
        for(int j=1; j<76; j++)
        {
            mvwaddch(informations, i, j, ' ');
        }
    }
}

// Affiche les informations d'un bouton dans le menu "Informations"
void affichageToolsInfo(WINDOW* informations, int boutonSelectionne)
{
    supprimeTextToolsInfo(informations);
    wattron(informations, COLOR_PAIR(COULEUR_RED_BLACK));
    switch(boutonSelectionne)
    {
        case DELETE_SIMPLE:
            mvwprintw(informations, 2, 1, "Click on a cell to delete it");
            break;
        case BLOCK:
            mvwprintw(informations, 2, 1, "Click one time and another one on the same row or column to place a row");
            mvwprintw(informations, 3, 1, "of blocks");
            break;
        case LADDER:
            mvwprintw(informations, 2, 1, "Click on a cell to place one line of a ladder");
            break;
        case TRAP:
            mvwprintw(informations, 2, 1, "Click on a cell to place a trap");
            break;
        case GATE:
            mvwprintw(informations, 2, 1, "Select a color");
            mvwprintw(informations, 3, 1, "Click on a cell to place a gate with your selected color");
            break;
        case KEY:
            mvwprintw(informations, 2, 1, "Select a color");
            mvwprintw(informations, 3, 1, "Click on a cell to place a key with your selected color");
            break;
        case DOOR:
            mvwprintw(informations, 2, 1, "Click on a cell to place a door");
            break;
        case EXIT:
            mvwprintw(informations, 2, 1, "Click on a cell to place an exit");
            break;
        case START:
            mvwprintw(informations, 2, 1, "Click on a cell to place a start");
            break;
        case ROBOT:
            mvwprintw(informations, 2, 1, "Click on a cell to place a robot");
            break;
        case PROBE:
            mvwprintw(informations, 2, 1, "Click on a cell to place a probe");
            break;
        case LIFE:
            mvwprintw(informations, 2, 1, "Click on a cell to place a life");
            break;
        case BOMB:
            mvwprintw(informations, 2, 1, "Click on a cell to place a bomb");
            break;
    }
    wattroff(informations, COLOR_PAIR(COULEUR_RED_BLACK));
    wrefresh(informations);
}

// Permet d'afficher le numéro de la porte dans le menu 'tools'
void affichageNumeroPorte(WINDOW* tools, int numeroPorte)
{
    if(numeroPorte<10){
        mvwprintw(tools, 7, 9, "<0%d>", numeroPorte);
    }else{
        mvwprintw(tools, 7, 9, "<%d>", numeroPorte);
    }
    wrefresh(tools);
}

// Permet d'afficher le numéro de la porte dans le niveau lorsqu'on pose une porte
void affichageNumeroPorteGame(int posX, int posY, int numeroPorte)
{
    if(numeroPorte<10){
        mvprintw(posY-3, posX, "0%d", numeroPorte);
    }else{
        mvprintw(posY-3, posX, "%d", numeroPorte);
    }
}

// Affichage du rectangle pour les portes, entrée et sortie
void affichagePorteStartExit(int posX, int posY)
{
    if(posX+1!=60 && posX+2!=60 && posY-1!=1 && posY-2!=1 && posY-3!=1)
    {
        for(int i=0; i<3; i++)
        {
            for(int j=0; j<4; j++)
            {
                mvaddch(posY-j, posX+i, ' ' | A_REVERSE);
            }
        } 
    }
}

// Affichage du niveau actuel dans le menu 'tools'
void affichageCurrentLevel(WINDOW* tools, int numeroLevel)
{
    if(numeroLevel==100){
        mvwprintw(tools, 17, 6, "%d", numeroLevel);
    }else if(numeroLevel<10){
        mvwprintw(tools, 17, 6, "00%d", numeroLevel);
    }else{
        mvwprintw(tools, 17, 6, "0%d", numeroLevel);
    }
    wrefresh(tools);
}

// Permet de crée le fichier bianire s'il n'existe pas et d'insérer la table d'adressage (vide) en debut de fichier
void creation(char* nomFichier){

    // Ouverture fichier
    int fd;
    if ((fd = open(nomFichier, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) == -1) {
        if (errno == EEXIST) {
            return;
        } else {
            perror("Impossible de créer le fichier");
            exit(EXIT_FAILURE);
        }
    }

    // Création de la table d'adressage vide
    adresse_complete_t adresseComplete;
    for(int i=0; i<TABLE_MAX_SIZE; i++){
        adresseComplete.tableAdressage[i].numeroMap=-1;
        adresseComplete.tableAdressage[i].position=sizeof(adresse_complete_t)+(sizeof(map_t)*i);
        adresseComplete.tableAdressage[i].taille=sizeof(map_t);
    }
    adresseComplete.prochaine_table_existe=0;

    // Écriture de la table d'adressage dans le fichier
    if (write(fd, &adresseComplete, sizeof(adresse_complete_t)) == -1) {
        perror("Erreur lors de l'écriture de l'adresse_suivante de vide dans le fichier");
        exit(EXIT_FAILURE);
    }

    // Fermeture du fichier
    if(close(fd) == -1){
        perror("Erreur lors de la fermeture du fichier");
        exit(EXIT_FAILURE);
    }
}

// Permet de sauvegarder une map dans le fichier binaire
void sauvegarder(char* nomFichier, map_t map, int numeroMap) {

    // Vérification de si la map est vide ou non
    int estVide=1;
    for(int i=0; i<NB_LIGNE; i++){
        for(int j=0; j<NB_COLONNE; j++) {
            if(i == 0 || i == NB_LIGNE-1 || j == 0 || j == NB_COLONNE-1) {
                // ne rien faire si la case est sur les bords de la carte
            } else if(map.matrice[i][j].type != -1 || map.matrice[i][j].couleur != -1 || map.matrice[i][j].numero != -1) {
                // si une case non vide est trouvée, alors la carte n'est pas vide
                estVide = 0;
                break;
            }
        }
        if(estVide == 0) {
            break;
        }
    }

    // Si la map n'est pas vide
    if(estVide==0){

        case_t case_block = {BLOCK, -1, -1};

        // re-crée les bords du level
        for(int i=1; i<NB_COLONNE+1; i++){
            map.matrice[0][i-1] = case_block;
            map.matrice[NB_LIGNE-1][i-1] = case_block;
            if(i<NB_LIGNE+1){
                map.matrice[i-1][0] = case_block;
                map.matrice[i-1][NB_COLONNE-1] = case_block;
            }
        }
        
        // Ouverture fichier
        int fd;
        if ((fd = open(nomFichier, O_RDWR)) == -1) {
            fprintf(stderr, "Impossible d'ouvrir le fichier %s : %s\n", nomFichier, strerror(errno));
            exit(EXIT_FAILURE);
        }

        // Sauvegarde de la map et de la table d'adressage mise à jour dans le fichier
        adresse_complete_t adresseComplete;
        int quitter=0;
        for(int i=0; i<TABLE_MAX_SIZE; i++){
            // Positionnement avant la table d'adressage
            if (lseek(fd, (sizeof(adresse_complete_t)+(sizeof(map_t)*TABLE_MAX_SIZE))*i, SEEK_SET) == -1) {
                perror("Erreur lors du positionnement dans le fichier.");
                exit(EXIT_FAILURE);
            }

            // Lecture de la table d'adressage
            if (read(fd, &adresseComplete, sizeof(adresse_complete_t)) == -1) {
                perror("Erreur lors de la lecture de la table d'adressage");
                exit(EXIT_FAILURE);
            }

            // Parcours de toutes les entrées de la table d'adressage
            for(int j=0; j<TABLE_MAX_SIZE; j++){
                // Si l'emplacement est vide
                if(adresseComplete.tableAdressage[j].numeroMap == -1){
                    // Positionnement à la pisition correspondant à l'emplacement vide
                    if (lseek(fd, adresseComplete.tableAdressage[j].position, SEEK_SET) == -1) {
                        perror("Erreur lors du positionnement dans le fichier.");
                        exit(EXIT_FAILURE);
                    }

                    // Écriture de la map dans le fichier
                    if (write(fd, &map, sizeof(map)) == -1) {
                        perror("Erreur lors de l'écriture de la table d'adressage dans le fichier.");
                        exit(EXIT_FAILURE);
                    }
                    adresseComplete.tableAdressage[j].numeroMap = numeroMap;

                    // Positionnement avant la table d'adressage
                    if (lseek(fd, (sizeof(adresse_complete_t)+(sizeof(map_t)*TABLE_MAX_SIZE))*i, SEEK_SET) == -1) {
                        perror("Erreur lors du positionnement dans le fichier.");
                        exit(EXIT_FAILURE);
                    }

                    // Écriture de la table d'adressage mise à jour dans le fichier
                    if (write(fd, &adresseComplete, sizeof(adresse_complete_t)) == -1) {
                        perror("Erreur lors de l'écriture de la table d'adressage dans le fichier.");
                        exit(EXIT_FAILURE);
                    }

                    quitter=1;
                    break;
                }
            }
            // Si la map est enregistrée, alors quitter sinon passer à la table suivante
            if(quitter==1){
                break;
            }

            // Si la table suivante n'éxiste pas, alors création de la prochaine table d'adressage
            if(adresseComplete.prochaine_table_existe==0){
                adresseComplete.prochaine_table_existe=1;
                
                // Positionnement à la position de l'ancienne table d'adressage
                if (lseek(fd, (sizeof(adresse_complete_t)+(sizeof(map_t)*TABLE_MAX_SIZE))*i, SEEK_SET) == -1) {
                    perror("Erreur lors du positionnement dans le fichier.");
                    exit(EXIT_FAILURE);
                }
                
                // Écriture de l'ancienne table d'adressage dans le fichier
                if (write(fd, &adresseComplete, sizeof(adresse_complete_t)) == -1) {
                    perror("Erreur lors de l'écriture de la table d'adressage dans le fichier.");
                    exit(EXIT_FAILURE);
                }

                // Création de la nouvelle table d'ardessage
                adresse_complete_t nouvelle_table_adr;
                for(int k=0; k<TABLE_MAX_SIZE; k++){
                    nouvelle_table_adr.tableAdressage[k].numeroMap=-1;
                    nouvelle_table_adr.tableAdressage[k].position=sizeof(adresse_complete_t)+(sizeof(map_t)*k)+ (sizeof(adresse_complete_t)+(sizeof(map_t)*TABLE_MAX_SIZE))*(i+1);
                    nouvelle_table_adr.tableAdressage[k].taille=sizeof(map_t);
                }
                nouvelle_table_adr.prochaine_table_existe=0;

                nouvelle_table_adr.tableAdressage[0].numeroMap = numeroMap;

                // Positionnement à la position de la nouvelle table d'adressage
                if (lseek(fd, adresseComplete.tableAdressage[TABLE_MAX_SIZE-1].position+adresseComplete.tableAdressage[TABLE_MAX_SIZE-1].taille, SEEK_SET) == -1) {
                    perror("Erreur lors du positionnement dans le fichier.");
                    exit(EXIT_FAILURE);
                }

                // Écriture de la nouvelle table d'adressage dans le fichier
                if (write(fd, &nouvelle_table_adr, sizeof(adresse_complete_t)) == -1) {
                    perror("Erreur lors de l'écriture de la table d'adressage dans le fichier.");
                    exit(EXIT_FAILURE);
                }

                // Positionnement à la première entrée vide de la nouvelle table d'adressage (0 car viens d'être crée)
                if (lseek(fd, nouvelle_table_adr.tableAdressage[0].position, SEEK_SET) == -1) {
                    perror("Erreur lors du positionnement dans le fichier.");
                    exit(EXIT_FAILURE);
                }

                // Écriture de la map dans le fichier
                if (write(fd, &map, sizeof(map)) == -1) {
                    perror("Erreur lors de l'écriture de la table d'adressage dans le fichier.");
                    exit(EXIT_FAILURE);
                }
                nouvelle_table_adr.tableAdressage[0].numeroMap = numeroMap;
                break;
            }
        }
        
        // Fermeture du fichier
        if(close(fd) == -1){
            perror("Erreur lors de la fermeture du fichier");
            exit(EXIT_FAILURE);
        }
    }
}

// Permet de charger une map depuis le ficher
void chargement(char* nomFichier, map_t* map, int numeroMap) {
    // Ouverture du fichier
    int fd;
    if ((fd = open(nomFichier, O_RDONLY)) == -1) {
        fprintf(stderr, "Impossible d'ouvrir le fichier %s : %s\n", nomFichier, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Pacrours des tables d'adressages
    adresse_complete_t adresseComplete;
    int quitter=0;
    for (int i = 0; i < TABLE_MAX_SIZE; i++){

        // Positionnement avant la table d'adressage
        if (lseek(fd, (sizeof(adresse_complete_t)+sizeof(map_t)*TABLE_MAX_SIZE)*i, SEEK_SET) == -1) {
            perror("Erreur lors du positionnement dans le fichier.");
            exit(EXIT_FAILURE);
        }

        // Lecture de la table d'adressage
        if (read(fd, &adresseComplete, sizeof(adresse_complete_t)) == -1) {
            perror("Erreur lors de la lecture de la table d'adressage");
            exit(EXIT_FAILURE);
        }

        // Parcours des entrées de la table d'adressage
        for (int j = 0; j < TABLE_MAX_SIZE; j++){
            // Si le numéro de map dans la table d'adressage correspond au numéro de map passé en paramètre
            if(adresseComplete.tableAdressage[j].numeroMap==numeroMap){

                // Positionnement avant la map
                if (lseek(fd, adresseComplete.tableAdressage[j].position, SEEK_SET) == -1) {
                    perror("Erreur lors du positionnement dans le fichier.");
                    exit(EXIT_FAILURE);
                }

                // Lecture de la map
                if (read(fd, map, sizeof(map_t)) == -1) {
                    perror("Erreur lors de la lecture de la map.");
                    exit(EXIT_FAILURE);
                }
                quitter=1;
                break;
            }
        }
        // Si la map est trouvée, alors quitter
        if(quitter==1){
            break;
        }
    }

    // Crée les bords de la map
    case_t case_block = {BLOCK, -1, -1};
    for(int i=1; i<61; i++){
        map->matrice[0][i-1] = case_block;
        map->matrice[NB_LIGNE-1][i-1] = case_block;
        if(i<21){
            map->matrice[i-1][0] = case_block;
            map->matrice[i-1][NB_COLONNE-1] = case_block;
        }
    }
    
    // Fermeture du fichier
    if(close(fd) == -1){
        perror("Erreur lors de la fermeture du fichier");
        exit(EXIT_FAILURE);
    }
}

// Permet de supprimer une map du fichier
void suppression(char* nomFichier, int numeroMap) {
    // Ouverture fichier
    int fd;
    if ((fd = open(nomFichier, O_RDWR)) == -1) {
        fprintf(stderr, "Impossible d'ouvrir le fichier %s : %s\n", nomFichier, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Pacrours des tables d'adressages
    adresse_complete_t adresseComplete;
    map_t empty_map = {0};

    int quitter=0;
    for (int i = 0; i < TABLE_MAX_SIZE; i++){
        // Parcours des entrées de la table d'adressage
        for (int j = 0; j < TABLE_MAX_SIZE; j++){

            // Positionnement avant la table d'adressage
            if (lseek(fd, (sizeof(adresse_complete_t)+sizeof(map_t)*TABLE_MAX_SIZE)*i, SEEK_SET) == -1) {
                perror("Erreur lors du positionnement dans le fichier.");
                exit(EXIT_FAILURE);
            }

            // Lecture de la table d'adressage
            if (read(fd, &adresseComplete, sizeof(adresse_complete_t)) == -1) {
                perror("Erreur lors de la lecture de la table d'adressage");
                exit(EXIT_FAILURE);
            }

            // Si le numéro de la map corréspond au numéro de map passé en paramètre
            if (adresseComplete.tableAdressage[j].position != 0 && adresseComplete.tableAdressage[j].taille != 0 && adresseComplete.tableAdressage[j].numeroMap == numeroMap) {
                
                // Positionnement à la position de la map à supprimer
                if (lseek(fd, adresseComplete.tableAdressage[j].position, SEEK_SET) == -1) {
                    perror("Erreur lors du positionnement dans le fichier 1.");
                    exit(EXIT_FAILURE);
                }

                // Ecriture de la map vide
                if (write(fd, &empty_map, sizeof(map_t)) == -1) {
                    perror("Erreur lors de l'écriture de la map (vide) dans le fichier.");
                    exit(EXIT_FAILURE);
                }

                adresseComplete.tableAdressage[j].numeroMap = -1;
                quitter=1;
                break;
            }
        }

        // Si la map à était supprimer
        if(quitter==1){
            // Positionnement avant la table d'adressage
            if (lseek(fd, (sizeof(adresse_complete_t)+sizeof(map_t)*TABLE_MAX_SIZE)*i, SEEK_SET) == -1) {
                perror("Erreur lors du positionnement dans le fichier 2.");
                exit(EXIT_FAILURE);
            }

            // Écriture de la table d'adressage mise à jour
            if (write(fd, &adresseComplete, sizeof(adresse_complete_t)) == -1) {
                perror("Erreur lors de l'écriture de la table d'adressage dans le fichier.");
                exit(EXIT_FAILURE);
            }
            break;
        }
    }

    // Fermeture du fichier
    if(close(fd) == -1){
        perror("Erreur lors de la fermeture du fichier");
        exit(EXIT_FAILURE);
    }
}

// Permet d'afficher la map à l'écran
void afficherMap(map_t map){
    // Parcours de la map 
    for (int i = 1; i < NB_LIGNE+1; i++){
        for (int j = 1; j < NB_COLONNE+1; j++){

            // Ajout des éléments sur la map selon leur type
            switch(map.matrice[i-1][j-1].type){
                case BLOCK:
                    attron(COLOR_PAIR(COULEUR_CYAN_BLACK));
                    mvaddch(i, j, ' ' | A_REVERSE);
                    attroff(COLOR_PAIR(COULEUR_CYAN_BLACK));
                    break;

                case LADDER:
                    attron(COLOR_PAIR(COULEUR_YELLOW_BLACK));
                    mvaddch(i, j, ACS_LTEE);
                    mvaddch(i, j+1, ACS_HLINE);
                    mvaddch(i, j+2, ACS_RTEE);
                    attroff(COLOR_PAIR(COULEUR_YELLOW_BLACK));
                    break;

                case TRAP:
                    attron(COLOR_PAIR(COULEUR_CYAN_BLACK));
                    mvaddch(i, j, '#' | A_REVERSE);
                    attroff(COLOR_PAIR(COULEUR_CYAN_BLACK));
                    break;

                case GATE:
                    attron(COLOR_PAIR(map.matrice[i-1][j-1].couleur));
                    mvaddch(i, j, ACS_PLUS);
                    attroff(COLOR_PAIR(map.matrice[i-1][j-1].couleur));
                    break;

                case KEY:
                    attron(COLOR_PAIR(map.matrice[i-1][j-1].couleur));
                    mvaddch(i, j, ACS_LLCORNER);
                    mvaddch(i-1, j, ' ' | A_REVERSE);
                    attroff(COLOR_PAIR(map.matrice[i-1][j-1].couleur));
                    break;

                case DOOR:
                    attron(COLOR_PAIR(COULEUR_GREEN_BLACK));
                    affichagePorteStartExit(j, i);
                    attroff(COLOR_PAIR(COULEUR_GREEN_BLACK));

                    affichageNumeroPorteGame(j, i, map.matrice[i-1][j-1].numero);
                    break;

                case EXIT:
                    attron(COLOR_PAIR(COULEUR_YELLOW_BLACK));
                    affichagePorteStartExit(j, i);
                    attroff(COLOR_PAIR(COULEUR_YELLOW_BLACK));
                    break;

                case START:
                    attron(COLOR_PAIR(COULEUR_MAGENTA_BLACK));
                    affichagePorteStartExit(j, i);
                    attroff(COLOR_PAIR(COULEUR_MAGENTA_BLACK));
                    break;

                case ROBOT:
                    // haut de la tete
                    mvaddch(i-3, j, ACS_ULCORNER);
                    mvaddch(i-3, j+1, ACS_BTEE);
                    mvaddch(i-3, j+2, ACS_URCORNER);

                    // bas de la tete
                    mvaddch(i-2, j, ACS_LLCORNER);
                    mvaddch(i-2, j+1, ACS_TTEE);
                    mvaddch(i-2, j+2, ACS_LRCORNER);

                    // corps
                    mvaddch(i-1, j, ACS_HLINE);
                    mvaddch(i-1, j+1, ACS_PLUS);
                    mvaddch(i-1, j+2, ACS_HLINE);

                    // jambes
                    mvaddch(i, j, ACS_ULCORNER);
                    mvaddch(i, j+1, ACS_BTEE);
                    mvaddch(i, j+2, ACS_URCORNER);
                    break;

                case PROBE:
                    // partie haute
                    mvaddch(i-1, j, ACS_LTEE);
                    mvaddch(i-1, j+1, ACS_HLINE);
                    mvaddch(i-1, j+2, ACS_RTEE);

                    // partie basse
                    mvaddch(i, j, ACS_LLCORNER);
                    mvaddch(i, j+1, ACS_HLINE);
                    mvaddch(i, j+2, ACS_LRCORNER);
                    break;

                case LIFE:
                    attron(COLOR_PAIR(COULEUR_RED_BLACK));
                    mvaddch(i, j, 'V');
                    attroff(COLOR_PAIR(COULEUR_RED_BLACK));
                    break;

                case BOMB:
                    mvaddch(i, j, 'o');
                    break;
            }
        }
    }
}

// Permet d'effacer la map de l'écran
void effacerAffichageMap(){
    for(int i=2; i<60; i++)
    {
        for(int j=2; j<20; j++)
        {
            mvaddch(j, i, ' ');
        }
    }
}

// Supprime la croix rouge du placement du bloc
void suppressionCroixRouge(int *premierClicBlock, int *AnciennePosAbscisse, int *AnciennePosOrdonne){
    *AnciennePosAbscisse=0;
    *AnciennePosOrdonne=0;
    *premierClicBlock=0;
}

// Permet de vérifier la taille du terminal de l'utilisateur
int verificationTailleTerminal(int *tailleTerminalX, int *tailleTerminalY){
    // Récupération des dimensions du terminal
    getmaxyx(stdscr, *tailleTerminalY, *tailleTerminalX); 

    if(*tailleTerminalY<27  || *tailleTerminalX<77){
        return true;
    }
    return false;
}

int main(int argc, char **argv)
{
    // verification argument
    if(argc!=2){
        printf("Utilisation\n");
        printf("  ./editeur fichierSource\n");
        printf("    où :\n");
        printf("      - fichierSource : Nom du fichier.bin\n");

        perror("Nombre d'argument non valide");
        exit(EXIT_FAILURE);
    }

    WINDOW *level, *tools, *informations;
    int tailleTerminalX, tailleTerminalY;
    int largeur=1, posX, posY, ch;
    int numeroPorte=1, couleurSelectionne=2, numeroLevel=1;
    int boutonSelectionne=-1, anciennePositionCouleur=71;
    int premierClicBlock=0, AnciennePosAbscisse=0, AnciennePosOrdonne=0;
    char* boutons[14];

    char* nom_fichier = malloc(strlen(argv[1]) + 7);
    sprintf(nom_fichier, "./Map/%s", argv[1]);

    map_t mapActuel;
    case_t case_vide = {-1, -1, -1}, case_block = {BLOCK, -1, -1};;
    
    for (int i = 0; i < NB_LIGNE; i++){
        for (int j = 0; j < NB_COLONNE; j++){
            mapActuel.matrice[i][j] = case_vide;
        }
    }

    // ncurses initialisation
    setlocale(LC_ALL, "");
    ncurses_init();

    // Vérifie si la dimension du termnial est assez grande pour afficher l'éditeur
    if(verificationTailleTerminal(&tailleTerminalX, &tailleTerminalY)){
        ncurses_stop();
        printf("Les dimension minimal du terminal doivent être de 27x77\nOr votre terminal fait %dx%d\n", tailleTerminalY, tailleTerminalX);

        exit(EXIT_FAILURE);
    }

    // Initialise les boutons
    initialisationBoutons(boutons);

    // Colors initialisation and palette definition
    ncurses_colors();
    palette();
    
    // Mouse initialisation
    ncurses_init_mouse();

    // Crée le fichier s'il n'existe pas, puis charge la première map si elle existe
    creation(nom_fichier);
    sauvegarder(nom_fichier, mapActuel, numeroLevel);
    chargement(nom_fichier, &mapActuel, numeroLevel);

    // Création des fenêtres et des cadres associés
    level = newwin(22, 62, 0, 0);
    tools = newwin(22, 15, 0, 62);
    informations = newwin(5, 77, 22, 0);

    // Crée les bordures des fenêtres
    box(level, 0, 0);
    box(tools, 0, 0);
    box(informations, 0, 0);
    
    mvwprintw(level, 0, 1, "Level");
    mvwprintw(tools, 0, 1, "Tools");
    mvwprintw(informations, 0, 1, "Informations");

    // Affichage des options dans Tools
    affichageBoutons(tools, boutons);

    // Affichage du Current level
    mvwprintw(tools, 15, 1, "Current level");
    mvwprintw(tools, 17, 4, "< 00%d >", numeroLevel);

    // Affichage bouton DELETE
    wattron(tools,COLOR_PAIR(COULEUR_RED_BLACK));
    mvwprintw(tools, 19, 4, "DELETE");
    wattroff(tools, COLOR_PAIR(COULEUR_RED_BLACK));

    // Affiche le prochain numéro de porte 
    affichageNumeroPorte(tools, numeroPorte);

    for(int k=0; k<4; k++)
    {
        wattron(tools, COLOR_PAIR(k+2));
        mvwaddch(tools, 5, 9+(largeur*k), ' ' | A_REVERSE); // dessine un espace avec le style A_REVERSE, qui inversera les couleurs de l'arrière-plan et du premier plan
        wattroff(tools, COLOR_PAIR(k+2));
        
        wrefresh(tools);
    }

    // Affichage dans Informations
    wattron(informations,COLOR_PAIR(COULEUR_RED_BLACK));
    mvwprintw(informations, 1, 1, "Press 'Q' to quit...");
    wattroff(informations, COLOR_PAIR(COULEUR_RED_BLACK));

    // Affichage de la map (level)
    afficherMap(mapActuel);

    // Actualise les fenêtres
    wrefresh(level);
    wrefresh(tools);
    wrefresh(informations);

    while((ch = getch()) != 'q')
    {
        if(ch == KEY_MOUSE)
        {
            if(mouse_getpos(&posX, &posY) == OK)
            {
                // si on est dans la fenêtre des informations.
                if(posX>=62 && posX<77 && posY>=0 && posY<=22)
                {
                    if(posY==1){changerBouton(tools, boutons, &boutonSelectionne, 0);} // delete
                    else if(posY==2){changerBouton(tools, boutons, &boutonSelectionne, 1);} // block
                    else if(posY==3){changerBouton(tools, boutons, &boutonSelectionne, 2);} // ladder
                    else if(posY==4){changerBouton(tools, boutons, &boutonSelectionne, 3);} // Trap
                    else if(posY==5 && posX<71){changerBouton(tools, boutons, &boutonSelectionne, 4);} // Gate
                    else if(posY==6){changerBouton(tools, boutons, &boutonSelectionne, 5);} // Key
                    else if(posY==7){changerBouton(tools, boutons, &boutonSelectionne, 6);} // Door
                    else if(posY==8){changerBouton(tools, boutons, &boutonSelectionne, 7);} // Exit
                    else if(posY==9){changerBouton(tools, boutons, &boutonSelectionne, 8);} // Start
                    else if(posY==10){changerBouton(tools, boutons, &boutonSelectionne, 9);} // Robot
                    else if(posY==11){changerBouton(tools, boutons, &boutonSelectionne, 10);} // Probe
                    else if(posY==12){changerBouton(tools, boutons, &boutonSelectionne, 11);} // Life
                    else if(posY==13){changerBouton(tools, boutons, &boutonSelectionne, 12);} // Bomb
                    else if(posY==19) // DELETE
                    {
                        // Change l'affichage du bouton séléctionner
                        changerBouton(tools, boutons, &boutonSelectionne, 13);

                        // Éfface la map
                        effacerAffichageMap();

                        // Supprime la map du fichier
                        suppression(nom_fichier, numeroLevel);

                        // Remplie la map de case vide
                        for (int i = 0; i < NB_LIGNE; i++){
                            for (int j = 0; j < NB_COLONNE; j++){
                                mapActuel.matrice[i][j] = case_vide;
                            }
                        }
                        
                        // Supprime la croix rouge de la map
                        suppressionCroixRouge(&premierClicBlock, &AnciennePosAbscisse, &AnciennePosOrdonne);
                    }

                    // si on clique sur une couleur pour le portail/clé
                    if(posY==5 && posX>=71 && posX<=74){
                        mvaddch(posY+1, anciennePositionCouleur, ' ');
                        anciennePositionCouleur = posX;
                        mvaddch(posY+1, posX, '^');

                        switch(posX)
                        {
                            case 71:
                                couleurSelectionne=COULEUR_MAGENTA_BLACK;
                                break;
                            case 72:
                                couleurSelectionne=COULEUR_GREEN_BLACK;
                                break;
                            case 73:
                                couleurSelectionne=COULEUR_YELLOW_BLACK;
                                break;
                            case 74: 
                                couleurSelectionne=COULEUR_BLUE_BLACK;
                                break;
                        }
                    }
                    affichageToolsInfo(informations, boutonSelectionne);

                    // Si on clique sur les flèches du numéro de porte
                    if(posY==7){
                        switch(posX){
                            case 71:    // Flèche gauche
                                if(--numeroPorte==0){
                                    numeroPorte=99;
                                }
                                affichageNumeroPorte(tools, numeroPorte);
                                break;
                            case 74:    // Flèche droite
                                if(++numeroPorte==100){
                                    numeroPorte=0;
                                }
                                affichageNumeroPorte(tools, numeroPorte);
                                break;
                        }
                    }

                    // Si on clique sur les flèches du current level
                    if(posY==17){
                        switch(posX){
                            case 66:    // Flèche gauche
                                // Sauvegarde la map dans le fichier
                                suppression(nom_fichier, numeroLevel);
                                sauvegarder(nom_fichier, mapActuel, numeroLevel);
                                memset(&mapActuel, -1, sizeof(mapActuel));

                                // Actualise le numéro de porte
                                if(numeroLevel>1){numeroLevel--;}
                                else {numeroLevel=100;}
                                
                                // Éfface l'ancienne map
                                effacerAffichageMap();

                                // Charge la nouvelle map 
                                chargement(nom_fichier, &mapActuel, numeroLevel);

                                // Affiche la nouvelle map
                                afficherMap(mapActuel);

                                suppressionCroixRouge(&premierClicBlock, &AnciennePosAbscisse, &AnciennePosOrdonne);
                                break;
                            case 72:    // Flèche droite
                                // Sauvegarde la map dans le fichier
                                suppression(nom_fichier, numeroLevel);
                                sauvegarder(nom_fichier, mapActuel, numeroLevel);
                                memset(&mapActuel, -1, sizeof(mapActuel));

                                // Actualise le numéro de porte
                                if(numeroLevel<100){numeroLevel++;}
                                else {numeroLevel=1;}

                                // Éfface l'ancienne map
                                effacerAffichageMap();

                                // Charge la nouvelle map 
                                chargement(nom_fichier, &mapActuel, numeroLevel);

                                // Affiche la nouvelle map
                                afficherMap(mapActuel);

                                suppressionCroixRouge(&premierClicBlock, &AnciennePosAbscisse, &AnciennePosOrdonne);
                                break;
                        }
                        affichageCurrentLevel(tools, numeroLevel);
                    }
                }    
            }
            if(posX>1 && posX<60 && posY>1 && posY<20)
            {
                if(boutonSelectionne==DELETE_SIMPLE) // Delete simple
                {
                    mvaddch(posY, posX, ' ');

                    mapActuel.matrice[posY-1][posX-1] = case_vide;

                    premierClicBlock=0;
                }
                else if(boutonSelectionne==BLOCK) // Block
                {
                    // Premier click du block
                    if(premierClicBlock==0)
                    {
                        AnciennePosAbscisse=posX;
                        AnciennePosOrdonne=posY;
                        premierClicBlock=1;

                        attron(COLOR_PAIR(COULEUR_RED_BLACK));
                        mvaddch(posY, posX, 'X');
                        attroff(COLOR_PAIR(COULEUR_RED_BLACK));
                    }
                    // Deuxième click du block
                    else
                    {
                        attron(COLOR_PAIR(COULEUR_CYAN_BLACK));
                        if(posX==AnciennePosAbscisse || posX==AnciennePosAbscisse-1 || posX==AnciennePosAbscisse+1){
                            // Du Haut vers le Bas
                            if(AnciennePosOrdonne<posY){
                                for(int i=AnciennePosOrdonne; i<posY+1; i++){
                                    mvaddch(i, AnciennePosAbscisse, ' ' | A_REVERSE);
                                    mapActuel.matrice[i-1][AnciennePosAbscisse-1] = case_block;
                                }
                            // Du Bas vers le Haut
                            }else{
                                for(int i=AnciennePosOrdonne; i>posY-1; i--){
                                    mvaddch(i, AnciennePosAbscisse, ' ' | A_REVERSE);
                                    mapActuel.matrice[i-1][AnciennePosAbscisse-1] = case_block;
                                }
                            }
                        }
                        if(posY==AnciennePosOrdonne || posY==AnciennePosOrdonne-1 || posY==AnciennePosOrdonne+1){
                            // De Gauche vers la Droite
                            if(AnciennePosAbscisse<posX){
                                for(int i=AnciennePosAbscisse; i<posX+1; i++){
                                    mvaddch(AnciennePosOrdonne, i, ' ' | A_REVERSE);
                                    mapActuel.matrice[AnciennePosOrdonne-1][i-1] = case_block;
                                }
                            // De Droite vers la Gauche
                            }else{
                                for(int i=AnciennePosAbscisse; i>posX-1; i--){
                                    mvaddch(AnciennePosOrdonne, i, ' ' | A_REVERSE);
                                    mapActuel.matrice[AnciennePosOrdonne-1][i-1] = case_block;
                                }
                            }
                        }
                        mvaddch(AnciennePosOrdonne, AnciennePosAbscisse, ' ' | A_REVERSE);

                        // Supprime la croix si l'utilisateur ne fait pas de deuxième clic
                        if(posY==AnciennePosOrdonne && posX==AnciennePosAbscisse){
                            mvaddch(AnciennePosOrdonne, AnciennePosAbscisse, ' '| A_REVERSE);
                            mapActuel.matrice[AnciennePosOrdonne-1][AnciennePosAbscisse-1] = case_block;
                        }
                        attroff(COLOR_PAIR(COULEUR_CYAN_BLACK));    
                        premierClicBlock=0;
                    }
                }
                else if(boutonSelectionne==LADDER) // Ladder
                {
                    if(((posX+1!=60) && (posX+2!=60)))
                    {
                        attron(COLOR_PAIR(COULEUR_YELLOW_BLACK));
                        mvaddch(posY, posX, ACS_LTEE);
                        mvaddch(posY, posX+1, ACS_HLINE);
                        mvaddch(posY, posX+2, ACS_RTEE);
                        attroff(COLOR_PAIR(COULEUR_YELLOW_BLACK));

                        case_t case_ladder = {LADDER, -1, -1};
                        mapActuel.matrice[posY-1][posX-1] = case_ladder;
                    }
                }
                else if(boutonSelectionne==TRAP) // Trap
                {
                    attron(COLOR_PAIR(COULEUR_CYAN_BLACK));
                    mvaddch(posY, posX, '#' | A_REVERSE);
                    attroff(COLOR_PAIR(COULEUR_CYAN_BLACK));

                    case_t case_trap = {TRAP, -1, -1};
                    mapActuel.matrice[posY-1][posX-1] = case_trap;
                }
                else if(boutonSelectionne==GATE) // gate
                {
                    attron(COLOR_PAIR(couleurSelectionne));
                    mvaddch(posY, posX, ACS_PLUS);
                    attroff(COLOR_PAIR(couleurSelectionne));

                    case_t case_gate = {GATE, couleurSelectionne, -1};
                    mapActuel.matrice[posY-1][posX-1] = case_gate;
                }
                else if(boutonSelectionne==KEY) // key
                {
                    if(posY-1!=1) // on vérifie que la partie haute de la clé ne dépasse pas. 
                    {
                        attron(COLOR_PAIR(couleurSelectionne));
                        mvaddch(posY, posX, ACS_LLCORNER);
                        mvaddch(posY-1, posX, ' ' | A_REVERSE);
                        attroff(COLOR_PAIR(couleurSelectionne));

                        case_t case_key = {KEY, couleurSelectionne, -1};
                        mapActuel.matrice[posY-1][posX-1] = case_key;
                    }
                }
                else if(boutonSelectionne==DOOR) // door
                {
                    attron(COLOR_PAIR(COULEUR_GREEN_BLACK));
                    affichagePorteStartExit(posX, posY);
                    attroff(COLOR_PAIR(COULEUR_GREEN_BLACK));
                    // Vérification sortie de map
                    if(posX+1!=60 && posX+2!=60 && posY-1!=1 && posY-2!=1 && posY-3!=1)
                    {
                        case_t case_door = {DOOR, COULEUR_GREEN_BLACK, numeroPorte};
                        mapActuel.matrice[posY-1][posX-1] = case_door;

                        affichageNumeroPorteGame(posX, posY, numeroPorte);
                    }
                }
                else if(boutonSelectionne==EXIT) // exit
                {
                    attron(COLOR_PAIR(COULEUR_YELLOW_BLACK));
                    affichagePorteStartExit(posX, posY);
                    attroff(COLOR_PAIR(COULEUR_YELLOW_BLACK));

                    if(posX+1!=60 && posX+2!=60 && posY-1!=1 && posY-2!=1 && posY-3!=1)
                    {
                        case_t case_exit = {EXIT, -1, -1};
                        mapActuel.matrice[posY-1][posX-1] = case_exit;
                    }
                }
                else if(boutonSelectionne==START) // start
                {
                    attron(COLOR_PAIR(COULEUR_MAGENTA_BLACK));
                    affichagePorteStartExit(posX, posY);
                    attroff(COLOR_PAIR(COULEUR_MAGENTA_BLACK));

                    if(posX+1!=60 && posX+2!=60 && posY-1!=1 && posY-2!=1 && posY-3!=1)
                    {
                        case_t case_start = {START, -1, -1};
                        mapActuel.matrice[posY-1][posX-1] = case_start;
                    }
                }
                else if(boutonSelectionne==ROBOT) // robot
                {
                    if(posX+1!=60 && posX+2!=60 && posY-1!=1 && posY-2!=1 && posY-3!=1)
                    {
                        // haut de la tete
                        mvaddch(posY-3, posX, ACS_ULCORNER);
                        mvaddch(posY-3, posX+1, ACS_BTEE);
                        mvaddch(posY-3, posX+2, ACS_URCORNER);

                        // bas de la tete
                        mvaddch(posY-2, posX, ACS_LLCORNER);
                        mvaddch(posY-2, posX+1, ACS_TTEE);
                        mvaddch(posY-2, posX+2, ACS_LRCORNER);

                        // corps
                        mvaddch(posY-1, posX, ACS_HLINE);
                        mvaddch(posY-1, posX+1, ACS_PLUS);
                        mvaddch(posY-1, posX+2, ACS_HLINE);

                        // jambes
                        mvaddch(posY, posX, ACS_ULCORNER);
                        mvaddch(posY, posX+1, ACS_BTEE);
                        mvaddch(posY, posX+2, ACS_URCORNER);

                        case_t case_robot = {ROBOT, -1, -1};
                        mapActuel.matrice[posY-1][posX-1] = case_robot;
                    }
                }
                else if(boutonSelectionne==PROBE) // probe
                {
                    if(posX+1!=60 && posX+2!=60 && posY-1!=1)
                    {
                        // partie haute
                        mvaddch(posY-1, posX, ACS_LTEE);
                        mvaddch(posY-1, posX+1, ACS_HLINE);
                        mvaddch(posY-1, posX+2, ACS_RTEE);

                        // partie basse
                        mvaddch(posY, posX, ACS_LLCORNER);
                        mvaddch(posY, posX+1, ACS_HLINE);
                        mvaddch(posY, posX+2, ACS_LRCORNER);

                        case_t case_probe = {PROBE, -1, -1};
                        mapActuel.matrice[posY-1][posX-1] = case_probe;
                    }
                }
                else if(boutonSelectionne==LIFE) // life
                {
                    attron(COLOR_PAIR(COULEUR_RED_BLACK));
                    mvaddch(posY, posX, 'V');
                    attroff(COLOR_PAIR(COULEUR_RED_BLACK));

                    case_t case_life = {LIFE, 1, -1};
                    mapActuel.matrice[posY-1][posX-1] = case_life;
                }
                else if(boutonSelectionne==BOMB) // bomb
                {
                    mvaddch(posY, posX, 'o');
                    case_t case_bomb = {BOMB, 1, -1};
                    mapActuel.matrice[posY-1][posX-1] = case_bomb;
                } // bomb
            }
        }
    }

    suppression(nom_fichier, numeroLevel);
    sauvegarder(nom_fichier, mapActuel, numeroLevel);

    delwin(level);
    delwin(tools);
    delwin(informations);

    ncurses_stop();

    return EXIT_SUCCESS;
}