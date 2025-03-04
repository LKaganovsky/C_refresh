/* udp_sendto.c */

#include "chap04.h"

int main(){
#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD, 2, 2, &d)) {
        fprintf(stderr, "ERROR: Issue with Windows-specific behaviour.\n");
        return 1;
    }
#endif

    /*
    The remo address 127.0.0.1 and port 8080 are hard-coded. This means that 
    this program will only connect to the UDP server if it is running on the 
    same computer. 

    Note also that ai_socktype = SOCK_DGRAM is the only field set in hints, 
    setting AF_INET/AF_INET6 is not necessary as getaddrinfo() will return 
    the appropriate address for IPv4/IPv6 based on the one directly given to 
    it. In this case, it will be IPv4 since 127.0.0.1 is passed into it.
    */
    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo *peer_address;
    if (getaddrinfo("127.0.0.1", "8080", &hints, &peer_address)) {
        fprintf(stderr, "ERROR: Issue configuring remote address. (%d)\n", 
            GETSOCKETERRNO());
        return 1;
    }

    /*
    Print remote address.
    */
    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];
    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen, 
        address_buffer, sizeof(address_buffer), service_buffer, 
        sizeof(service_buffer), NI_NUMERICHOST | NI_NUMERICSERV);
    printf("%s %s\n", address_buffer, service_buffer);

    /*
    Create socket.
    */
    printf("Creating socket...\n");
    SOCKET socket_peer;
    socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype, 
        peer_address->ai_protocol);
    if(!ISVALIDSOCKET(socket_peer)) {
        fprintf(stderr, "ERROR: Issue creating socket. (%d)\n", 
            GETSOCKETERRNO());
        return 1;
    }

    /*
    Send data. Since sendto() is used, connect() is not necessary. Note that 
    in the event of an error, no error code will be returned--sendto() is a 
    best effort service that will not do anything about a message being lost 
    or misdelivered on the way to its intended destination. It is up to the 
    application as a whole to figure out a way to deal with data loss.

    Provided the type of address is the same (IPv4), peer_socket can be reused 
    to send data to another address, or receive data with recvfrom(). Note 
    that with recvfrom(), data can be received from any peer--not necessarily 
    the server we just sent to!

    When we send our data, our socket is assigned an emphemeral port number 
    by the operating system. From then on, our socket listens for a reply on 
    this port number--if port number is important, use bind() to associate a 
    specific port before calling send().

    If multiple applications on the same system are connecting to a remote 
    server at the same port, the operating system uses the local emphemeral 
    port number to keep track of replies and separate them so they are sent 
    to their correct destinations.
    */
    const char* message = "Hello World";
    printf("Sending: %s\n", message);
    int bytes_sent = sendto(socket_peer, message, sizeof(message), 0, 
        peer_address->ai_addr, peer_address->ai_addrlen);
    printf("Send %d bytes.\n", bytes_sent);

    /*
    Typical closing procedures.
    */
    freeaddrinfo(peer_address);
    CLOSESOCKET(socket_peer); 

#if defined(_WIN32)
    WSACleanup();
#endif

    printf("Finished.\n");
    return 0;

}