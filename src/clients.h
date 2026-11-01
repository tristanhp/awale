/* Table des clients connectes au serveur
 * Tableau de taille fixe : pas de malloc, donc pas de fuite ni de debordement dynamique.
 * Les autres modules designent un client par son INDICE dans clients[] (jamais par pointeur :
 * une case liberee ne laisse alors aucun pointeur perime).
 */

#ifndef CLIENTS_H
#define CLIENTS_H

#include "tampon.h"

#define MAX_CLIENTS 64
#define PSEUDO_MAX 16
#define MSG_MAX 2048 /* taille maximale d'un message envoye par le serveur */

/* etats d'un client : un bit chacun, pour qu'une commande indique un MASQUE d'etats autorises */
#define ETAT_CONNECTE (1 << 0)    /* connecte, pas encore de pseudo */
#define ETAT_LIBRE (1 << 1)       /* pseudo choisi, disponible */
#define ETAT_INSCRIT (ETAT_LIBRE) /* a un pseudo (s'etendra : en partie...) */
#define ETAT_TOUS (ETAT_CONNECTE | ETAT_INSCRIT)

typedef struct
{
  int fd;   /* -1 = case libre */
  int etat; /* un seul des bits ETAT_* */
  char pseudo[PSEUDO_MAX + 1];
  TamponEntree in;
  FileSortie out;
  int a_fermer; /* 1 = a deconnecter a la fin du tour de boucle de select */
} Client;

extern Client clients[MAX_CLIENTS];

void clients_init(void);

/* occupe une case libre pour la socket fd ; retourne l'indice, ou -1 si la table est pleine */
int clients_ajouter(int fd);

/* ferme la socket et libere la case */
void clients_retirer(int i);

/* indice du client inscrit portant ce pseudo (casse ignoree), ou -1 */
int clients_chercher_pseudo(const char *pseudo);

/* 1 si le pseudo est valide : 1 a PSEUDO_MAX caracteres parmi [A-Za-z0-9_] */
int pseudo_valide(const char *pseudo);

/* nom d'un etat, utilise dans les messages ("libre"...) */
const char *etat_nom(int etat);

/* ajoute un message (une ligne) a la file de sortie du client i.
 * Si la file est pleine (client qui ne lit plus), le client est marque a_fermer. */
void client_envoyer(int i, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* envoie un message a tous les clients inscrits sauf l'indice 'sauf' (-1 pour tous) */
void clients_diffuser(int sauf, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#endif
