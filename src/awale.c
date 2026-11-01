/* Moteur du jeu d'Awale (voir awale.h) */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "awale.h"

void awale_init(Awale *a, int premier)
{
  for (int i = 0; i < AWALE_CASES; i++)
    a->cases[i] = AWALE_GRAINES_DEPART;
  a->score[0] = a->score[1] = 0;
  a->tour = premier;
  a->fini = 0;
  a->gagnant = AWALE_NUL;
}

int awale_proprietaire(int c)
{
  return c / AWALE_PAR_JOUEUR;
}

int awale_case_du_coup(int joueur, int numero)
{
  if (numero < 1 || numero > AWALE_PAR_JOUEUR)
    return -1;
  return joueur * AWALE_PAR_JOUEUR + (numero - 1);
}

int awale_graines_camp(const Awale *a, int joueur)
{
  int total = 0;
  for (int i = 0; i < AWALE_PAR_JOUEUR; i++)
    total += a->cases[joueur * AWALE_PAR_JOUEUR + i];
  return total;
}

/* seme les graines de la case c ; retourne l'indice de la case ou tombe la derniere.
 * A partir de 12 graines on fait un tour complet : la case d'origine est sautee. */
static int semer(Awale *a, int c)
{
  int n = a->cases[c];
  int i = c;
  a->cases[c] = 0;
  while (n > 0)
  {
    i = (i + 1) % AWALE_CASES;
    if (i == c)
      continue;
    a->cases[i]++;
    n--;
  }
  return i;
}

int awale_coup_legal(const Awale *a, int joueur, int c)
{
  if (a->fini || c < 0 || c >= AWALE_CASES)
    return 0;
  if (awale_proprietaire(c) != joueur || a->cases[c] == 0)
    return 0;

  /* obligation de nourrir : si l'adversaire n'a plus rien, le coup doit lui donner des graines */
  if (awale_graines_camp(a, 1 - joueur) == 0)
  {
    Awale essai = *a;
    semer(&essai, c);
    if (awale_graines_camp(&essai, 1 - joueur) == 0)
      return 0;
  }
  return 1;
}

int awale_a_coup(const Awale *a, int joueur)
{
  for (int i = 0; i < AWALE_PAR_JOUEUR; i++)
    if (awale_coup_legal(a, joueur, joueur * AWALE_PAR_JOUEUR + i))
      return 1;
  return 0;
}

/* verifie les conditions de fin apres un coup ; a->tour est deja le prochain joueur */
static void verifier_fin(Awale *a, int joueur_precedent)
{
  if (a->score[joueur_precedent] >= AWALE_SCORE_VICTOIRE)
  {
    a->fini = 1;
    a->gagnant = joueur_precedent;
    return;
  }

  if (!awale_a_coup(a, a->tour))
  {
    /* blocage : chacun ajoute a son score les graines restees dans son camp */
    for (int j = 0; j < 2; j++)
      a->score[j] += awale_graines_camp(a, j);
    for (int i = 0; i < AWALE_CASES; i++)
      a->cases[i] = 0;
    a->fini = 1;
    if (a->score[0] > a->score[1])
      a->gagnant = 0;
    else if (a->score[1] > a->score[0])
      a->gagnant = 1;
    else
      a->gagnant = AWALE_NUL;
  }
}

int awale_jouer(Awale *a, int c)
{
  int joueur = a->tour;
  int adv = 1 - joueur;

  if (!awale_coup_legal(a, joueur, c))
    return -1;

  int derniere = semer(a, c);

  /* capture : on part de la derniere case et on remonte tant que c'est 2 ou 3 chez l'adversaire.
   * Le total est d'abord calcule sur une copie pour pouvoir annuler en cas de grand chelem. */
  int copie[AWALE_CASES];
  memcpy(copie, a->cases, sizeof(copie));
  int pris = 0;
  int i = derniere;
  while (awale_proprietaire(i) == adv && (copie[i] == 2 || copie[i] == 3))
  {
    pris += copie[i];
    copie[i] = 0;
    i = (i + AWALE_CASES - 1) % AWALE_CASES;
  }

  /* grand chelem : si la capture viderait tout le camp adverse, on ne capture rien */
  if (pris > 0 && pris == awale_graines_camp(a, adv))
    pris = 0;

  if (pris > 0)
  {
    memcpy(a->cases, copie, sizeof(copie));
    a->score[joueur] += pris;
  }

  a->tour = adv;
  verifier_fin(a, joueur);
  return pris;
}

/* ajoute du texte formate a buf a partir de la position *pos, sans depasser n */
static void ajouter(char *buf, size_t n, size_t *pos, const char *fmt, ...)
{
  if (*pos >= n)
    return;
  va_list ap;
  va_start(ap, fmt);
  int r = vsnprintf(buf + *pos, n - *pos, fmt, ap);
  va_end(ap);
  if (r < 0)
    return;
  *pos += (size_t)r;
  if (*pos > n - 1)
    *pos = n - 1;
}

void awale_afficher(const Awale *a, char *buf, size_t n)
{
  size_t pos = 0;
  if (n == 0)
    return;
  buf[0] = '\0';

  /* camp Nord en haut, affiche de la case 11 a la case 6 (coups numerotes 6 a 1) */
  ajouter(buf, n, &pos, "        ");
  for (int k = AWALE_PAR_JOUEUR; k >= 1; k--)
    ajouter(buf, n, &pos, " %2d  ", k);
  ajouter(buf, n, &pos, "  <- Nord\n");
  ajouter(buf, n, &pos, "      +----+----+----+----+----+----+\n");
  ajouter(buf, n, &pos, "      |");
  for (int i = AWALE_CASES - 1; i >= AWALE_PAR_JOUEUR; i--)
    ajouter(buf, n, &pos, " %2d |", a->cases[i]);
  ajouter(buf, n, &pos, "\n");
  ajouter(buf, n, &pos, "      +----+----+----+----+----+----+\n");
  ajouter(buf, n, &pos, "      |");
  for (int i = 0; i < AWALE_PAR_JOUEUR; i++)
    ajouter(buf, n, &pos, " %2d |", a->cases[i]);
  ajouter(buf, n, &pos, "\n");
  ajouter(buf, n, &pos, "      +----+----+----+----+----+----+\n");
  ajouter(buf, n, &pos, "        ");
  for (int k = 1; k <= AWALE_PAR_JOUEUR; k++)
    ajouter(buf, n, &pos, " %2d  ", k);
  ajouter(buf, n, &pos, "  <- Sud\n");
  ajouter(buf, n, &pos, "Score : Sud %d - Nord %d", a->score[0], a->score[1]);
  if (a->fini)
    ajouter(buf, n, &pos, " | Partie terminee\n");
  else
    ajouter(buf, n, &pos, " | Tour : %s\n", a->tour == 0 ? "Sud" : "Nord");
}

int awale_sauver(const Awale *a, const char *fichier)
{
  FILE *f = fopen(fichier, "w");
  if (f == NULL)
    return -1;
  fprintf(f, "AWALE 1\n");
  for (int i = 0; i < AWALE_CASES; i++)
    fprintf(f, "%d ", a->cases[i]);
  fprintf(f, "\n%d %d\n%d %d %d\n", a->score[0], a->score[1], a->tour, a->fini, a->gagnant);
  if (fclose(f) != 0)
    return -1;
  return 0;
}

int awale_charger(Awale *a, const char *fichier)
{
  FILE *f = fopen(fichier, "r");
  if (f == NULL)
    return -1;

  Awale lu;
  int version;
  int ok = (fscanf(f, "AWALE %d", &version) == 1 && version == 1);
  for (int i = 0; ok && i < AWALE_CASES; i++)
    ok = (fscanf(f, "%d", &lu.cases[i]) == 1 && lu.cases[i] >= 0 && lu.cases[i] <= 48);
  ok = ok && fscanf(f, "%d %d", &lu.score[0], &lu.score[1]) == 2;
  ok = ok && fscanf(f, "%d %d %d", &lu.tour, &lu.fini, &lu.gagnant) == 3;
  fclose(f);

  /* on ne fait pas confiance au contenu du fichier */
  ok = ok && lu.score[0] >= 0 && lu.score[0] <= 48 && lu.score[1] >= 0 && lu.score[1] <= 48;
  ok = ok && (lu.tour == 0 || lu.tour == 1) && (lu.fini == 0 || lu.fini == 1);
  ok = ok && lu.gagnant >= AWALE_NUL && lu.gagnant <= 1;
  if (!ok)
    return -1;

  *a = lu;
  return 0;
}
