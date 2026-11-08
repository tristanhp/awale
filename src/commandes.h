/* Commandes du protocole : une fonction par commande
 * Chacune est appelee par le dispatcher (protocole.c) avec l'indice du client et ses arguments.
 * L'etat du client a deja ete verifie par le dispatcher.
 */

#ifndef COMMANDES_H
#define COMMANDES_H

void cmd_nick(int i, char *args); /* NICK pseudo : s'enregistrer */
void cmd_who(int i, char *args);  /* WHO : liste des joueurs en ligne */
void cmd_help(int i, char *args); /* HELP : commandes disponibles */
void cmd_quit(int i, char *args); /* QUIT : quitter proprement */
void cmd_challenge(int i, char *args); /* CHALLENGE pseudo : defier un joueur libre */
void cmd_decline(int i, char *args);   /* DECLINE : refuser le defi recu */
void cmd_cancel(int i, char *args);    /* CANCEL : retirer son defi */

/* appelee par le serveur juste avant de liberer la case d'un client (depart volontaire
 * ou brutal) : previent les autres et nettoie ce qui concerne ce client */
void evenement_deconnexion(int i);

#endif
