/* tcp_client.c */

#include "chap03.h"

/* 
On Windows, the _kbhit() function is required to detect terminal input, 
while on non-Windows systems, select() does this. _kbhit() is contained in
the conio.h header, so it must be included.
*/

#if defined(_WIN32)
#include <conio.h>
#endif

int main(int argc, char *argv[]) {
    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d)) {
            fprintf(stderr, "Failed to initialize.\n");
            return 1;
        }
    #endif

    /*
    Checks if the user has supplied a hostname and port. argc includes
    both the executable name ("./tcp_client") and fields after it.
    */
   if (argc < 3) {
        fprintf(stderr, "Usage: tcp_client hostname port\n");
        return 1;
   }

    /*
    Configure a remote address for connection. This is similar to what was done
    in the chapter 2 example, but in this case, we configure a remote address
    instead of a local addess!
    TODO: How exactly does the client configure a remote address..?

    hints.ai_socktype = SOCK_STREAM is set to inform getaddrinfo() that we are
    interested in a TCP connection (SOCK_DGRAM for UDP). Unlike the chapter 2
    example, we do not need to set hints.ai_family, since getaddrinfo() will
    decide if IPv4 or IPv6 is the appropriate protocol to use.

    argv[1] and argv[2] are the hostname and port provided by the user via the
    terminal. If there is an issue, getaddrinfo() returns non-zero, and the if
    statement catches the error and terminates the program--otherwise, the 
    remote address is stored in peer_address. Additionally, getaddrinfo() is 
    quite flexible in what it accepts as input. The hostname could be inputted
    as "example.com," "192.168.17.23," or "::1" and still be processed. 
    Similarly, the port can be a number (80) or protol with an associated port
    (http).
    */
    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *peer_address;
    if (getaddrinfo(argv[1], argv[2], &hints, &peer_address)) {
        fprintf(stderr, "getaddrinfo() filed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    /*
    Just for good measure, print out the remote address. This requires use
    of getnameinfo().
    */

    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];
    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen, 
                    address_buffer, sizeof(address_buffer), service_buffer, 
                    sizeof(service_buffer), NI_NUMERICHOST);
    printf("%s %s\n", address_buffer, service_buffer);

    /*
    Create our socket.
    */

    printf("Creating socket...\n");
    SOCKET socket_peer;
    socket_peer = socket(peer_address->ai_family, peer_address->ai_socktype, 
        peer_address->ai_protocol);
    if(!ISVALIDSOCKET(socket_peer)) {
        fprintf(stderr, "ERROR: socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    /*
    Connect socket.
    */
    printf("Connecting...\n");
    if(connect(socket_peer, peer_address->ai_addr, peer_address->ai_addrlen)) {
        fprintf(stderr, "ERROR: connect() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    freeaddrinfo(peer_address);

    printf("Connected.\n");
    printf("To send data, enter text followed by enter:\n");

    while(1) {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(socket_peer, &reads);
        #if !defined(_WIN32)
            FD_SET(0, &reads);
        #endif
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        if(select(socket_peer+1, &reads, 0, 0, &timeout) < 0) {
            fprintf(stderr, "ERROR: select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }

        if(FD_ISSET(socket_peer, &reads)) {
            char read[4096];
            int bytes_received = recv(socket_peer, read, 4096, 0);
            if(bytes_received < 1) {
                printd("Connection closed by peer.\n");
                break;
            }
            printf("Received (%d bytes): %.*s", bytes_received, bytes_received, read);
        }

    #if defined(_WIN32)
        if(khbit()) {
    #else
        if(FD_ISSET(0, &reads)) {
    #endif
            char read[4096];
            if(!fgets(read, 4096, stdin)) break;
            printf("Sending: %s", read);
            int bytes_sent = send(socket_peer, read, strlen(read), 0);
            printf("Sent %d bytes.\n", bytes_sent);
        }
    }

    printf("Closing socket...\n");
    CLOSESOCKET(socket_peer);

#if defined(_WIN32)
    WSACleanup()
#endif
    printf("Finished.\n");
    return 0;

}
