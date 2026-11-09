/* Commandes du protocole (voir commandes.h) */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "commandes.h"
#include "clients.h"
#include "protocole.h"
#include "parties.h"

void cmd_nick(int i, char *args)
{
  if (clients[i].etat != ETAT_CONNECTE)
  {
    client_envoyer(i, "ERR deja_inscrit");
    return;
  }
  if (args[0] == '\0')
  {
    client_envoyer(i, "ERR syntaxe");
    return;
  }
  /* un pseudo contenant un espace ou un caractere special est refuse ici */
  if (!pseudo_valide(args))
  {
    client_envoyer(i, "ERR pseudo_invalide");
    return;
  }
  if (clients_chercher_pseudo(args) >= 0)
  {
    client_envoyer(i, "ERR pseudo_pris");
    return;
  }

  snprintf(clients[i].name, sizeof(clients[i].name), "%s", args);
  clients[i].etat = ETAT_LIBRE;
  client_envoyer(i, "OK NICK %s", clients[i].name);
  clients_diffuser(i, "EVT JOIN %s", clients[i].name);
}

void cmd_elo(int i, char *args)
{
  if (*args != '\0')
  {
    char *fin;
    errno = 0;
    long valeur = strtol(args, &fin, 10);
    if (fin == args || *fin != '\0' || errno == ERANGE || valeur < INT_MIN || valeur > INT_MAX)
    {
      client_envoyer(i, "ERR syntaxe");
      return;
    }
    clients[i].elo = (int)valeur;
  }
  client_envoyer(i, "OK ELO %s %d", clients[i].name, clients[i].elo);
}

void cmd_who(int i, char *args)
{
  (void)args;
  char msg[MSG_MAX];
  size_t pos = 0;

  pos += (size_t)snprintf(msg, sizeof(msg), "OK WHO");
  for (int k = 0; k < MAX_CLIENTS; k++)
  {
    if (clients[k].sock < 0 || !(clients[k].etat & ETAT_INSCRIT))
      continue;
    int r = snprintf(msg + pos, sizeof(msg) - pos, " %s:%s:%d", clients[k].name, etat_nom(clients[k].etat), clients[k].elo);
    if (r < 0 || (size_t)r >= sizeof(msg) - pos)
    {
      msg[pos] = '\0'; /* plus de place : on envoie ce qui tient (ne devrait pas arriver avec 64 clients) */
      break;
    }
    pos += (size_t)r;
  }
  client_envoyer(i, "%s", msg);
}

void cmd_help(int i, char *args)
{
  (void)args;
  char liste[256];
  protocole_lister(liste, sizeof(liste), clients[i].etat);
  client_envoyer(i, "OK HELP %s", liste);
}

void cmd_quit(int i, char *args)
{
  (void)args;
  client_envoyer(i, "OK QUIT");
  clients[i].a_fermer = 1; /* le serveur essaiera d'envoyer le "OK QUIT" avant de fermer */
}

void cmd_challenge(int i, char *args)
{
  if (args[0] == '\0')
  {
    client_envoyer(i, "ERR syntaxe");
    return;
  }

  int cible = clients_chercher_pseudo(args);
  if (cible < 0 || !(clients[cible].etat & ETAT_INSCRIT))
  {
    client_envoyer(i, "ERR joueur_inconnu");
    return;
  }
  if (cible == i)
  {
    client_envoyer(i, "ERR defi_soi");
    return;
  }
  if (clients[cible].etat != ETAT_LIBRE)
  {
    client_envoyer(i, "ERR joueur_occupe");
    return;
  }

  clients[i].etat = ETAT_DEFI_ENVOYE;
  clients[i].adversaire = cible;
  clients[cible].etat = ETAT_DEFI_RECU;
  clients[cible].adversaire = i;
  client_envoyer(i, "OK CHALLENGE %s (%d)", clients[cible].name, clients[cible].elo);
  client_envoyer(cible, "EVT CHALLENGE %s (%d)", clients[i].name, clients[i].elo);
}

/* remet les deux joueurs d'un defi a l'etat libre */
static void defi_terminer(int i)
{
  int autre = clients[i].adversaire;
  clients[i].etat = ETAT_LIBRE;
  clients[i].adversaire = -1;
  if (autre >= 0)
  {
    clients[autre].etat = ETAT_LIBRE;
    clients[autre].adversaire = -1;
  }
}

void cmd_accept(int i, char *args)
{
  (void)args;
  int defiant = clients[i].adversaire;
  /* le defi reste en place si la table des parties est pleine : le defie pourra reessayer */
  if (partie_creer(defiant, i) < 0)
    client_envoyer(i, "ERR serveur_plein");
}

/* rang (0 = Sud, 1 = Nord) du client i dans sa partie */
static int rang_dans_partie(int i)
{
  return parties[clients[i].partie].joueur[0] == i ? 0 : 1;
}

void cmd_move(int i, char *args)
{
  /* nombre strict : "3" seulement, pas "3x", "1 2" ni "" */
  char *fin;
  long n = strtol(args, &fin, 10);
  if (args[0] == '\0' || *fin != '\0' || n < 1 || n > AWALE_PAR_JOUEUR)
  {
    client_envoyer(i, "ERR syntaxe");
    return;
  }

  int p = clients[i].partie;
  int rang = rang_dans_partie(i);
  Awale *jeu = &parties[p].jeu;
  if (jeu->tour != rang)
  {
    client_envoyer(i, "ERR pas_votre_tour");
    return;
  }

  /* le moteur est seul juge : -1 = coup illegal, plateau inchange */
  int captures = awale_jouer(jeu, awale_case_du_coup(rang, (int)n));
  if (captures < 0)
  {
    client_envoyer(i, "ERR coup_illegal");
    return;
  }

  partie_diffuser(p, "EVT MOVE %d %s %ld %d", p, clients[i].name, n, captures);
  partie_envoyer_plateau(p, -1);
  if (jeu->fini)
  {
    const char *raison = (jeu->gagnant >= 0 && jeu->score[jeu->gagnant] >= AWALE_SCORE_VICTOIRE) ? "score" : "blocage";
    partie_terminer(p, jeu->gagnant, raison);
  }
}

void cmd_board(int i, char *args)
{
  (void)args;
  partie_envoyer_plateau(clients[i].partie, i);
}

void cmd_decline(int i, char *args)
{
  (void)args;
  int defiant = clients[i].adversaire;
  defi_terminer(i);
  client_envoyer(i, "OK DECLINE");
  client_envoyer(defiant, "EVT DECLINED %s", clients[i].name);
}

void cmd_cancel(int i, char *args)
{
  (void)args;
  int defie = clients[i].adversaire;
  defi_terminer(i);
  client_envoyer(i, "OK CANCEL");
  client_envoyer(defie, "EVT CANCELLED %s", clients[i].name);
}

void evenement_deconnexion(int i)
{
  /* depart en pleine partie : l'adversaire gagne par forfait */
  if (clients[i].etat == ETAT_EN_PARTIE)
  {
    int p = clients[i].partie;
    partie_terminer(p, 1 - rang_dans_partie(i), "forfait");
  }
  /* defi en cours : l'autre joueur redevient libre et en est informe */
  if (clients[i].etat == ETAT_DEFI_ENVOYE || clients[i].etat == ETAT_DEFI_RECU)
  {
    int autre = clients[i].adversaire;
    int envoye = (clients[i].etat == ETAT_DEFI_ENVOYE);
    defi_terminer(i);
    if (envoye)
      client_envoyer(autre, "EVT CANCELLED %s", clients[i].name);
    else
      client_envoyer(autre, "EVT DECLINED %s", clients[i].name);
  }
  if (clients[i].etat & ETAT_INSCRIT)
    clients_diffuser(i, "EVT LEAVE %s", clients[i].name);
}
