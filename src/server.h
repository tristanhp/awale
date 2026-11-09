/* Portabilite reseau et constantes communes (equivalent de server2.h / client2.h de la V2).
 * Inclus par le serveur et par le client. */

#ifndef SERVER_H
#define SERVER_H

#if defined(WIN32) && !defined(__CYGWIN__)

#include <winsock2.h>

#elif defined(__linux__) || defined(__CYGWIN__)

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h>  /* gethostbyname */
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;

#else

#error not defined for this platform

#endif

#define PORT 1977 /* port par defaut quand aucun n'est donne en argument */

#endif
