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
#include "awale.h"

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
      {"joueur_inconnu", "aucun joueur connecte sous ce pseudo"},
      {"defi_soi", "vous ne pouvez pas vous defier vous-meme"},
      {"joueur_occupe", "ce joueur est deja en defi ou en partie"},
      {"pas_votre_tour", "ce n'est pas votre tour"},
      {"coup_illegal", "coup illegal (case vide, ou il faut nourrir l'adversaire)"},
      {"serveur_plein", "le serveur est plein"},
      {"ligne_trop_longue", "ligne trop longue, deconnexion"},
  };
  for (size_t k = 0; k < sizeof(erreurs) / sizeof(erreurs[0]); k++)
    if (strcmp(code, erreurs[k].code) == 0)
      return erreurs[k].texte;
  return code;
}

/* ce que le client retient pour mettre les messages en forme */
static char mon_pseudo[32] = "";
static char nom_joueur[2][32] = {"Sud", "Nord"}; /* [0] = Sud, [1] = Nord (EVT START) */

/* EVT BOARD <id> <12 cases> <score_sud> <score_nord> <tour> : dessine le plateau */
static void afficher_plateau(const char *args)
{
  Awale a;
  int id, n = 0;
  if (sscanf(args, "%d%n", &id, &n) != 1)
    return;
  args += n;
  for (int c = 0; c < AWALE_CASES; c++)
  {
    if (sscanf(args, "%d%n", &a.cases[c], &n) != 1)
      return;
    args += n;
  }
  a.fini = 0;
  if (sscanf(args, "%d %d %d %d", &a.score[0], &a.score[1], &a.tour, &a.fini) < 3 || (a.tour != 0 && a.tour != 1))
    return;
  a.gagnant = AWALE_NUL;

  char buf[1024];
  awale_afficher_joueurs(&a, buf, sizeof(buf), nom_joueur[0], nom_joueur[1]);
  printf("%s", buf);
  if (a.fini)
    return;
  if (strcmp(nom_joueur[a.tour], mon_pseudo) == 0)
    printf("A vous de jouer, %s : MOVE <1-6>\n", mon_pseudo);
  else
    printf("Tour de %s.\n", nom_joueur[a.tour]);
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
        char *elo = strchr(deux_points + 1, ':');
        if (elo != NULL)
        {
          *elo++ = '\0';
          /* Elo fourni uniquement par la reponse WHO. */
        }
        printf("  - %s (%s) (%s)\n", mot, elo != NULL ? elo : "600", deux_points + 1);
      }
    }
  }
  else if (strncmp(ligne, "OK NICK ", 8) == 0)
  {
    snprintf(mon_pseudo, sizeof(mon_pseudo), "%.31s", ligne + 8);
    printf("Vous etes inscrit sous le pseudo %s\n", mon_pseudo);
  }
  else if (strncmp(ligne, "OK ELO ", 7) == 0)
  {
    char nom[32];
    int elo;
    if (sscanf(ligne + 7, "%31s %d", nom, &elo) == 2)
    {
      /* Reponse visible uniquement par le joueur qui utilise ELO. */
      printf("Elo : %s (%d)\n", nom, elo);
    }
  }
  else if (strncmp(ligne, "EVT START ", 10) == 0)
  {
    int id;
    char sud[32], nord[32], premier[32];
    if (sscanf(ligne + 10, "%d %31s %31s %31s", &id, sud, nord, premier) == 4)
    {
      snprintf(nom_joueur[0], sizeof(nom_joueur[0]), "%s", sud);
      snprintf(nom_joueur[1], sizeof(nom_joueur[1]), "%s", nord);
      printf("*** Partie %d : %s (Sud) contre %s (Nord). %s commence.\n", id, sud, nord, premier);
    }
  }
  else if (strncmp(ligne, "EVT BOARD ", 10) == 0)
    afficher_plateau(ligne + 10);
  else if (strncmp(ligne, "EVT MOVE ", 9) == 0)
  {
    int id, numero, captures;
    char qui[32];
    if (sscanf(ligne + 9, "%d %31s %d %d", &id, qui, &numero, &captures) == 4)
      printf("*** %s joue la case %d%s\n", qui, numero, captures > 0 ? " et capture des graines" : "");
  }
  else if (strncmp(ligne, "EVT END ", 8) == 0)
  {
    int id, ss, sn;
    char gagnant[32], raison[16];
    if (sscanf(ligne + 8, "%d %31s %d %d %15s", &id, gagnant, &ss, &sn, raison) == 5)
    {
      if (strcmp(gagnant, "NUL") == 0)
        printf("*** Partie terminee : match nul (%d - %d).\n", ss, sn);
      else
        printf("*** Partie terminee : %s gagne (%d - %d)%s.\n", gagnant, ss, sn,
               strcmp(raison, "forfait") == 0 ? " par forfait" : "");
    }
  }
  else if (strncmp(ligne, "OK HELP ", 8) == 0)
    printf("Commandes disponibles : %s\n", ligne + 8);
  else if (strcmp(ligne, "OK QUIT") == 0)
    printf("Au revoir.\n");
  else if (strncmp(ligne, "EVT HELLO ", 10) == 0)
    printf("%s\n", ligne + 10);
  else if (strncmp(ligne, "OK CHALLENGE ", 13) == 0)
    printf("Defi envoye a %s, en attente de sa reponse.\n", ligne + 13);
  else if (strncmp(ligne, "EVT CHALLENGE ", 14) == 0)
    printf("*** %s vous defie !\n", ligne + 14);
  else if (strcmp(ligne, "OK DECLINE") == 0)
    printf("Defi refuse.\n");
  else if (strcmp(ligne, "OK CANCEL") == 0)
    printf("Defi retire.\n");
  else if (strncmp(ligne, "EVT DECLINED ", 13) == 0)
    printf("*** %s a refuse votre defi (ou est parti)\n", ligne + 13);
  else if (strncmp(ligne, "EVT CANCELLED ", 14) == 0)
    printf("*** %s a retire son defi (ou est parti)\n", ligne + 14);
  else if (strncmp(ligne, "EVT JOIN ", 9) == 0)
    printf("*** %s vient de se connecter\n", ligne + 9);
  else if (strncmp(ligne, "EVT LEAVE ", 10) == 0)
  {
    printf("*** %s est parti\n", ligne + 10);
  }
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
