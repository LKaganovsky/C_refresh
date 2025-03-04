#include "chap04.h"

/*
CHAPTER 4:  Establishing UDP Connections
            A UDP Server (pg. 108 - 112)

To execute: gcc udp_recvfrom.c -o udp_recvfrom
            ./udp_recvfrom

Simple UDP Server. TODO: Fill this in later.
*/

int main(){
    /*
    Winsock initialization for Windows systems, same as chapter 2 example.
    */
#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) {
        fprintf(stderr, "ERROR: Issue with Windows-specific initialization");
        return 1;
    }
#endif

    printf("Getting local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(0, "8080", &hints, &bind_address);

    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "ERROR: Issue with creating socket. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "ERROR: Issue with bind. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    freeaddrinfo(bind_address);

    /*
    Here is where the UDP server begins to diverge from the TCP server. There 
    is no need for listen() or accept()--once the local address is bound, we 
    are free to use recvfrom() to listen to all incoming data. 

    The recvfrom() function works similarly to recv(), except that it also 
    returns the sender's address as well as the received data--kind of like 
    a TCP server's accept() and recv() as one function.
    */
    struct sockaddr_storage client_address;
    socklen_t client_len = sizeof(client_address);
    char read[1024];
    int bytes_read = recvfrom(socket_listen, read, 1024, 0, (struct sockaddr*) &client_address, &client_len);

    printf("Received %d bytes: %.*s\n", bytes_read, bytes_read, read);
    

    /*
    In previous programs, we only used the NI_NUMERICHOST flag to indicate 
    that we wanted the host name returned in a numeric form. Here, we indicate 
    we also want the port number in numeric form.

    Without these values, getnameinfo() would attempt to return a hostname or 
    protocol name if the port number matches an existing protocol. If you do 
    not want a protocol name, pass in the NI_DGRAM flag to inform 
    getnameinfo() that you are working on a UDP port. This may be important 
    for the few protocols that have different ports for TCP and UDP.

    Note also that the client will rarely set its local port number 
    explicitly. The number that getnameinfo() will return here is more likely 
    to be a high number chosen randomly by the client's operating system. Even 
    if the client did select its own port number, the port number seen here 
    might have been changed by network address translation (NAT).
    */
    printf("Remote address is:\n");
    char address_buffer[100];
    char service_buffer[100];
    getnameinfo((struct sockaddr*) &client_address, client_len, address_buffer, 
        sizeof(address_buffer), service_buffer, sizeof(service_buffer), 
        NI_NUMERICHOST | NI_NUMERICSERV);
    printf("%s %s\n", address_buffer, service_buffer);

    /*
    If our server were to send data back to the client, it would use the above 
    address and port number (TODO: So does the port number provided by 
    getnameinfo() work here?)

    It does not, so instead we simply clean up.
    */

    CLOSESOCKET(socket_listen);

#if defined(_WIN32)
    WSACleanup();
#endif    

    printf("Finished.\n");
    return 0;
}