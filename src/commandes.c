/* Commandes du protocole (voir commandes.h) */

#include <stdio.h>
#include <string.h>
#include "commandes.h"
#include "clients.h"
#include "protocole.h"

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
    int r = snprintf(msg + pos, sizeof(msg) - pos, " %s:%s", clients[k].name, etat_nom(clients[k].etat));
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

void evenement_deconnexion(int i)
{
  if (clients[i].etat & ETAT_INSCRIT)
    clients_diffuser(i, "EVT LEAVE %s", clients[i].name);
}
