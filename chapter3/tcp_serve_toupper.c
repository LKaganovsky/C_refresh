/* tcp_serve_toupper.c */

#include "chap03.h"
#include <ctype.h>

int main() {
    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d)) {
            fprintf(stderr, "Failed to initialize.\n");
            return 1;
        }        
    #endif

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_protocol = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    printf("Configuring local address...\n");
    struct addrinfo* bind_address;
    if(getaddrinfo(0, "9999", &hints, &bind_address) != 0){
        fprintf(stderr, "ERROR: Issue with getting address. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if(!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "ERROR: issue with creating socket (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    if(bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen) == -1){
        fprintf(stderr, "ERROR: Issue with binding socket. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    freeaddrinfo(bind_address);

    if(listen(socket_listen, 10) == -1) {
        fprintf(stderr, "ERROR: Issue setting socket to listening mode. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    fd_set master;
    FD_ZERO(&master);
    FD_SET(socket_listen, &master);
    SOCKET max_socket = socket_listen;

    printf("Waiting for connections...\n");

    while (1) {
        fd_set reads;
        reads = master;
        if(select(max_socket+1, &reads, 0, 0, 0) < 0){
            fprintf(stderr, "ERROR: Issue with select() function. (%d)\n", GETSOCKETERRNO());
            return 1;
        }

        SOCKET i;
        for(i = 0; i <= max_socket; ++i) {
            if(FD_ISSET(i, &reads)){
                if(i == socket_listen) {
                    struct sockaddr_storage client_address;
                    socklen_t client_len = sizeof(client_address);
                    SOCKET socket_client = accept(i, (struct sockaddr*) &client_address, &client_len);
                    if(!ISVALIDSOCKET(socket_client)){
                        fprintf(stderr, "ERROR: Issue with accept() function. (%d)\n", GETSOCKETERRNO());
                        return 1;
                    }
                    FD_SET(socket_client, &master);
                    if(socket_client > max_socket) max_socket = socket_client;
                    char address_buffer[100];
                    getnameinfo((struct sockaddr*) &client_address, client_len, address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
                    printf("New connection from %s\n", address_buffer);

                }
            } else {
                char read[1024];
                int bytes_received = recv(i, read, 1024, 0);
                if (bytes_received < 1) {
                    FD_CLR(i, &master);
                    CLOSESOCKET(i);
                    continue;
                }  
                int j;
                for (j = 0; j < bytes_received; ++j) {
                    read[j] = toupper(read[j]);
                }

                send(i, read, bytes_received, 0);

            }
        }
    }

    printf("Closing listening socket...\n");
    CLOSESOCKET(socket_listen);

    #if defined(_WIN32)
        WSACleanup();
    #endif

    printf("Finished.\n");
    return 0;
}