/* tcp_serve_toupper.c */

/*
CHAPTER 3:  An In-Depth Overview of TCP Connections
            A TCP Server (pg. 88 - 95)

To execute: gcc tcp_serve_toupper.c -o tcp_serve_toupper
            ./tcp_serve_toupper

This program creates a TCP server to complement the client program. It 
performs a very simple service, echoing back a capitalized version of whatever 
string it is sent, as a proof of concept for TCP client-server networking 
utilizing the terminal.

*/

#include "chap03.h"
#include <ctype.h>

int main() {
    /*
    Winsock initialization for Windows systems, same as chapter 2 example.
    */
    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d)) {
            fprintf(stderr, "Failed to initialize.\n");
            return 1;
        }        
    #endif

    /*
    Get local address, create socket and bind the address to the socket with 
    the bind() function, same as previous examples.

    Note that hints.ai_family is set to AF_INET, so this program only works 
    with IPv4. Use AF_INET6 for IPv6.
    */
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

    /*
    Now, we define our fd_set which will hold all file descriptors for active 
    sockets. Currently it only holds one socket, the listening socket that is 
    monitoring for incoming connections. As such, the listening socket must 
    have the largest file descriptor and therefore mac_socket can be set to 
    its value.
    */
    fd_set master;
    FD_ZERO(&master);
    FD_SET(socket_listen, &master);
    SOCKET max_socket = socket_listen;

    printf("Waiting for connections...\n");

    /*
    Since select() modifies the fd_sets it is given, we preserve our original 
    set of file descriptors (master) by making a copy (reads). The select() 
    function is also passed a timeout value of 0 to prevent it from returning 
    until a socket in the master set has data to read.

    Now, we loop through each possible socket and check if select() has 
    flagged it as being ready to read from. This can be determined by checking 
    if FD_ISSET(i, &reads) is true, and since sockets are sequential, i can
    be incremented repeatedly until it reaches the value of the largest socket 
    in the set.
    */
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
            /*
            FD_ISSET is only true for sockets that are ready to be read--if it 
            is true for socket_listen, that means that a new connection is 
            ready to be established with accept(). If it is true for any other 
            socket, that indicates that a socket has data to be read.
            */
            if (FD_ISSET(i, &reads)) {
                /*
                Check if the socket that has data to read is the listening 
                socket (if so, this is a new connection).
                */
                if (i == socket_listen) {
                    /*
                    Accept the connection and add the new connection to the 
                    master socket set. max_socket is also increased if 
                    necessary.
                    */
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
                    /*
                    Print out the information of the client, not necessary 
                    but helpful for debugging.
                    */
                    char address_buffer[100];
                    getnameinfo((struct sockaddr*) &client_address, 
                    client_len, address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
                    printf("New connection made from %s\n", address_buffer);
                /*
                If the socket that has data to read is not the listening 
                socket, it is a client sending data. In this case, we perform 
                the intended function of this program--reading the data with
                recv().

                If the client has disconnected, then recv() will return -1, 
                and this indicates to the server that that socket should be 
                removed from the master set.
                */
                } else {
                    char read[1024];
                    int bytes_received = recv(i, read, 1024, 0);
                    if (bytes_received < 1) {
                        FD_CLR(i, &master);
                        CLOSESOCKET(i);
                        continue;
                    }

                    /*
                    Perform the actual function of the server--use toupper() 
                    to convert the given string to uppercase. and send it.
                    */
                    int j;
                    for (j = 0; j < bytes_received; ++j) {
                        read[j] = toupper(read[j]);
                    }
                    send(i, read, bytes_received, 0);

                }
            }   
        }
    }

    /*
    Perform necessary cleanup and end program. This code will never actually 
    run because there is no exit to the while(1) loop above. But in the 
    interests of good practice, it is put here anyway.
    */

    printf("Closing listening socket...\n");
    CLOSESOCKET(socket_listen);

    #if defined(_WIN21)
        WSACleanup();
    #endif

    printf("Finished.\n");
    return 0;
}