/* Tampons de communication
 *
 * TCP est un flux d'octets : un read() peut rendre une demi-ligne, ou trois lignes d'un coup.
 *  - TamponEntree : accumule les octets lus et en extrait les lignes completes ("...\n")
 *  - FileSortie   : accumule les messages a envoyer ; write() peut n'en ecrire qu'une partie,
 *                   le reste attend que select() dise que la socket est de nouveau prete.
 * Les deux sont de taille FIXE : un client ne peut jamais faire deborder le serveur.
 */

#ifndef TAMPON_H
#define TAMPON_H

#include <stddef.h>

#define ENTREE_MAX 1024     /* une ligne fait au plus ENTREE_MAX - 1 caracteres (+ le '\n') */
#define SORTIE_MAX 16384

typedef struct
{
  char data[ENTREE_MAX];
  size_t len;
} TamponEntree;

typedef struct
{
  char data[SORTIE_MAX];
  size_t debut;   /* premier octet pas encore ecrit */
  size_t len;     /* nombre d'octets en attente */
} FileSortie;

/* resultats de entree_remplir */
#define ENTREE_OK 1        /* lecture faite (ou rien a lire pour l'instant) */
#define ENTREE_FERME 0     /* l'autre a ferme la connexion */
#define ENTREE_ERREUR -1   /* erreur reseau (ou tampon plein) */

void entree_init(TamponEntree *t);

/* fait UN read() non bloquant dans le tampon */
int entree_remplir(TamponEntree *t, int fd);

/* extrait la premiere ligne complete dans ligne (sans '\n' ni '\r', terminee par '\0') ;
 * retourne 1 si une ligne a ete extraite, 0 s'il n'y en a pas (pas encore de '\n') */
int entree_extraire_ligne(TamponEntree *t, char *ligne, size_t taille);

/* 1 si le tampon est plein sans contenir de '\n' : la ligne est trop longue */
int entree_pleine(const TamponEntree *t);

void sortie_init(FileSortie *f);

/* ajoute texte suivi de '\n' ; retourne 0 si OK, -1 si la file est pleine
 * (dans ce cas rien n'est ajoute : jamais de message coupe) */
int sortie_ajouter(FileSortie *f, const char *texte);

/* fait UN write() non bloquant de ce qui est en attente ; retourne 0 si OK, -1 si erreur
 * (client parti : EPIPE, ECONNRESET...) */
int sortie_vider(FileSortie *f, int fd);

int sortie_vide(const FileSortie *f);

#endif
