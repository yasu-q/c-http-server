#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

/*
*   HTTP server logic stuff
*/

#include "arraylist.h"
#include "http-util.h"
#include "sock-util.h"

// =------ typedefs and structs ------=

typedef struct {
    char *ip;
    char *port;
    int listenfd;
    int backlog;
    int connection_count;
    time_t start_time;

    // Multiplexing stuff
    struct pollfd *pfds;
    int nfds; // Number of file descriptors
    int cap;  // Maximum number of connections

    // Connection information
    ArrayList* connection_info;

    // Metadata
    long long bytes_served;
    long long requests_served;
    long long connections_served;
} Http_Server;

typedef struct {
    int fd;
    struct sockaddr_storage connection_info;
} Http_Server_Connection_Info;

// =------ functions ------=

/*
*   Return a malloc'd Http_Server based on the passed information bound to
*   a listening port. Expects passed server to be allocated i.e. non-null.
*
*   Will also insert a poll fd into pfds setup with the listening socket
*   socketfd. i.e. pfds will have 1 element already placed inside of it.
*   malloc'd
*;   
*   -1 on fail 1 on success. TODO: better errors
*/
int Http_Server_init(Http_Server *server, const char *ip, const char *port, int backlog);

/*
*   From Beej's chapter 7   
*
*   Add a new pfd into our pfd list.
*
*   -1 on fail 1 on success
*/
int Http_Server_addpfd(Http_Server *server, int newfd);

/*
*   From Beej's chapter 7
*
*   Remove pfd  
*
*   Return -1 on fail 1 otherwise
*/
int Http_Server_removepfd(Http_Server *server, int index);

/*
*   From Beej's chapter 7
*
*   Handles new connections
*
*   Return -1 on fail 1 otherwise
*/
int Http_Server_newconnection(Http_Server *server);

/*
*   From Beej's chapter 7
*
*   Handle connections 
*
*   Return -1 on fail 1 otherwise
*/
int Http_Server_processconnections(Http_Server *server, char *dir);

/*
*   Handle STDIN input
*/
int Http_Server_handlestdin(Http_Server *server);

/*
*   Handle client connection
*/
int Http_Server_handleclient(Http_Server *server, int client_index, char *dir);

/*
*   Close and remove a client by pollfd index.
*   Keeps pollfds, connection_info, and connection_count in sync.
*/
int Http_Server_disconnect_client(Http_Server *server, int client_index);

/*
*   Allocate new connection info struct
*/
// Http_Server_Connection_Info* Http_Server_new_connection_info(int fd, struct sockaddr_storage info);

/*
*   Remove connection info given specific fd
*/
int Http_Server_remove_connection_info(Http_Server *server, int fd);

/*
*   Print connections
*/
void Http_Server_print_connections(Http_Server *server);

/*
*   Exit the program gracefully
*/
void Http_Server_exit(Http_Server *server);

/*
*   Print server messages without messing up the cursor
*/
void Http_Server_printmsg(const char* msg);

/*
*   Print help information
*/
void Http_Server_printhelp();

/*
*   Print >> to stdout
*/
void Http_Server_cursor();

 

#endif