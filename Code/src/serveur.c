#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>

#include <pthread.h>


#include "includes.h"


int stop = 0, stop_element[NB_PARTIES_MAX]={0};
game_info_t game_info[NB_PARTIES_MAX]; 


// affiche les parties
void afficher_liste_partie(partie_t* tab, int compteur_partie)
{
    for(int i=0; i<compteur_partie; i++)
    {
        printf("%d. %s\t %d/%d\n", tab[i].id, tab[i].nomMonde, tab[i].nbJoueurConnecte, tab[i].nbJoueurMax);
    }
    printf("\n");
}


// Créer et initialise une nouvelle partie
void creerPartie(int* id, partie_t* tabPartie, int nbJoueurMax, char* nomsMondes)
{
    partie_t* game = (partie_t*)malloc(sizeof(partie_t));

    game->id = *id;
    game->nbJoueurConnecte = 0;
    game->nbJoueurMax = nbJoueurMax;
    strcpy(game->nomMonde, nomsMondes);
    game->tab_adresses_clients = (int*)malloc(sizeof(int)*nbJoueurMax);

    tabPartie[*id] = *game;
    (*id)++;
}



// Récupération du nom des fichiers dans le sous dossier ./map
void recuperer_nom_map(char tab_fichier[NB_MAP_MAX][MAX_CHAR], int* nb_fichier){
    DIR *dir;
    struct dirent *entree;
    char *dir_path = "./Map";       // Chemin du sous-dossier à parcourir

    // Ouverture du sous-dossier
    dir = opendir(dir_path);
    if (dir == NULL) { 
        perror("Erreur lors de l'ouverture du sous-dossier.");
        exit(EXIT_FAILURE);
    }

    // Parcours des fichiers du sous-dossier
    while ((entree = readdir(dir)) != NULL) { 
        // Exclusion des fichier "." et ".."
        if (strcmp(entree->d_name, ".") == 0 || strcmp(entree->d_name, "..") == 0) {
            continue; 
        }

        // Récupération de l'extension du fichier
        char *extension = strrchr(entree->d_name, '.');

        // Enregistrement des fichier avec l'extension .bin
        if (extension != NULL && strcmp(extension, ".bin") == 0) {
            strcpy(tab_fichier[*nb_fichier], entree->d_name);
            (*nb_fichier)++;
        }
    }

    // Gestion d'erreur pour la fermeture du dossier
    if (closedir(dir) == -1) {
        perror("Erreur lors de la fermeture du sous-dossier.");
        exit(EXIT_FAILURE);
    }
}

// Récupère toute les levels d'une map
void lecture_fichier_map(char* nomFichier, map_t* all_map, int* nombre_map_total){

    // Ouverture du fichier
    char* nom_fichier = malloc(strlen(nomFichier) + 5);
    sprintf(nom_fichier, "Map/%s", nomFichier);
    int fd;
    if ((fd = open(nom_fichier, O_RDONLY)) == -1) {
        fprintf(stderr, "Impossible d'ouvrir le fichier %s : %s\n", nom_fichier, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Pacrours des tables d'adressages
    adresse_complete_t adresseComplete;
    int compteur_map=0;
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
            if(adresseComplete.tableAdressage[j].numeroMap!=-1){

                // Positionnement avant la map
                if (lseek(fd, adresseComplete.tableAdressage[j].position, SEEK_SET) == -1) {
                    perror("Erreur lors du positionnement dans le fichier.");
                    exit(EXIT_FAILURE);
                }

                // Lecture de la map
                if (read(fd, &all_map[compteur_map], sizeof(map_t)) == -1) {
                    perror("Erreur lors de la lecture de la map.");
                    exit(EXIT_FAILURE);
                }
                compteur_map++;
            }
        }
        // Si la map est trouvée, alors quitter
        if(adresseComplete.prochaine_table_existe==0){
            break;
        }
    }
    *nombre_map_total = compteur_map;
    
    // Fermeture du fichier
    if(close(fd) == -1){
        perror("Erreur lors de la fermeture du fichier");
        exit(EXIT_FAILURE);
    }
}

// Recherche sur quel level est le start
void recherche_start_map(map_t* all_map, int nombre_map_total, int* id_map_start, position_t* pos_start){
    for (int i = 0; i < nombre_map_total; i++){
        for(int j=0; j<NB_LIGNE; j++){
            for(int k=0; k<NB_COLONNE; k++){
                if(all_map[i].matrice[j][k].type==START){
                    *id_map_start=i;
                    pos_start->posY=j;
                    pos_start->posX=k;
                }
            }
        }
    }
}

// Routine pour le thread dédiée à la réaparitoin d'un item LIFE
// Fait réaparaître l'item LIFE après 10 secondes
void *routine_thread_life(void *arg){
    int *couleur = (int*) arg;
    sleep(10);
    *couleur = VISIBLE;
    return NULL;
}

// Crée le thread pour l'item LIFE
void thread_gestion_life(case_t *case_life){
    pthread_t thread_id;
    int *ptr_couleur = &case_life->couleur;

    if (pthread_create(&thread_id, NULL, routine_thread_life, ptr_couleur) != 0)
        fprintf(stderr, "Probleme creation thread\n");
}


// Routine pour le thread dédiée à la réaparitoin d'un item BOMB
// Fait réaparaître l'item BOMB après 10 secondes
void *routine_thread_item_bomb(void *arg){
    int *couleur = (int*) arg;
    sleep(10);
    *couleur = VISIBLE;
    return NULL;
}

// Crée le thread pour l'item BOMB
void thread_gestion_bomb(case_t *case_bomb){
    pthread_t thread_id_bomb;
    int *ptr_couleur = &case_bomb->couleur;

    if (pthread_create(&thread_id_bomb, NULL, routine_thread_item_bomb, ptr_couleur) != 0)
        fprintf(stderr, "Probleme creation thread\n");
}

// Routine pour le thread dédiée au statut du joueur
// Passe le joueur en état NORMAL après 3 secondes
void *routine_thread_statut_joueur(void *arg){
    int *statut = (int*) arg;
    sleep(3);
    *statut = NORMAL;
    return NULL;
}

// Permet de passer le joueur en GOD MODE et de crée le thread pour repasser le joueur en état NORMAL après 3 secondes
void activation_GOD_MODE(joueur_t* joueur){
    joueur->statut=GOD_MODE;
    joueur->etat=NORMAL;

    pthread_t thread_id_statut;
    int *ptr_statut = &joueur->statut;

    if (pthread_create(&thread_id_statut, NULL, routine_thread_statut_joueur, ptr_statut) != 0)
        fprintf(stderr, "Probleme creation thread\n");
}

// Vérifie si le joueur récupère un item LIFE ou BOMB
void verif_recup_item(joueur_t* joueur, int id_partie){
    // Parcours la hitbox du joueur
    for (int i=0; i<3; i++){
        for (int j=0; j<3; j++){
            // Vérifie si le joueur prend un item LIFE
            if((game_info[id_partie].tab_map[joueur->id_level].matrice[joueur->pos.posY-i][joueur->pos.posX+j].type == LIFE && game_info[id_partie].tab_map[joueur->id_level].matrice[joueur->pos.posY-i][joueur->pos.posX+j].couleur == VISIBLE)){
                if(joueur->nb_Life<5){
                    joueur->nb_Life=5;
                    game_info[id_partie].tab_map[joueur->id_level].matrice[joueur->pos.posY-i][joueur->pos.posX+j].couleur = INVISIBLE;
                    thread_gestion_life(&game_info[id_partie].tab_map[joueur->id_level].matrice[joueur->pos.posY-i][joueur->pos.posX+j]);
                }
            }
            // Vérifie si le joueur prend un item BOMB
            if((game_info[id_partie].tab_map[joueur->id_level].matrice[joueur->pos.posY-i][joueur->pos.posX+j].type == BOMB && game_info[id_partie].tab_map[joueur->id_level].matrice[joueur->pos.posY-i][joueur->pos.posX+j].couleur == VISIBLE)){
                if(joueur->nb_Bomb<3){
                    joueur->nb_Bomb=3;
                    game_info[id_partie].tab_map[joueur->id_level].matrice[joueur->pos.posY-i][joueur->pos.posX+j].couleur = INVISIBLE;
                    thread_gestion_bomb(&game_info[id_partie].tab_map[joueur->id_level].matrice[joueur->pos.posY-i][joueur->pos.posX+j]);
                }
            }
        }
    }
}



// Vérifie si le joueur peut monter
int peut_monter(joueur_t joueur, map_t map){
    // Si le joueur est sur une échelle
    if(map.matrice[joueur.pos.posY][joueur.pos.posX].type == LADDER){
        // S'il y a un BLOCK au dessus du joueur
        if(map.matrice[joueur.pos.posY-3][joueur.pos.posX].type == BLOCK || map.matrice[joueur.pos.posY-3][joueur.pos.posX+1].type == BLOCK || map.matrice[joueur.pos.posY-3][joueur.pos.posX+2].type == BLOCK)
            return 0;

        // S'il y a un TRAP VISIBLE au dessus du joueur
        if((map.matrice[joueur.pos.posY-3][joueur.pos.posX].type == TRAP && map.matrice[joueur.pos.posY-3][joueur.pos.posX].couleur == VISIBLE) || (map.matrice[joueur.pos.posY-3][joueur.pos.posX+1].type == TRAP && map.matrice[joueur.pos.posY-3][joueur.pos.posX+1].couleur == VISIBLE) || (map.matrice[joueur.pos.posY-3][joueur.pos.posX+2].type == TRAP && map.matrice[joueur.pos.posY-3][joueur.pos.posX+2].couleur == VISIBLE))
            return 0;

        return 1;
    }
    return 0;
}


// Vérifie si le joueur peut monter
int peut_descendre(joueur_t joueur, map_t map){
    // Si le joueur est sur une echelle ou qu'il y a une echelle en dessous de lui
    if(map.matrice[joueur.pos.posY][joueur.pos.posX].type == LADDER || map.matrice[joueur.pos.posY+1][joueur.pos.posX].type == LADDER){
        // S'il n'y a pas de BLOCK ou TRAP en dessous du joueur
        if(map.matrice[joueur.pos.posY+1][joueur.pos.posX].type == BLOCK || map.matrice[joueur.pos.posY+1][joueur.pos.posX+1].type == BLOCK || map.matrice[joueur.pos.posY+1][joueur.pos.posX+2].type == BLOCK || (map.matrice[joueur.pos.posY+1][joueur.pos.posX].type == TRAP && map.matrice[joueur.pos.posY+1][joueur.pos.posX].couleur == VISIBLE) || (map.matrice[joueur.pos.posY+1][joueur.pos.posX+1].type == TRAP && map.matrice[joueur.pos.posY+1][joueur.pos.posX+1].couleur == VISIBLE) || (map.matrice[joueur.pos.posY+1][joueur.pos.posX+2].type == TRAP && map.matrice[joueur.pos.posY+1][joueur.pos.posX+2].couleur == VISIBLE)){
            return 0;
        }
        return 1;
    }
    return 0;
}


// Vérifie si le joueur peut aller à gauche
int aller_a_gauche(joueur_t* joueur, map_t* map){

    // Vérifie si le joueur est sur une échelle
    if(map->matrice[joueur->pos.posY][joueur->pos.posX].type == LADDER){
        if(map->matrice[joueur->pos.posY+1][joueur->pos.posX-1].type != BLOCK){
            return 0;
        }
    }

    // Vérifie si le joueur est bloqué par un GATE
    if(map->matrice[joueur->pos.posY][joueur->pos.posX-1].type == GATE || map->matrice[joueur->pos.posY-1][joueur->pos.posX-1].type == GATE || map->matrice[joueur->pos.posY-2][joueur->pos.posX-1].type == GATE){
        int compteur_ok=0;
        for(int i=0; i<3; i++){
            switch(map->matrice[joueur->pos.posY-i][joueur->pos.posX-1].couleur){
                case COULEUR_MAGENTA_BLACK:
                    if(joueur->got_key[0]==1)
                        compteur_ok++;
                    break;

                case COULEUR_GREEN_BLACK:
                    if(joueur->got_key[1]==1)
                        compteur_ok++;
                    break;

                case COULEUR_YELLOW_BLACK:
                    if(joueur->got_key[2]==1)
                        compteur_ok++;
                    break;

                case COULEUR_BLUE_BLACK:
                    if(joueur->got_key[3]==1)
                        compteur_ok++;
                    break;

                default:
                    compteur_ok++;
                    break;
            }
        }

        if(compteur_ok==3)
            return 1;
        return 0;
    }


    // Vérifie si le joueur prend une KEY
    if(map->matrice[joueur->pos.posY][joueur->pos.posX-1].type == KEY || map->matrice[joueur->pos.posY-1][joueur->pos.posX-1].type == KEY || map->matrice[joueur->pos.posY-2][joueur->pos.posX-1].type == KEY){
        int couleur=-1;
        for(int i=0; i<3; i++){
            if(map->matrice[joueur->pos.posY-i][joueur->pos.posX-1].couleur != -1)
                couleur = map->matrice[joueur->pos.posY-i][joueur->pos.posX-1].couleur;
        }

        // Actualise le tableau de clé du joueur
        if(couleur == COULEUR_MAGENTA_BLACK)
            joueur->got_key[0] = 1;
        else if(couleur == COULEUR_GREEN_BLACK)
            joueur->got_key[1] = 1;
        else if(couleur == COULEUR_YELLOW_BLACK)
            joueur->got_key[2] = 1;
        else if(couleur == COULEUR_BLUE_BLACK)
            joueur->got_key[3] = 1;

        return 1;
    }

    // Vérifie si les cases à gauche du joueur sont des block (un mur)
    if(map->matrice[joueur->pos.posY][joueur->pos.posX-1].type == BLOCK || map->matrice[joueur->pos.posY-1][joueur->pos.posX-1].type == BLOCK || map->matrice[joueur->pos.posY-2][joueur->pos.posX-1].type == BLOCK){
        return 0;
    }

    // Vérifie si les cases à gauche du joueur sont des traps
    for(int i=0; i<3; i++){
        if(map->matrice[joueur->pos.posY-i][joueur->pos.posX-1].type == TRAP && map->matrice[joueur->pos.posY-i][joueur->pos.posX-1].couleur == VISIBLE)
            return 0;
    }

    // Change l'état du joueur s'il bouge après avoir pris une porte
    if(joueur->etat == TRANSITION_PORTE){
        joueur->etat = NORMAL;
    }

    return 1;
}

// Vérifie si le joueur peut aller à droite
int aller_a_droite(joueur_t* joueur, map_t* map){

    // Vérifie si le joueur est sur une échelle
    if(map->matrice[joueur->pos.posY][joueur->pos.posX].type == LADDER){
        if(map->matrice[joueur->pos.posY+1][joueur->pos.posX+3].type != BLOCK){
            return 0;
        }
    }

    // Vérifie si le joueur est bloqué par un GATE
    if(map->matrice[joueur->pos.posY][joueur->pos.posX+3].type == GATE || map->matrice[joueur->pos.posY-1][joueur->pos.posX+3].type == GATE || map->matrice[joueur->pos.posY-2][joueur->pos.posX+3].type == GATE){
        int compteur_ok=0;
        for(int i=0; i<3; i++){
            switch(map->matrice[joueur->pos.posY-i][joueur->pos.posX+3].couleur){
                case COULEUR_MAGENTA_BLACK:
                    if(joueur->got_key[0]==1)
                        compteur_ok++;
                    break;

                case COULEUR_GREEN_BLACK:
                    if(joueur->got_key[1]==1)
                        compteur_ok++;
                    break;

                case COULEUR_YELLOW_BLACK:
                    if(joueur->got_key[2]==1)
                        compteur_ok++;
                    break;

                case COULEUR_BLUE_BLACK:
                    if(joueur->got_key[3]==1)
                        compteur_ok++;
                    break;

                default:
                    compteur_ok++;
                    break;
            }
        }

        if(compteur_ok==3)
            return 1;
        return 0;
    }

    // Vérifie si le joueur prend une KEY
    if(map->matrice[joueur->pos.posY][joueur->pos.posX+3].type == KEY || map->matrice[joueur->pos.posY-1][joueur->pos.posX+3].type == KEY || map->matrice[joueur->pos.posY-2][joueur->pos.posX+3].type == KEY){
        int couleur=-1;
        for(int i=0; i<3; i++){
            if(map->matrice[joueur->pos.posY-i][joueur->pos.posX+3].couleur != -1)
                couleur = map->matrice[joueur->pos.posY-i][joueur->pos.posX+3].couleur;
        }

        // Actualise le tableau de clé du joueur
        if(couleur == COULEUR_MAGENTA_BLACK)
            joueur->got_key[0] = 1;
        else if(couleur == COULEUR_GREEN_BLACK)
            joueur->got_key[1] = 1;
        else if(couleur == COULEUR_YELLOW_BLACK)
            joueur->got_key[2] = 1;
        else if(couleur == COULEUR_BLUE_BLACK)
            joueur->got_key[3] = 1;

        return 1;
    }

    // Vérifie si les cases à gauche du joueur sont des block (un mur)
    if(map->matrice[joueur->pos.posY][joueur->pos.posX+3].type == BLOCK || map->matrice[joueur->pos.posY-1][joueur->pos.posX+3].type == BLOCK || map->matrice[joueur->pos.posY-2][joueur->pos.posX+3].type == BLOCK){
        return 0;
    }

    // Vérifie si les cases à gauche du joueur sont des traps
    for(int i=0; i<3; i++){
        if(map->matrice[joueur->pos.posY-i][joueur->pos.posX+3].type == TRAP && map->matrice[joueur->pos.posY-i][joueur->pos.posX+3].couleur == VISIBLE)
            return 0;
    }

    // Change l'état du joueur s'il bouge après avoir pris une porte
    if(joueur->etat == TRANSITION_PORTE){
        joueur->etat = NORMAL;
    }

    return 1;
}


int player_falling(joueur_t* joueur, map_t map, int touche){
    // Si le joueur est dans un état normal
    if(joueur->etat == NORMAL){
        // Si le joueur n'a pas de block sous ses pieds
        if(map.matrice[joueur->pos.posY+1][joueur->pos.posX].type == -1 && map.matrice[joueur->pos.posY+1][joueur->pos.posX+1].type == -1 && map.matrice[joueur->pos.posY+1][joueur->pos.posX+2].type == -1){
            if(map.matrice[joueur->pos.posY+1][joueur->pos.posX-1].type == LADDER || map.matrice[joueur->pos.posY+1][joueur->pos.posX-2].type == LADDER){
                // Si le joueur est sur le bord droit de l'échelle
            }else{
                joueur->etat=FALLING;
                return 1;
            }
        }
        
        // Si le joueur est sur un trap
        if(map.matrice[joueur->pos.posY+1][joueur->pos.posX].type == TRAP && map.matrice[joueur->pos.posY+1][joueur->pos.posX+1].type == TRAP && map.matrice[joueur->pos.posY+1][joueur->pos.posX+2].type == TRAP){
            // Si le trap est ouvert
            if(map.matrice[joueur->pos.posY+1][joueur->pos.posX].couleur == INVISIBLE && map.matrice[joueur->pos.posY+1][joueur->pos.posX+1].couleur == INVISIBLE && map.matrice[joueur->pos.posY+1][joueur->pos.posX+2].couleur == INVISIBLE){
                joueur->etat=FALLING;
                return 1;
            }
        }

        // Si le joueur est SUR des GATE
        if(map.matrice[joueur->pos.posY+1][joueur->pos.posX].type == GATE || map.matrice[joueur->pos.posY+1][joueur->pos.posX+1].type == GATE || map.matrice[joueur->pos.posY+1][joueur->pos.posX+2].type == GATE){
            int compteur=0;
            for(int i=0; i<3; i++)
                if(map.matrice[joueur->pos.posY+1][joueur->pos.posX+i].type == -1 || map.matrice[joueur->pos.posY+1][joueur->pos.posX+i].type == GATE)
                    compteur++;
                
            if(compteur == 3){
                int compteur_ok=0;
                for(int i=0; i<3; i++){
                    switch(map.matrice[joueur->pos.posY+1][joueur->pos.posX+i].couleur){
                        case COULEUR_MAGENTA_BLACK:
                            if(joueur->got_key[0]==1)
                                compteur_ok++;
                            break;

                        case COULEUR_GREEN_BLACK:
                            if(joueur->got_key[1]==1)
                                compteur_ok++;
                            break;

                        case COULEUR_YELLOW_BLACK:
                            if(joueur->got_key[2]==1)
                                compteur_ok++;
                            break;

                        case COULEUR_BLUE_BLACK:
                            if(joueur->got_key[3]==1)
                                compteur_ok++;
                            break;

                        default:
                            compteur_ok++;
                            break;
                    }
                }
                if(compteur_ok==3){
                    joueur->etat=FALLING;
                    return 1;
                }
            }
        }
    }
    // Vérifie si le joueur peut tomber
    else if(joueur->etat == FALLING){
        if((map.matrice[joueur->pos.posY+1][joueur->pos.posX].type == BLOCK   || map.matrice[joueur->pos.posY+1][joueur->pos.posX].type == GATE   || map.matrice[joueur->pos.posY+1][joueur->pos.posX].type == TRAP   || map.matrice[joueur->pos.posY+1][joueur->pos.posX].type == LADDER) 
        || (map.matrice[joueur->pos.posY+1][joueur->pos.posX+1].type == BLOCK || map.matrice[joueur->pos.posY+1][joueur->pos.posX+1].type == GATE || map.matrice[joueur->pos.posY+1][joueur->pos.posX+1].type == TRAP || map.matrice[joueur->pos.posY+1][joueur->pos.posX+1].type == LADDER)
        || (map.matrice[joueur->pos.posY+1][joueur->pos.posX+2].type == BLOCK || map.matrice[joueur->pos.posY+1][joueur->pos.posX+2].type == GATE || map.matrice[joueur->pos.posY+1][joueur->pos.posX+2].type == TRAP || map.matrice[joueur->pos.posY+1][joueur->pos.posX+2].type == LADDER)
        || (map.matrice[joueur->pos.posY+1][joueur->pos.posX-1].type == LADDER || map.matrice[joueur->pos.posY+1][joueur->pos.posX-2].type == LADDER))
        {
            if(joueur->statut == NORMAL){
                joueur->nb_Life--;
                activation_GOD_MODE(joueur);
            }
            joueur->etat = NORMAL;
        }else{
            return 1;
        }
    }

    if(pthread_mutex_unlock(&joueur->mutex_joueur)!=0)
        fprintf(stderr, "Error while unlocking the player mutex of the player n°%d when checking if falling\n", joueur->id);

    return 0;
}



// Vérifie si le joueur est sur une porte
void verif_changer_map(joueur_t* joueur, map_t* all_map, int nombre_map_total){
    // Si le joueur est sur une porte
    if(joueur->etat!=TRANSITION_PORTE && joueur->etat!=PARALYSER){
        if(all_map[joueur->id_level].matrice[joueur->pos.posY][joueur->pos.posX].type == DOOR){
            int numero_porte = all_map[joueur->id_level].matrice[joueur->pos.posY][joueur->pos.posX].numero;
            // Parcours les map pour trouver l'autre porte
            for (int i = 0; i < nombre_map_total; i++){
                // Parcours du level
                for(int j=0; j<NB_LIGNE; j++){
                    for(int k=0; k<NB_COLONNE; k++){
                        // Si une porte est trouvée
                        if(all_map[i].matrice[j][k].type==DOOR && all_map[i].matrice[j][k].numero == numero_porte){
                            if((j!=joueur->pos.posY || k!=joueur->pos.posX) || (j==joueur->pos.posY && k==joueur->pos.posX && i!=joueur->id_level)){
                                joueur->id_level = i;
                                joueur->pos.posY = j;
                                joueur->pos.posX = k;
                                joueur->etat = TRANSITION_PORTE;
                                return;
                            }
                        }
                    }
                }
            }
        }  
    }
}

// Vérifie si le joueur passe la porte EXIT
void verif_exit(joueur_t* joueur, map_t map, int id_partie, int nb_joueur){
    if(map.matrice[joueur->pos.posY][joueur->pos.posX].type == EXIT){
        memset(&game_info[id_partie].tab_map[1], -1, sizeof(map_t));

        case_t case_block = {BLOCK, -1, -1};
        case_t case_vide = {-1, -1, -1};

        // Retire les robots et probes du level temporairement
        for(int i=0; i<NB_MAX_MOB; i++){
            game_info[id_partie].tab_robot[1][i].pos.posX = -10;
            game_info[id_partie].tab_robot[1][i].pos.posY = -10;
            game_info[id_partie].tab_probe[1][i].pos.posX = -10;
            game_info[id_partie].tab_probe[1][i].pos.posY = -10;
        }

        // re-crée les bords du level
        for(int i=1; i<NB_COLONNE+1; i++){
            game_info[id_partie].tab_map[1].matrice[0][i-1] = case_block;
            game_info[id_partie].tab_map[1].matrice[NB_LIGNE-1][i-1] = case_block;
            if(i<NB_LIGNE+1){
                game_info[id_partie].tab_map[1].matrice[i-1][0] = case_block;
                game_info[id_partie].tab_map[1].matrice[i-1][NB_COLONNE-1] = case_block;
            }
        }

        //  Crée le level WINNER
        int winner_map[NB_LIGNE][NB_COLONNE] = {
            {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,1,1,1,0,0,1,0,0,0,1,0,0,1,0,0,0,1,0,0,1,1,1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,1,0,0,0,1,1,0,0,1,0,0,1,1,0,0,1,0,0,1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,1,0,0,1,0,0,1,0,0,0,1,0,0,0,1,0,1,0,1,0,0,1,0,1,0,1,0,0,1,1,1,0,0,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,1,0,1,0,1,0,1,0,0,0,1,0,0,0,1,0,0,1,1,0,0,1,0,0,1,1,0,0,1,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,1,1,0,0,1,0,0,0,1,0,0,1,0,0,0,1,0,0,1,1,1,1,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
            {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}};

        for(int i=0; i<NB_LIGNE; i++){
            for(int j=0; j<NB_COLONNE; j++){
                if(winner_map[i][j] == 1)
                    game_info[id_partie].tab_map[1].matrice[i][j] = case_block;
                else 
                    game_info[id_partie].tab_map[1].matrice[i][j] = case_vide;
            }
        }

        // Modifie les valeur du joueur pour le téléporter sur le level WINNER
        for(int i=0; i<nb_joueur; i++){
            game_info[id_partie].tab_joueur[i].id_level = 1;
            game_info[id_partie].tab_joueur[i].nb_Life = 5;
            game_info[id_partie].tab_joueur[i].nb_Bomb = 0;

            if(game_info[id_partie].tab_joueur[i].id==joueur->id)
            {
                game_info[id_partie].tab_joueur[i].pos.posX = 30;
                game_info[id_partie].tab_joueur[i].pos.posY = 11;
            }
            else
            {
                game_info[id_partie].tab_joueur[i].pos.posX = 30;
                game_info[id_partie].tab_joueur[i].pos.posY = 18;
            }

            for(int j=0;j<4;j++)
                game_info[id_partie].tab_joueur[i].got_key[j] = 1;

            game_info[id_partie].tab_joueur[i].etat = NORMAL;
            game_info[id_partie].tab_joueur[i].statut = NORMAL;
        }
    }
}

// Vérifie si le ROBOT peut se deplacer
int peut_se_deplacer_robot(thread_map_robot_t* thread_map_robot){
    switch(thread_map_robot->robot->direction){
        // S'il va vers la gauche
        case TOUCHE_GAUCHE:
            // Vérifie s'il y a un mur
            for(int i=0; i<4; i++){
                if(thread_map_robot->map->matrice[thread_map_robot->robot->pos.posX-i][thread_map_robot->robot->pos.posY-1].type == BLOCK){
                    thread_map_robot->robot->direction = TOUCHE_DROITE;
                    return 0;
                }
            }

            // Vérifie s'il y a un sol
            if(thread_map_robot->map->matrice[thread_map_robot->robot->pos.posX+1][thread_map_robot->robot->pos.posY-1].type != BLOCK){
                thread_map_robot->robot->direction = TOUCHE_DROITE;
                return 0;
            }
            break;

        // S'il va vers la gauche
        case TOUCHE_DROITE:
            // Vérifie s'il y a un mur
            for(int i=0; i<4; i++){
                if(thread_map_robot->map->matrice[thread_map_robot->robot->pos.posX-i][thread_map_robot->robot->pos.posY+3].type == BLOCK){
                    thread_map_robot->robot->direction = TOUCHE_GAUCHE;
                    return 0;
                }
            }
            // Vérifie s'il y a un sol
            if(thread_map_robot->map->matrice[thread_map_robot->robot->pos.posX+1][thread_map_robot->robot->pos.posY+3].type != BLOCK){
                thread_map_robot->robot->direction = TOUCHE_GAUCHE;
                return 0;
            }
            break;
    }
    return 1;
}

// Déplace le robot
void deplacement_robot(thread_map_robot_t* thread_map_robot){
    if(pthread_mutex_lock(&thread_map_robot->robot->mutex_robot)!=0)
        fprintf(stderr, "Error while locking the robot mutex when he want to move\n");

    // Déplacement aléatoire lors de l'apparition
    if(thread_map_robot->robot->etat==SPAWN){
        int direction=0;

        srand(time(NULL)*thread_map_robot->robot->pos.posX*thread_map_robot->robot->pos.posY);
        direction = (rand() % 2)+1;

        if(direction==TOUCHE_GAUCHE){
            thread_map_robot->robot->pos.posY--;
        }else if(direction==TOUCHE_DROITE){
            thread_map_robot->robot->pos.posY++;
        }

        thread_map_robot->robot->etat=NORMAL;
        thread_map_robot->robot->direction = direction;

    }if(thread_map_robot->robot->etat==NORMAL){
        // Déplacement de gauche à droite ou inversement
        switch(thread_map_robot->robot->direction){
            case TOUCHE_GAUCHE:
                if(peut_se_deplacer_robot(thread_map_robot)){
                    thread_map_robot->robot->pos.posY--;
                }else{
                    thread_map_robot->robot->pos.posY++;
                }
                break;

            case TOUCHE_DROITE:
                if(peut_se_deplacer_robot(thread_map_robot)){
                    thread_map_robot->robot->pos.posY++;
                }else{
                    thread_map_robot->robot->pos.posY--;
                }
                break;
        }
    }

    if(pthread_mutex_unlock(&thread_map_robot->robot->mutex_robot)!=0)
        fprintf(stderr, "Error while unlocking the robot mutex when he want to move\n");
}

// Routine qui fait déplacer le robot toute les 0.5 seconde
void *routine_thread_robot(void *arg) {
    thread_map_robot_t *thread_map_robot = (thread_map_robot_t *) arg;
    struct timespec temps = { 0, 500000000 }; // 500000000 = 0.5sec

    while(stop_element[thread_map_robot->id_partie]==0){
        if(thread_map_robot->robot->etat != PARALYSER)
            deplacement_robot(thread_map_robot);
        
        if(nanosleep(&temps, NULL) == -1) {
            perror("Erreur lors de l’initialisation de la pause ");
            exit(EXIT_FAILURE);
        }
    }
    free(thread_map_robot);
    return NULL;
}


// Déplace le probe
void deplacement_probe(thread_map_probe_t* thread_map_probe){
    int direction=0;

    srand(time(NULL)*thread_map_probe->probe->pos.posX*thread_map_probe->probe->pos.posY);
    direction = (rand() % 4)+1;

    if(pthread_mutex_lock(&thread_map_probe->probe->mutex_probe)!=0)
        fprintf(stderr, "Error while locking the probe mutex when he want to move\n");
    
    // Déplacement aléatoire
    switch(direction){
        case TOUCHE_GAUCHE:
            if(thread_map_probe->map->matrice[thread_map_probe->probe->pos.posX][thread_map_probe->probe->pos.posY-1].type != BLOCK && thread_map_probe->map->matrice[thread_map_probe->probe->pos.posX-1][thread_map_probe->probe->pos.posY-1].type != BLOCK){
                thread_map_probe->probe->pos.posY--;
            }
            break;

        case TOUCHE_DROITE:
            if(thread_map_probe->map->matrice[thread_map_probe->probe->pos.posX][thread_map_probe->probe->pos.posY+3].type != BLOCK && thread_map_probe->map->matrice[thread_map_probe->probe->pos.posX-1][thread_map_probe->probe->pos.posY+3].type != BLOCK){
                thread_map_probe->probe->pos.posY++;
            }
            break;

        case TOUCHE_HAUT:
            if(thread_map_probe->map->matrice[thread_map_probe->probe->pos.posX-2][thread_map_probe->probe->pos.posY].type != BLOCK && thread_map_probe->map->matrice[thread_map_probe->probe->pos.posX-2][thread_map_probe->probe->pos.posY+1].type != BLOCK && thread_map_probe->map->matrice[thread_map_probe->probe->pos.posX-2][thread_map_probe->probe->pos.posY+2].type != BLOCK){
                thread_map_probe->probe->pos.posX--;
            }
            break;

        case TOUCHE_BAS:
            if(thread_map_probe->map->matrice[thread_map_probe->probe->pos.posX+1][thread_map_probe->probe->pos.posY].type != BLOCK && thread_map_probe->map->matrice[thread_map_probe->probe->pos.posX+1][thread_map_probe->probe->pos.posY+1].type != BLOCK && thread_map_probe->map->matrice[thread_map_probe->probe->pos.posX+1][thread_map_probe->probe->pos.posY+2].type != BLOCK){
                thread_map_probe->probe->pos.posX++;
            }
            break;
    }

    if(pthread_mutex_unlock(&thread_map_probe->probe->mutex_probe)!=0)
        fprintf(stderr, "Error while unlocking the probe mutex when he want to move\n");
}

// Routine qui fait déplacer le probe toute les 0.5 seconde
void *routine_thread_probe(void *arg) {
    thread_map_probe_t *thread_map_probe = (thread_map_probe_t *) arg;
    struct timespec temps = { 0, 500000000 }; // 500000000 = 0.5sec

    while(stop_element[thread_map_probe->id_partie]==0){
        if(thread_map_probe->probe->etat != PARALYSER)
            deplacement_probe(thread_map_probe);
        
        if(nanosleep(&temps, NULL) == -1) {
            perror("Erreur lors de l’initialisation de la pause ");
            exit(EXIT_FAILURE);
        }
    }

    free(thread_map_probe);
    return NULL;
}

// Permet de compter et initialiser toutes les entitées d'une map
void compte_entite(int nombre_map_total, int id_partie, int *nb_robot, int *nb_probe, int* nb_trap){

    int compteur_id_robot=1,compteur_id_probe=1; // compteur du nombre total de monstres.
    int compteur_robot=0, compteur_probe=0; // compteur de monstre par level
    int compteur_trap=0;

    // Parcours de tout les levels de la map
    for(int i = 0; i < nombre_map_total; i++){
        // Parcours du level
        for(int j=0; j<NB_LIGNE; j++){
            for(int k=0; k<NB_COLONNE; k++){
                // Si un ROBOT est trouvé
                if(game_info[id_partie].tab_map[i].matrice[j][k].type==ROBOT){
                    game_info[id_partie].tab_map[i].matrice[j][k].type=-1;
                    robot_t robot = {compteur_id_robot, {j,k}, 0, SPAWN};

                    game_info[id_partie].tab_robot[i][compteur_robot] = robot;

                    compteur_id_robot++;
                    compteur_robot++;

                // Si un PROBE est trouvé
                }else if(game_info[id_partie].tab_map[i].matrice[j][k].type==PROBE){
                    game_info[id_partie].tab_map[i].matrice[j][k].type=-1;
                    probe_t probe = {compteur_id_probe, {j,k}, 0, NORMAL};

                    game_info[id_partie].tab_probe[i][compteur_probe] = probe;

                    compteur_id_probe++;
                    compteur_probe++;

                }
                // Si un TRAP est trouvé
                else if(game_info[id_partie].tab_map[i].matrice[j][k].type==TRAP){
                    game_info[id_partie].tab_map[i].matrice[j][k].couleur=VISIBLE;

                    compteur_trap++;
                }
            }
        }
        compteur_robot=0;
        compteur_probe=0;
    }
    *nb_robot=compteur_id_robot;
    *nb_probe=compteur_id_probe;
    *nb_trap=compteur_trap;
}


// Pemret d'initialiser tous les traps d'une map
void initialiser_trap(int nombre_map_total, thread_trap_t thread_trap, int id_partie){
    int compteur_trap=0;
    for(int i = 0; i < nombre_map_total; i++){
        for(int j=0; j<NB_LIGNE; j++){
            for(int k=0; k<NB_COLONNE; k++){
                if(game_info[id_partie].tab_map[i].matrice[j][k].type==TRAP){
                    thread_trap.tab_trap[compteur_trap]=&game_info[id_partie].tab_map[i].matrice[j][k];
                    thread_trap.tab_id_level_trap[compteur_trap] = i;
                    compteur_trap++;
                }
            }
        }
    }
}

// Routine de thread pour faire apparaître et disparaître les TRAPS
void *routine_thread_trap(void *arg) {
    thread_trap_t *thread_trap = (thread_trap_t *) arg;
    int id_partie = thread_trap->id_partie;

    int id_trouver=0;

    while(stop_element[id_partie]==0){
        // Permet de vérifier si un joueur est sur le même level que le trap
        for(int i=0; i<thread_trap->nb_trap; i++){
            for(int j=0; j<NB_MAX_PLAYER; j++){
                if(thread_trap->tab_id_level_trap[i] == game_info[id_partie].tab_joueur[j].id_level && game_info[id_partie].tab_joueur[j].id_level>=0 && game_info[id_partie].tab_joueur[j].pos.posX!=0 && game_info[id_partie].tab_joueur[j].pos.posY!=0){
                    id_trouver=1;
                    break;
                }
            }

            // Si un joueur est sur le même level qu'un trap
            if(id_trouver==1){
                if(thread_trap->tab_trap[i]->couleur == VISIBLE){
                    thread_trap->tab_trap[i]->couleur = INVISIBLE;
                }else{
                    thread_trap->tab_trap[i]->couleur = VISIBLE;
                }
            }
            id_trouver=0;
        }
        
        sleep(2);
    }

    return NULL;
}


// Vérifie si le joueur touche un robot ou un probe (hitbox) ou si le joueur est dans un trap lors de sa réapparition
int verif_perdre_vie(joueur_t* joueur, robot_t tab_robot[], probe_t tab_probe[], int id_partie){
    // Parcour tout les robots/probes
    for (int i = 0; i < NB_MAX_MOB; i++){
        if(tab_robot[i].etat != PARALYSER){
            if ((tab_robot[i].pos.posX<=(joueur->pos.posY+3) && tab_robot[i].pos.posX>=(joueur->pos.posY-2)) && 
                (tab_robot[i].pos.posY<=(joueur->pos.posX+2) && tab_robot[i].pos.posY>=(joueur->pos.posX-2))) {
                if(joueur->statut != GOD_MODE){
                    joueur->nb_Life--;
                    activation_GOD_MODE(joueur);
                }

                if(pthread_mutex_unlock(&joueur->mutex_joueur)!=0)
                {
                    fprintf(stderr, "Error while locking the player mutex\n");
                    exit(EXIT_FAILURE);
                }
                return 1;
            }
        }

        if(tab_probe[i].etat != PARALYSER){
            if ((tab_probe[i].pos.posX<=(joueur->pos.posY+1) && tab_probe[i].pos.posX>=(joueur->pos.posY-2)) && 
                (tab_probe[i].pos.posY<=(joueur->pos.posX+2) && tab_probe[i].pos.posY>=(joueur->pos.posX-2))) {
                if(joueur->statut != GOD_MODE){
                    joueur->nb_Life--;
                    activation_GOD_MODE(joueur);
                }


                if(pthread_mutex_unlock(&joueur->mutex_joueur)!=0)
                {
                    fprintf(stderr, "Error while locking the player mutex\n");
                    exit(EXIT_FAILURE);
                }
                return 1;
            }
        }
    }

    // tue le joueur s'il est sur la même case qu'un trap lors de la réapparition du trap
    for (int i=0; i<3; i++){
        for (int j=0; j<3; j++){
            if((game_info[id_partie].tab_map[joueur->id_level].matrice[joueur->pos.posY-i][joueur->pos.posX+j].type == TRAP && game_info[id_partie].tab_map[joueur->id_level].matrice[joueur->pos.posY-i][joueur->pos.posX+j].couleur == VISIBLE)){
                if(joueur->statut!=GOD_MODE){
                    return 2;
                }
            }
        }
    }

    if(pthread_mutex_unlock(&joueur->mutex_joueur)!=0)
    {
        fprintf(stderr, "Error while locking the player mutex\n");
        exit(EXIT_FAILURE);
    }
    return 0;
}


// Fait respawn le joueur sur le start
void respawn_joueur_start(joueur_t* joueur, position_t pos_start, int id_map_start){
    if(pthread_mutex_lock(&joueur->mutex_joueur)!=0)
    {
        fprintf(stderr, "Error while locking the player mutex\n");
        exit(EXIT_FAILURE);
    }

    joueur->pos.posX = pos_start.posX;
    joueur->pos.posY = pos_start.posY;
    joueur->id_level = id_map_start;
    joueur->nb_Bomb = 0;
    joueur->nb_Life = 5;
    for(int i=0;i<4;i++)
        joueur->got_key[i] = 0;
    joueur->etat = NORMAL;
    joueur->statut = NORMAL;

    if(pthread_mutex_unlock(&joueur->mutex_joueur)!=0)
    {
        fprintf(stderr, "Error while locking the player mutex\n");
        exit(EXIT_FAILURE);
    }
}


// Vérifie si le joueur est mort
void verif_joueur_mort(joueur_t* joueur, position_t pos_start, int id_map_start){
    if(joueur->nb_Life<=0){
        joueur->statut = MORT;
    }
}


// Routine de thread de la paralysie d'un monstre
void *routine_thread_paralyser(void *arg){
    int *etat = (int*) arg;
    sleep(5);
    *etat = NORMAL;
    return NULL;
}


// Permet de paralyser le monstre
void paralyser_entite(int *etat){
    pthread_t thread_id_paralyser_entite;

    *etat = PARALYSER;

    if (pthread_create(&thread_id_paralyser_entite, NULL, routine_thread_paralyser, etat) != 0)
        fprintf(stderr, "Probleme creation thread\n");
}


// Permet de vérifier s'il y a des joueurs/robots/probes dans le rayon de l'explosion
void verif_explosion(bomb_t *bomb, int id_partie){
    // Parcours tout les tableaux
    for(int i=0; i<NB_MAX_PLAYER; i++){
        // Joueurs
        if((game_info[id_partie].tab_joueur[i].pos.posX >= (bomb->pos.posX-5) && game_info[id_partie].tab_joueur[i].pos.posX <= (bomb->pos.posX+3)) && game_info[id_partie].tab_joueur[i].pos.posY == bomb->pos.posY-2 && game_info[id_partie].tab_joueur[i].id_level == bomb->id_level && game_info[id_partie].tab_joueur[i].statut != GOD_MODE){
            game_info[id_partie].tab_joueur[i].nb_Life--;
            paralyser_entite(&game_info[id_partie].tab_joueur[i].etat);
        }
        if((game_info[id_partie].tab_joueur[i].pos.posX >= (bomb->pos.posX-6) && game_info[id_partie].tab_joueur[i].pos.posX <= (bomb->pos.posX+4)) && game_info[id_partie].tab_joueur[i].pos.posY == bomb->pos.posY-1 && game_info[id_partie].tab_joueur[i].id_level == bomb->id_level && game_info[id_partie].tab_joueur[i].statut != GOD_MODE){
            game_info[id_partie].tab_joueur[i].nb_Life--;
            paralyser_entite(&game_info[id_partie].tab_joueur[i].etat);
        }
        if((game_info[id_partie].tab_joueur[i].pos.posX >= (bomb->pos.posX-7) && game_info[id_partie].tab_joueur[i].pos.posX <= (bomb->pos.posX+5)) && game_info[id_partie].tab_joueur[i].pos.posY == bomb->pos.posY && game_info[id_partie].tab_joueur[i].id_level == bomb->id_level && game_info[id_partie].tab_joueur[i].statut != GOD_MODE){
            game_info[id_partie].tab_joueur[i].nb_Life--;
            paralyser_entite(&game_info[id_partie].tab_joueur[i].etat);
        }
        if((game_info[id_partie].tab_joueur[i].pos.posX >= (bomb->pos.posX-7) && game_info[id_partie].tab_joueur[i].pos.posX <= (bomb->pos.posX+5)) && game_info[id_partie].tab_joueur[i].pos.posY == bomb->pos.posY+1 && game_info[id_partie].tab_joueur[i].id_level == bomb->id_level && game_info[id_partie].tab_joueur[i].statut != GOD_MODE){
            game_info[id_partie].tab_joueur[i].nb_Life--;
            paralyser_entite(&game_info[id_partie].tab_joueur[i].etat);
        }
        if((game_info[id_partie].tab_joueur[i].pos.posX >= (bomb->pos.posX-7) && game_info[id_partie].tab_joueur[i].pos.posX <= (bomb->pos.posX+5)) && game_info[id_partie].tab_joueur[i].pos.posY == bomb->pos.posY+2 && game_info[id_partie].tab_joueur[i].id_level == bomb->id_level && game_info[id_partie].tab_joueur[i].statut != GOD_MODE){
            game_info[id_partie].tab_joueur[i].nb_Life--;
            paralyser_entite(&game_info[id_partie].tab_joueur[i].etat);
        }
        if((game_info[id_partie].tab_joueur[i].pos.posX >= (bomb->pos.posX-6) && game_info[id_partie].tab_joueur[i].pos.posX <= (bomb->pos.posX+4)) && game_info[id_partie].tab_joueur[i].pos.posY == bomb->pos.posY+3 && game_info[id_partie].tab_joueur[i].id_level == bomb->id_level && game_info[id_partie].tab_joueur[i].statut != GOD_MODE){
            game_info[id_partie].tab_joueur[i].nb_Life--;
            paralyser_entite(&game_info[id_partie].tab_joueur[i].etat);
        }
        if((game_info[id_partie].tab_joueur[i].pos.posX >= (bomb->pos.posX-5) && game_info[id_partie].tab_joueur[i].pos.posX <= (bomb->pos.posX+3)) && game_info[id_partie].tab_joueur[i].pos.posY == bomb->pos.posY+4 && game_info[id_partie].tab_joueur[i].id_level == bomb->id_level && game_info[id_partie].tab_joueur[i].statut != GOD_MODE){
            game_info[id_partie].tab_joueur[i].nb_Life--;
            paralyser_entite(&game_info[id_partie].tab_joueur[i].etat);
        }
    }
    for(int i=0; i<NB_MAX_MOB; i++){
        // Robots

        if(pthread_mutex_lock(&game_info[id_partie].tab_robot[bomb->id_level][i].mutex_robot)!=0)
            fprintf(stderr, "Error while locking the robot mutex of the robot n°%d when checking if the bomb it him\n", i);

        if((game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY >= (bomb->pos.posX-5) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY <= (bomb->pos.posX+3)) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posX == bomb->pos.posY-2){
            paralyser_entite(&game_info[id_partie].tab_robot[bomb->id_level][i].etat);
        }
        if((game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY >= (bomb->pos.posX-6) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY <= (bomb->pos.posX+4)) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posX == bomb->pos.posY-1){
            paralyser_entite(&game_info[id_partie].tab_robot[bomb->id_level][i].etat);
        }
        if((game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY >= (bomb->pos.posX-7) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY <= (bomb->pos.posX+5)) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posX == bomb->pos.posY){
            paralyser_entite(&game_info[id_partie].tab_robot[bomb->id_level][i].etat);
        }
        if((game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY >= (bomb->pos.posX-7) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY <= (bomb->pos.posX+5)) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posX == bomb->pos.posY+1){
            paralyser_entite(&game_info[id_partie].tab_robot[bomb->id_level][i].etat);
        }
        if((game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY >= (bomb->pos.posX-7) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY <= (bomb->pos.posX+5)) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posX == bomb->pos.posY+2){
            paralyser_entite(&game_info[id_partie].tab_robot[bomb->id_level][i].etat);
        }
        if((game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY >= (bomb->pos.posX-6) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY <= (bomb->pos.posX+4)) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posX == bomb->pos.posY+3){
            paralyser_entite(&game_info[id_partie].tab_robot[bomb->id_level][i].etat);
        }
        if((game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY >= (bomb->pos.posX-5) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY <= (bomb->pos.posX+3)) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posX == bomb->pos.posY+4){
            paralyser_entite(&game_info[id_partie].tab_robot[bomb->id_level][i].etat);
        }
        if((game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY >= (bomb->pos.posX-5) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posY <= (bomb->pos.posX+3)) && game_info[id_partie].tab_robot[bomb->id_level][i].pos.posX == bomb->pos.posY+5){
            paralyser_entite(&game_info[id_partie].tab_robot[bomb->id_level][i].etat);
        }

        if(pthread_mutex_unlock(&game_info[id_partie].tab_robot[bomb->id_level][i].mutex_robot)!=0)
            fprintf(stderr, "Error while unlocking the robot mutex of the robot n°%d when checking if the bomb it him\n", i);


       
        // Probes
        if(pthread_mutex_lock(&game_info[id_partie].tab_probe[bomb->id_level][i].mutex_probe)!=0)
            fprintf(stderr, "Error while locking the probe mutex of the probe n°%d when checking if the bomb it him\n", i);

        if((game_info[id_partie].tab_probe[bomb->id_level][i].pos.posY >= (bomb->pos.posX-5) && game_info[id_partie].tab_probe[bomb->id_level][i].pos.posY <= (bomb->pos.posX+3)) && game_info[id_partie].tab_probe[bomb->id_level][i].pos.posX == bomb->pos.posY-2){
            paralyser_entite(&game_info[id_partie].tab_probe[bomb->id_level][i].etat);
        }
        if((game_info[id_partie].tab_probe[bomb->id_level][i].pos.posY >= (bomb->pos.posX-6) && game_info[id_partie].tab_probe[bomb->id_level][i].pos.posY <= (bomb->pos.posX+4)) && game_info[id_partie].tab_probe[bomb->id_level][i].pos.posX == bomb->pos.posY-1){
            paralyser_entite(&game_info[id_partie].tab_probe[bomb->id_level][i].etat);
        }
        if((game_info[id_partie].tab_probe[bomb->id_level][i].pos.posY >= (bomb->pos.posX-7) && game_info[id_partie].tab_probe[bomb->id_level][i].pos.posY <= (bomb->pos.posX+5)) && game_info[id_partie].tab_probe[bomb->id_level][i].pos.posX == bomb->pos.posY){
            paralyser_entite(&game_info[id_partie].tab_probe[bomb->id_level][i].etat);
        }
        if((game_info[id_partie].tab_probe[bomb->id_level][i].pos.posY >= (bomb->pos.posX-7) && game_info[id_partie].tab_probe[bomb->id_level][i].pos.posY <= (bomb->pos.posX+5)) && game_info[id_partie].tab_probe[bomb->id_level][i].pos.posX == bomb->pos.posY+1){
            paralyser_entite(&game_info[id_partie].tab_probe[bomb->id_level][i].etat);
        }
        if((game_info[id_partie].tab_probe[bomb->id_level][i].pos.posY >= (bomb->pos.posX-6) && game_info[id_partie].tab_probe[bomb->id_level][i].pos.posY <= (bomb->pos.posX+5)) && game_info[id_partie].tab_probe[bomb->id_level][i].pos.posX == bomb->pos.posY+2){
            paralyser_entite(&game_info[id_partie].tab_probe[bomb->id_level][i].etat);
        }
        if((game_info[id_partie].tab_probe[bomb->id_level][i].pos.posY >= (bomb->pos.posX-5) && game_info[id_partie].tab_probe[bomb->id_level][i].pos.posY <= (bomb->pos.posX+4)) && game_info[id_partie].tab_probe[bomb->id_level][i].pos.posX == bomb->pos.posY+3){
            paralyser_entite(&game_info[id_partie].tab_probe[bomb->id_level][i].etat);
        }

        if(pthread_mutex_unlock(&game_info[id_partie].tab_probe[bomb->id_level][i].mutex_probe)!=0)
            fprintf(stderr, "Error while unlocking the probe mutex of the probe n°%d when checking if the bomb it him\n", i);
    }
}

// Routine du thread pour attendre l'explosion de la BOMB
void *routine_thread_bombe(void *arg){
    info_thread_bomb_t* info_thread_bomb = (info_thread_bomb_t*)arg;
    bomb_t* bomb = info_thread_bomb->bombe;
    int id_partie = info_thread_bomb->id_partie;
    struct timespec temps = { 0, 500000000 }; // 500000000 = 0.5sec

    sleep(5);
    bomb->etat = EXPLOSION;
    verif_explosion(bomb, id_partie);

    if(nanosleep(&temps, NULL) == -1) {
        perror("Erreur lors de l’initialisation de la pause ");
        exit(EXIT_FAILURE);
    }

    // Supprime la bombe après explosion
    bomb->etat = INEXISTANTE;
    bomb->pos.posX = 0;
    bomb->pos.posY = 0;
    bomb->id_level = -1;
   
    return NULL;
}

// Crée un trhead pour une bombe quand elle est posée
void gestion_thread_bomb(bomb_t* bombe, int id_partie){
    pthread_t thread_id_bombe;
    bomb_t *ptr_bombe = bombe;

    info_thread_bomb_t* info_thread_bomb = (info_thread_bomb_t*)malloc(sizeof(info_thread_bomb_t));
    info_thread_bomb->id_partie = id_partie;
    info_thread_bomb->bombe = ptr_bombe;

    
    if (pthread_create(&thread_id_bombe, NULL, routine_thread_bombe, info_thread_bomb) != 0){
        fprintf(stderr, "Error creating bomb thread\n");
        exit(EXIT_FAILURE);
    }
}

// Permet crée une bombe lorsque le joueur presse la touche 'b'
void poser_bomb(joueur_t* joueur, int id_partie){
    if(joueur->nb_Bomb>0){
        for(int i=0; i<NB_MAX_BOMB; i++){
            if(joueur->tab_bomb[i].etat == INEXISTANTE){
                joueur->nb_Bomb--;
                joueur->tab_bomb[i].etat = ATTENTE;
                joueur->tab_bomb[i].pos.posX = joueur->pos.posX+1;
                joueur->tab_bomb[i].pos.posY = joueur->pos.posY;
                joueur->tab_bomb[i].id_level = joueur->id_level;

                gestion_thread_bomb(&joueur->tab_bomb[i], id_partie);
                break;
            }
        }
    }
}

// Crée la map de respawn
void create_map_respawn(map_t* respawn)
{
    case_t case_block = {BLOCK, -1, -1};
    case_t case_vide = {-1, -1, -1};
    int respawn_map[NB_LIGNE][NB_COLONNE] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,1,1,1,1,0,0,1,1,1,1,0,0,1,1,1,1,0,0,1,1,1,1,0,0,1,1,1,1,0,0,1,0,0,0,0,0,1,0,0,1,0,0,0,1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,1,0,0,1,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,1,0,0,1,0,0,1,0,0,1,0,0,0,0,0,1,0,0,1,1,0,0,1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,1,1,1,0,0,0,1,1,1,0,0,0,1,1,1,1,0,0,1,1,1,1,0,0,1,1,1,1,0,0,1,0,0,1,0,0,1,0,0,1,0,1,0,1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,1,0,0,1,0,0,1,0,0,0,0,0,0,0,0,1,0,0,1,0,0,0,0,0,1,0,0,1,0,0,1,0,1,0,1,0,1,0,0,1,0,0,1,1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,1,0,0,1,0,0,1,1,1,1,0,0,1,1,1,1,0,0,1,0,0,0,0,0,1,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,1,0,0,0,1,0,0,1,1,1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,1,0,0,0,1,0,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,1,1,1,1,1,0,0,1,1,1,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,1,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,1,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,1,1,1,1,1,0,0,1,1,1,1,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}};

    for(int i=0; i<NB_LIGNE; i++){
        for(int j=0; j<NB_COLONNE; j++){
            if(respawn_map[i][j] == 1)
                respawn->matrice[i][j] = case_block;
            else 
                respawn->matrice[i][j] = case_vide;
        }
    }
}



void* thread_joueur(void* arg)
{                              
    deplacement_t deplacement;
    map_t map_respawn;
    info_thread_player_t* info_player = (info_thread_player_t*)arg;

    int id_partie = info_player->id_partie;
    int adresse_joueur = info_player->adresse_joueur;
    int nombre_map_total = info_player->nombre_map_total;

    int fin=0;

    joueur_t tab_joueur_vide[NB_MAX_PLAYER];
    robot_t tab_robot_vide[NB_MAX_MOB];
    probe_t tab_probe_vide[NB_MAX_MOB];

    thread_affichage_client_t affichage_client;

    create_map_respawn(&map_respawn);

    struct timespec temps = { 0, 10000000 }; // 500000000 = 0.5sec
    while(fin==0)
    {
        if(read(adresse_joueur, &deplacement, sizeof(deplacement)) == -1) {
            perror("Error receiving player move");
            exit(EXIT_FAILURE);
        }

        if(game_info[id_partie].tab_joueur[deplacement.id_joueur].etat != PARALYSER){
            // Vérifie la touche du joueur
            switch(deplacement.touche){
                // Si le joueur fait flèche GAUCHE
                case TOUCHE_GAUCHE:
                    if(pthread_mutex_lock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                        fprintf(stderr, "Error while locking the player mutex of the player n°%d when moving left\n", deplacement.id_joueur);

                    if(aller_a_gauche(&game_info[id_partie].tab_joueur[deplacement.id_joueur], &game_info[id_partie].tab_map[game_info[id_partie].tab_joueur[deplacement.id_joueur].id_level])==1){
                        game_info[id_partie].tab_joueur[deplacement.id_joueur].pos.posX--;
                        game_info[id_partie].tab_joueur[deplacement.id_joueur].dernier_mouvement=TOUCHE_GAUCHE;
                    }

                    if(pthread_mutex_unlock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                        fprintf(stderr, "Error while unlocking the player mutex of the player n°%d when moving left\n", deplacement.id_joueur);
                    break;
                
                // Si le joueur fait flèche DROITE
                case TOUCHE_DROITE:
                    if(pthread_mutex_lock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                        fprintf(stderr, "Error while locking the player mutex of the player n°%d when moving right\n", deplacement.id_joueur);

                    if(aller_a_droite(&game_info[id_partie].tab_joueur[deplacement.id_joueur], &game_info[id_partie].tab_map[game_info[id_partie].tab_joueur[deplacement.id_joueur].id_level])==1){
                        game_info[id_partie].tab_joueur[deplacement.id_joueur].pos.posX++;
                        game_info[id_partie].tab_joueur[deplacement.id_joueur].dernier_mouvement=TOUCHE_DROITE;
                    }

                    if(pthread_mutex_unlock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                        fprintf(stderr, "Error while unlocking the player mutex of the player n°%d when moving right\n", deplacement.id_joueur);
                    break;

                // Si le joueur fait flèche HAUT
                case TOUCHE_HAUT:
                    if(pthread_mutex_lock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                        fprintf(stderr, "Error while locking the player mutex of the player n°%d when moving up\n", deplacement.id_joueur);

                    if(peut_monter(game_info[id_partie].tab_joueur[deplacement.id_joueur], game_info[id_partie].tab_map[game_info[id_partie].tab_joueur[deplacement.id_joueur].id_level])==1){
                        game_info[id_partie].tab_joueur[deplacement.id_joueur].pos.posY--;
                        game_info[id_partie].tab_joueur[deplacement.id_joueur].dernier_mouvement=TOUCHE_HAUT;
                    }

                    if(pthread_mutex_unlock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                        fprintf(stderr, "Error while unlocking the player mutex of the player n°%d when moving up\n", deplacement.id_joueur);
                    break;

                // Si le joueur fait flèche BAS
                case TOUCHE_BAS:
                   if(pthread_mutex_lock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                        fprintf(stderr, "Error while locking the player mutex of the player n°%d when moving down\n", deplacement.id_joueur);

                    if(peut_descendre(game_info[id_partie].tab_joueur[deplacement.id_joueur], game_info[id_partie].tab_map[game_info[id_partie].tab_joueur[deplacement.id_joueur].id_level])==1){
                        game_info[id_partie].tab_joueur[deplacement.id_joueur].pos.posY++;
                        game_info[id_partie].tab_joueur[deplacement.id_joueur].dernier_mouvement=TOUCHE_BAS;
                    }

                    if(pthread_mutex_unlock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                        fprintf(stderr, "Error while unlocking the player mutex of the player n°%d when moving down\n", deplacement.id_joueur);
                    break;

                // Si le joueur presse la touche 'b'
                case TOUCHE_BOMB:
                    if(pthread_mutex_lock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                        fprintf(stderr, "Error while locking the player mutex of the player n°%d when placing bomb\n", deplacement.id_joueur);

                    if(game_info[id_partie].tab_joueur[deplacement.id_joueur].dernier_mouvement != TOUCHE_BOMB){
                        poser_bomb(&game_info[id_partie].tab_joueur[deplacement.id_joueur], id_partie);
                        game_info[id_partie].tab_joueur[deplacement.id_joueur].dernier_mouvement=TOUCHE_BOMB;
                    }
                    
                    if(pthread_mutex_unlock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                        fprintf(stderr, "Error while unlocking the player mutex of the player n°%d when placing bomb\n", deplacement.id_joueur);

                    break;

                // Si le joueur presse la touche 'q'
                case QUITTER:
                    if(pthread_mutex_lock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                        fprintf(stderr, "Error while locking the player mutex of the player n°%d when leaving\n", deplacement.id_joueur);
                    
                    game_info[id_partie].tab_joueur[deplacement.id_joueur].dernier_mouvement=QUITTER;
                    
                    if(pthread_mutex_unlock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                        fprintf(stderr, "Error while unlocking the player mutex of the player n°%d when leaving\n", deplacement.id_joueur);
                    break;
                
                // Si le joueur est en train de respawn
                case RESPAWN:
                    respawn_joueur_start(&game_info[id_partie].tab_joueur[deplacement.id_joueur], game_info[id_partie].pos_start, game_info[id_partie].id_map_start);
                    break;

            }
        }
        // si le joueur quitte quand il est paralyser
        else if(game_info[id_partie].tab_joueur[deplacement.id_joueur].etat == PARALYSER){
            if(deplacement.touche == QUITTER)
            {
                if(pthread_mutex_lock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                    fprintf(stderr, "Error while locking the player mutex of the player n°%d when leaving\n", deplacement.id_joueur);

                game_info[id_partie].tab_joueur[deplacement.id_joueur].dernier_mouvement=QUITTER;

                if(pthread_mutex_unlock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                    fprintf(stderr, "Error while unlocking the player mutex of the player n°%d when leaving\n", deplacement.id_joueur);
            }
        }
        

        // Vérifie les règles du jeu
        if(game_info[id_partie].tab_joueur[deplacement.id_joueur].dernier_mouvement!=QUITTER)
        {
            // Vérifie si le joueur tombe
            if(pthread_mutex_lock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                fprintf(stderr, "Error while locking the player mutex of the player n°%d when cheking if the player is falling\n", deplacement.id_joueur);
            if(player_falling(&game_info[id_partie].tab_joueur[deplacement.id_joueur], game_info[id_partie].tab_map[game_info[id_partie].tab_joueur[deplacement.id_joueur].id_level], deplacement.touche)==1){
                game_info[id_partie].tab_joueur[deplacement.id_joueur].pos.posY++;
                deplacement.touche=-1;
            }
            if(pthread_mutex_unlock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                fprintf(stderr, "Error while unlocking the player mutex of the player n°%d when checking if the player is falling\n", deplacement.id_joueur);


            
            
            // Vérifie si le joueur récupère un item
            if(pthread_mutex_lock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                fprintf(stderr, "Error while locking the player mutex of the player n°%d when checking if the player take an item\n", deplacement.id_joueur);
            verif_recup_item(&game_info[id_partie].tab_joueur[deplacement.id_joueur], id_partie);
            if(pthread_mutex_unlock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                fprintf(stderr, "Error while locking the player mutex of the player n°%d when checking if the player take an item\n", deplacement.id_joueur);
            

            
            // Vérifie si le joueur est sur une porte
            if(pthread_mutex_lock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                fprintf(stderr, "Error while locking the player mutex of the player n°%d when checking if the player is changing map\n", deplacement.id_joueur);
            verif_changer_map(&game_info[id_partie].tab_joueur[deplacement.id_joueur], game_info[id_partie].tab_map, nombre_map_total);
            if(pthread_mutex_unlock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                fprintf(stderr, "Error while locking the player mutex of the player n°%d when checking if the player changing map\n", deplacement.id_joueur);


            // Vérifie si le joueur perd de la vie
            if(pthread_mutex_lock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                fprintf(stderr, "Error while locking the player mutex of the player n°%d when checking if the player lose life\n", deplacement.id_joueur);
            if(verif_perdre_vie(&game_info[id_partie].tab_joueur[deplacement.id_joueur], game_info[id_partie].tab_robot[game_info[id_partie].tab_joueur[deplacement.id_joueur].id_level], game_info[id_partie].tab_probe[game_info[id_partie].tab_joueur[deplacement.id_joueur].id_level], id_partie) == 2){

                game_info[id_partie].tab_joueur[deplacement.id_joueur].statut=MORT;
                game_info[id_partie].tab_joueur[deplacement.id_joueur].nb_Life=0;
            }
            if(pthread_mutex_unlock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                fprintf(stderr, "Error while unlocking the player mutex of the player n°%d when checking if the player lose life\n", deplacement.id_joueur);
            


            // Vérifie si le joueur est mort
            if(pthread_mutex_lock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                fprintf(stderr, "Error while locking the player mutex of the player n°%d when checking if the player is dead\n", deplacement.id_joueur);
            verif_joueur_mort(&game_info[id_partie].tab_joueur[deplacement.id_joueur], game_info[id_partie].pos_start, game_info[id_partie].id_map_start);
            if(pthread_mutex_unlock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                fprintf(stderr, "Error while unlocking the player mutex of the player n°%d when checking if the player id dead\n", deplacement.id_joueur);


            // Vérifie si le joueur passe la porte EXIT
            if(pthread_mutex_lock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                fprintf(stderr, "Error while locking the player mutex of the player n°%d when checking if the player is on the exit door\n", deplacement.id_joueur);
            verif_exit(&game_info[id_partie].tab_joueur[deplacement.id_joueur], game_info[id_partie].tab_map[game_info[id_partie].tab_joueur[deplacement.id_joueur].id_level], id_partie, game_info[id_partie].nb_joueur);
            if(pthread_mutex_unlock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                fprintf(stderr, "Error while unlocking the player mutex of the player n°%d when checking if the player is on the exit door\n", deplacement.id_joueur);




            // Si le joueur est en vie, on lui envoie les données actuelles sinon on efface les entités puis on envoie la map de respawn
            if(pthread_mutex_lock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                 fprintf(stderr, "Error while locking the player mutex of the player n°%d when checking if the player is alive before sending data\n", deplacement.id_joueur);

            if(game_info[id_partie].tab_joueur[deplacement.id_joueur].statut!=MORT)
            {
                affichage_client.map = game_info[id_partie].tab_map[game_info[id_partie].tab_joueur[deplacement.id_joueur].id_level];
                for(int i=0; i<game_info[id_partie].nb_joueur; i++)
                {
                    affichage_client.tab_joueur[i] = game_info[id_partie].tab_joueur[i];
                }

                for(int i=0; i<NB_MAX_MOB; i++)
                {
                    affichage_client.tab_robot[i] = game_info[id_partie].tab_robot[game_info[id_partie].tab_joueur[deplacement.id_joueur].id_level][i];
                    affichage_client.tab_probe[i] = game_info[id_partie].tab_probe[game_info[id_partie].tab_joueur[deplacement.id_joueur].id_level][i];
                }

                if(write(adresse_joueur, &affichage_client, sizeof(affichage_client)) == -1) {
                    perror("Error sending map to the player");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                game_info[id_partie].tab_joueur[deplacement.id_joueur].pos.posX=-10;
                game_info[id_partie].tab_joueur[deplacement.id_joueur].pos.posY=-10;
                game_info[id_partie].tab_joueur[deplacement.id_joueur].id_level=-1;

                
                memset(&tab_joueur_vide, -2, sizeof(tab_joueur_vide));
                tab_joueur_vide[deplacement.id_joueur].statut=MORT;
                memset(&tab_robot_vide, -2, sizeof(tab_robot_vide));
                memset(&tab_probe_vide, -2, sizeof(tab_probe_vide));



                affichage_client.map = map_respawn;
                for(int i=0; i<game_info[id_partie].nb_joueur; i++)
                {
                    affichage_client.tab_joueur[i] = tab_joueur_vide[i];
                }

                for(int i=0; i<NB_MAX_MOB; i++)
                {
                    affichage_client.tab_robot[i] = tab_robot_vide[i];
                    affichage_client.tab_probe[i] = tab_probe_vide[i];
                }

                if(write(adresse_joueur, &affichage_client, sizeof(affichage_client)) == -1) {
                    perror("Error sending map to the player");
                    exit(EXIT_FAILURE);
                }
            }
            if(pthread_mutex_unlock(&game_info[id_partie].tab_joueur[deplacement.id_joueur].mutex_joueur)!=0)
                fprintf(stderr, "Error while unlocking the player mutex of the player n°%d when checking if the player is alive before sending data\n", deplacement.id_joueur);


            

            if(nanosleep(&temps, NULL) == -1) {
                perror("Erreur lors de l’initialisation de la pause ");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            fin=1;

            // fait disparaitre le joueur l'interface lorsqu'il quitte.
            memset(&game_info[id_partie].tab_joueur[deplacement.id_joueur], -1, sizeof(game_info[id_partie].tab_joueur[deplacement.id_joueur]));
            // Close socket
            if(close(adresse_joueur) == -1) {
                perror("Error closing socket (2)");
                exit(EXIT_FAILURE);
            }
        }
    }
    return NULL;
}




// Gestionnaire de SIGCHLD lorsqu'un fils se termine
void handler_SIGCHLD(int signum)
{
    int status;
    pid_t pid;
    
    // Récupération du PID du processus fils qui a terminé
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            // printf("Processus fils %d terminé avec le code de retour %d\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            // printf("Processus fils %d terminé par un signal %d\n", pid, WTERMSIG(status));
        }
    }
}


// Gestionnaire lorsque le serveur est arrêté par un SIGINT
void handler(int signum) {
    printf("Stop request received.\n");
    pid_t pid;
    stop = 1;

    for(int i=0; i<NB_PARTIES_MAX; i++)
        stop_element[i]=1;

    // Wait for children end
    do {
        pid = waitpid(-1, NULL, WNOHANG);
    } while((pid != -1) || (errno == EINTR));
}




int main()
{
    int sock_UDP_fd, sock_TCP_fd=-1, sock_TCP_Client=-1;
    int compteur_partie=0, attributs, nb_map=0;
    int port_TCP;
    pid_t pid;
    char nomsMondes[NB_MAP_MAX][MAX_CHAR];

    partie_t tabPartie[NB_PARTIES_MAX];
    game_request_t game_request;
    info_game_t infoGame;
    info_map_t infoMap;

    struct sigaction action, act;

    struct sockaddr_in server_UDP_address, client_address, server_TCP_address, tmp_sock;
    socklen_t size = sizeof(struct sockaddr_in);
    

    recuperer_nom_map(nomsMondes, &nb_map);

    infoMap.nbMap=nb_map;
    for(int i=0; i<infoMap.nbMap; i++)
        strcpy(infoMap.nomsMaps[i], nomsMondes[i]);


    // Specify handler
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NODEFER | SA_RESTART;
    act.sa_handler = handler_SIGCHLD;
    if(sigaction(SIGCHLD, &act, NULL) == -1) {
        perror("Error positioning handler (1)");
        exit(EXIT_FAILURE);    
    }


    // Specify handler
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    action.sa_handler = handler;
    if(sigaction(SIGINT, &action, NULL) == -1) {
        perror("Error positioning handler (2)");
        exit(EXIT_FAILURE);    
    }




    // Create UDP socket
    if((sock_UDP_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Fill server address
    memset(&server_UDP_address, 0, sizeof(struct sockaddr_in));
    server_UDP_address.sin_family = AF_INET;
    server_UDP_address.sin_port = htons(SERVER_UDP_PORT);
    server_UDP_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sock_UDP_fd, (struct sockaddr*)&server_UDP_address, sizeof(struct sockaddr_in)) == -1) {
        perror("Error naming socket");
        exit(EXIT_FAILURE);
    }



    while(stop == 0)
    {
        // Waiting for the client choice
        printf("Wait for a request (CRTL + C to stop)\n");
        if(recvfrom(sock_UDP_fd, &game_request, sizeof(game_request), 0, (struct sockaddr*)&client_address, &size) == -1) {
            perror("Error receiving message");
            exit(EXIT_FAILURE);
        }
        
        if(game_request.type==CREATION_REQUEST)
        {   
            if(game_request.idMap==-1 && game_request.nbJoueurMax==-1)
            {
                // Send request
                if(sendto(sock_UDP_fd, &infoMap, sizeof(infoMap), 0, (struct sockaddr*)&client_address, sizeof(struct sockaddr_in)) == -1) {
                    perror("Error sending response");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                game_request.game_choice=compteur_partie; 
                creerPartie(&compteur_partie, tabPartie, game_request.nbJoueurMax, nomsMondes[game_request.idMap]);
                afficher_liste_partie(tabPartie, compteur_partie);
                if(game_request.join_game)
                    game_request.type=JOIN_REQUEST;


                if((sock_TCP_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
                    perror("Error creating socket");
                    exit(EXIT_FAILURE);
                }

                // Pour permettre que le accept soit non bloquant et en attente passive 
                attributs = fcntl(sock_TCP_fd, F_GETFL);
                fcntl(sock_TCP_fd, F_SETFL, attributs | O_NONBLOCK);


                memset(&server_TCP_address, 0, sizeof(struct sockaddr_in));
                server_TCP_address.sin_family = AF_INET;
                server_TCP_address.sin_addr.s_addr = htonl(INADDR_ANY);
                server_TCP_address.sin_port = htons(0);

                if(bind(sock_TCP_fd, (struct sockaddr*)&server_TCP_address, sizeof(struct sockaddr_in)) == -1) {
                    perror("Error naming socket");
                    exit(EXIT_FAILURE);
                }

                // Switch the socket in passive mode
                if(listen(sock_TCP_fd, 1) == -1) {
                    perror("Error switching socket in passive mode");
                    exit(EXIT_FAILURE);
                }
            }
        }


        // Un if pour que le créateur d'une partie passe dans la condition pour la rejoindre
        if(game_request.type==JOIN_REQUEST)
        {
            if(game_request.game_choice==-1)
            {
                // envoie du tableau de parties et de sa taille actuelle.
                infoGame.nbPartie = compteur_partie;
                for(int i=0; i<infoGame.nbPartie; i++)
                    infoGame.tabPartie[i] = tabPartie[i];

                if(sendto(sock_UDP_fd, &infoGame, sizeof(infoGame), 0, (struct sockaddr*)&client_address, sizeof(struct sockaddr_in)) == -1) {
                    perror("Error sending response");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                if(sock_TCP_fd != -1)
                {
                    if(getsockname(sock_TCP_fd, (struct sockaddr*) &tmp_sock, &size)==-1)
                    {
                        perror("Erreur lors de l'obtention du nom de la socket TCP");
                        exit(EXIT_FAILURE);
                    }
                    port_TCP = ntohs(tmp_sock.sin_port);


                    // Send TCP port to all players of this game
                    if(sendto(sock_UDP_fd, &port_TCP, sizeof(port_TCP), 0, (struct sockaddr*)&client_address, sizeof(struct sockaddr_in)) == -1) {
                        perror("Error sending response");
                        exit(EXIT_FAILURE);
                    }

                    
                    fd_set ensemble;
                    FD_ZERO(&ensemble);
                    FD_SET(sock_TCP_fd, &ensemble);

                    // attente de connexion de tous les joueurs et incrémentation du nombre de joueurs connectés
                    if(select(sock_TCP_fd + 1, &ensemble, NULL, NULL, NULL) < 0) {
                        perror("Error waiting for events");
                        exit(EXIT_FAILURE);
                    }


                    if(FD_ISSET(sock_TCP_fd, &ensemble)) {
                        // Wait for a connexion
                        printf("Server: waiting for a connexion...\n");
                        if((sock_TCP_Client = accept(sock_TCP_fd, NULL, NULL)) == -1) {
                            if(errno != EINTR) {
                                perror("Error waiting connexion");
                                exit(EXIT_FAILURE);
                            }
                        }
                        else
                        {
                            // sauvegarde de l'adresse du client
                            if(sock_TCP_Client!=-1)
                                tabPartie[game_request.game_choice].tab_adresses_clients[tabPartie[game_request.game_choice].nbJoueurConnecte] = sock_TCP_Client;

                            tabPartie[game_request.game_choice].nbJoueurConnecte++;
                            afficher_liste_partie(tabPartie, compteur_partie);

                            // si suffisament de joueurs ont rejoint, création de la socket TCP et du fils pour démarrer la partie.
                            if(tabPartie[game_request.game_choice].nbJoueurConnecte == tabPartie[game_request.game_choice].nbJoueurMax)
                            {
                                // fork
                                if((pid = fork()) == -1) {
                                    perror("Error creating child");
                                    exit(EXIT_FAILURE);
                                }

                                if(pid==0)
                                {
                                    // Close socket
                                    if(close(sock_TCP_fd) == -1) {
                                        perror("Error closing socket (1)");
                                        exit(EXIT_FAILURE);
                                    }

                                    int nombre_map_total=0, id_map_start=0, nb_trap, nb_robot=0, nb_probe=0, compteur_joueur=0;
                                    int id_partie = game_request.game_choice;
                                    int nb_joueur = tabPartie[game_request.game_choice].nbJoueurMax;
                                    pthread_t thread_id_trap;
                                    pthread_t* id_thread = (pthread_t*)malloc(sizeof(pthread_t)*nb_joueur);



                                    joueur_t* joueur;
                                    bomb_t bombe = {{0,0}, INEXISTANTE, -1};
                                    position_t pos_start = {-1, -1};
                                    
                                    info_thread_player_t* info_thread_player;
                                    thread_affichage_client_t affichage_client;
                                    affichage_client.nb_joueur = nb_joueur;

                                    thread_map_robot_t* thread_map_robot;
                                    thread_map_probe_t* thread_map_probe;
                                    thread_trap_t thread_trap;                                    
                                                                            
                                    // Lecture du fichier pour stocker la map choisie
                                    lecture_fichier_map(tabPartie[game_request.game_choice].nomMonde, game_info[id_partie].tab_map, &nombre_map_total);
                                    
                                    // Cherche le start sur la map
                                    recherche_start_map(game_info[id_partie].tab_map, nombre_map_total, &id_map_start, &pos_start);

                                    // initialise la partie
                                    game_info[id_partie].pos_start = pos_start;
                                    game_info[id_partie].id_map_start = id_map_start;
                                    game_info[id_partie].nb_joueur = nb_joueur;


                                    // compte les robots, probes et traps
                                    compte_entite(nombre_map_total, id_partie, &nb_robot, &nb_probe, &nb_trap);

                                    // create robot and probe threads
                                    pthread_t thread_id_robot[nombre_map_total][nb_robot];
                                    pthread_t thread_id_probe[nombre_map_total][nb_probe];

                                    for(int i = 0; i < nombre_map_total; i++){
                                        for(int j=0; j<NB_MAX_MOB; j++){
                                            if(game_info[id_partie].tab_robot[i][j].id!=0){
                                                thread_map_robot = (thread_map_robot_t *) malloc(sizeof(thread_map_robot_t));
                                                thread_map_robot->robot = &game_info[id_partie].tab_robot[i][j];
                                                thread_map_robot->map = &game_info[id_partie].tab_map[i];

                                                if(pthread_create(&thread_id_robot[i][j], NULL, routine_thread_robot, (void *) thread_map_robot)!=0){
                                                    fprintf(stderr, "Error creating robot threads\n");
                                                    exit(EXIT_FAILURE);
                                                }
                                            }

                                            if(game_info[id_partie].tab_probe[i][j].id!=0){
                                                thread_map_probe = (thread_map_probe_t *) malloc(sizeof(thread_map_probe_t));
                                                thread_map_probe->probe = &game_info[id_partie].tab_probe[i][j];
                                                thread_map_probe->map = &game_info[id_partie].tab_map[i];

                                                if(pthread_create(&thread_id_probe[i][j], NULL, routine_thread_probe, (void *) thread_map_probe)!=0){
                                                    fprintf(stderr, "Error creating probe threads\n");
                                                    exit(EXIT_FAILURE);
                                                }
                                            }
                                        }
                                    }



                                    // création du thread des traps
                                    thread_trap.id_partie = id_partie;
                                    thread_trap.nb_trap = nb_trap; 
                                    thread_trap.tab_trap = malloc(nb_trap * sizeof(case_t*));
                                    thread_trap.tab_id_level_trap = malloc(nb_trap * sizeof(case_t*));
                                    initialiser_trap(nombre_map_total, thread_trap, id_partie);

                                    if(pthread_create(&thread_id_trap, NULL, routine_thread_trap, &thread_trap)!=0){
                                        fprintf(stderr, "Probleme creating trap threads\n");
                                        exit(EXIT_FAILURE);
                                    }





                                    // Création du tableau de joueur et envoie l'id de joueur à chacun d'entre eux
                                    for(int i=0; i<nb_joueur; i++)
                                    {
                                        joueur = (joueur_t*)malloc(sizeof(joueur_t));
                                        joueur->id = compteur_joueur;
                                        joueur->pos = pos_start;
                                        joueur->id_level = id_map_start;
                                        joueur->nb_Life = DEFAULT_LIFE;
                                        joueur->nb_Bomb = DEFAULT_BOMB;
                                        joueur->dernier_mouvement = 0;
                                        joueur->etat = NORMAL;
                                        joueur->statut = NORMAL;

                                        for(int j=0; j<NB_MAX_KEY; j++)
                                            joueur->got_key[j] = 0;

                                        for(int j=0; j<NB_MAX_BOMB; j++)
                                            joueur->tab_bomb[j] = bombe;


                                        game_info[id_partie].tab_joueur[i] = *joueur;
                                        compteur_joueur++;
                                        if(write(tabPartie[id_partie].tab_adresses_clients[i], &i, sizeof(int))==-1)
                                        {
                                            perror("Error sending the player id to players");
                                            exit(EXIT_FAILURE);
                                        }
                                    }

                                    // envoie du tableau de joueurs et de la map au client
                                    for(int i=0; i<nb_joueur; i++)
                                        affichage_client.tab_joueur[i] = game_info[id_partie].tab_joueur[i];
                                    affichage_client.map = game_info[id_partie].tab_map[game_info[id_partie].tab_joueur[0].id_level];

                                    for(int i=0; i<nb_joueur; i++)
                                    {
                                        if(write(tabPartie[id_partie].tab_adresses_clients[i], &affichage_client, sizeof(affichage_client)) == -1) {
                                            perror("Error sending the player array to players");
                                            exit(EXIT_FAILURE);
                                        }
                                    }



                                    // création des threads joueurs

                                    for(int i=0; i<nb_joueur; i++)
                                    {
                                        info_thread_player = (info_thread_player_t*)malloc(sizeof(info_thread_player_t));
                                        info_thread_player->id_partie = id_partie;
                                        info_thread_player->nombre_map_total = nombre_map_total;
                                        info_thread_player->adresse_joueur = tabPartie[id_partie].tab_adresses_clients[i];
                                        
                                        if(pthread_create(&id_thread[i], NULL, thread_joueur, info_thread_player) == -1){
                                            fprintf(stderr, "Error creating threads\n");
                                            exit(EXIT_FAILURE);
                                        }
                                    }


                                    for(int i=0; i<nb_joueur; i++)
                                    {
                                        if(pthread_join(id_thread[i], NULL) != 0){
                                            fprintf(stderr, "Error joining players threads\n");
                                            exit(EXIT_FAILURE);
                                        }
                                    }
                                    


                                    // arret des threads trap et monstres
                                    stop_element[id_partie]=1;

                                    if(nb_trap>0)
                                    {
                                        if(pthread_join(thread_id_trap, NULL)!=0)
                                        {
                                            fprintf(stderr, "Error joining traps threads\n");
                                            exit(EXIT_FAILURE);
                                        }
                                    }

                                    printf("Server-child: done.\n");
                                    exit(EXIT_SUCCESS);
                                }
                                else
                                {
                                    for(int i=0; i<tabPartie[game_request.game_choice].nbJoueurMax; i++)
                                    {
                                        // Close socket
                                        if(close(tabPartie[game_request.game_choice].tab_adresses_clients[i]) == -1) {
                                            perror("Error closing socket (2)");
                                            exit(EXIT_FAILURE);
                                        }
                                    }
                                }
                                

                                // Close socket
                                if(close(sock_TCP_fd) == -1) {
                                    perror("Error closing socket (3)");
                                    exit(EXIT_FAILURE);
                                }
                            }
                        }
                    }
                }
            }
        }
    }


    // Close socket
    if(close(sock_UDP_fd) == -1) {
        perror("Error closing socket");
        exit(EXIT_FAILURE);
    }

    printf("Server: done.\n");
    return EXIT_SUCCESS;
}