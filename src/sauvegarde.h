/* Comptes des joueurs, conserves entre les sessions : pseudo + mot de passe + Elo.
 *
 * Tout est dans UN fichier texte au format CSV, une ligne par compte :
 *     pseudo,elo,sel,hachage
 *     alice,616,3fa9...,9c1e...
 * Le mot de passe n'y est jamais en clair (voir mdp.h). Au demarrage le serveur charge le
 * fichier en memoire ; a chaque creation de compte ou changement d'Elo (commande ELO, fin de
 * partie) il est reecrit en entier (quelques lignes : c'est instantane).
 *
 * Le fichier est donne par la variable d'environnement AWALE_DONNEES (defaut : joueurs.csv,
 * dans le dossier ou le serveur est lance).
 */

#ifndef SAUVEGARDE_H
#define SAUVEGARDE_H

#define SAUVEGARDE_DEFAUT "joueurs.csv"

/* charge le fichier (absent = aucun compte, pas une erreur) ; les lignes mal formees sont
 * ignorees. Retourne le nombre de comptes charges. */
int sauvegarde_charger(const char *fichier);

/* 1 si un compte existe pour ce pseudo (casse ignoree) */
int sauvegarde_compte_existe(const char *pseudo);

/* cree le compte et reecrit le fichier. Retourne 0 si OK, -1 si le pseudo a deja un compte,
 * si la table est pleine ou si l'ecriture a echoue (dans ce cas le compte n'est pas cree). */
int sauvegarde_creer_compte(const char *pseudo, const char *mdp, int elo);

/* 1 si le compte existe ET que le mot de passe est bon : *elo et nom (PSEUDO_MAX + 1 octets,
 * pseudo tel qu'ecrit a la creation du compte) sont alors remplis ; 0 sinon.
 * Prend le meme temps pour un compte inconnu que pour un mauvais mot de passe. */
int sauvegarde_verifier(const char *pseudo, const char *mdp, int *elo, char *nom);

/* met a jour l'Elo d'un compte existant et reecrit le fichier ; 0 si OK, -1 sinon */
int sauvegarde_enregistrer_elo(const char *pseudo, int elo);

#endif
