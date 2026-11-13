/* Serveur de jeu Awale, structure de la V2 : init() / app() / end(), init_connection(),
 * write_client(), remove_client(), clear_clients()... Un SEUL processus, toutes les sockets
 * surveillees par select().
 *    awale_serveur [port]   (defaut : PORT de server.h ; port > 1024 sauf root)
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
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include "server.h"
#include "clients.h"
#include "protocole.h"
#include "commandes.h"
#include "parties.h"
#include "sauvegarde.h"
#include <time.h>

/* mis a 1 par CTRL-C : la boucle se termine proprement */
static volatile sig_atomic_t arret = 0;

static void sur_signal_arret(int sig)
{
  (void)sig;
  arret = 1;
}

static int mettre_non_bloquant(SOCKET fd)
{
  int flags = fcntl(fd, F_GETFL);
  if (flags < 0)
    return -1;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* init_connection : socket d'ecoute, meme sequence socket / bind / listen que dans le TP1 */
static SOCKET init_connection(int port)
{
  SOCKADDR_IN serv_addr;

  SOCKET sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd == INVALID_SOCKET)
  {
    perror("impossible d'ouvrir le socket");
    return INVALID_SOCKET;
  }

  /* permet de relancer le serveur tout de suite apres une fermeture (TIME_WAIT) */
  int opt = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  bzero((char *)&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(port);

  if (bind(sockfd, (SOCKADDR *)&serv_addr, sizeof(serv_addr)) < 0)
  {
    perror("impossible de faire le bind");
    closesocket(sockfd);
    return INVALID_SOCKET;
  }
  if (listen(sockfd, 16) < 0)
  {
    perror("listen");
    closesocket(sockfd);
    return INVALID_SOCKET;
  }

  /* non bloquante aussi : un accept() ne doit jamais endormir le serveur */
  if (mettre_non_bloquant(sockfd) < 0)
  {
    perror("fcntl");
    closesocket(sockfd);
    return INVALID_SOCKET;
  }
  return sockfd;
}

/* envoi direct (hors file de sortie) d'un court message sur une socket neuve : ne bloque pas */
static void write_client(SOCKET sock, const char *buffer)
{
  ssize_t r = send(sock, buffer, strlen(buffer), 0);
  (void)r;
}

/* un client se connecte : accept() ne bloque pas car select() a dit que c'est pret */
static void accepter_client(SOCKET sockfd)
{
  SOCKADDR_IN cli_addr;
  socklen_t clilen = sizeof(cli_addr);

  SOCKET fd = accept(sockfd, (SOCKADDR *)&cli_addr, &clilen);
  if (fd == INVALID_SOCKET)
  {
    if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR && errno != ECONNABORTED)
      perror("accept");
    return;
  }

  /* FD_SET sur un descripteur >= FD_SETSIZE ecrirait hors de l'ensemble : on refuse */
  if (fd >= FD_SETSIZE || mettre_non_bloquant(fd) < 0)
  {
    closesocket(fd);
    return;
  }

  int i = clients_ajouter(fd);
  if (i < 0)
  {
    /* table pleine : message de courtoisie (un write sur une socket neuve ne bloque pas) */
    write_client(fd, "ERR serveur_plein\n");
    closesocket(fd);
    printf("[serveur] connexion refusee (serveur plein) de %s\n", inet_ntoa(cli_addr.sin_addr));
    fflush(stdout);
    return;
  }

  printf("[serveur] client %d connecte depuis %s\n", i, inet_ntoa(cli_addr.sin_addr));
  fflush(stdout);
  client_envoyer(i, "EVT HELLO Bienvenue sur le serveur Awale. REGISTER <pseudo> pour creer un compte, LOGIN <pseudo> pour vous connecter, ou HELP.");
}

/* des octets sont arrives sur la socket du client i */
static void lire_client(int i)
{
  Client *c = &clients[i];

  int r = entree_remplir(&c->in, c->sock);
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
  if (sortie_vider(&clients[i].out, clients[i].sock) < 0)
    clients[i].a_fermer = 1; /* EPIPE, ECONNRESET... */
}

/* ferme proprement un client (a appeler seulement hors des boucles qui parcourent les clients) */
static void remove_client(int i)
{
  /* derniere tentative d'envoi (par exemple le "OK QUIT") ; l'echec est sans importance */
  sortie_vider(&clients[i].out, clients[i].sock);
  printf("[serveur] client %d (%s) deconnecte\n", i,
         clients[i].name[0] != '\0' ? clients[i].name : "sans pseudo");
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
      if (clients[i].sock >= 0 && clients[i].a_fermer)
      {
        remove_client(i);
        trouve = 1;
      }
    }
  } while (trouve);
}

static void init(void)
{
#if defined(WIN32) && !defined(__CYGWIN__)
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) < 0)
  {
    puts("WSAStartup failed !");
    exit(EXIT_FAILURE);
  }
#endif
}

static void end(void)
{
#if defined(WIN32) && !defined(__CYGWIN__)
  WSACleanup();
#endif
}

static void end_connection(SOCKET sock)
{
  closesocket(sock);
}

/* ferme tous les clients encore connectes (arret du serveur) */
static void clear_clients(void)
{
  for (int i = 0; i < MAX_CLIENTS; i++)
    if (clients[i].sock >= 0)
      clients_retirer(i);
}

/* boucle principale : select() sur la socket d'ecoute et sur celle de chaque client */
static void app(int port)
{
  clients_init();
  parties_init();
  const char *fichier = getenv("AWALE_DONNEES");
  if (fichier == NULL || fichier[0] == '\0')
    fichier = SAUVEGARDE_DEFAUT;
  printf("[serveur] %d compte(s) charge(s) depuis %s\n", sauvegarde_charger(fichier), fichier);
  SOCKET sock = init_connection(port);
  if (sock == INVALID_SOCKET)
    exit(1);
  printf("[serveur] en ecoute sur le port %d\n", port);
  fflush(stdout);

  while (!arret)
  {
    fd_set rdfs, wfds;
    FD_ZERO(&rdfs);
    FD_ZERO(&wfds);
    FD_SET(sock, &rdfs);
    int max = sock;

    /* select() modifie ses ensembles : on les reconstruit a chaque tour */
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      if (clients[i].sock < 0)
        continue;
      FD_SET(clients[i].sock, &rdfs);
      /* on ne surveille l'ecriture que s'il y a quelque chose a envoyer */
      if (!sortie_vide(&clients[i].out))
        FD_SET(clients[i].sock, &wfds);
      if (clients[i].sock > max)
        max = clients[i].sock;
    }

    if (select(max + 1, &rdfs, &wfds, NULL, NULL) < 0)
    {
      if (errno == EINTR)
        continue; /* signal recu (CTRL-C : arret est a 1) */
      perror("select");
      break;
    }

    if (FD_ISSET(sock, &rdfs))
      accepter_client(sock);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
      if (clients[i].sock < 0)
        continue;
      if (FD_ISSET(clients[i].sock, &rdfs))
        lire_client(i);
      if (!clients[i].a_fermer && FD_ISSET(clients[i].sock, &wfds))
        ecrire_client(i);
    }

    balayer_clients();
  }

  printf("[serveur] arret\n");
  clear_clients();
  end_connection(sock);
}

int main(int argc, char **argv)
{
  long port = PORT;
  if (argc > 2)
  {
    printf("usage: awale_serveur [port]\n");
    exit(0);
  }
  if (argc == 2)
  {
    char *fin;
    port = strtol(argv[1], &fin, 10);
    if (*argv[1] == '\0' || *fin != '\0' || port < 1 || port > 65535)
    {
      printf("port invalide : %s\n", argv[1]);
      exit(0);
    }
  }

  /* ecrire sur une socket fermee par l'autre enverrait SIGPIPE et tuerait le serveur.
   * Ignore, write() renvoie simplement -1 (EPIPE) et on deconnecte ce client */
  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, sur_signal_arret);
  signal(SIGTERM, sur_signal_arret);

  srand((unsigned)time(NULL) ^ (unsigned)getpid());
  init();
  app((int)port);
  end();

  return EXIT_SUCCESS;
}
