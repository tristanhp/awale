/* Parties en cours : une case de parties[] par partie, jusqu'a MAX_PARTIES en meme temps.
 * Comme pour les clients, on designe une partie par son INDICE. Le moteur (awale.c) reste
 * seul juge des coups ; ce module gere seulement qui joue et ce qu'on envoie aux joueurs.
 */

#ifndef PARTIES_H
#define PARTIES_H

#include "awale.h"

#define MAX_PARTIES 32
#define MAX_OBSERVATEURS 8 /* par partie */

typedef struct
{
  int actif;      /* 0 = case libre */
  int joueur[2];  /* indices de clients : [0] = Sud (le defiant), [1] = Nord (le defie) */
  int observateur[MAX_OBSERVATEURS]; /* indices de clients, -1 = place libre */
  Awale jeu;
} Partie;

extern Partie parties[MAX_PARTIES];

void parties_init(void);

/* cree une partie entre le defiant a (Sud) et le defie b (Nord) : tire au sort qui commence,
 * met les deux clients EN_PARTIE et envoie EVT START puis EVT BOARD.
 * Retourne l'indice de la partie, ou -1 si la table est pleine (rien n'est modifie). */
int partie_creer(int a, int b);

/* envoie un message aux deux joueurs et aux observateurs (point d'envoi unique) */
void partie_diffuser(int p, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* EVT BOARD <id> <12 cases> <score_sud> <score_nord> <tour>
 * envoye aux deux joueurs (dest = -1) ou au seul client dest */
void partie_envoyer_plateau(int p, int dest);

/* ajoute le client c aux observateurs de la partie p (il passe ETAT_OBSERVATEUR) ;
 * 0 si OK, -1 si la partie a deja MAX_OBSERVATEURS observateurs */
int partie_ajouter_observateur(int p, int c);

/* retire le client c des observateurs de p (il repasse ETAT_LIBRE) ; sans effet s'il n'y est pas */
void partie_retirer_observateur(int p, int c);

/* termine la partie : EVT END aux deux, les joueurs et les observateurs redeviennent libres, la case est liberee.
 * gagnant = 0, 1 ou AWALE_NUL ; raison = "score", "blocage" ou "forfait".
 * Idempotente : sans effet si la partie est deja terminee. */
void partie_terminer(int p, int gagnant, const char *raison);

#endif
