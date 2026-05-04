# ifndef SOCK_UTIL_H
# define SOCK_UTIL_H

#include <arpa/inet.h> // Types

/*
*   This is from Beej's
*
*   Returns the IPv4 or IPv6 stores in sa depending
*   on whether it's storing an IPv4 address or IPv6 
*   address. Checks sa->sa_family
*/  
void *get_in_addr(struct sockaddr *sa);

/*
*   Returns the sockfd of a listener socket bound to the 
*   input IP and port with parameter backlog.
*
*   The socket returned uses TCP. If the passed IP is NULL, 
*   then the ip utilized defaults to local (i.e. it uses
*   the AI_PASSIVE flag)
*
*   Returns -1 on error.
*/
int get_listener_socket(const char *ip, const char *port, int backlog);

/*
*   Accepts a connection given a listening socket.
*
*   Returns the sockfd that corresponding to a brand new connection
*   given on a listener socket listenfd. Additionally, if successful
*   the parameter connection_addr will be filled with the address 
*   information of the new connection. 
*
*   Returns -1 on error.
*/
int accept_listen_connection(int listenfd, struct sockaddr_storage *connection_addr, socklen_t *sin_size);

/*
*   From Beej's Chapter 7
*
*   The usual inet_ntop call but it converts from sockaddr_storage
*   to the correct IPv4/IPv6 version automatically
*
*   Returns NULL on error
*/
const char *inet_ntop2(void *addr, char *buf, size_t size);

#endif