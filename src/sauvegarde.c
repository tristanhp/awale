/* Persistance de l'Elo dans un fichier CSV (voir sauvegarde.h) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <errno.h>
#include "sauvegarde.h"
#include "clients.h" /* PSEUDO_MAX, pseudo_valide */

#define MAX_SAUVEGARDES 4096

typedef struct
{
  char pseudo[PSEUDO_MAX + 1];
  int elo;
} Enregistrement;

static Enregistrement table[MAX_SAUVEGARDES];
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

  char ligne[256];
  while (fgets(ligne, sizeof(ligne), f) != NULL)
  {
    ligne[strcspn(ligne, "\r\n")] = '\0';
    char *virgule = strchr(ligne, ',');
    if (virgule == NULL)
      continue; /* ligne sans virgule : ignoree */
    *virgule = '\0';

    char *fin;
    errno = 0;
    long elo = strtol(virgule + 1, &fin, 10);
    /* ignore l'en-tete "pseudo,elo", les pseudos invalides, les nombres faux et les doublons */
    if (!pseudo_valide(ligne) || fin == virgule + 1 || *fin != '\0' || errno == ERANGE ||
        elo < INT_MIN || elo > INT_MAX || trouver(ligne) >= 0 || nb >= MAX_SAUVEGARDES)
      continue;

    snprintf(table[nb].pseudo, sizeof(table[nb].pseudo), "%.16s", ligne);
    table[nb].elo = (int)elo;
    nb++;
  }
  fclose(f);
  return nb;
}

int sauvegarde_chercher(const char *pseudo, int *elo)
{
  int k = trouver(pseudo);
  if (k < 0)
    return 0;
  *elo = table[k].elo;
  return 1;
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
  fprintf(f, "pseudo,elo\n");
  for (int k = 0; k < nb; k++)
    fprintf(f, "%s,%d\n", table[k].pseudo, table[k].elo);
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

int sauvegarde_enregistrer(const char *pseudo, int elo)
{
  if (chemin[0] == '\0')
    return -1; /* sauvegarde_charger n'a pas ete appelee */

  int k = trouver(pseudo);
  if (k < 0)
  {
    if (nb >= MAX_SAUVEGARDES)
      return -1;
    k = nb++;
    snprintf(table[k].pseudo, sizeof(table[k].pseudo), "%s", pseudo);
  }
  table[k].elo = elo;

  if (ecrire_fichier() < 0)
  {
    perror("[serveur] sauvegarde impossible");
    return -1;
  }
  return 0;
}
