#include "http-server.h"
#include "arraylist.h"
#include "http-util.h"
#include "sock-util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>

#define INIT_CAP 8 // Number of connections server can initially accept

// --- server commands ---

static const char *HELP_CMD = "help\n";
static const char *CONNECTIONS_CMD = "connections\n";
static const char *EXIT_CMD = "quit\n";
static const char *UNRECOGNIZED_CMD = "unrecognized command\n";
static const char *SERVER_CMD = "server\n";
static const char *UPTIME_CMD = "uptime\n";
static const char *NOTIFS_CMD = "notifs\n";

// --- other static variables ---

static int MSG_SILENCE = 1; // for Http_Server_printmsg

int Http_Server_init(Http_Server *server, const char *ip, const char *port, int backlog) {
    int listenfd = 0;
    struct pollfd *listenpoll;

    if (port == NULL || server == NULL) {
        fprintf(stderr, "Http_Server_init: port or server was NULL\n");
        return -1;
    }
    if (backlog <= 0) {
        fprintf(stderr, "Http_Server_init: invalid backlog %d\n", backlog);
        return -1;
    }

    // =---- Start listening socket ----=

    listenfd = get_listener_socket(ip, port, backlog);
    if (listenfd == -1) {
        fprintf(stderr, "Http_Server_init: fail to listen\n");
        return -1;
    }

    // =---- Set server params ----=

    if (ip != NULL) { // Passed ip may be NULL
        server->ip = strdup(ip);
        if (!server->ip) {
            fprintf(stderr, "Http_Server_init: ip strdup() fail\n");
            return -1;
        }
    } else {
        server->ip = NULL; // Indicate that we are using local IP. Be weary of this
    }
    server->port = strdup(port);
    if (!server->port) {
        fprintf(stderr, "Http_Server_init: port strdup() fail\n");
        if (server->ip != NULL) { // Only free if IP was not NULL
            free(server->ip);
        }
        return -1;
    }
    server->listenfd = listenfd;
    server->backlog = backlog;

    // Setup server pfd array
    listenpoll = malloc(sizeof *listenpoll * INIT_CAP);
    if (listenpoll == NULL) {
        fprintf(stderr, "Http_Server_init: listenpoll malloc() fail\n");

        if (server->ip != NULL) { // Only free if IP was not NULL
            free(server->ip);
        }
        free(server->port);
        close(server->listenfd);

        return -1;
    }
    // Listener socket goes in pdf[0]
    listenpoll[0].fd = listenfd;
    listenpoll[0].events = POLLIN;
    listenpoll[0].revents = 0;
    // stdin goes in pfd[1] for server commands
    listenpoll[1].fd = STDIN_FILENO;
    listenpoll[1].events = POLLIN;
    listenpoll[1].revents = 0;

    // Initialize connection_info array list
    // Not sure what to put in as the free function right now
    server->connection_info = ArrayList_new(free);

    // Set params
    server->pfds = listenpoll;
    server->nfds = 2; // 1 for the listener and stdin
    server->cap = INIT_CAP;
    // Metadata
    server->bytes_served = 0;
    server->requests_served = 0;
    server->connections_served = 0;

    server->connection_count = 0;
    server->start_time = time(NULL);
    
    return 1;
}

int Http_Server_addpfd(Http_Server *server, int new_fd) {
    if (server == NULL) {
        fprintf(stderr, "Http_Server_addpf: server was NULL\n");
        return -1;
    }

    // If at capacity, increase
    if (server->cap == server->nfds) {
        server->cap *= 2; // Double I guess
        struct pollfd *temp = realloc(server->pfds, server->cap * sizeof *server->pfds);
        if (temp == NULL) {
            fprintf(stderr, "Http_Server_addpfd: realloc() fail\n");
            return -1;
        }
        server->pfds = temp;
    }

    server->pfds[server->nfds].fd = new_fd;
    server->pfds[server->nfds].events = POLLIN; // Ready to receive
    server->pfds[server->nfds].revents = 0;
    server->nfds++;

    return 1;
}

int Http_Server_removepfd(Http_Server *server, int index) {
    if (server == NULL) {
        fprintf(stderr, "Http_Server_removefd: server was NULL\n");
        return -1;
    }
    if (index >= server->nfds) {
        fprintf(stderr, "Http_Server_removefd: invalid index\n");
        return -1;
    }

    server->pfds[index] = server->pfds[server->nfds - 1];
    server->nfds--;

    return 1;
}

int Http_Server_newconnection(Http_Server *server) {
    Http_Server_Connection_Info* info = 
        malloc(sizeof(Http_Server_Connection_Info));

    struct sockaddr_storage connection_addr;
    socklen_t sin_size;
    int acceptfd = 0;

    if (server == NULL) {
        fprintf(stderr, "Http_Server_newconnection: server was NULL\n");
        return -1;
    }
    if (info == NULL) {
        fprintf(stderr, "Http_Server_newconnection: info malloc() fail\n");
        return -1;
    }

    // Accept new connection
    sin_size = sizeof connection_addr;
    acceptfd = accept_listen_connection(server->listenfd, &connection_addr, &sin_size);
    if (acceptfd == -1) { // Failed to connect
        perror("accept"); 
        return -1;
    }
    if (Http_Server_addpfd(server, acceptfd) == -1) {
        fprintf(stderr, "Http_Server_newconnection: fail to add pfd\n");
        return -1;
    };

    // Add to server connectio info list
    info->fd = acceptfd;
    info->connection_info = connection_addr;
    if (ArrayList_insert(server->connection_info, info) == 0) {
        // If this happens, some cleanup has to occur
        // or maybe the connection just doesn't have information
        // about it listed 
        fprintf(stderr, "Http_Server_newconnection: fail to add connection info\n");
        return -1; 
    } 

    server->connection_count += 1;

    Http_Server_printmsg("server: we have a new connection!");
    server->connections_served += 1;

    return 1;
}

int Http_Server_disconnect_client(Http_Server *server, int client_index) {
    int fd = 0;

    if (server == NULL) {
        fprintf(stderr, "Http_Server_disconnect_client: server was NULL\n");
        return -1;
    }
    if (client_index < 0 || client_index >= server->nfds) {
        fprintf(stderr, "Http_Server_disconnect_client: invalid client index\n");
        return -1;
    }

    fd = server->pfds[client_index].fd;
    close(fd);

    if (Http_Server_remove_connection_info(server, fd) == -1) {
        fprintf(stderr, "Http_Server_disconnect_client: failed to remove connection info for fd %d\n", fd);
    }
    if (Http_Server_removepfd(server, client_index) == -1) {
        fprintf(stderr, "Http_Server_disconnect_client: failed to remove pollfd for index %d\n", client_index);
        return -1;
    }
    if (server->connection_count > 0) {
        server->connection_count -= 1;
    }

    return 1;
}

// Handle receiving http request and send out response here
int Http_Server_handleclient(Http_Server *server, int client_index, char *dir) {
    char buf[4096 * 2];
    int bytes_received;

    if (server == NULL) {
        fprintf(stderr, "Http_Server_handleclient: server was NULL\n");
        return -1;
    }
    if (client_index >= server->nfds) {
        fprintf(stderr, "Http_Server_handleclient: invalid client index\n");
        return -1;
    }

    // get bytes
    bytes_received = recv(server->pfds[client_index].fd, buf, sizeof(buf), 0);
    if (bytes_received < 0) {
        // sometimes, when the client disconnects in a certain way, recv can fail with 
        // ECONNRESET or EPIPE. this isn't really a problem, so just ignore these errors
        if (errno != ECONNRESET && errno != EPIPE) {
            perror("Http_Server_handleclient: recv error");
        }
        Http_Server_disconnect_client(server, client_index);
        return -1;
    }
    if (bytes_received == 0) {
        Http_Server_printmsg("server: client closed connection");
        Http_Server_disconnect_client(server, client_index);
        return 0;
    }

    // Null terminate buffer. Needed for upcoming function calls
    // just hope the request fits the buffer
    buf[bytes_received] = '\0';

    // Build request and response
    Http_Request req;
    Http_Response res;
    char *tosend = NULL;
    size_t send_len = 0;
    size_t sent = 0;

    if (new_request(&req, buf) != HTTP_UTIL_OK) {
        fprintf(stderr, "Http_Server_handleclient: failed to parse request; closing connection\n");
        Http_Server_disconnect_client(server, client_index);
        return -1;
    }
    // print_request(&req); // debug
    // RESPONSE HERE
    if (new_response(&res, &req, dir) != HTTP_UTIL_OK) {
        fprintf(stderr, "Http_Server_handleclient: failed to make response; closing connection\n");
        Http_Server_disconnect_client(server, client_index);
        return -1;
    }
    if (response_mssg(&tosend, &send_len, &res) != HTTP_UTIL_OK) {
        fprintf(stderr, "Http_Server_handleclient: failed to build response message; closing connection\n");
        Http_Server_disconnect_client(server, client_index);
        return -1;
    }

    // Send everything out
    while (sent < send_len) {
        size_t n = send(server->pfds[client_index].fd, tosend + sent, send_len - sent, 0);
        if (n < 0) {
            perror("send");
            break;
        }
        sent += (size_t)n;
        server->bytes_served += n; // update metadata
    }

    // debug
    // print_response(&res);

    free_request(&req);
    free_response(&res);
    server->requests_served += 1; // update metadata

    return 1;
}

int Http_Server_handlestdin(Http_Server *server) {
    char buf[4096]; // arbitrary size

    fgets(buf, sizeof(buf), stdin);
    if (strcmp(buf, HELP_CMD) == 0) {
        Http_Server_printhelp();
        printf("\n");
        Http_Server_cursor();
    } else if (strcmp(buf, CONNECTIONS_CMD) == 0) {
        printf("\n   Active connections: %d\n\n", server->connection_count);
        Http_Server_print_connections(server);
        printf("\n");
        Http_Server_cursor();
    } else if (strcmp(buf, EXIT_CMD) == 0) {
        Http_Server_exit(server);
    } else if ((strcmp(buf, SERVER_CMD) == 0)) {
        // debug command to print server info
        printf("\n   Server IP: %s\n", server->ip ? server->ip : "local IP");
        printf("   Server port: %s\n", server->port);
        printf("   Backlog: %d\n", server->backlog);
        printf("\n");
        printf("   Bytes served: %lld\n", server->bytes_served);
        printf("   Requests served: %lld\n", server->requests_served);
        printf("   Connections served: %lld\n", server->connections_served);
        printf("\n");
        Http_Server_cursor();
    } else if (strcmp(buf, UPTIME_CMD) == 0) {
        time_t now = time(NULL);
        double uptime = difftime(now, server->start_time);
        printf("\n   server uptime: %.2f seconds\n\n", uptime);
        Http_Server_cursor();
    } else if ((strcmp(buf, "exit\n") == 0)) {
        printf("did you mean \"quit\"?\n");
        Http_Server_cursor();
    } else if ((strcmp(buf, NOTIFS_CMD) == 0)) {
        MSG_SILENCE = !MSG_SILENCE;
        if (MSG_SILENCE) {
            printf("server notifications silenced\n");
        } else {
            printf("server notifications enabled\n");
        }
        Http_Server_cursor();
    } else {
        printf("%s", UNRECOGNIZED_CMD);
        Http_Server_cursor();
    }

    return 1;
}

int Http_Server_processconnections(Http_Server *server, char *dir) {
    if (server == NULL) {
        fprintf(stderr, "Http_Server_processconnections: server was NULL\n");
        return -1;
    }

    // well sometimes the list shrinks and a later index can become 
    // invalid which gets a error message printed. this isn't a big deal
    // but I don't wanna see the message printed so
    // int original_nfds = server->nfds; // nfds can increase while processing

    for (int i = 0; i < server->nfds; i++) {
        // Check listen or read? Not sure here
        if (server->pfds[i].revents & (POLLIN | POLLHUP)) {
            // If listen event
            if (server->pfds[i].fd == server->listenfd) {
               if (Http_Server_newconnection(server) == -1) {
                    fprintf(stderr, "Http_Server_processconnections: newconnection fail\n");
                    return -1;
               }
            } else if (server->pfds[i].fd == STDIN_FILENO) {
                Http_Server_handlestdin(server);
            } else { 
                Http_Server_handleclient(server, i, dir);
            }
        }
    }

    return 1;
}

int Http_Server_remove_connection_info(Http_Server *server, int fd) {
    if (server == NULL) {
        fprintf(stderr, "Http_Server_remove_connection_info: server was NULL\n");
        return -1;
    }

    for (uint32_t i = 0; i < ArrayList_size(server->connection_info); i++) {
        Http_Server_Connection_Info* info = ArrayList_get(server->connection_info, i);
        if (info->fd == fd) {
            // debug
            // printf("removing connection info for fd %d\n", fd);

            ArrayList_remove(server->connection_info, i);
            free(info); // free() I am pretty sure
            return 1;
        }
    }

    fprintf(stderr, "Http_Server_remove_connection_info: no connection info found for fd %d\n", fd);
    return -1;
}

void Http_Server_print_connections(Http_Server *server) {
    if (server == NULL) {
        fprintf(stderr, "Http_Server_print_connections: server was null\n");
        return;
    }
    if (server->connection_info == NULL) {
        fprintf(stderr, "Http_Server_print_connections: connection_info was NULL\n");
        return;
    }

    ArrayList* connections = server->connection_info;
    Http_Server_Connection_Info *info = NULL;
    struct sockaddr_storage *store = NULL;
    socklen_t len = sizeof(*info);
    char connection_ip[4096];
    char port_info[4096];

    for (int i = 0; i < ArrayList_size(connections); ++i) {
        info = (Http_Server_Connection_Info*)ArrayList_get(connections, i);
        if (info == NULL) {
            fprintf(stderr, "Http_Server_print_connections: info for %d was NULL\n", i);
            continue;
        }
        store = &info->connection_info;

        int ret = getnameinfo(
            (struct sockaddr *)store, len,
            connection_ip, sizeof(connection_ip),
            port_info, sizeof(port_info),
            NI_NUMERICHOST | NI_NUMERICSERV
        );

        if (ret != 0) {
            fprintf(stderr, "Http_Server_print_connections: %s\n", gai_strerror(ret));
            return;
        }

        printf("   %03d  Connection IP: %s\n", i+1, connection_ip);
        printf("        Connected to port: %s\n", port_info);
        if (i != ArrayList_size(connections) - 1) {
            printf("\n");
        }
    }

    if (ArrayList_size(connections) == 0) {
        printf("   [No active connections]\n");
    }
}

void Http_Server_exit(Http_Server *server) {
    close(server->listenfd);
    printf("   exiting. cya lata\n");
    printf("\nsession information\n");
    printf("   bytes sent: %lld\n", server->bytes_served);
    printf("   requests served: %lld\n", server->requests_served);
    printf("   connections made: %lld\n", server->connections_served);
    exit(1);
}

void Http_Server_printmsg(const char *msg) {
    if (!MSG_SILENCE) {
        printf("\n%s\n", msg);
        Http_Server_cursor();
    }
}

void Http_Server_printhelp() {  
    printf("\n");
    printf("   %-15s %s\n", "commands", "description");
    printf("   %-15s %s\n", "--------", "-----------");
    printf("   %-15s %s\n", "help", "this message");
    printf("   %-15s %s\n", "connections", "active connections");
    printf("   %-15s %s\n", "quit", "exit the program");
    printf("   %-15s %s\n", "server", "server information");
    printf("   %-15s %s\n", "uptime", "server uptime");
    printf("   %-15s %s\n", "notifs", "toggle notifications");
}

void Http_Server_cursor() {
    printf(">> ");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    Http_Server server;
    struct stat sta;
    const char *port = "3490"; 
    char *dir = "./content"; // Default to example

    if (argc > 1) {
        dir = argv[1];
    }
    if (argc > 2) {
        port = argv[2];
    }

    // Make sure this exists
    if (stat(dir, &sta) == -1 || !S_ISDIR(sta.st_mode)) {
        fprintf(stderr, "error: '%s' is not a valid directory\n", dir);
        return 1;
    }

    printf("starting server...\n");
    if (Http_Server_init(&server, NULL, port, 10) == -1) {
        printf("main: failed to initialize http server. exiting\n");
        return 0;
    }
    printf("   initialized successfully\n");
    printf("   listening on port %s\n", port);
    printf("   serving files from: %s\n", dir);
    printf("\n");
    printf("hint: type 'help' for a list of commands\n");
    Http_Server_cursor();

    while (1) {
        poll(server.pfds, server.nfds, -1);
        Http_Server_processconnections(&server, dir);
    }

    // unreachable
    return 0;
}