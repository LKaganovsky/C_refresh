#if defined(_WIN_32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#endif

#if defined (_WIN32)
#define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)    // Why is s in an extra set of brackets?
#define CLOSESOCKET(s) closesocket(s)
#define GETSOCKETERRNO(s) (WSAGetLastError(s))

#else
#define ISVALIDSOCKET(s) ((s) != 0)     // Why is s in an extra set of brackets?
#define CLOSESOCKET(s) close(s) 
#define GETSOCKETERRNO(s) (errno)       // Macro for the global variable
#define SOCKET int
#define LISTENING_PORT "9999"

#endif

#include <stdio.h>
#include <string.h>
#include <time.h>

int main(){
    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d) != 0) {
            fprintf(stderr, stderr, "ERROR: WinSock initialization failed!\n");
            return 1;
        }
    #endif

    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bindings;
    if (getaddrinfo(0, LISTENING_PORT, &hints, &bindings)) {
        fprintf(stderr, "ERROR: getaddrinfo failure. (%d)\n", GETSOCKETERRNO());    // Does this work?
    }

    printf("Creating socket...\n");
    SOCKET socket_listener;
    socket_listener = socket(bindings->ai_family, bindings->ai_socktype, bindings->ai_protocol);

    if(!ISVALIDSOCKET(socket_listener)) {
        fprintf(stderr, "ERROR: issue with socket creation. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    printf("Binding socket to local address...\n");
    if(bind(socket_listener, bindings->ai_addr, bindings->ai_addrlen)) {
        fprintf(stderr, "ERROR: issue with binding socket to local address. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    printf("Listening for incoming connections...\n");
    if(listen(socket_listener, 10)){
        fprintf(stderr, "ERROR: issue with listening for new connections. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    
    printf("Waiting for an incoming connection...\n");
    struct sockaddr_storage client_address_storage;
    socklen_t socket_length = sizeof(client_address_storage);

    // Define variable for socket length, cast client address storage as (struct sockaddr*)?
    SOCKET connected_client = accept(socket_listener, (struct sockaddr *) &client_address_storage, &socket_length);

    if(!ISVALIDSOCKET(connected_client)) {
        fprintf(stderr, "ERROR: Issue with accepting connection.(%d)\n", GETSOCKETERRNO());
        return 1;
    }

    printf("A client has connected!\n");
    char client_address_buffer[100];
    getnameinfo((struct sockaddr *) &client_address_storage, socket_length, client_address_buffer, 
    sizeof(client_address_buffer), 0, 0, NI_NUMERICHOST);
    printf("%s\n", client_address_buffer);

    printf("Receiving a message...\n");
    char request_buffer[1000];
    int request_length = recv(connected_client, request_buffer, sizeof(request_buffer), 0);
    if (request_length == -1){
        printf("ERROR: Issue with receiving message from client.\n");
        return 1;
    }

    // Recall that there is no guarantee that the data received from recv() is null terminated.
    // request_length added so this can actually be printed (previously the return value from)
    // recv() was used just
    printf("%.*s", request_length, request_buffer);

    printf("Sending response...\n");
    const char* response = 
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n\r\n"
        "Current time is: ";

    if(send(connected_client, response, strlen(response), 0) == -1){
        printf("ERROR: Issue with sending response to client.\n");
        return 1;
    }

    printf("Getting time...\n");
    time_t current_time;
    time(&current_time);
    char* current_time_message = ctime(&current_time);
    int bytes_sent = send(connected_client, current_time_message, strlen(current_time_message), 0);
    if (bytes_sent != (int) strlen(current_time_message)) {
        printf("ERROR: Did not send entire time message.\n");
        return 1;
    }

    printf("Closing connection to client.\n");
    CLOSESOCKET(connected_client);

    #if defined(_WIN32)
        WSACleanup();
    #endif

    printf("Program finished.\n");
    return 0;
}