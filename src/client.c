/* Client de jeu Awale
 *    awale_client host port
 * host : nom ou adresse IP du serveur.
 *
 * Le client surveille DEUX descripteurs avec select() :
 *   - l'entree standard (0) : ce que l'utilisateur tape, envoye au serveur ligne par ligne ;
 *   - la socket : ce que le serveur envoie (reponse, defi, coup adverse, chat...), affiche
 *     tout de suite, meme pendant que l'utilisateur est en train de taper.
 * On lit le clavier avec read() (et non fgets) : fgets garde des octets dans le tampon de stdio
 * que select() ne voit pas, et une ligne tapee pourrait rester en attente sans etre envoyee.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include "server.h"
#include "tampon.h"

/* ecrit toute la chaine sur la socket (write peut n'ecrire qu'une partie) ; 0 si OK, -1 sinon */
static int ecrire_tout(int fd, const char *data, size_t len)
{
  size_t fait = 0;
  while (fait < len)
  {
    ssize_t n = write(fd, data + fait, len - fait);
    if (n < 0)
    {
      if (errno == EINTR)
        continue;
      return -1;
    }
    fait += (size_t)n;
  }
  return 0;
}

/* explication en francais d'un code d'erreur du serveur */
static const char *texte_erreur(const char *code)
{
  static const struct
  {
    const char *code;
    const char *texte;
  } erreurs[] = {
      {"pseudo_pris", "ce pseudo est deja utilise"},
      {"pseudo_invalide", "pseudo invalide (1 a 16 caracteres : lettres, chiffres, _)"},
      {"deja_inscrit", "vous avez deja un pseudo"},
      {"syntaxe", "commande mal formee"},
      {"commande_inconnue", "commande inconnue (tapez HELP)"},
      {"etat_invalide", "commande impossible dans votre situation actuelle"},
      {"serveur_plein", "le serveur est plein"},
      {"ligne_trop_longue", "ligne trop longue, deconnexion"},
  };
  for (size_t k = 0; k < sizeof(erreurs) / sizeof(erreurs[0]); k++)
    if (strcmp(code, erreurs[k].code) == 0)
      return erreurs[k].texte;
  return code;
}

/* met en forme une ligne recue du serveur ; une ligne inconnue est affichee telle quelle,
 * ce qui permet au serveur d'ajouter des messages sans casser un vieux client */
static void afficher_message(char *ligne)
{
  if (strncmp(ligne, "OK WHO", 6) == 0 && (ligne[6] == ' ' || ligne[6] == '\0'))
  {
    printf("Joueurs en ligne :\n");
    char *sauve;
    for (char *mot = strtok_r(ligne + 6, " ", &sauve); mot != NULL; mot = strtok_r(NULL, " ", &sauve))
    {
      char *deux_points = strchr(mot, ':');
      if (deux_points != NULL)
      {
        *deux_points = '\0';
        printf("  - %s (%s)\n", mot, deux_points + 1);
      }
    }
  }
  else if (strncmp(ligne, "OK NICK ", 8) == 0)
    printf("Vous etes inscrit sous le pseudo %s\n", ligne + 8);
  else if (strncmp(ligne, "OK HELP ", 8) == 0)
    printf("Commandes disponibles : %s\n", ligne + 8);
  else if (strcmp(ligne, "OK QUIT") == 0)
    printf("Au revoir.\n");
  else if (strncmp(ligne, "EVT HELLO ", 10) == 0)
    printf("%s\n", ligne + 10);
  else if (strncmp(ligne, "EVT JOIN ", 9) == 0)
    printf("*** %s vient de se connecter\n", ligne + 9);
  else if (strncmp(ligne, "EVT LEAVE ", 10) == 0)
    printf("*** %s est parti\n", ligne + 10);
  else if (strncmp(ligne, "ERR ", 4) == 0)
    printf("Erreur : %s\n", texte_erreur(ligne + 4));
  else
    printf("%s\n", ligne);
  fflush(stdout);
}

int main(int argc, char **argv)
{
  struct hostent *server;
  struct sockaddr_in serv_addr;

  if (argc != 3)
  {
    printf("usage: awale_client host port\n");
    exit(0);
  }

  char *fin;
  long port = strtol(argv[2], &fin, 10);
  if (*argv[2] == '\0' || *fin != '\0' || port < 1 || port > 65535)
  {
    printf("port invalide : %s\n", argv[2]);
    exit(0);
  }

  /* gethostbyname accepte aussi bien un nom qu'une adresse IP en notation pointee */
  server = gethostbyname(argv[1]);
  if (server == NULL)
  {
    printf("host resolution error\n");
    exit(0);
  }

  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy(&serv_addr.sin_addr, server->h_addr_list[0], server->h_length);
  serv_addr.sin_port = htons(port);

  /* le serveur peut fermer pendant qu'on ecrit : on ne veut pas mourir sur SIGPIPE */
  signal(SIGPIPE, SIG_IGN);

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    perror("socket error");
    exit(1);
  }
  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("connexion impossible");
    exit(1);
  }

  TamponEntree depuis_serveur, depuis_clavier;
  entree_init(&depuis_serveur);
  entree_init(&depuis_clavier);
  int clavier_ouvert = 1; /* passe a 0 apres CTRL-D : on attend alors les dernieres reponses */
  char ligne[ENTREE_MAX + 1];

  while (1)
  {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);
    if (clavier_ouvert)
      FD_SET(STDIN_FILENO, &rfds);

    if (select(sockfd + 1, &rfds, NULL, NULL, NULL) < 0)
    {
      if (errno == EINTR)
        continue;
      perror("select");
      break;
    }

    /* messages du serveur */
    if (FD_ISSET(sockfd, &rfds))
    {
      int r = entree_remplir(&depuis_serveur, sockfd);
      while (entree_extraire_ligne(&depuis_serveur, ligne, sizeof(ligne)))
        afficher_message(ligne);
      if (r == ENTREE_FERME)
      {
        printf("Connexion fermee par le serveur.\n");
        break;
      }
      if (r == ENTREE_ERREUR || entree_pleine(&depuis_serveur))
      {
        printf("Erreur de communication avec le serveur.\n");
        break;
      }
    }

    /* texte tape par l'utilisateur */
    if (clavier_ouvert && FD_ISSET(STDIN_FILENO, &rfds))
    {
      int r = entree_remplir(&depuis_clavier, STDIN_FILENO);
      int erreur_ecriture = 0;
      while (!erreur_ecriture && entree_extraire_ligne(&depuis_clavier, ligne, sizeof(ligne)))
      {
        strcat(ligne, "\n"); /* ligne fait au plus ENTREE_MAX - 1 caracteres : la place existe */
        if (ecrire_tout(sockfd, ligne, strlen(ligne)) < 0)
          erreur_ecriture = 1;
      }
      if (erreur_ecriture)
      {
        printf("Erreur d'envoi vers le serveur.\n");
        break;
      }
      if (entree_pleine(&depuis_clavier))
      {
        printf("Ligne trop longue (max %d caracteres), ignoree.\n", ENTREE_MAX - 1);
        entree_init(&depuis_clavier);
      }
      if (r != ENTREE_OK)
      {
        /* CTRL-D (ou stdin ferme) : on previent le serveur qu'on n'enverra plus rien
         * (FIN TCP), mais on continue a lire ses dernieres reponses jusqu'a sa fermeture */
        clavier_ouvert = 0;
        shutdown(sockfd, SHUT_WR);
      }
    }
  }

  close(sockfd);
  return 0;
}
