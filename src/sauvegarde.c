/* Comptes des joueurs dans un fichier CSV (voir sauvegarde.h) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <errno.h>
#include "sauvegarde.h"
#include "mdp.h"
#include "clients.h" /* PSEUDO_MAX, pseudo_valide */

#define MAX_COMPTES 4096

typedef struct
{
  char pseudo[PSEUDO_MAX + 1];
  int elo;
  char sel[SEL_HEX];
  char hash[HASH_HEX];
} Compte;

static Compte table[MAX_COMPTES];
static int nb = 0;
static char chemin[1024] = "";

static int trouver(const char *pseudo)
{
  for (int k = 0; k < nb; k++)
    if (strcasecmp(table[k].pseudo, pseudo) == 0)
      return k;
  return -1;
}

int sauvegarde_charger(const char *fichier)
{
  snprintf(chemin, sizeof(chemin), "%s", fichier);
  nb = 0;

  FILE *f = fopen(chemin, "r");
  if (f == NULL)
    return 0; /* premier lancement : pas encore de fichier */

  char ligne[512];
  while (fgets(ligne, sizeof(ligne), f) != NULL)
  {
    ligne[strcspn(ligne, "\r\n")] = '\0';

    /* pseudo,elo,sel,hachage */
    char *champ[4];
    char *p = ligne;
    int n = 0;
    while (n < 4)
    {
      champ[n++] = p;
      char *virgule = strchr(p, ',');
      if (virgule == NULL)
        break;
      *virgule = '\0';
      p = virgule + 1;
    }
    if (n != 4 || strchr(champ[3], ',') != NULL)
      continue; /* en-tete, ancien format sans mot de passe, ligne abimee : ignoree */

    char *fin;
    errno = 0;
    long elo = strtol(champ[1], &fin, 10);
    if (!pseudo_valide(champ[0]) || fin == champ[1] || *fin != '\0' || errno == ERANGE ||
        elo < INT_MIN || elo > INT_MAX || !mdp_est_hex(champ[2], SEL_HEX - 1) ||
        !mdp_est_hex(champ[3], HASH_HEX - 1) || trouver(champ[0]) >= 0 || nb >= MAX_COMPTES)
      continue;

    snprintf(table[nb].pseudo, sizeof(table[nb].pseudo), "%.16s", champ[0]);
    table[nb].elo = (int)elo;
    snprintf(table[nb].sel, sizeof(table[nb].sel), "%s", champ[2]);
    snprintf(table[nb].hash, sizeof(table[nb].hash), "%s", champ[3]);
    nb++;
  }
  fclose(f);
  return nb;
}

int sauvegarde_compte_existe(const char *pseudo)
{
  return trouver(pseudo) >= 0;
}

/* reecrit tout le fichier : on ecrit dans un fichier temporaire puis on le renomme, pour ne
 * jamais laisser un fichier a moitie ecrit si le serveur est tue au mauvais moment */
static int ecrire_fichier(void)
{
  char temporaire[sizeof(chemin) + 8];
  snprintf(temporaire, sizeof(temporaire), "%s.tmp", chemin);

  FILE *f = fopen(temporaire, "w");
  if (f == NULL)
    return -1;
  fprintf(f, "pseudo,elo,sel,hachage\n");
  for (int k = 0; k < nb; k++)
    fprintf(f, "%s,%d,%s,%s\n", table[k].pseudo, table[k].elo, table[k].sel, table[k].hash);
  if (fclose(f) != 0)
  {
    remove(temporaire);
    return -1;
  }
  if (rename(temporaire, chemin) != 0)
  {
    remove(temporaire);
    return -1;
  }
  return 0;
}

int sauvegarde_creer_compte(const char *pseudo, const char *mdp, int elo)
{
  if (chemin[0] == '\0' || nb >= MAX_COMPTES || trouver(pseudo) >= 0)
    return -1;

  Compte *c = &table[nb];
  snprintf(c->pseudo, sizeof(c->pseudo), "%.16s", pseudo);
  c->elo = elo;
  mdp_nouveau_sel(c->sel);
  mdp_hacher(mdp, c->sel, c->hash);
  nb++;

  if (ecrire_fichier() < 0)
  {
    nb--; /* pas de compte fantome : il disparaitrait au prochain redemarrage */
    perror("[serveur] sauvegarde impossible");
    return -1;
  }
  return 0;
}

int sauvegarde_verifier(const char *pseudo, const char *mdp, int *elo, char *nom)
{
  int k = trouver(pseudo);
  if (k < 0)
  {
    /* compte inconnu : on fait quand meme le calcul, pour qu'on ne puisse pas deviner
     * quels pseudos existent en mesurant le temps de reponse */
    char sel[SEL_HEX] = "00000000000000000000000000000000";
    char hash[HASH_HEX];
    mdp_hacher(mdp, sel, hash);
    return 0;
  }
  if (!mdp_verifier(mdp, table[k].sel, table[k].hash))
    return 0;
  *elo = table[k].elo;
  snprintf(nom, PSEUDO_MAX + 1, "%s", table[k].pseudo);
  return 1;
}

int sauvegarde_enregistrer_elo(const char *pseudo, int elo)
{
  int k = trouver(pseudo);
  if (k < 0)
    return -1;
  table[k].elo = elo;
  if (ecrire_fichier() < 0)
  {
    perror("[serveur] sauvegarde impossible");
    return -1;
  }
  return 0;
}
