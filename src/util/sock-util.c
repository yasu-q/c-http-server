#include "sock-util.h"

#include <stdio.h> 
#include <unistd.h> 
#include <string.h> 
#include <netdb.h> 

/*
*   This is from Beej's
*
*   Returns the IPv4 or IPv6 stores in sa depending
*   on whether it's storing an IPv4 address or IPv6 
*   address. Checks sa->sa_family
*/  
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr); // IPv4
    }
    // Else it's IPv6
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

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
int get_listener_socket(const char *ip, const char *port, int backlog) {
    int sockfd = -1;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int yes = 1;

    // Check if port passed is not NULL
    if (port == NULL) {
        fprintf(stderr, "port is NULL\n");
        return -1;
    }

    // Setup hints
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // Listener uses TCP
    hints.ai_flags = (ip == NULL) ? AI_PASSIVE : 0; // We will use the passed IP

    // Get address info
    rv = getaddrinfo(ip, port, &hints, &servinfo);
    if (rv != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv)); // There was an error
        return -1;
    }

    // Loop through results and bind to the first one we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        // Create a socket
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("server: sockets");
            continue; // If there was an error for this entry, try the next one
        }

        // Configure the socket
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == - 1) {
            perror("setsockopt");
            close(sockfd);
            sockfd = -1;
            continue; 
        }

        // Bind the socket
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("server: bind");
            close(sockfd);
            sockfd = -1;
            continue;
        }

        // If everything went according to plan, we are done
        break;
    }

    freeaddrinfo(servinfo); // No longer need the information

    // Check if we got a bind at all
    if (p == NULL) {
        fprintf(stderr, "failed to bind");
        return -1;
    }
    // Finally, listen to the socket
    if (listen(sockfd, backlog) == -1) {
        perror("listen");
        return -1;
    }

    return sockfd;
}

/*
*   Accepts a connection given a listening socket.
*
*   Returns the sockfd that corresponding to a brand new connection
*   given on a listener socket listenfd. Additionally, if successful
*   the parameter connection_addr will be filled with the address 
*   information of the new connection. sin_size will also have the 
*   corresponding return value.
*
*   Returns -1 on error.
*/
int accept_listen_connection(int listenfd, struct sockaddr_storage *connection_addr, socklen_t *sin_size) {
    int connection_fd = -1;
    *sin_size = sizeof *connection_addr;

    if (connection_addr == NULL || sin_size == NULL) {
        fprintf(stderr, "connection  or sin size is NULL\n");
        return -1;
    }

    connection_fd = accept(listenfd, (struct sockaddr *)connection_addr, sin_size);

    if (connection_fd == -1) {
        perror("accept error");
        connection_addr = NULL;
        return -1;
    }

    return connection_fd;
}

/*
*   From Beej's Chapter 7
*
*   The usual inet_ntop call but it converts from sockaddr_storage
*   to the correct IPv4/IPv6 version automatically
*
*   Returns NULL on error
*/
const char *inet_ntop2(void *addr, char *buf, size_t size) {
    struct sockaddr_storage *sas = addr; // We need to check ss_family
    struct sockaddr_in *sa4; // IPv4 case
    struct sockaddr_in6 *sa6; // IPv6 case

    if (addr == NULL || buf == NULL) {
        fprintf(stderr, "addr or buf is NULL\n");
        return NULL;
    }

    // This has to be void because we will use to store either
    // an IPv4 or IPv6 address which can be either struct id_addr
    // or struct id6_addr. Same reason why addr is void, btw
    void *src; 

    switch (sas->ss_family) {
        case AF_INET:
            sa4 = addr;
            src = &(sa4->sin_addr);
            break;
        case AF_INET6:
            sa6 = addr;
            src = &(sa6->sin6_addr);
            break;
        default:
            fprintf(stderr, "invalid sockaddr_storage\n");
            return NULL;
    }

    return inet_ntop(sas->ss_family, src, buf, size);
}