#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <locale.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#include "functions.h"
#include "includes.h"



WINDOW *level, *tools, *informations;
thread_affichage_client_t struct_affichage;
pthread_mutex_t mutex_affichage = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_affichage = PTHREAD_COND_INITIALIZER;



// Palette pour les couleurs
void palette() {
	init_pair(COULEUR_RED_BLACK, COLOR_RED, COLOR_BLACK);
    init_pair(COULEUR_MAGENTA_BLACK, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(COULEUR_GREEN_BLACK, COLOR_GREEN, COLOR_BLACK);
    init_pair(COULEUR_YELLOW_BLACK, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COULEUR_BLUE_BLACK, COLOR_BLUE, COLOR_BLACK);
    init_pair(COULEUR_CYAN_BLACK, COLOR_CYAN, COLOR_BLACK);
}



// Fonction qui permet de supprimer les informations dans le menu "Informations"
void supprimeTextInfo(WINDOW* informations)
{
    for(int i=2; i<4; i++)
    {
        for(int j=1; j<70; j++)
        {
            mvwaddch(informations, i, j, ' ');
        }
    }
}

// Permet d'afficher le numéro de la porte dans le niveau lorsqu'on pose une porte
void affichageNumeroPorteGame(int posX, int posY, int numeroPorte){
    if(numeroPorte<10){
        mvprintw(posY-3, posX, "0%d", numeroPorte);
    }else{
        mvprintw(posY-3, posX, "%d", numeroPorte);
    }
}

// Affichage du rectangle pour les portes, entrée et sortie
void affichagePorteStartExit(int posX, int posY){
    if(posX+1!=60 && posX+2!=60 && posY-1!=1 && posY-2!=1 && posY-3!=1){
        for(int i=0; i<3; i++){
            for(int j=0; j<4; j++){
                mvaddch(posY-j, posX+i, ' ' | A_REVERSE);
            }
        } 
    }
}


// Permet d'afficher la map à l'écran
void afficherMap(map_t map){
    // Parcours du level 
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
                    if(map.matrice[i-1][j-1].couleur == VISIBLE){
                        attron(COLOR_PAIR(COULEUR_CYAN_BLACK));
                        mvaddch(i, j, ' ' | A_REVERSE);
                        attroff(COLOR_PAIR(COULEUR_CYAN_BLACK));
                    }
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

                case LIFE:
                    if(map.matrice[i-1][j-1].couleur == VISIBLE){
                        attron(COLOR_PAIR(COULEUR_RED_BLACK));
                        mvaddch(i, j, 'V');
                        attroff(COLOR_PAIR(COULEUR_RED_BLACK));
                    }
                    break;

                case BOMB:
                    if(map.matrice[i-1][j-1].couleur == VISIBLE)
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


// Permet de vérifier la taille du terminal de l'utilisateur
int verificationTailleTerminal(int *tailleTerminalX, int *tailleTerminalY){
    // Récupération des dimensions du terminal
    getmaxyx(stdscr, *tailleTerminalY, *tailleTerminalX); 

    if(*tailleTerminalY<27  || *tailleTerminalX<77){
        return true;
    }
    return false;
}

// Permet d'afficher le numéro du level
void afficher_numero_level(WINDOW* tools, int numeroLevel){
    if(numeroLevel<=0)
        mvwprintw(tools, 16, 3, "00 ");
    else if(numeroLevel<10)
        mvwprintw(tools, 16, 3, "0%d ", numeroLevel);
    else
        mvwprintw(tools, 16, 3, "%d ", numeroLevel);
}

// Permet d'afficher les joueurs
void afficherJoueur(joueur_t* tab_joueur, int nb_joueur, int id_joueur_actuel){
    int rand_couleur=1, rand_oeil=1, rand_bouche=1;
    char tableau_oeil[4] = {'O', 'U', '*', '^'};
    char tableau_bouche[4] = {'_', 'w', '-'};
    
    srand(time(NULL)*tab_joueur[id_joueur_actuel].pos.posX*tab_joueur[id_joueur_actuel].pos.posY);

    // Parcours la liste de joueur
    for (int i = 0; i < nb_joueur; i++) {
        if(tab_joueur[i].id_level == tab_joueur[id_joueur_actuel].id_level)
        {
            // Si le joueur est en GOD MODE
            if(tab_joueur[i].statut==GOD_MODE)
            {
                // Oeil gauche
                rand_couleur = (rand() % 6) + 1;
                attron(COLOR_PAIR(rand_couleur));
                rand_oeil = (rand() % 4);
                mvprintw(tab_joueur[i].pos.posY-1, tab_joueur[i].pos.posX+1, "%c", tableau_oeil[rand_oeil]);
                attroff(COLOR_PAIR(rand_couleur));

                // Bouche
                rand_couleur = (rand() % 6) + 1;
                attron(COLOR_PAIR(rand_couleur));
                rand_bouche = (rand() % 3);
                mvprintw(tab_joueur[i].pos.posY-1, tab_joueur[i].pos.posX+2, "%c", tableau_bouche[rand_bouche]);
                attroff(COLOR_PAIR(rand_couleur));

                // Oeil droit
                rand_couleur = (rand() % 6) + 1;
                attron(COLOR_PAIR(rand_couleur));
                rand_oeil = (rand() % 4);
                mvprintw(tab_joueur[i].pos.posY-1, tab_joueur[i].pos.posX+3, "%c", tableau_oeil[rand_oeil]);
                attroff(COLOR_PAIR(rand_couleur));
                
                // Buste
                rand_couleur = (rand() % 6) + 1;
                attron(COLOR_PAIR(rand_couleur));
                mvaddch(tab_joueur[i].pos.posY, tab_joueur[i].pos.posX+1, ACS_HLINE);
                attroff(COLOR_PAIR(rand_couleur));
                rand_couleur = (rand() % 6) + 1;
                attron(COLOR_PAIR(rand_couleur));
                mvaddch(tab_joueur[i].pos.posY, tab_joueur[i].pos.posX+2, ACS_PLUS);
                attroff(COLOR_PAIR(rand_couleur));
                rand_couleur = (rand() % 6) + 1;
                attron(COLOR_PAIR(rand_couleur));
                mvaddch(tab_joueur[i].pos.posY, tab_joueur[i].pos.posX+3, ACS_HLINE);
                attroff(COLOR_PAIR(rand_couleur));

                // Jambe
                rand_couleur = (rand() % 6) + 1;
                attron(COLOR_PAIR(rand_couleur));
                mvaddch(tab_joueur[i].pos.posY+1, tab_joueur[i].pos.posX+1, ACS_ULCORNER);
                attroff(COLOR_PAIR(rand_couleur));
                rand_couleur = (rand() % 6) + 1;
                attron(COLOR_PAIR(rand_couleur));
                mvaddch(tab_joueur[i].pos.posY+1, tab_joueur[i].pos.posX+2, ACS_BTEE);
                attroff(COLOR_PAIR(rand_couleur));
                rand_couleur = (rand() % 6) + 1;
                attron(COLOR_PAIR(rand_couleur));
                mvaddch(tab_joueur[i].pos.posY+1, tab_joueur[i].pos.posX+3, ACS_URCORNER);
                attroff(COLOR_PAIR(rand_couleur));
            }
            else
            {
                // Si le joueur est PARALYSER
                if(tab_joueur[i].etat == PARALYSER){
                    attron(COLOR_PAIR(COULEUR_RED_BLACK));
                    // Tête
                    mvprintw(tab_joueur[i].pos.posY-1, tab_joueur[i].pos.posX+1, "O_O"); // UwU  *-*  O_O  ^o^ ^_^
                    
                    // Buste
                    mvaddch(tab_joueur[i].pos.posY, tab_joueur[i].pos.posX+1, ACS_HLINE);
                    mvaddch(tab_joueur[i].pos.posY, tab_joueur[i].pos.posX+2, ACS_PLUS);
                    mvaddch(tab_joueur[i].pos.posY, tab_joueur[i].pos.posX+3, ACS_HLINE);

                    // Jambe
                    mvaddch(tab_joueur[i].pos.posY+1, tab_joueur[i].pos.posX+1, ACS_ULCORNER);
                    mvaddch(tab_joueur[i].pos.posY+1, tab_joueur[i].pos.posX+2, ACS_BTEE);
                    mvaddch(tab_joueur[i].pos.posY+1, tab_joueur[i].pos.posX+3, ACS_URCORNER);
                    attroff(COLOR_PAIR(COULEUR_BLUE_BLACK));

                // Si le joueur est NORMAL
                }else{
                    attron(COLOR_PAIR(COULEUR_BLUE_BLACK));
                    // Tête
                    mvprintw(tab_joueur[i].pos.posY-1, tab_joueur[i].pos.posX+1, "O_O"); // UwU  *-*  O_O  ^o^ ^_^
                    
                    // Buste
                    mvaddch(tab_joueur[i].pos.posY, tab_joueur[i].pos.posX+1, ACS_HLINE);
                    mvaddch(tab_joueur[i].pos.posY, tab_joueur[i].pos.posX+2, ACS_PLUS);
                    mvaddch(tab_joueur[i].pos.posY, tab_joueur[i].pos.posX+3, ACS_HLINE);

                    // Jambe
                    mvaddch(tab_joueur[i].pos.posY+1, tab_joueur[i].pos.posX+1, ACS_ULCORNER);
                    mvaddch(tab_joueur[i].pos.posY+1, tab_joueur[i].pos.posX+2, ACS_BTEE);
                    mvaddch(tab_joueur[i].pos.posY+1, tab_joueur[i].pos.posX+3, ACS_URCORNER);
                    attroff(COLOR_PAIR(COULEUR_BLUE_BLACK));
                }
            }
        }
    }
}

// Actualise les informations du joueur
void actualiser_info_joueur(WINDOW* tools, joueur_t joueur){
    // Actualisation des clés 
    wattron(tools,COLOR_PAIR(COULEUR_RED_BLACK));
    for(int i=0; i<4; i++){
        mvwaddch(tools, 4, 1+(i*2), ACS_LLCORNER);
        mvwaddch(tools, 3, 1+(i*2), ' ' | A_REVERSE);
    }
    wattroff(tools,COLOR_PAIR(COULEUR_RED_BLACK));

    // Actualisation des clés
    if(joueur.got_key[0]==1){
        wattron(tools,COLOR_PAIR(COULEUR_MAGENTA_BLACK));
        mvwaddch(tools, 4, 1, ACS_LLCORNER);
        mvwaddch(tools, 3, 1, ' ' | A_REVERSE);
        wattroff(tools,COLOR_PAIR(COULEUR_MAGENTA_BLACK));
    }
    if(joueur.got_key[1]==1){
        wattron(tools,COLOR_PAIR(COULEUR_GREEN_BLACK));
        mvwaddch(tools, 4, 3, ACS_LLCORNER);
        mvwaddch(tools, 3, 3, ' ' | A_REVERSE);
        wattroff(tools,COLOR_PAIR(COULEUR_GREEN_BLACK));
    }
    if(joueur.got_key[2]==1){
        wattron(tools,COLOR_PAIR(COULEUR_YELLOW_BLACK));
        mvwaddch(tools, 4, 5, ACS_LLCORNER);
        mvwaddch(tools, 3, 5, ' ' | A_REVERSE);
        wattroff(tools,COLOR_PAIR(COULEUR_YELLOW_BLACK));
    }
    if(joueur.got_key[3]==1){
        wattron(tools,COLOR_PAIR(COULEUR_BLUE_BLACK));
        mvwaddch(tools, 4, 7, ACS_LLCORNER);
        mvwaddch(tools, 3, 7, ' ' | A_REVERSE);
        wattroff(tools,COLOR_PAIR(COULEUR_BLUE_BLACK));
    }

    // Actualise la vie du joueur
    wattron(tools,COLOR_PAIR(COULEUR_RED_BLACK));
    for(int i=0; i<5; i++)
        mvwaddch(tools, 8, 2+i, 'V');
    wattroff(tools,COLOR_PAIR(COULEUR_RED_BLACK));
    
    for(int i=0; i<joueur.nb_Life; i++)
        mvwaddch(tools, 8, 2+i, 'V');

    // Actualise les bombes
    wattron(tools,COLOR_PAIR(COULEUR_RED_BLACK));
    for(int i=0; i<3; i++)
        mvwaddch(tools, 12, 3+i, 'O');
    wattroff(tools,COLOR_PAIR(COULEUR_RED_BLACK));
    for(int i=0; i<joueur.nb_Bomb; i++)
        mvwaddch(tools, 12, 3+i, 'O');

    // Actualise le level
    afficher_numero_level(tools, joueur.id_level);
}

// Permet d'afficher les monstres
void afficherMonstre(robot_t tab_robot[10], probe_t tab_probe[10]){
    for(int i=0; i<10; i++){
        // Robots
        if(tab_robot[i].pos.posX!=0 && tab_robot[i].pos.posY!=0){
            if(tab_robot[i].etat == PARALYSER){
                attron(COLOR_PAIR(COULEUR_RED_BLACK));
                // tete
                mvaddch(tab_robot[i].pos.posX-2, tab_robot[i].pos.posY+1, ACS_ULCORNER);
                mvaddch(tab_robot[i].pos.posX-2, tab_robot[i].pos.posY+2, ACS_BTEE);
                mvaddch(tab_robot[i].pos.posX-2, tab_robot[i].pos.posY+3, ACS_URCORNER);

                // bas de la tete
                mvaddch(tab_robot[i].pos.posX-1, tab_robot[i].pos.posY+1, ACS_LLCORNER);
                mvaddch(tab_robot[i].pos.posX-1, tab_robot[i].pos.posY+2, ACS_TTEE);
                mvaddch(tab_robot[i].pos.posX-1, tab_robot[i].pos.posY+3, ACS_LRCORNER);

                // corps
                mvaddch(tab_robot[i].pos.posX, tab_robot[i].pos.posY+1, ACS_HLINE);
                mvaddch(tab_robot[i].pos.posX, tab_robot[i].pos.posY+2, ACS_PLUS);
                mvaddch(tab_robot[i].pos.posX, tab_robot[i].pos.posY+3, ACS_HLINE);

                // jambe
                mvaddch(tab_robot[i].pos.posX+1, tab_robot[i].pos.posY+1, ACS_ULCORNER);
                mvaddch(tab_robot[i].pos.posX+1, tab_robot[i].pos.posY+2, ACS_BTEE);
                mvaddch(tab_robot[i].pos.posX+1, tab_robot[i].pos.posY+3, ACS_URCORNER);
                attroff(COLOR_PAIR(COULEUR_RED_BLACK));
            }else{
                // tete
                mvaddch(tab_robot[i].pos.posX-2, tab_robot[i].pos.posY+1, ACS_ULCORNER);
                mvaddch(tab_robot[i].pos.posX-2, tab_robot[i].pos.posY+2, ACS_BTEE);
                mvaddch(tab_robot[i].pos.posX-2, tab_robot[i].pos.posY+3, ACS_URCORNER);

                // bas de la tete
                mvaddch(tab_robot[i].pos.posX-1, tab_robot[i].pos.posY+1, ACS_LLCORNER);
                mvaddch(tab_robot[i].pos.posX-1, tab_robot[i].pos.posY+2, ACS_TTEE);
                mvaddch(tab_robot[i].pos.posX-1, tab_robot[i].pos.posY+3, ACS_LRCORNER);

                // corps
                mvaddch(tab_robot[i].pos.posX, tab_robot[i].pos.posY+1, ACS_HLINE);
                mvaddch(tab_robot[i].pos.posX, tab_robot[i].pos.posY+2, ACS_PLUS);
                mvaddch(tab_robot[i].pos.posX, tab_robot[i].pos.posY+3, ACS_HLINE);

                // jambe
                mvaddch(tab_robot[i].pos.posX+1, tab_robot[i].pos.posY+1, ACS_ULCORNER);
                mvaddch(tab_robot[i].pos.posX+1, tab_robot[i].pos.posY+2, ACS_BTEE);
                mvaddch(tab_robot[i].pos.posX+1, tab_robot[i].pos.posY+3, ACS_URCORNER);
            }
        }

        // Probe
        if(tab_probe[i].pos.posX!=0 && tab_probe[i].pos.posY!=0){
            if(tab_probe[i].etat == PARALYSER){
                attron(COLOR_PAIR(COULEUR_RED_BLACK));
                // partie haute
                mvaddch(tab_probe[i].pos.posX, tab_probe[i].pos.posY+1, ACS_LTEE);
                mvaddch(tab_probe[i].pos.posX, tab_probe[i].pos.posY+2, ACS_HLINE);
                mvaddch(tab_probe[i].pos.posX, tab_probe[i].pos.posY+3, ACS_RTEE);

                // partie basse
                mvaddch(tab_probe[i].pos.posX+1, tab_probe[i].pos.posY+1, ACS_LLCORNER);
                mvaddch(tab_probe[i].pos.posX+1, tab_probe[i].pos.posY+2, ACS_HLINE);
                mvaddch(tab_probe[i].pos.posX+1, tab_probe[i].pos.posY+3, ACS_LRCORNER);
                attroff(COLOR_PAIR(COULEUR_RED_BLACK));
            }else{
                // partie haute
                mvaddch(tab_probe[i].pos.posX, tab_probe[i].pos.posY+1, ACS_LTEE);
                mvaddch(tab_probe[i].pos.posX, tab_probe[i].pos.posY+2, ACS_HLINE);
                mvaddch(tab_probe[i].pos.posX, tab_probe[i].pos.posY+3, ACS_RTEE);

                // partie basse
                mvaddch(tab_probe[i].pos.posX+1, tab_probe[i].pos.posY+1, ACS_LLCORNER);
                mvaddch(tab_probe[i].pos.posX+1, tab_probe[i].pos.posY+2, ACS_HLINE);
                mvaddch(tab_probe[i].pos.posX+1, tab_probe[i].pos.posY+3, ACS_LRCORNER);
            }
        }
    }
}

// Permet d'afficher la bomb
void afficherBomb(joueur_t* tab_joueur, int nb_joueur, map_t map, int id_level_joueur_actuel){
    for (int i = 0; i < nb_joueur; i++) {
        for (int j = 0; j < 3; j++) {
            if(id_level_joueur_actuel == tab_joueur[i].tab_bomb[j].id_level){
                // Affiche la bomb en ATTENTE
                if(tab_joueur[i].tab_bomb[j].etat == ATTENTE){
                    mvaddch(tab_joueur[i].tab_bomb[j].pos.posY+1, tab_joueur[i].tab_bomb[j].pos.posX+1, '@');
                }
                // Affiche l'explosion de la bomb
                else if(tab_joueur[i].tab_bomb[j].etat == EXPLOSION){
                    mvaddch(tab_joueur[i].tab_bomb[j].pos.posY+1, tab_joueur[i].tab_bomb[j].pos.posX+1, '@');
                    attron(COLOR_PAIR(COULEUR_RED_BLACK));

                    // Affichage explosion gauche
                    for(int k=0; k<5; k++){
                        if((tab_joueur[i].tab_bomb[j].pos.posY+1>1 && tab_joueur[i].tab_bomb[j].pos.posY+1<=19) && (tab_joueur[i].tab_bomb[j].pos.posX-k>1 && tab_joueur[i].tab_bomb[j].pos.posX-k<=59))
                            mvaddch(tab_joueur[i].tab_bomb[j].pos.posY+1, tab_joueur[i].tab_bomb[j].pos.posX-k, '~' | A_REVERSE);
                    }

                    // Affichage explosion droite
                    for(int k=1; k<6; k++){
                        if((tab_joueur[i].tab_bomb[j].pos.posY+1>1 && tab_joueur[i].tab_bomb[j].pos.posY+1<=19) && (tab_joueur[i].tab_bomb[j].pos.posX+k+1>1 && tab_joueur[i].tab_bomb[j].pos.posX+k+1<=59))
                            mvaddch(tab_joueur[i].tab_bomb[j].pos.posY+1, tab_joueur[i].tab_bomb[j].pos.posX+k+1, '~' | A_REVERSE);
                    }

                    // Affichage explosion haut
                    for(int k=0; k<2; k++){
                        if((tab_joueur[i].tab_bomb[j].pos.posY-k>1 && tab_joueur[i].tab_bomb[j].pos.posY-k<=19) && (tab_joueur[i].tab_bomb[j].pos.posX+k+1>1 && tab_joueur[i].tab_bomb[j].pos.posX+k+1<=59))
                            mvaddch(tab_joueur[i].tab_bomb[j].pos.posY-k, tab_joueur[i].tab_bomb[j].pos.posX+1, '~' | A_REVERSE);
                    }

                    for(int k=0; k<2; k++){
                        if((tab_joueur[i].tab_bomb[j].pos.posY+2+k>1 && tab_joueur[i].tab_bomb[j].pos.posY+2+k<=19) && (tab_joueur[i].tab_bomb[j].pos.posX+1>1 && tab_joueur[i].tab_bomb[j].pos.posX+1<=59))
                            mvaddch(tab_joueur[i].tab_bomb[j].pos.posY+2+k, tab_joueur[i].tab_bomb[j].pos.posX+1, '~' | A_REVERSE);
                    }

                    // Vérification bord de map
                    for(int k=0; k<4; k++){
                        if((tab_joueur[i].tab_bomb[j].pos.posY>1 && tab_joueur[i].tab_bomb[j].pos.posY<=19) && (tab_joueur[i].tab_bomb[j].pos.posX+2+k>1 && tab_joueur[i].tab_bomb[j].pos.posX+2+k<=59))
                            mvaddch(tab_joueur[i].tab_bomb[j].pos.posY, tab_joueur[i].tab_bomb[j].pos.posX+2+k, '~' | A_REVERSE);

                        if((tab_joueur[i].tab_bomb[j].pos.posY>1 && tab_joueur[i].tab_bomb[j].pos.posY<=19) && (tab_joueur[i].tab_bomb[j].pos.posX-k>1 && tab_joueur[i].tab_bomb[j].pos.posX-k<=59))
                            mvaddch(tab_joueur[i].tab_bomb[j].pos.posY, tab_joueur[i].tab_bomb[j].pos.posX-k, '~' | A_REVERSE);

                        if((tab_joueur[i].tab_bomb[j].pos.posY+2>1 && tab_joueur[i].tab_bomb[j].pos.posY+2<=19) && (tab_joueur[i].tab_bomb[j].pos.posX+2+k>1 && tab_joueur[i].tab_bomb[j].pos.posX+2+k<=59))
                            mvaddch(tab_joueur[i].tab_bomb[j].pos.posY+2, tab_joueur[i].tab_bomb[j].pos.posX+2+k, '~' | A_REVERSE);

                        if((tab_joueur[i].tab_bomb[j].pos.posY+2>1 && tab_joueur[i].tab_bomb[j].pos.posY+2<=19) && (tab_joueur[i].tab_bomb[j].pos.posX-k>1 && tab_joueur[i].tab_bomb[j].pos.posX-k<=59))
                            mvaddch(tab_joueur[i].tab_bomb[j].pos.posY+2, tab_joueur[i].tab_bomb[j].pos.posX-k, '~' | A_REVERSE);
                    }

                    // Vérification bord de map
                    for(int k=0; k<3; k++){
                        if((tab_joueur[i].tab_bomb[j].pos.posY-1>1 && tab_joueur[i].tab_bomb[j].pos.posY-1<=19) && (tab_joueur[i].tab_bomb[j].pos.posX+2+k>1 && tab_joueur[i].tab_bomb[j].pos.posX+2+k<=59))
                            mvaddch(tab_joueur[i].tab_bomb[j].pos.posY-1, tab_joueur[i].tab_bomb[j].pos.posX+2+k, '~' | A_REVERSE);

                        if((tab_joueur[i].tab_bomb[j].pos.posY-1>1 && tab_joueur[i].tab_bomb[j].pos.posY-1<=19) && (tab_joueur[i].tab_bomb[j].pos.posX-k>1 && tab_joueur[i].tab_bomb[j].pos.posX-k<=59))                        
                            mvaddch(tab_joueur[i].tab_bomb[j].pos.posY-1, tab_joueur[i].tab_bomb[j].pos.posX-k, '~' | A_REVERSE);

                        if((tab_joueur[i].tab_bomb[j].pos.posY+3>1 && tab_joueur[i].tab_bomb[j].pos.posY+3<=19) && (tab_joueur[i].tab_bomb[j].pos.posX+2+k>1 && tab_joueur[i].tab_bomb[j].pos.posX+2+k<=59))                        
                            mvaddch(tab_joueur[i].tab_bomb[j].pos.posY+3, tab_joueur[i].tab_bomb[j].pos.posX+2+k, '~' | A_REVERSE);

                        if((tab_joueur[i].tab_bomb[j].pos.posY+3>1 && tab_joueur[i].tab_bomb[j].pos.posY+3<=19) && (tab_joueur[i].tab_bomb[j].pos.posX-k>1 && tab_joueur[i].tab_bomb[j].pos.posX-k<=59))                        
                            mvaddch(tab_joueur[i].tab_bomb[j].pos.posY+3, tab_joueur[i].tab_bomb[j].pos.posX-k, '~' | A_REVERSE);
                    }
                    attroff(COLOR_PAIR(COULEUR_RED_BLACK));
                }
            }
        }
    }
}


// Permet d'afficher le nom du jeu au lancement du client
void afficher_nom_jeu(){
    printf("   _____            _        _      _____                      \n");
    printf("  |  __ \\          | |      | |    / ____|                     \n");
    printf("  | |__) |___   ___| | _____| |_  | (___  _ __   __ _  ___ ___ \n");
    printf("  |  _  // _ \\ / __| |/ / _ \\ __|  \\___ \\| '_ \\ / _` |/ __/ _ \\\n");
    printf("  | | \\ \\ (_) | (__|   <  __/ |_   ____) | |_) | (_| | (_|  __/\n");
    printf("  |_|  \\_\\___/ \\___|_|\\_\\___|\\__| |_____/| .__/ \\__,_|\\___\\___|\n");
    printf("                                         | |                   \n");
    printf("          _____                     _    |_|                          \n");
    printf("         |_   _|                   | |                                \n");
    printf("           | |  _ ____   ____ _  __| | ___ _ __ ___                   \n");
    printf("           | | | '_ \\ \\ / / _` |/ _` |/ _ \\ '__/ __|                  \n");
    printf("          _| |_| | | \\ V / (_| | (_| |  __/ |  \\__ \\                  \n");
    printf("         |_____|_| |_|\\_/ \\__,_|\\__,_|\\___|_|  |___/                  \n\n");
}




// Routine pour le thread dédiée à l'affichage
void* routine_thread_affichage(void *arg){
    while(1){
        pthread_mutex_lock(&mutex_affichage);

        // Attend le signal
        pthread_cond_wait(&cond_affichage, &mutex_affichage);

        // Actualise affichage
        effacerAffichageMap();
        afficherMap(struct_affichage.map);
        afficherBomb(struct_affichage.tab_joueur, struct_affichage.nb_joueur, struct_affichage.map, struct_affichage.tab_joueur[struct_affichage.id_joueur_actuel].id_level);
        afficherMonstre(struct_affichage.tab_robot, struct_affichage.tab_probe);
        afficherJoueur(struct_affichage.tab_joueur, struct_affichage.nb_joueur, struct_affichage.id_joueur_actuel);
        actualiser_info_joueur(tools, struct_affichage.tab_joueur[struct_affichage.id_joueur_actuel]);

        refresh();
        wrefresh(tools);

        pthread_mutex_unlock(&mutex_affichage);
    }
    return NULL;
}



// Fonction de jeu. Permet d'initaliser socket TCP et la partie puis de jouer en gérant les différents affichages
void jouer(char* adresse_serveur, int port_TCP)
{
    int sock_TCP_fd;
    struct sockaddr_in server_TCP_address;

    int ch=-1;
    int numeroLevel=1, fin=0;
    int posX, posY;

    int id_joueur, nb_joueur;
    deplacement_t deplacement;

    pthread_t thread_id_affichage;


    if(pthread_create(&thread_id_affichage, NULL, routine_thread_affichage, NULL) != 0)
    {
        fprintf(stderr, "Error creating thread\n");
        exit(EXIT_FAILURE);
    }

    // Création de la socket et connexion
    if((sock_TCP_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    
    // Fill the address structure
    memset(&server_TCP_address, 0, sizeof(struct sockaddr_in));
    server_TCP_address.sin_family = AF_INET;
    server_TCP_address.sin_port = htons(port_TCP);
    if(inet_pton(AF_INET, adresse_serveur, &server_TCP_address.sin_addr) != 1) {
        perror("Error converting address");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    if(connect(sock_TCP_fd, (struct sockaddr*)&server_TCP_address, sizeof(server_TCP_address)) == -1) {
        perror("Error connecting to the server");
        exit(EXIT_FAILURE);
    }

    printf("\nLa partie est en attente de joueurs\n");
    printf("Merci de patienter quelques instants\n");

    if(read(sock_TCP_fd, &id_joueur, sizeof(id_joueur)) == -1) {
        perror("Error receiving response");
        exit(EXIT_FAILURE);
    }


    // Attente de réponse
    if(read(sock_TCP_fd, &struct_affichage, sizeof(struct_affichage)) == -1) {
        perror("Error receiving response");
        exit(EXIT_FAILURE);
    }

    deplacement.id_joueur = id_joueur;
    nb_joueur = struct_affichage.nb_joueur;
    struct_affichage.id_joueur_actuel = id_joueur;

    // démarrage de ncurses et initialisation du plateau de jeu

    // ncurses initialisation
    setlocale(LC_ALL, "");
    ncurses_init();
    

    // Colors initialisation and palette definition
    ncurses_colors();
    palette();
    
    // Mouse initialisation
    ncurses_init_mouse();

    // Création des fenêtres et des cadres associés
    level = newwin(22, 62, 0, 0);
    tools = newwin(22, 9, 0, 62);
    informations = newwin(5, 71, 22, 0);

    // Bords des fenêtres
    box(level, 0, 0);
    box(tools, 0, 0);
    box(informations, 0, 0);
    
    mvwprintw(level, 0, 1, "Level");
    mvwprintw(informations, 0, 1, "Informations");

    // Affichage des keys
    mvwprintw(tools, 1, 1, "Keys");
    wattron(tools,COLOR_PAIR(COULEUR_RED_BLACK));
    for(int i=0; i<4; i++){
        mvwaddch(tools, 4, 1+(i*2), ACS_LLCORNER);
        mvwaddch(tools, 3, 1+(i*2), ' ' | A_REVERSE);
    }
    wattroff(tools,COLOR_PAIR(COULEUR_RED_BLACK));

    // Affichage des Lives
    mvwprintw(tools, 6, 1, "Lives");
    for(int i=0; i<5; i++){
        mvwaddch(tools, 8, 2+i, 'V');
    }

    // Affichage des Bombs
    mvwprintw(tools, 10, 1, "Bombs");
    wattron(tools,COLOR_PAIR(COULEUR_RED_BLACK));
    for(int i=0; i<3; i++){
        mvwaddch(tools, 12, 3+i, 'O');
    }
    wattroff(tools,COLOR_PAIR(COULEUR_RED_BLACK));

    // Affichage du Levels
    mvwprintw(tools, 14, 1, "Level");
    afficher_numero_level(tools, numeroLevel);

    // Affichage dans Informations
    wattron(informations,COLOR_PAIR(COULEUR_RED_BLACK));
    mvwprintw(informations, 1, 1, "Press 'Q' to quit...");
    mvwprintw(informations, 2, 1, "Press 'B' to place bomb");
    wattroff(informations, COLOR_PAIR(COULEUR_RED_BLACK));

    // Actualise les fenêtres
    wrefresh(level);
    wrefresh(tools);
    wrefresh(informations);



    // sleep de 1sec pour simuler le chargement du plateau
    sleep(1);


    timeout(100); // définir un temps limite d'une seconde pour getch()   1000 = 1 sec 
    while((ch = getch()) != 'q' && fin==0)
    {
        if(struct_affichage.tab_joueur[id_joueur].etat != FALLING && struct_affichage.tab_joueur[id_joueur].statut != MORT){
            // Récupère la touche du joueur
            switch(ch) {
                case KEY_LEFT:
                    deplacement.touche=TOUCHE_GAUCHE;
                    break;
                case KEY_RIGHT: 
                    deplacement.touche=TOUCHE_DROITE;
                    break;
                case KEY_UP: 
                    deplacement.touche=TOUCHE_HAUT;
                    break;
                case KEY_DOWN: 
                    deplacement.touche=TOUCHE_BAS;
                    break;
                case 'b': 
                case 'B': 
                    deplacement.touche=TOUCHE_BOMB;
                    break;
                case 'q':
                    deplacement.touche=QUITTER;
                    break;
                default:
                    deplacement.touche=-1;
                    break;
            }
        }else{
            if(struct_affichage.tab_joueur[id_joueur].statut == MORT)
            {
                mvwprintw(informations, 2, 1, "Click 'Yes' to respawn or 'No' to leave");
                wrefresh(informations);
                supprimeTextInfo(informations);

                wattron(informations,COLOR_PAIR(COULEUR_RED_BLACK));
                mvwprintw(informations, 2, 1, "Press 'B' to place bomb");
                wattroff(informations, COLOR_PAIR(COULEUR_RED_BLACK));
            }
            deplacement.touche=-1;
        }

        // Position du click du joueur s'il est dans le level respawn
        if(struct_affichage.tab_joueur[id_joueur].statut == MORT)
        {
            if(ch == KEY_MOUSE){
                if(mouse_getpos(&posX, &posY) == OK){
                    if((posX>=39 && posX<=50) && (posY>=12 && posY<=16)){
                        deplacement.touche=QUITTER;
                        wrefresh(informations);
                        fin=1;
                    }
                    if((posX>=8 && posX<=24) && (posY>=12 && posY<=16)){
                        deplacement.touche=RESPAWN;
                        wrefresh(informations);
                    }
                }     
            }
        }

        // Envoie requête
        if(write(sock_TCP_fd, &deplacement, sizeof(deplacement)) == -1) {
            perror("Error sending the player's move");
            exit(EXIT_FAILURE);
        }

        if(deplacement.touche!=QUITTER)
        {
            if(read(sock_TCP_fd, &struct_affichage, sizeof(struct_affichage)) == -1) {
                perror("Error receiving test");
                exit(EXIT_FAILURE);
            }


            struct_affichage.nb_joueur = nb_joueur;
            struct_affichage.id_joueur_actuel = id_joueur;


            pthread_mutex_lock(&mutex_affichage);
            pthread_cond_signal(&cond_affichage);
            pthread_mutex_unlock(&mutex_affichage);
        }
    }

    // Si l'utilisateur quitte
    if(ch=='q')
    {
        deplacement.touche=QUITTER;

        // Envoie requête
        if(write(sock_TCP_fd, &deplacement, sizeof(deplacement)) == -1) {
            perror("Error sending the player's move");
            exit(EXIT_FAILURE);
        }
    }
    supprimeTextInfo(informations);
    mvwprintw(informations, 2, 1, "Leaving");
    wrefresh(informations);

    // sleep de 1sec pour donner un effet de déconnexion
    sleep(1);

    // Suppression des fenêtres et fermeture de ncurses
    delwin(level);
    delwin(tools);
    delwin(informations);

    ncurses_stop();
    close(sock_TCP_fd);
}


// Fonction principale qui permet de gérer la création et l'accès à une partie.
int main(int argc, char* argv[])
{
    // verification argument
    if(argc!=3){
        printf("Utilisation\n");
        printf("  ./client IP PORT\n");
        printf("    où :\n");
        printf("      - IP : adresse IP serveur\n");
        printf("      - PORT : numéro de port\n");

        perror("Nombre d'argument non valide");
        exit(EXIT_FAILURE);
    }

    int tailleTerminalX, tailleTerminalY;
    // ncurses initialisation
    setlocale(LC_ALL, "");
    ncurses_init();

    // Vérifie si la dimension du termnial est assez grande pour afficher l'éditeur
    if(verificationTailleTerminal(&tailleTerminalX, &tailleTerminalY)){
        ncurses_stop();
        printf("Les dimension minimal du terminal doivent être de 27x77\nOr votre terminal fait %dx%d\n", tailleTerminalY, tailleTerminalX);

        exit(EXIT_FAILURE);
    }
    ncurses_stop();


    int sock_UDP_fd;
    int port_TCP;
    int stop=0, choix_join_partie=0, validation_join=0;
    char c;

    struct sockaddr_in server_UDP_address;

    game_request_t game_request;
    info_map_t infoMap;
    info_game_t infoGame;


    afficher_nom_jeu();



    // Create socket
    if((sock_UDP_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Fill server address
    memset(&server_UDP_address, 0, sizeof(struct sockaddr_in));
    server_UDP_address.sin_family = AF_INET;
    server_UDP_address.sin_port = htons(atoi(argv[2]));
    if(inet_pton(AF_INET, argv[1], &server_UDP_address.sin_addr) != 1) {
        perror("Error converting address");
        exit(EXIT_FAILURE);
    }


    do
    {
        printf("1. Créer une partie\n");
        printf("2. Rejoindre une partie\n");
        printf("0. Quitter\n");

        printf("Votre choix : ");
        if(scanf("%d", &game_request.type)!=1)
        {
            while(((c = getchar()) != '\n') || (c == EOF));
            fprintf(stderr, "Problème lors du choix du menu\n");
            exit(EXIT_FAILURE);
        }

        game_request.idMap=-1;
        game_request.nbJoueurMax=-1;
        game_request.game_choice=-1;
        game_request.join_game = -1;

        if(game_request.type!=0)
        {
            // Send request
            if(sendto(sock_UDP_fd, &game_request, sizeof(game_request), 0, (struct sockaddr*)&server_UDP_address, sizeof(struct sockaddr_in)) == -1) {
                perror("Error sending message");
                exit(EXIT_FAILURE);
            }
        }
        else
            stop=1;

        if(game_request.type==CREATION_REQUEST)
        {
            // Wait for response
            if(recvfrom(sock_UDP_fd, &infoMap, sizeof(infoMap), 0, NULL, 0) == -1) {
                perror("Error receiving response");
                exit(EXIT_FAILURE);
            }

            do
            {
                printf("\nVeuillez choisir un monde parmis les suivants :\n");
                for(int i=0; i<infoMap.nbMap; i++)
                    printf("%d. %s\n", i, infoMap.nomsMaps[i]);

                printf("Votre Map : ");
                if(scanf("%d", &game_request.idMap)!=1){
                    while(((c = getchar()) != '\n') || (c == EOF));
                    fprintf(stderr, "Erreur lors du choix du monde\n");
                    exit(EXIT_FAILURE);
                }
            }while(game_request.idMap<0 || game_request.idMap>=infoMap.nbMap);
            

            do
            {
                printf("Choisissez le nombre de joueur maximum (1-10) : ");
                if(scanf("%d", &game_request.nbJoueurMax)!=1){
                    while(((c = getchar()) != '\n') || (c == EOF));
                    fprintf(stderr, "Erreur lors du choix du nombre de joueurs maximum\n");
                    exit(EXIT_FAILURE);
                }
            }while(game_request.nbJoueurMax<1 || game_request.nbJoueurMax>10);
            

            do
            {
                printf("\nVoulez-vous rejoindre cette partie ?\n");
                printf("1. Oui\n");
                printf("0. Non\n");
                printf("Votre choix : ");
                if(scanf("%d", &game_request.join_game)!=1){
                    while(((c = getchar()) != '\n') || (c == EOF));
                    fprintf(stderr, "Erreur lors du choix de validation pour rejoindre la partie\n");
                    exit(EXIT_FAILURE);
                }
            }while(game_request.join_game<0 || game_request.join_game>1);

            // Send request
            if(sendto(sock_UDP_fd, &game_request, sizeof(game_request), 0, (struct sockaddr*)&server_UDP_address, sizeof(struct sockaddr_in)) == -1) {
                perror("Error sending message");
                exit(EXIT_FAILURE);
            }

            if(game_request.join_game)
            {
                game_request.type=JOIN_REQUEST;
                validation_join=1;
            }
        }

        if(game_request.type==JOIN_REQUEST)
        {
            if(game_request.join_game==-1)
            {
                // Wait for response
                if(recvfrom(sock_UDP_fd, &infoGame, sizeof(infoGame), 0, NULL, 0) == -1) {
                    perror("Error receiving response");
                    exit(EXIT_FAILURE);
                }

                printf("\n\nVoici les différentes parties disponibles :\n");
                printf("0. Aucune\n");
                for(int i=0; i<infoGame.nbPartie; i++)
                    printf("%d. %s\t %d/%d\n", infoGame.tabPartie[i].id+1, infoGame.tabPartie[i].nomMonde, infoGame.tabPartie[i].nbJoueurConnecte, infoGame.tabPartie[i].nbJoueurMax);

                do
                {
                    printf("Choisissez une partie à rejoindre : ");
                    if(scanf("%d", &game_request.game_choice)!=1){
                        while(((c = getchar()) != '\n') || (c == EOF));
                        fprintf(stderr, "Erreur lors du choix de la partie\n");
                        exit(EXIT_FAILURE);
                    }

                    if(game_request.game_choice<0 || game_request.game_choice>infoGame.nbPartie)
                        choix_join_partie=0;
                    else
                    {
                        if(game_request.game_choice>0)
                        {
                            if(infoGame.tabPartie[game_request.game_choice-1].nbJoueurConnecte == infoGame.tabPartie[game_request.game_choice-1].nbJoueurMax)
                            {
                                printf("\nCette partie est déjà pleine.\n");
                                printf("Merci d'en sélectionner une autre\n\n");
                                choix_join_partie=0;
                            }
                            else
                            {
                                choix_join_partie=1;
                                game_request.game_choice--;
                                if(game_request.game_choice>=0)
                                    validation_join=1;
                                else
                                    validation_join=0;
                            }
                        }
                        else // ==0, cela corresond à revenir au menu principal
                        {
                            choix_join_partie=1;
                            game_request.game_choice--;
                            validation_join=0;
                        }
                    }

                }while(!choix_join_partie);
                
                if(validation_join)
                {
                    // Send request
                    if(sendto(sock_UDP_fd, &game_request, sizeof(game_request), 0, (struct sockaddr*)&server_UDP_address, sizeof(struct sockaddr_in)) == -1) {
                        perror("Error sending message");
                        exit(EXIT_FAILURE);
                    }

                    game_request.join_game = 1;
                }
            }

            if(validation_join)
            {
                // Wait for TCP port
                if(recvfrom(sock_UDP_fd, &port_TCP, sizeof(port_TCP), 0, NULL, 0) == -1) {
                    perror("Error receiving response");
                    exit(EXIT_FAILURE);
                }            

                if(game_request.join_game)
                {
                    jouer(argv[1], port_TCP);
                }
            }
            
        }

        printf("\n\n\n\n");

    }while(game_request.type<0 || game_request.type>2 || stop!=1);


    // Close socket
    if(close(sock_UDP_fd) == -1) {
        perror("Error closing socket");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}