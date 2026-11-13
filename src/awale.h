/* Moteur du jeu d'Awale
 * Module PUR : aucune entree/sortie reseau, aucune dependance vers le reste du projet.
 * Regle retenue : voir docs/regles-awale.md
 *
 * Plateau : 12 cases (indices 0 a 11). Joueur 0 ("Sud") = cases 0 a 5,
 * joueur 1 ("Nord") = cases 6 a 11. On seme dans le sens des indices croissants.
 */

#ifndef AWALE_H
#define AWALE_H

#include <stddef.h>

#define AWALE_CASES 4
#define AWALE_PAR_JOUEUR 2
#define AWALE_GRAINES_DEPART 2
#define AWALE_SCORE_VICTOIRE 2 /* temporaire pour les essais ; valeur normale : 25 */
#define AWALE_NUL (-1)          /* valeur de gagnant en cas de match nul */

typedef struct
{
  int cases[AWALE_CASES];
  int score[2]; /* graines capturees par chaque joueur */
  int tour;     /* joueur dont c'est le tour (0 ou 1) */
  int fini;     /* 1 quand la partie est terminee */
  int gagnant;  /* 0, 1 ou AWALE_NUL ; significatif seulement si fini */
} Awale;

/* nouvelle partie ; premier = joueur qui commence (0 ou 1) */
void awale_init(Awale *a, int premier);

/* joueur qui possede la case c (0 ou 1) */
int awale_proprietaire(int c);

/* convertit un numero de coup (1 a 6, relatif au joueur) en indice de case (0 a 11) ;
 * retourne -1 si le numero est hors de 1..6 */
int awale_case_du_coup(int joueur, int numero);

/* nombre de graines presentes dans le camp d'un joueur */
int awale_graines_camp(const Awale *a, int joueur);

/* 1 si le joueur peut jouer la case c selon les regles (case a lui, non vide,
 * obligation de nourrir l'adversaire s'il n'a plus de graines) ; 0 sinon */
int awale_coup_legal(const Awale *a, int joueur, int c);

/* 1 si le joueur a au moins un coup legal */
int awale_a_coup(const Awale *a, int joueur);

/* joue la case c pour le joueur dont c'est le tour.
 * retourne le nombre de graines capturees (>= 0), ou -1 si le coup est illegal
 * (le plateau n'est alors pas modifie). Met a jour tour, fini et gagnant. */
int awale_jouer(Awale *a, int c);

/* ecrit le plateau en ASCII (plusieurs lignes) dans buf, sans depasser n octets */
void awale_afficher(const Awale *a, char *buf, size_t n);
/* Meme plateau avec les noms des deux joueurs (Sud puis Nord). */
void awale_afficher_joueurs(const Awale *a, char *buf, size_t n,
                           const char *sud, const char *nord);

/* sauvegarde / chargement d'une position dans un fichier texte ; 0 si OK, -1 sinon */
int awale_sauver(const Awale *a, const char *fichier);
int awale_charger(Awale *a, const char *fichier);

#endif
