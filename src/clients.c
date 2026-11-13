/* Table des clients (voir clients.h) */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "clients.h"

Client clients[MAX_CLIENTS];

static void vider_case(Client *c)
{
  c->sock = -1;
  c->etat = ETAT_CONNECTE;
  c->elo = 600;
  c->compte = 0;
  c->echecs = 0;
  c->name[0] = '\0';
  entree_init(&c->in);
  sortie_init(&c->out);
  c->adversaire = -1;
  c->partie = -1;
  c->a_fermer = 0;
}

void clients_init(void)
{
  for (int i = 0; i < MAX_CLIENTS; i++)
    vider_case(&clients[i]);
}

int clients_ajouter(SOCKET sock)
{
  for (int i = 0; i < MAX_CLIENTS; i++)
  {
    if (clients[i].sock == -1)
    {
      vider_case(&clients[i]);
      clients[i].sock = sock;
      return i;
    }
  }
  return -1;
}

void clients_retirer(int i)
{
  if (clients[i].sock >= 0)
    close(clients[i].sock);
  vider_case(&clients[i]);
}

int clients_chercher_pseudo(const char *pseudo)
{
  for (int i = 0; i < MAX_CLIENTS; i++)
    if (clients[i].sock >= 0 && clients[i].name[0] != '\0' && strcasecmp(clients[i].name, pseudo) == 0)
      return i;
  return -1;
}

int pseudo_valide(const char *pseudo)
{
  size_t n = strlen(pseudo);
  if (n == 0 || n > PSEUDO_MAX)
    return 0;
  for (size_t k = 0; k < n; k++)
  {
    char c = pseudo[k];
    int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
    if (!ok)
      return 0;
  }
  return 1;
}

const char *etat_nom(int etat)
{
  switch (etat)
  {
  case ETAT_CONNECTE:
    return "connecte";
  case ETAT_LIBRE:
    return "libre";
  case ETAT_DEFI_ENVOYE:
    return "defi_envoye";
  case ETAT_DEFI_RECU:
    return "defi_recu";
  case ETAT_EN_PARTIE:
    return "en_partie";
  case ETAT_OBSERVATEUR:
    return "observe";
  default:
    return "inconnu";
  }
}

/* envoi d'un message deja formate */
static void envoyer_texte(int i, const char *texte)
{
  if (clients[i].sock < 0 || clients[i].a_fermer)
    return;
  if (sortie_ajouter(&clients[i].out, texte) < 0)
    clients[i].a_fermer = 1; /* file pleine : ce client ne lit plus, on le deconnecte */
}

void client_envoyer(int i, const char *fmt, ...)
{
  char msg[MSG_MAX];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap); /* tronque si trop long, ne deborde jamais */
  va_end(ap);
  envoyer_texte(i, msg);
}

void clients_diffuser(int sauf, const char *fmt, ...)
{
  char msg[MSG_MAX];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);
  for (int i = 0; i < MAX_CLIENTS; i++)
    if (i != sauf && clients[i].sock >= 0 && (clients[i].etat & ETAT_INSCRIT))
      envoyer_texte(i, msg);
}
