/* Protocole : table des commandes et dispatcher (voir protocole.h) */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "protocole.h"
#include "commandes.h"
#include "clients.h"

/* TABLE DES COMMANDES : pour en ajouter une, ajouter une ligne ici */
static const Commande commandes[] = {
    /* nom        fonction     etats autorises */
    {"NICK", cmd_nick, ETAT_TOUS},
    {"WHO", cmd_who, ETAT_INSCRIT},
    {"HELP", cmd_help, ETAT_TOUS},
    {"QUIT", cmd_quit, ETAT_TOUS},
    {"CHALLENGE", cmd_challenge, ETAT_LIBRE},
    {"ACCEPT", cmd_accept, ETAT_DEFI_RECU},
    {"MOVE", cmd_move, ETAT_EN_PARTIE},
    {"BOARD", cmd_board, ETAT_EN_PARTIE},
    {"DECLINE", cmd_decline, ETAT_DEFI_RECU},
    {"CANCEL", cmd_cancel, ETAT_DEFI_ENVOYE},
};

#define NB_COMMANDES (sizeof(commandes) / sizeof(commandes[0]))

void protocole_traiter_ligne(int i, char *ligne)
{
  /* saute les espaces de debut */
  while (isspace((unsigned char)*ligne))
    ligne++;
  if (*ligne == '\0')
    return; /* ligne vide : on l'ignore */

  /* separe le nom de la commande et ses arguments */
  char *args = ligne;
  while (*args != '\0' && !isspace((unsigned char)*args))
    args++;
  if (*args != '\0')
    *args++ = '\0';
  while (isspace((unsigned char)*args))
    args++;
  /* supprime les espaces de fin */
  size_t n = strlen(args);
  while (n > 0 && isspace((unsigned char)args[n - 1]))
    args[--n] = '\0';

  for (size_t k = 0; k < NB_COMMANDES; k++)
  {
    if (strcasecmp(ligne, commandes[k].nom) == 0)
    {
      if (!(clients[i].etat & commandes[k].etats))
        client_envoyer(i, "ERR etat_invalide");
      else
        commandes[k].fonction(i, args);
      return;
    }
  }
  client_envoyer(i, "ERR commande_inconnue");
}

void protocole_lister(char *buf, size_t n, int etat)
{
  size_t pos = 0;
  if (n == 0)
    return;
  buf[0] = '\0';
  for (size_t k = 0; k < NB_COMMANDES; k++)
  {
    if (!(etat & commandes[k].etats))
      continue;
    int r = snprintf(buf + pos, n - pos, "%s%s", pos > 0 ? " " : "", commandes[k].nom);
    if (r < 0 || (size_t)r >= n - pos)
    {
      buf[pos] = '\0'; /* plus de place : on s'arrete avant la commande tronquee */
      return;
    }
    pos += (size_t)r;
  }
}
