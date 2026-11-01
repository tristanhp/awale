/* Serveur de jeu Awale : un SEUL processus, toutes les sockets surveillees par select()
 *    awale_serveur port (port > 1024 sauf root)
 *
 * Pourquoi pas de fork : le serveur doit connaitre tous les joueurs, les defis et les plateaux.
 * Apres un fork la memoire n'est plus partagee ; avec select() tout vit dans les memes variables.
 *
 * Boucle principale :
 *   1. on declare a select() les sockets a surveiller (ecoute + clients) ;
 *   2. select() endort le processus jusqu'a ce qu'une d'elles soit prete ;
 *   3. on traite chacune : nouvelle connexion, donnees recues, place pour ecrire.
 * Toutes les sockets sont NON BLOQUANTES : un client lent ou muet ne bloque jamais les autres.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include "clients.h"
#include "protocole.h"
#include "commandes.h"

/* mis a 1 par CTRL-C : la boucle se termine proprement */
static volatile sig_atomic_t arret = 0;

static void sur_signal_arret(int sig)
{
  (void)sig;
  arret = 1;
}

static int mettre_non_bloquant(int fd)
{
  int flags = fcntl(fd, F_GETFL);
  if (flags < 0)
    return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* socket d'ecoute : meme sequence socket / bind / listen que dans le TP1 */
static int creer_socket_ecoute(int port)
{
  struct sockaddr_in serv_addr;

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
  {
    perror("impossible d'ouvrir le socket");
    return -1;
  }

  /* permet de relancer le serveur tout de suite apres une fermeture (TIME_WAIT) */
  int opt = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(port);

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("impossible de faire le bind");
    close(sockfd);
    return -1;
  }
  if (listen(sockfd, 16) < 0)
  {
    perror("listen");
    close(sockfd);
    return -1;
  }

  /* non bloquante aussi : un accept() ne doit jamais endormir le serveur */
  if (mettre_non_bloquant(sockfd) < 0)
  {
    perror("fcntl");
    close(sockfd);
    return -1;
  }
  return sockfd;
}

/* un client se connecte : accept() ne bloque pas car select() a dit que c'est pret */
static void accepter_client(int sockfd)
{
  struct sockaddr_in cli_addr;
  socklen_t clilen = sizeof(cli_addr);

  int fd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
  if (fd < 0)
  {
    if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR && errno != ECONNABORTED)
      perror("accept");
    return;
  }

  /* FD_SET sur un descripteur >= FD_SETSIZE ecrirait hors de l'ensemble : on refuse */
  if (fd >= FD_SETSIZE || mettre_non_bloquant(fd) < 0)
  {
    close(fd);
    return;
  }

  int i = clients_ajouter(fd);
  if (i < 0)
  {
    /* table pleine : message de courtoisie (un write sur une socket neuve ne bloque pas) */
    ssize_t r = write(fd, "ERR serveur_plein\n", 18);
    (void)r;
    close(fd);
    printf("[serveur] connexion refusee (serveur plein) de %s\n", inet_ntoa(cli_addr.sin_addr));
    fflush(stdout);
    return;
  }

  printf("[serveur] client %d connecte depuis %s\n", i, inet_ntoa(cli_addr.sin_addr));
  fflush(stdout);
  client_envoyer(i, "EVT HELLO Bienvenue sur le serveur Awale. Tapez NICK <pseudo>, ou HELP.");
}

/* des octets sont arrives sur la socket du client i */
static void lire_client(int i)
{
  Client *c = &clients[i];

  int r = entree_remplir(&c->in, c->fd);
  if (r != ENTREE_OK)
  {
    /* fermeture (read = 0) ou erreur (reset...) : le client est parti */
    c->a_fermer = 1;
    return;
  }

  /* le tampon peut contenir 0, 1 ou plusieurs lignes completes, plus eventuellement le debut
   * d'une ligne : on traite toutes les lignes completes et on garde le reste */
  char ligne[ENTREE_MAX];
  while (!c->a_fermer && entree_extraire_ligne(&c->in, ligne, sizeof(ligne)))
    protocole_traiter_ligne(i, ligne);

  /* tampon plein sans '\n' : ligne trop longue, client fautif ou malveillant */
  if (!c->a_fermer && entree_pleine(&c->in))
  {
    client_envoyer(i, "ERR ligne_trop_longue");
    c->a_fermer = 1;
  }
}

/* la socket du client i peut accepter des octets : on envoie ce qui attend dans sa file */
static void ecrire_client(int i)
{
  if (sortie_vider(&clients[i].out, clients[i].fd) < 0)
    clients[i].a_fermer = 1; /* EPIPE, ECONNRESET... */
}

/* ferme proprement un client (a appeler seulement hors des boucles qui parcourent les clients) */
static void deconnecter_client(int i)
{
  /* derniere tentative d'envoi (par exemple le "OK QUIT") ; l'echec est sans importance */
  sortie_vider(&clients[i].out, clients[i].fd);
  printf("[serveur] client %d (%s) deconnecte\n", i,
         clients[i].pseudo[0] != '\0' ? clients[i].pseudo : "sans pseudo");
  fflush(stdout);
  evenement_deconnexion(i);
  clients_retirer(i);
}

/* ferme tous les clients marques a_fermer ; recommence tant qu'il en reste, car prevenir les
 * autres d'un depart peut remplir leur file de sortie et en marquer de nouveaux */
static void balayer_clients(void)
{
  int trouve;
  do
  {
    trouve = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      if (clients[i].fd >= 0 && clients[i].a_fermer)
      {
        deconnecter_client(i);
        trouve = 1;
      }
    }
  } while (trouve);
}

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    printf("usage: awale_serveur port\n");
    exit(0);
  }
  char *fin;
  long port = strtol(argv[1], &fin, 10);
  if (*argv[1] == '\0' || *fin != '\0' || port < 1 || port > 65535)
  {
    printf("port invalide : %s\n", argv[1]);
    exit(0);
  }

  /* ecrire sur une socket fermee par l'autre enverrait SIGPIPE et tuerait le serveur.
   * Ignore, write() renvoie simplement -1 (EPIPE) et on deconnecte ce client */
  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, sur_signal_arret);
  signal(SIGTERM, sur_signal_arret);

  clients_init();
  int sockfd = creer_socket_ecoute((int)port);
  if (sockfd < 0)
    exit(1);
  printf("[serveur] en ecoute sur le port %ld\n", port);
  fflush(stdout);

  while (!arret)
  {
    fd_set rfds, wfds;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_SET(sockfd, &rfds);
    int maxfd = sockfd;

    /* select() modifie ses ensembles : on les reconstruit a chaque tour */
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      if (clients[i].fd < 0)
        continue;
      FD_SET(clients[i].fd, &rfds);
      /* on ne surveille l'ecriture que s'il y a quelque chose a envoyer */
      if (!sortie_vide(&clients[i].out))
        FD_SET(clients[i].fd, &wfds);
      if (clients[i].fd > maxfd)
        maxfd = clients[i].fd;
    }

    if (select(maxfd + 1, &rfds, &wfds, NULL, NULL) < 0)
    {
      if (errno == EINTR)
        continue; /* signal recu (CTRL-C : arret est a 1) */
      perror("select");
      break;
    }

    if (FD_ISSET(sockfd, &rfds))
      accepter_client(sockfd);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      if (clients[i].fd < 0)
        continue;
      if (FD_ISSET(clients[i].fd, &rfds))
        lire_client(i);
      if (!clients[i].a_fermer && FD_ISSET(clients[i].fd, &wfds))
        ecrire_client(i);
    }

    balayer_clients();
  }

  printf("[serveur] arret\n");
  for (int i = 0; i < MAX_CLIENTS; i++)
    if (clients[i].fd >= 0)
      clients_retirer(i);
  close(sockfd);
  return 0;
}
