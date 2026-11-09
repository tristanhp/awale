/* Parties en cours (voir parties.h) */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include "parties.h"
#include "clients.h"

Partie parties[MAX_PARTIES];

void parties_init(void)
{
  for (int p = 0; p < MAX_PARTIES; p++)
    parties[p].actif = 0;
}

void partie_diffuser(int p, const char *fmt, ...)
{
  char msg[MSG_MAX];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  for (int k = 0; k < 2; k++)
    client_envoyer(parties[p].joueur[k], "%s", msg);
}

void partie_envoyer_plateau(int p, int dest)
{
  const Awale *a = &parties[p].jeu;
  char msg[MSG_MAX];
  int pos = snprintf(msg, sizeof(msg), "EVT BOARD %d", p);
  for (int c = 0; c < AWALE_CASES; c++)
    pos += snprintf(msg + pos, sizeof(msg) - (size_t)pos, " %d", a->cases[c]);
  snprintf(msg + pos, sizeof(msg) - (size_t)pos, " %d %d %d %d", a->score[0], a->score[1], a->tour, a->fini);
  if (dest < 0)
    partie_diffuser(p, "%s", msg);
  else
    client_envoyer(dest, "%s", msg);
}

int partie_creer(int a, int b)
{
  for (int p = 0; p < MAX_PARTIES; p++)
  {
    if (parties[p].actif)
      continue;
    Partie *pt = &parties[p];
    pt->actif = 1;
    pt->joueur[0] = a;
    pt->joueur[1] = b;
    int premier = rand() % 2;
    awale_init(&pt->jeu, premier);

    clients[a].etat = ETAT_EN_PARTIE;
    clients[b].etat = ETAT_EN_PARTIE;
    clients[a].partie = clients[b].partie = p;
    clients[a].adversaire = b;
    clients[b].adversaire = a;

    partie_diffuser(p, "EVT START %d %s %s %s", p, clients[a].name, clients[b].name,
                    clients[pt->joueur[premier]].name);
    partie_envoyer_plateau(p, -1);
    return p;
  }
  return -1;
}

void partie_terminer(int p, int gagnant, const char *raison)
{
  Partie *pt = &parties[p];
  if (!pt->actif)
    return;
  pt->actif = 0; /* d'abord : si un envoi marque un client a_fermer, on ne reviendra pas ici */

  int a = pt->joueur[0], b = pt->joueur[1];
  double attendu = 1.0 / (1.0 + pow(10.0, ((double)clients[b].elo - clients[a].elo) / 400.0));
  double resultat = gagnant == 0 ? 1.0 : (gagnant == 1 ? 0.0 : 0.5);
  int variation = (int)round(32.0 * (resultat - attendu));
  /* Une partie decisive fait toujours evoluer les scores entiers. */
  if (variation == 0 && gagnant != AWALE_NUL)
    variation = gagnant == 0 ? 1 : -1;
  /* Evite tout debordement, meme apres ELO INT_MIN ou INT_MAX. */
  double nouveau_a = (double)clients[a].elo + variation;
  double nouveau_b = (double)clients[b].elo - variation;
  clients[a].elo = nouveau_a > INT_MAX ? INT_MAX : (nouveau_a < INT_MIN ? INT_MIN : (int)nouveau_a);
  clients[b].elo = nouveau_b > INT_MAX ? INT_MAX : (nouveau_b < INT_MIN ? INT_MIN : (int)nouveau_b);

  char nom[PSEUDO_MAX + 4];
  if (gagnant == 0 || gagnant == 1)
    snprintf(nom, sizeof(nom), "%s", clients[pt->joueur[gagnant]].name);
  else
    snprintf(nom, sizeof(nom), "NUL");

  for (int k = 0; k < 2; k++)
  {
    int c = pt->joueur[k];
    client_envoyer(c, "EVT END %d %s %d %d %s", p, nom, pt->jeu.score[0], pt->jeu.score[1], raison);
    if (clients[c].sock >= 0 && clients[c].partie == p)
    {
      clients[c].etat = ETAT_LIBRE;
      clients[c].partie = -1;
      clients[c].adversaire = -1;
    }
  }
}
