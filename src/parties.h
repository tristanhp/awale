/* Parties en cours : une case de parties[] par partie, jusqu'a MAX_PARTIES en meme temps.
 * Comme pour les clients, on designe une partie par son INDICE. Le moteur (awale.c) reste
 * seul juge des coups ; ce module gere seulement qui joue et ce qu'on envoie aux joueurs.
 */

#ifndef PARTIES_H
#define PARTIES_H

#include "awale.h"

#define MAX_PARTIES 32

typedef struct
{
  int actif;      /* 0 = case libre */
  int joueur[2];  /* indices de clients : [0] = Sud (le defiant), [1] = Nord (le defie) */
  Awale jeu;
} Partie;

extern Partie parties[MAX_PARTIES];

void parties_init(void);

/* cree une partie entre le defiant a (Sud) et le defie b (Nord) : tire au sort qui commence,
 * met les deux clients EN_PARTIE et envoie EVT START puis EVT BOARD.
 * Retourne l'indice de la partie, ou -1 si la table est pleine (rien n'est modifie). */
int partie_creer(int a, int b);

/* envoie un message aux deux joueurs (point d'envoi unique : les observateurs s'y ajouteront) */
void partie_diffuser(int p, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* EVT BOARD <id> <12 cases> <score_sud> <score_nord> <tour> */
void partie_envoyer_plateau(int p);

/* termine la partie : EVT END aux deux, les joueurs redeviennent libres, la case est liberee.
 * gagnant = 0, 1 ou AWALE_NUL ; raison = "score", "blocage" ou "forfait".
 * Idempotente : sans effet si la partie est deja terminee. */
void partie_terminer(int p, int gagnant, const char *raison);

#endif
