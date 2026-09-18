#ifndef COMPAT_NET_H
#define COMPAT_NET_H

/* compat_net.h — cross-platform socket networking for symbols-server */

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

typedef SOCKET socket_t;
typedef int socklen_compat_t;

#define CLOSESOCKET(s) closesocket(s)
#define SOCKET_INIT() do { WSADATA _wsa; (void)WSAStartup(MAKEWORD(2, 2), &_wsa); } while (0)
#define SOCKET_CLEANUP() WSACleanup()
#define IS_VALID_SOCKET(s) ((s) != INVALID_SOCKET)

#else /* POSIX */

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

typedef int socket_t;
typedef socklen_t socklen_compat_t;

#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)
#define CLOSESOCKET(s) close(s)
#define closesocket(s) close(s)
#define SOCKET_INIT()    ((void)0)
#define SOCKET_CLEANUP() ((void)0)
#define IS_VALID_SOCKET(s) ((s) >= 0)

#endif /* _WIN32 */

#endif /* COMPAT_NET_H */
