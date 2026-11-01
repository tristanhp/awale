/* Protocole : decoupage d'une ligne recue et appel de la bonne commande
 * Chaque commande est une ligne de la table 'commandes' (protocole.c) :
 *     { nom, fonction, masque des etats dans lesquels elle est autorisee }
 * Ajouter une commande = ecrire la fonction dans commandes.c, la declarer dans commandes.h
 * et ajouter UNE ligne dans la table.
 */

#ifndef PROTOCOLE_H
#define PROTOCOLE_H

#include <stddef.h>

/* une commande recoit l'indice du client qui l'a envoyee et le reste de la ligne
 * (arguments, sans espaces en debut ni en fin ; chaine vide s'il n'y en a pas) */
typedef void (*FonctionCommande)(int i, char *args);

typedef struct
{
  const char *nom;
  FonctionCommande fonction;
  int etats; /* masque d'etats (ETAT_*) dans lesquels la commande est permise */
} Commande;

/* traite une ligne complete recue du client i (sans le '\n') ; modifie la ligne */
void protocole_traiter_ligne(int i, char *ligne);

/* ecrit dans buf les noms des commandes permises dans l'etat 'etat', separes par des espaces */
void protocole_lister(char *buf, size_t n, int etat);

#endif
