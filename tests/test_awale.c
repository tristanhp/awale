/* Tests unitaires du moteur d'Awale
 *    make test   (ou : tests/bin/test_awale)
 * Chaque CHECK affiche la ligne en cas d'echec ; le code de retour vaut 0 si tout passe.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/awale.h"

static int nb_tests = 0;
static int nb_echecs = 0;

#define CHECK(cond)                                            \
  do                                                           \
  {                                                            \
    nb_tests++;                                                \
    if (!(cond))                                               \
    {                                                          \
      nb_echecs++;                                             \
      printf("ECHEC %s:%d : %s\n", __FILE__, __LINE__, #cond); \
    }                                                          \
  } while (0)

/* plateau vide, Sud (joueur 0) a le trait */
static void plateau_vide(Awale *a)
{
  awale_init(a, 0);
  for (int i = 0; i < AWALE_CASES; i++)
    a->cases[i] = 0;
}

static int total_graines(const Awale *a)
{
  int t = a->score[0] + a->score[1];
  for (int i = 0; i < AWALE_CASES; i++)
    t += a->cases[i];
  return t;
}

static void test_init(void)
{
  Awale a;
  awale_init(&a, 1);
  CHECK(total_graines(&a) == 48);
  CHECK(a.tour == 1);
  CHECK(!a.fini);
  CHECK(awale_graines_camp(&a, 0) == 24 && awale_graines_camp(&a, 1) == 24);
  CHECK(awale_case_du_coup(0, 1) == 0 && awale_case_du_coup(1, 1) == 6);
  CHECK(awale_case_du_coup(1, 6) == 11);
  CHECK(awale_case_du_coup(0, 0) == -1 && awale_case_du_coup(0, 7) == -1);
}

static void test_plateau_vide(void)
{
  Awale a;
  plateau_vide(&a);
  CHECK(!awale_a_coup(&a, 0) && !awale_a_coup(&a, 1));
  CHECK(awale_jouer(&a, 0) == -1);
  CHECK(total_graines(&a) == 0);
}

static void test_semis_simple(void)
{
  Awale a;
  awale_init(&a, 0);
  CHECK(awale_jouer(&a, 0) == 0);
  CHECK(a.cases[0] == 0 && a.cases[1] == 5 && a.cases[4] == 5 && a.cases[5] == 4);
  CHECK(a.tour == 1);
  CHECK(total_graines(&a) == 48);
}

static void test_coups_illegaux(void)
{
  Awale a;
  awale_init(&a, 0);
  CHECK(awale_jouer(&a, 6) == -1); /* case de l'adversaire */
  CHECK(awale_jouer(&a, -1) == -1);
  CHECK(awale_jouer(&a, 12) == -1);
  CHECK(a.tour == 0 && a.cases[6] == 4); /* rien n'a change */
  a.cases[2] = 0;
  CHECK(awale_jouer(&a, 2) == -1); /* case vide */
}

static void test_capture_avec_remontee(void)
{
  Awale a;
  plateau_vide(&a);
  a.cases[5] = 2;
  a.cases[6] = 1;
  a.cases[7] = 2;
  a.cases[11] = 3;                /* pour que le camp Nord ne soit pas entierement capture */
  CHECK(awale_jouer(&a, 5) == 5); /* 3 en case 7 + 2 en case 6 */
  CHECK(a.score[0] == 5);
  CHECK(a.cases[6] == 0 && a.cases[7] == 0 && a.cases[11] == 3);
}

static void test_remontee_s_arrete(void)
{
  Awale a;
  plateau_vide(&a);
  a.cases[5] = 2;
  a.cases[7] = 2; /* devient 3 : capturee ; la case 6 devient 1 : la remontee s'arrete */
  a.cases[10] = 1;
  a.cases[1] = 1; /* Sud garde une graine : la partie continue */
  CHECK(awale_jouer(&a, 5) == 3);
  CHECK(a.cases[6] == 1 && a.cases[7] == 0);
}

static void test_pas_de_capture(void)
{
  Awale a;
  plateau_vide(&a);
  a.cases[5] = 1;
  a.cases[6] = 3; /* devient 4 : pas de capture */
  a.cases[11] = 1;
  CHECK(awale_jouer(&a, 5) == 0);
  CHECK(a.cases[6] == 4 && a.score[0] == 0);

  /* derniere graine dans son propre camp : jamais de capture */
  plateau_vide(&a);
  a.cases[0] = 1;
  a.cases[1] = 1; /* devient 2, mais c'est sa propre case */
  a.cases[8] = 2;
  CHECK(awale_jouer(&a, 0) == 0);
  CHECK(a.score[0] == 0 && a.cases[1] == 2);

  /* case adverse vide avant le coup : elle passe a 1, pas de capture */
  plateau_vide(&a);
  a.cases[5] = 1;
  a.cases[9] = 1;
  CHECK(awale_jouer(&a, 5) == 0);
}

static void test_capture_cote_nord(void)
{
  Awale a;
  plateau_vide(&a);
  a.tour = 1;
  a.cases[11] = 2; /* Nord : 11 -> 0, 1 */
  a.cases[0] = 1;
  a.cases[1] = 1;
  a.cases[4] = 5;
  CHECK(awale_jouer(&a, 11) == 4); /* 2 en case 1, 2 en case 0 */
  CHECK(a.score[1] == 4);
}

static void test_grand_chelem(void)
{
  Awale a;
  plateau_vide(&a);
  a.cases[5] = 2;
  a.cases[6] = 1;
  a.cases[7] = 2; /* tout le camp Nord serait capture */
  a.cases[1] = 1; /* Sud garde une graine : la partie continue */
  CHECK(awale_jouer(&a, 5) == 0);
  CHECK(a.score[0] == 0);
  CHECK(a.cases[6] == 2 && a.cases[7] == 3);
  CHECK(total_graines(&a) == 6);
}

static void test_gros_semis(void)
{
  Awale a;
  plateau_vide(&a);
  a.cases[0] = 12;
  a.cases[8] = 1; /* pour ne pas etre bloque */
  CHECK(awale_jouer(&a, 0) >= 0);
  CHECK(a.cases[0] == 0); /* la case d'origine est sautee */
  CHECK(a.cases[1] == 2); /* 12 graines : la case 1 recoit la 1re et la 12e */
  CHECK(a.cases[2] == 1 && a.cases[11] == 1);
  CHECK(total_graines(&a) == 13);
}

static void test_obligation_de_nourrir(void)
{
  Awale a;
  plateau_vide(&a);
  a.cases[0] = 1; /* ne peut pas atteindre le camp Nord */
  a.cases[5] = 1; /* atteint la case 6 */
  CHECK(!awale_coup_legal(&a, 0, 0));
  CHECK(awale_coup_legal(&a, 0, 5));
  CHECK(awale_jouer(&a, 0) == -1);
  CHECK(a.cases[0] == 1);
  CHECK(awale_jouer(&a, 5) == 0);
  CHECK(a.cases[6] == 1);
}

static void test_fin_par_victoire(void)
{
  Awale a;
  plateau_vide(&a);
  a.score[0] = 22;
  a.cases[5] = 2;
  a.cases[6] = 1;
  a.cases[7] = 2;
  a.cases[11] = 3;
  CHECK(awale_jouer(&a, 5) == 5);
  CHECK(a.score[0] == 27);
  CHECK(a.fini && a.gagnant == 0);
  CHECK(awale_jouer(&a, 11) == -1); /* plus de coup une fois la partie finie */
}

static void test_fin_par_blocage(void)
{
  Awale a;
  /* Nord vide son dernier camp vers Sud ; Sud ne peut alors plus nourrir Nord */
  plateau_vide(&a);
  a.tour = 1;
  a.cases[11] = 1;
  a.cases[0] = 3;
  a.score[0] = 20;
  a.score[1] = 21;
  CHECK(awale_jouer(&a, 11) == 0);
  CHECK(a.fini);
  CHECK(a.score[0] == 24 && a.score[1] == 21); /* Sud ramasse ses 4 graines */
  CHECK(a.gagnant == 0);
  CHECK(total_graines(&a) == 45);

  /* meme position avec un nul 24-24 */
  plateau_vide(&a);
  a.tour = 1;
  a.cases[11] = 1;
  a.cases[0] = 3;
  a.score[0] = 20;
  a.score[1] = 24;
  CHECK(awale_jouer(&a, 11) == 0);
  CHECK(a.fini && a.gagnant == AWALE_NUL);
  CHECK(a.score[0] == 24 && a.score[1] == 24);
}

static void test_fin_joueur_sans_graines(void)
{
  Awale a;
  /* Sud donne sa derniere graine ; Nord ne peut pas nourrir Sud (obligation) :
   * la partie s'arrete et Nord ramasse ce qui reste dans son camp */
  plateau_vide(&a);
  a.tour = 0;
  a.cases[5] = 1;
  a.cases[8] = 2;
  a.score[0] = 10;
  a.score[1] = 10;
  CHECK(awale_jouer(&a, 5) == 0);
  CHECK(a.fini);
  CHECK(a.score[1] == 10 + 3 && a.score[0] == 10);
  CHECK(a.gagnant == 1);
  CHECK(total_graines(&a) == 23);

  /* meme situation mais Nord peut nourrir Sud (case 11 atteint le camp Sud) : on continue */
  plateau_vide(&a);
  a.cases[5] = 1;
  a.cases[11] = 2;
  CHECK(awale_jouer(&a, 5) == 0);
  CHECK(!a.fini);
  CHECK(awale_coup_legal(&a, 1, 11) && !awale_coup_legal(&a, 1, 6));
}

static void test_sauvegarde(void)
{
  Awale a, b;
  const char *f = "test_awale_sauvegarde.tmp";
  awale_init(&a, 1);
  awale_jouer(&a, 6);
  a.score[0] = 3;
  CHECK(awale_sauver(&a, f) == 0);
  CHECK(awale_charger(&b, f) == 0);
  CHECK(memcmp(&a, &b, sizeof(a)) == 0);

  /* fichier corrompu ou inexistant : refus, sans toucher a la structure */
  FILE *fp = fopen(f, "w");
  fputs("AWALE 1\n 1 2 3 abc\n", fp);
  fclose(fp);
  Awale c = a;
  CHECK(awale_charger(&c, f) == -1);
  CHECK(memcmp(&a, &c, sizeof(a)) == 0);
  unlink(f);
  CHECK(awale_charger(&c, f) == -1);
  CHECK(awale_sauver(&a, "/repertoire/inexistant/x") == -1);
}

static void test_affichage(void)
{
  Awale a;
  char buf[1024];
  awale_init(&a, 0);
  awale_afficher(&a, buf, sizeof(buf));
  CHECK(strstr(buf, "Sud") != NULL && strstr(buf, "Nord") != NULL);
  CHECK(strstr(buf, "Score : Sud 0 - Nord 0") != NULL);

  awale_afficher_joueurs(&a, buf, sizeof(buf), "LET", "LeJ");
  CHECK(strstr(buf, "<- LET") != NULL && strstr(buf, "<- LeJ") != NULL);
  CHECK(strstr(buf, "Score : LET 0 - LeJ 0 | Tour : LET") != NULL);
  CHECK(strstr(buf, "Sud") == NULL && strstr(buf, "Nord") == NULL);
  a.tour = 1;
  awale_afficher_joueurs(&a, buf, sizeof(buf), "LET", "LeJ");
  CHECK(strstr(buf, "Tour : LeJ") != NULL);
  a.fini = 1;
  awale_afficher_joueurs(&a, buf, sizeof(buf), "LET", "LeJ");
  CHECK(strstr(buf, "Partie terminee") != NULL && strstr(buf, "Tour :") == NULL);

  /* tampons trop petits : jamais de debordement, toujours termine par \0 */
  char petit[20];
  memset(petit, 'x', sizeof(petit));
  awale_afficher(&a, petit, sizeof(petit));
  CHECK(strlen(petit) < sizeof(petit));
  char un[1] = {'x'};
  awale_afficher(&a, un, 1);
  CHECK(un[0] == '\0');
  awale_afficher(&a, buf, 0); /* ne doit rien faire */
}

/* parties aleatoires : le nombre total de graines ne change jamais, le moteur
 * n'accepte que des coups legaux et la plupart des parties se terminent */
static void test_parties_aleatoires(void)
{
  unsigned int graine = 12345;
  int terminees = 0;
  for (int partie = 0; partie < 2000; partie++)
  {
    Awale a;
    awale_init(&a, partie % 2);
    int coups = 0;
    while (!a.fini && coups < 3000)
    {
      int candidats[AWALE_PAR_JOUEUR], nb = 0;
      for (int k = 1; k <= AWALE_PAR_JOUEUR; k++)
      {
        int c = awale_case_du_coup(a.tour, k);
        if (awale_coup_legal(&a, a.tour, c))
          candidats[nb++] = c;
      }
      CHECK(nb > 0); /* partie non finie => le joueur au trait a un coup */
      if (nb == 0)
        break;
      graine = graine * 1103515245u + 12345u;
      int c = candidats[(graine >> 16) % (unsigned)nb];
      CHECK(awale_jouer(&a, c) >= 0);
      if (total_graines(&a) != 48)
      {
        CHECK(total_graines(&a) == 48);
        break;
      }
      coups++;
    }
    if (a.fini)
      terminees++;
  }
  CHECK(terminees > 1800);
}

int main(void)
{
  test_init();
  test_plateau_vide();
  test_semis_simple();
  test_coups_illegaux();
  test_capture_avec_remontee();
  test_remontee_s_arrete();
  test_pas_de_capture();
  test_capture_cote_nord();
  test_grand_chelem();
  test_gros_semis();
  test_obligation_de_nourrir();
  test_fin_par_victoire();
  test_fin_par_blocage();
  test_fin_joueur_sans_graines();
  test_sauvegarde();
  test_affichage();
  test_parties_aleatoires();

  printf("%d verifications, %d echec(s)\n", nb_tests, nb_echecs);
  return nb_echecs == 0 ? 0 : 1;
}
