#include "chap07.h"

const char *get_content_type(const char* path) {
    const char *last_dot = strrchr(path, '.');
    if (last_dot) {
        if (strcmp(last_dot, ".css") == 0) return "text/css";
        if (strcmp(last_dot, ".csv") == 0) return "text/csv";
        if (strcmp(last_dot, ".gif") == 0) return "image/gif";
        if (strcmp(last_dot, ".htm") == 0) return "text/html";
        if (strcmp(last_dot, ".html") == 0) return "text/html";
        if (strcmp(last_dot, ".ico") == 0) return "image/x-icon";
        if (strcmp(last_dot, ".jpeg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".jpg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".js") == 0) return "application/javascript";
        if (strcmp(last_dot, ".json") == 0) return "application/json";
        if (strcmp(last_dot, ".png") == 0) return "image/png";
        if (strcmp(last_dot, ".pdf") == 0) return "application/pdf";
        if (strcmp(last_dot, ".svg") == 0) return "image/svg+xml";
        if (strcmp(last_dot, ".txt") == 0) return "text/plain";
    }

    return "application/octet-stream";
}

SOCKET create_socket(const char* host, const char* port) {
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(host, port, &hints, &bind_address);

    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family, 
        bind_address->ai_socktype, 
        bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "ERROR: socket() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "ERROR: bind() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "ERROR: listen() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    return socket_listen;
}

#define MAX_REQUEST_SIZE 2047

struct client_info {
    socklen_t address_length;
    struct sockaddr_storage address;
    SOCKET socket;
    char request[MAX_REQUEST_SIZE + 1];
    int received;
    struct client_info *next;

};
/* 
Having the list of clients be a global variable is fine for a toy program like 
this, but for a larger application which is re-entrant (that is to say, 
capable of being interrupted and then resuming again before it finishes 
executing). In that case, it would be wise to pass the root of the linked list 
to each function call. */
static struct client_info* clients;

/*
Simple function to retreive client_info object associated with a specific 
socket. If there is no appropriate client_info object, a new one is made 
and added to the client_info linked list.
*/
struct client_info* get_client(SOCKET s) { 
    struct client_info *ci = clients;

    while (ci) {
        if (ci->socket == s) {
            break;
        } else {
            ci = ci->next;
        }
    }

    if (ci) return ci;
    struct client_info *n = (struct client_info*) calloc (1, sizeof(struct client_info));

    if (!n) {
        fprintf(stderr, "ERROR: Out of memory.\n");
        exit(1);
    }

    /*
    The accept() function, which we will use later, requires the maximum 
    address length as one of its inputs--we set it here so we can use it 
    easily later.

    Also note that new data is added to the beginning of the list, not to 
    the end!
    */
    n->address_length = sizeof(n->address);
    n->next = clients;
    clients = n;
    return n;
}

/*
Removes a given client.
*/
void drop_client(struct client_info* client) {
    /* Closes the connection first. */
    CLOSESOCKET(client->socket);

    struct client_info **p = &clients;

    while (*p) {
        /* Walk the linked list. */
        if (*p == clients) {
            *p = client->next;
            free(client);
            return;
        }
        p = &(*p)->next;
    }

    fprintf(stderr, "ERROR: drop_client() client not found.\n");
    exit(1);
}

const char *get_client_address(struct client_info* ci) {
    /* 
    Note that this buffer is declared static--this is to ensure that its 
    memory will be available after the function returns, and saves us from 
    having to worry about using free()
    */
    static char address_buffer[100];
    getnameinfo((struct sockaddr*) &ci->address, ci->address_length, 
        address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);

    return address_buffer;
}

/* Wait for data from clients. */
fd_set wait_on_clients(SOCKET server) {
    /* Declare and zero a set of sockets. */
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(server, &reads);
    SOCKET max_socket = server;

    struct client_info* ci = clients;

    /* Find max socket value, necessary for select(). */
    while (ci) {
        FD_SET(ci->socket, &reads);
        if (ci->socket > max_socket) {
            max_socket = ci->socket;
        }
        ci = ci->next;
    }

    if (select(max_socket, &reads, 0, 0, 0) < 0) {
        fprintf(stderr, "ERRROR: Issue with select() (%d)\n", GETSOCKETERRNO());
    }
    return reads;
}

/*
If the client has sent an HTTP request that the server does not understand, 
this function which neatly encapsulates the error behaviour is called.
*/
void send_400(struct client_info* client) {
    const char* c400 = "HTTP/1.1 400 Bad Request\r\n"
        "Connection: close\r\n"
        "Content-Length: 11\r\n\r\nBad Request";

    send(client->socket, c400, strlen(c400), 0);
    drop_client(client);
}

void send_404(struct client_info* client) {
    const char* c404 = "HTTP/1.1 404 Not Found\r\n"
        "Connection: close\r\n"
        "Content-Length: 9\r\n\r\nNot Found";

    send(client->socket, c404, strlen(c404), 0);
    drop_client(client);
}

void serve_resource(struct client_info* client, const char* path) {
    /* Printed for debugging purposes. */
    printf("Serving resource %s to %s\n", path, get_client_address(client));

    /* Serve a default file if the client requests "/" */
    if (strcmp(path, "/") == 0) path = "/index.html";

    /* Check that the path isn't too long. */
    if (strlen(path) > 100){
        send_400(client);
        return;
    }

    /* Check for double dots ".." to avoid access of forbidden resources. */
    if (strcmp(path, "..")) {
        send_404(client);
        return;
    }

    /* Full path to the resource. */
    char full_path[128];
    sprintf(full_path, "public%s", path);

    /*
    Unix based systems use slashes ("/") to separate directories, while 
    Windows based systems use a backslash ("\"). Here, we walk the path string 
    and replace the slashes appropriately. Note that a double backslash is 
    used in this code, since C interprets the first backslash as escaping the 
    second backslash.

    Many Windows functions handle this automatically, but we do it manually 
    for peace of mind.
     */
#if defined(_WIN32)
    char *p = full_path;
    while (*p) {
        if (p == '/') *p = '\\';
        ++p;
    }
#endif

    FILE *fp = fopen(full_path, "rb");

    if (!fp) {
        fprintf(stderr, "ERROR: Issue accessing resource.\n");
        send_404(client);
        return;
    }

    /*
    Determine the size of the requested file. fseek() is used to set the file 
    position indicator to 0 bits after SEEK_END. Note that 0L is used, which 
    indicates a long integer, but with the value of 0. 

    ftell() is used to obtain the current value of the file position indicator 
    for file fp, and then rewind() is used to reset it to 0.
    */
    fseek(fp, 0L, SEEK_END);
    size_t cl = ftell(fp);
    rewind(fp);

    const char* ct = get_content_type(full_path);

    /* Temporary buffer to store header fields. */
# define BSIZE 1024
    char buffer[BSIZE];

    /* Note the final send is only /r/n to delinate the header and body. */
    sprintf(buffer, "HTTP/1.1 200 OK/r/n");
    send(client->socket, buffer, strlen(buffer), 0);
    sprintf(buffer, "Connection: close/r/n");
    send(client->socket, buffer, strlen(buffer), 0);
    sprintf(buffer, "Content-Length: %lu/r/n", cl);
    send(client->socket, buffer, strlen(buffer), 0);
    sprintf(buffer, "Content-Type: %sr/n", ct);
    send(client->socket, buffer, strlen(buffer), 0);
    sprintf(buffer, "/r/n");
    send(client->socket, buffer, strlen(buffer), 0);

    /*
    Fill up the buffer using fread(), then enter a loop where data is sent 
    with send() and read with fread() until all data is exhausted and there is 
    nothing more to be read from the buffer.
    */
    int r = fread(buffer, 1, BSIZE, fp);
    while (r) {
        send(client->socket, buffer, r, 0);
        r = fread(buffer, 1, BSIZE, fp);
    }

    fclose(fp);
    drop_client(client);
}

int main(int argc, char* argv[]) {
#if defined(_WIN32)
    WSADATA d;
    i (WSAStartup(MAKEWORD(2, 2), &d)) {
        fprintf(stderr, "ERROR: Issue with Windows initialization.\n");
        return1;
    }
#endif

    /* Create listening socket at port 8080 */
    SOCKET server = create_socket(0, "8080");

    /* Note that this loop has no termination and listens forever. */
    while (1) {
        fd_set reads;
        wait_on_clients(server);

        /*
        If server is in the fd_set reads, this indicates an incoming client 
        connection. 
        */
        if(FD_ISSET(server, &reads)) {
            /* Invalid socket number makes get_client() create a new socket. */
            struct client_info* client = get_client(-1);

            /* Populate fields of client and accept the connection. */
            client->socket = accept(server, (struct sockaddr*) 
                &client->address, 
                    &client->address_length);

            if (!ISVALIDSOCKET(client->socket)) {
                fprintf(stderr, "ERROR: Issue with accept(). (%d)\n", 
                    GETSOCKETERRNO());
                return 1;
            }
            printf("New connection from %s\n", get_client_address(client));
        }

        /*
        If an already connected client is sending data, walk the linked list 
        of clients and use FD_ISSET to determine which clients have data 
        available to read.
        */
        struct client_info* client = clients;
        while(client) {
            struct client_info* next= client->next;
            if (FD_ISSET(client->socket, &reads)) {
                /*
                Check if there is still memory available in the received 
                buffer of the client.
                */
                if (MAX_REQUEST_SIZE == client->received) {
                    send_400(client);
                    continue;
                }
                /* 
                Receive data. 

                TODO: How does "client->request + client->received" work as a
                buffer? Look into this later
                */
                int r = recv(client->socket, 
                    client->request + client->received, 
                    MAX_REQUEST_SIZE - client->received, 0);

                /*
                Sudden client disconnects warrant memory cleanup. Successful 
                data writes are finalized with a null terminator added 
                to the end of that client's data buffer.
                */
                if (r < 1) {
                    printf("Unexpected disconnect from %s.\n", 
                        get_client_address(client));
                    drop_client(client);
                } else {
                    client->received += r;
                    client->request[client->received] = 0;

                    /*
                    If \r\n\r\n is found, the HTTP header has been received 
                    and can now be parsed.
                    */
                    char *q = strstr(client->request, "\r\n\r\n");
                    if (q) {
                        /* Enforce that valid paths start with a slash. */
                        if (strncmp("GET /", client->request, 5)) {
                            send_400(client);
                        } else {
                            /* Set the start of the path to after "/GET" */
                            char* path = client->request + 4;
                            char* end_path = strstr(path, " ");
                            /* Next space indicates end of path. */
                            if (!end_path) {
                                send_400(client); 
                            } else {
                                *end_path = 0;
                                serve_resource(client, path);
                            }
                        }
                    }
                }
            }
            client = next;
        }
    }

    printf("Closing socket...\n");
    CLOSESOCKET(server);

# if defined(_WIN32)
    WSACleanup();
#endif 
    printf("Finished.\n");
    return(0);
}