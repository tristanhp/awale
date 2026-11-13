/* Persistance entre les sessions : l'Elo de chaque joueur, retrouve grace a son pseudo.
 *
 * Tout est dans UN fichier texte au format CSV, lisible et modifiable a la main :
 *     pseudo,elo
 *     alice,616
 *     bob,584
 * Au demarrage le serveur le charge en memoire ; a chaque changement d'Elo (commande ELO ou
 * fin de partie) le fichier est reecrit en entier (quelques lignes : c'est instantane).
 * Quand un joueur fait NICK, on retrouve son Elo s'il est deja connu, sinon il part de 600.
 *
 * Le fichier est donne par la variable d'environnement AWALE_DONNEES (defaut : joueurs.csv,
 * dans le dossier ou le serveur est lance). Un pseudo n'est pas un mot de passe : quiconque
 * choisit "alice" reprend l'Elo d'alice.
 */

#ifndef SAUVEGARDE_H
#define SAUVEGARDE_H

#define SAUVEGARDE_DEFAUT "joueurs.csv"

/* charge le fichier (absent = aucun joueur connu, pas une erreur) ; les lignes mal formees
 * sont ignorees. Retourne le nombre de joueurs charges. */
int sauvegarde_charger(const char *fichier);

/* Elo sauvegarde du joueur (pseudo compare sans tenir compte de la casse) :
 * 1 et *elo rempli s'il est connu, 0 sinon */
int sauvegarde_chercher(const char *pseudo, int *elo);

/* enregistre l'Elo du joueur (nouveau ou existant) et reecrit le fichier.
 * Retourne 0 si OK, -1 si la sauvegarde a echoue (le serveur continue sans). */
int sauvegarde_enregistrer(const char *pseudo, int elo);

#endif
