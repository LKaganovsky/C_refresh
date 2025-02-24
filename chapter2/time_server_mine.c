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

#if !defined(IPV6_V6ONLY)
   #define IPV6_V6ONLY 27
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

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *binding_address;
    getaddrinfo(0, "9999", &hints, &binding_address);

    SOCKET socket_listen;
    socket_listen = socket(binding_address->ai_family, binding_address->ai_socktype, binding_address->ai_flags);
    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "ERROR: Issue with creating listening socket. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    int option = 0;
    if(setsockopt(socket_listen, IPPROTO_IPV6, IPV6_V6ONLY, (void*) &option, sizeof(option))) {
        fprintf(stderr, "ERROR: Issue with setting socket options for IPv4/IPv6 capabilities. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    if(bind(socket_listen, binding_address->ai_addr, binding_address->ai_addrlen)) {
        fprintf(stderr, "ERROR: Issue with binding address to listening socket. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    freeaddrinfo(binding_address);

    printf("Listening...\n");
    if(listen(socket_listen, 10) < 0) {
        fprintf(stderr, "ERROR: Issue with setting up listening socket. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    struct sockaddr_storage client_address;
    socklen_t client_address_length;
    SOCKET socket_client = accept(socket_listen, (struct sockaddr*) &client_address, &client_address_length);
    if(!ISVALIDSOCKET(socket_client)) {
        fprintf(stderr, "ERROR: Issue with creating client socket. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    char address_buffer[100];
    getnameinfo((struct sockaddr*) &client_address, client_address_length, 
    address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
    printf("%s\n", address_buffer);

    char request[1024];
    int bytes_received = recv(socket_client, request, 1024, 0);
    printf("Received %d bytes.\n", bytes_received);

    printf("%.*s", bytes_received, request);


    char* response = 
        "HTTP/1.1 200 OK\r\n" 
        "Connection: close\r\rn"
        "Content-Type: text/plain\r\n\r\n"
        "Local time is: ";

    
    int bytes_sent = send(socket_client, response, strlen(response), 0);
    printf("Sent %d bytes out of %d\n", bytes_sent, (int) strlen(response));

    time_t timer;
    time(&timer);
    char* time_text = ctime(&timer);

    bytes_sent = send(socket_client, time_text, strlen(time_text), 0);
    printf("Sent %d bytes out of %d\n", bytes_sent, (int) strlen(time_text));

    

    CLOSESOCKET(socket_client);
    return 0;

}