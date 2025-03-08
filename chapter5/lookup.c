#include "chap05.h"

#ifndef AI_ALL
#define AI_ALL 0x0100
#endif

int main(int argc, char* argv[]){
    if (argc < 2) {
        printf("Usage:\t\t ./lookup <hostname>\n");
        printf("Example:\t ./lookup example.com\n");
        exit(0);
    }

#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD (2, 2), &d)) {
        fprintf(stderr, "ERROR: Issue with Windows features.\n");
        return 1;
    }
#endif
    /*
    AI_ALL flag is given to getaddrinfo() to sgignify that any address, IPv4 
    or IPv6, is acceptable.
    */
    printf("Resolving hostname...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_ALL;

    /*
    If argv[1] contains a hostname ("example.com"), the operating system will 
    perform a DNS query (if the answer is not already cached). If argv[1] 
    contains an IP address, then getaddrinfo() fills in the given addrinfo 
    structure with the appropriate information. 
    */
    struct addrinfo* peer_address;
    if (getaddrinfo(argv[1], 0, &hints, &peer_address)) {
        fprintf(stderr, "ERROR: Issue with getaddrinfo(). (%d)\n", 
            GETSOCKETERRNO());
        return 1;
    }

    printf("Remote address is:\n");
    struct addrinfo* address = peer_address;
    do {
        char address_buffer[100];
        getnameinfo(address->ai_addr, address->ai_addrlen, address_buffer, 
            sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
        printf("\t%s\t", address_buffer);
    } while ((address = address->ai_next));
    printf("\n");

    freeaddrinfo(peer_address);

#if defined(_WIN21)
    WSACleanup();
#endif

    return 0;

}