/* tcp_serve_chat.c */

/*
CHAPTER 3:  An In-Depth Overview of TCP Connections
            Building A Chatroom (pg. 96 - 97)

To execute: gcc tcp_serve_chat.c -o tcp_serve_chat
            ./tcp_serve_chat

This program is a modified version of tcp_serve_toupper.c which allows for
messages being sent to the server to be echoed over to all connected clients 
as a rudimentary chat room.

Duplicate comments from tcp_serve_toupper.c removed.

*/

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

    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* bind_address;
    getaddrinfo(0, "8080", &hints, &bind_address);

    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, 
    bind_address->ai_protocol);
    if(!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "ERROR: issue with creating socket (%d)\n", 
        GETSOCKETERRNO());
        return 1;
    }

    if(bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)){
        fprintf(stderr, "ERROR: Issue with binding socket. (%d)\n", 
        GETSOCKETERRNO());
        return 1;
    }

    freeaddrinfo(bind_address);

    if(listen(socket_listen, 10) == -1) {
        fprintf(stderr, "ERROR: Issue setting socket to listen mode. (%d)\n", 
        GETSOCKETERRNO());
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
        if (select(max_socket+1, &reads, 0, 0, 0) < 0 ) {
            fprintf(stderr, "ERROR: Issue with select(). (%d)\n", 
            GETSOCKETERRNO());
            return 1;
        }

        SOCKET i;
        for(i = 1; i <= max_socket; ++i) {
            if (FD_ISSET(i, &reads)) {
                if (i == socket_listen) {
                    struct sockaddr_storage client_address;
                    socklen_t client_len = sizeof(client_address);
                    SOCKET socket_client = accept(socket_listen, 
                    (struct sockaddr*) &client_address, &client_len);
                    if (!ISVALIDSOCKET(socket_client)) {
                        fprintf(stderr, "ERROR: Issue with client socket."
                        "(%d)\n", GETSOCKETERRNO());
                        return 1;
                    }
                    FD_SET(socket_client, &master);

                    if (socket_client > max_socket) {
                        max_socket = socket_client;
                    }

                    char address_buffer[100];
                    getnameinfo((struct sockaddr*) &client_address, 
                    client_len, address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
                    printf("New connection made from %s\n", address_buffer);

                } else {
                    char read[1024];
                    int bytes_received = recv(i, read, 1024, 0);
                    if (bytes_received < 1) {
                        FD_CLR(i, &master);
                        CLOSESOCKET(i);
                        continue;
                    }

                    /*
                    Loop through all of the sockets in the master set--check 
                    if each socket is the listening socket or the same socket 
                    that is sending data. If it's neither, send data to that 
                    client.

                    TODO: Interesting bug here. With 3 terminals, one running 
                    the server, two acting as clients. Client 1 and client 2 
                    both initiate connections, server indicates two 
                    connections have come in. Client 1 sends some data, which 
                    is seemingly not received by either server or client 2. 
                    But when client 2 sends data, client 1 receives it.

                    Adding a third client--When connecting to the server in 
                    order (1, 2, and then 3), client 1 can send and receive 
                    from 2 and 3, client 2 can send and receive from 1 and 3, 
                    but client 3 can sent to 1 and 2 but cannot receive from
                    either. Sort out whatever nonsense is going on with the
                    file descriptors later.
                    */
                    SOCKET j;
                    for(j = 1; j < max_socket;j++){
                        if(FD_ISSET(j, &master)) {
                            if(j == socket_listen || j == i) {
                                continue;
                            } else {
                                send(j, read, bytes_received, 0);
                            }
                        }
                    }
                }
            }   
        }
    }

    printf("Closing listening socket...\n");
    CLOSESOCKET(socket_listen);

    #if defined(_WIN21)
        WSACleanup();
    #endif

    printf("Finished.\n");
    return 0;
}