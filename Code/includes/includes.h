#ifndef __INCLUDES_H__
#define __INCLUDES_H__


#define SERVER_UDP_PORT         12345

#define MAX_CHAR                  100
#define NB_MAP_MAX                 20
#define NB_PARTIES_MAX             20
#define NB_LEVELS_MAX             100
#define NB_MAX_PLAYER              10
#define NB_MAX_MOB                 10
#define NB_MAX_KEY                  4
#define NB_MAX_BOMB                 3

#define CREATION_REQUEST            1
#define JOIN_REQUEST                2



#define TABLE_MAX_SIZE             10
#define NB_LIGNE                   20
#define NB_COLONNE                 60

#define COULEUR_RED_BLACK           1
#define COULEUR_MAGENTA_BLACK       2
#define COULEUR_GREEN_BLACK         3
#define COULEUR_YELLOW_BLACK        4
#define COULEUR_BLUE_BLACK          5
#define COULEUR_CYAN_BLACK          6

#define DELETE_SIMPLE               0
#define BLOCK                       1
#define LADDER                      2
#define TRAP                        3
#define GATE                        4
#define KEY                         5
#define DOOR                        6
#define EXIT                        7
#define START                       8
#define ROBOT                       9
#define PROBE                      10
#define LIFE                       11
#define BOMB                       12
#define PLAYER                     13

#define TOUCHE_GAUCHE               1
#define TOUCHE_DROITE               2
#define TOUCHE_HAUT                 3
#define TOUCHE_BAS                  4
#define TOUCHE_BOMB                 5
#define QUITTER                     6
#define RESPAWN                     7

#define NORMAL                      1
#define FALLING                     2
#define TRANSITION_PORTE            3
#define SPAWN                       4
#define GOD_MODE                    5
#define PARALYSER                   6
#define MORT                        7

#define VISIBLE                     1
#define INVISIBLE                   0

#define DEFAULT_LIFE                5
#define DEFAULT_BOMB                0

#define INEXISTANTE                 0
#define ATTENTE                     1
#define EXPLOSION                   2


// Structure qui gère la partie
typedef struct partie_t{
    int id;                         // ID de la partie
    int nbJoueurMax;                // Nombre de joueur max dans la partie
    int nbJoueurConnecte;           // Nombre de joueur connecté à la partie
    char nomMonde[MAX_CHAR];        // Nom de la map
    int *tab_adresses_clients;      // Contient les adresses des clients
}partie_t;

// Structure envoyé du client vers serveur contenant les choix du client
typedef struct {
    int type;               // Type de la partie (création/rejoindre)
    int idMap;              // ID de la map
    int nbJoueurMax;        // Nombre de joueur max
    int game_choice;        // Choix du joueur
    int join_game;          // Permet de savoir si le joueur à rejoind la game ou non
}game_request_t;

// Structure contenant le nom de tout les fichiers (les maps) disponible
typedef struct {
    int nbMap;                                  // Nombre de map
    char nomsMaps[NB_MAP_MAX][MAX_CHAR];        // Nom des maps
}info_map_t;

// Structure qui contient les toutes le partie
typedef struct {
    int nbPartie;                               // Nombre de partie
    partie_t tabPartie[NB_PARTIES_MAX];         // Liste des parties
}info_game_t;


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


// Structure qui représente la map
typedef struct position_t{
    int posX;       // Position en abscisse
    int posY;       // Position en ordonné
} position_t;


// Structure qui représente la bombe
typedef struct bomb_t{
    position_t pos;         // Position de la BOMB
    int etat;               // État de la BOMB (EXPLOSION, ATTENTE, INEXISTANTE)
    int id_level;           // ID du level de la bomb
} bomb_t;


// Structure qui représente le joueur
typedef struct joueur_t{
    int id;                             // ID du joueur
    position_t pos;                     // Position du joueur
    int id_level;                       // Level sur lequel est le joueur
    int nb_Life;                        // Nombre de vie
    int nb_Bomb;                        // Nombre de bombe
    int got_key[NB_MAX_KEY];            // Clé récuperer [MAGENTA, VERT, JAUNE, BLEU], 0 si clé non possédé, 1 si clé possédé
    int dernier_mouvement;              // Dernière touche de l'utilisateur
    int etat;                           // État du joueur (NORMAL, FALLING, TRANSITION_PORTE)
    int statut;                         // Statut du joueur (NORMAL, GOD_MODE)
    bomb_t tab_bomb[NB_MAX_BOMB];       // Stock les bombes du joueurs
    pthread_mutex_t mutex_joueur;       // mutex du joueur
} joueur_t;


// Structure qui représente le robot
typedef struct robot_t{
    int id;                         // ID du robot
    position_t pos;                 // Position du robot
    int direction;                  // Gauche, droite
    int etat;                       // etat du robot
    pthread_mutex_t mutex_robot;    // mutex du robot
} robot_t;


// Structure qui représente le probe
typedef struct probe_t{
    int id;                         // ID du probe
    position_t pos;                 // Position du probe
    int direction;                  // Gauche, droite
    int etat;                       // etat du probe
    pthread_mutex_t mutex_probe;    // mutex du probe
} probe_t;


// Structure à passer en paramètre du pthread create pour avoir la map et l'adresse du joueur lié au thread
typedef struct {
    int id_partie;          // Id de la partie
    int nombre_map_total;   // le nombre de niveau dans la map
    int adresse_joueur;     // L'adresse TCP du joueur
}info_thread_player_t;


// Strcuture qui gère une bombe qui vient d'être posée
typedef struct {
    int id_partie;      // Id de la partie
    bomb_t* bombe;      // La bombe qui est posée
}info_thread_bomb_t;


// Structure à passer en paramètre du pthread create pour envoyer la map et le robot
typedef struct thread_map_robot_t{
    int id_partie;          // ID de la partie
    robot_t* robot;         // Le robot
    map_t* map;             // La map
} thread_map_robot_t;


// Structure à passer en paramètre du pthread create pour envoyer la map et le probe
typedef struct thread_map_probe_t{
    int id_partie;          // ID de la partie
    probe_t* probe;         // Le probe
    map_t* map;             // La map
} thread_map_probe_t;


// Structure à passer en paramètre du pthread create pour envoyer le tableau de trap
typedef struct thread_trap_t{
    int id_partie;              // ID de la partie
    case_t** tab_trap;          // Tableau contenant les TRAPS
    int nb_trap;                // Nombre total de TRAP
    int *tab_id_level_trap;     // ID du level de chaque TRAP
} thread_trap_t;



// Structure qui représente la game
typedef struct game_info_t{
    probe_t tab_probe[NB_LEVELS_MAX][NB_MAX_MOB];           // Tableau de probe
    robot_t tab_robot[NB_LEVELS_MAX][NB_MAX_MOB];           // Tableau de robot    
    joueur_t tab_joueur[NB_MAX_PLAYER];                     // Tableau de joueur
    int nb_joueur;                                          // Le nombre de joueurs

    map_t tab_map[NB_LEVELS_MAX];                           // Tableau de tous les levels de la map
    position_t pos_start;                                   // Position de la porte start
    int id_map_start;                                       // Level dans lequel se trouve la porte start
} game_info_t;


// Structure à passer en paramètre du pthread create pour envoyé le tableau de trap
typedef struct thread_affichage_client_t{
    map_t map;                              // Un level
    joueur_t tab_joueur[NB_MAX_PLAYER];     // Le tableau de joueur
    robot_t tab_robot[NB_MAX_MOB];          // Le tableau de robot
    probe_t tab_probe[NB_MAX_MOB];          // Le tableau de probe
    int nb_joueur;                          // Le nombre de joueur
    int id_joueur_actuel;                   // L'ID du joueur actuel
} thread_affichage_client_t;


// Structure qui est envoyé entre le client et le serveur pour gérer le déplacement
typedef struct {
    int id_joueur;      // ID du joueur
    int touche;         // Touche du joueur (GAUCHE, DROITE, HAUT, BAS, B, Q)
} deplacement_t;

#endif