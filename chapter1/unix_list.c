/* unix_list.c*/

#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>

/*
CHAPTER 1:  Introducting Networks and Protocols
            Listing network adapters on Linux and MacOS (pg. 37 - 38)

To execute: gcc unix_list.c -o unix_list
            ./unix_list

This simple program retrieves a structure containing information on the
network adapters of the local system.

Curiously, the API for listing local IP addresses differs quite a bit between
Windows and Unix-based systems. Most other networking functions are quite 
similar between operating systems. Consult pages 32 - 36 for a implementation
in Windows.

*/

int main() {
    /* 
    A call to getifaddrs allocates memory and fills it with a linked list of
    structures describing the network interfaces on the local system. The 
    ifaddrs structure has the following relevant fields:
    
        struct ifaddrs  *ifa_next   (the next item in the linked list)
        char            *ifaname    (the name of the interface)
        struct sockaddr *ifa_addr   (the address of the interface)

    The sockaddr structure includes the following relevant fields:

        sa_family_t     sa_family   (the family of the socket address)
        char            sa_data[]   (the socket address)
    
    */

    struct ifaddrs *addresses;

    /* 
    If this call failed and no network interfaces were found, then the 
    program returns -1 and ends.
    */
    if (getifaddrs(&addresses) == -1){
        printf("getifaddrs call failed\n");
        return -1;
    }

    /*
    This new pointer is initialized to walk the list of addresses in the 
    while loop below. The loop is stopped when address->ifa_next == 0, the
    value used to signify there is no following address struct.
    */
    struct ifaddrs *address = addresses;

    while (address){
        /*
        For each address, we identify the address family. This unusual format 
        (x->y->z) occurs because we must first access the ifa_addr field of the
        ifaddrs structure, and then the sa_family subfield.  
        */
        int family = address->ifa_addr->sa_family;

        /*
        Check if the family of the address is in IPv4 or IPv6 format--this is 
        necessary because there are other types of addresses, and it cannot be
        guaranteed that family will be in a type we accept or not.
        */
        if (family == AF_INET || family == AF_INET6) {

            /*
            Print the address name and class of address. This is a more compact
            alternative to an if statement that prints out an almost identical 
            message depending on the address type.
            */
            printf("%s\t", address->ifa_name);
            printf("%s\t", family == AF_INET ? "IPv4" : "IPv6");

            /*
            ap is a buffer intended to store the textual address of the 
            interface. The family_size variable is required for the call
            to getnameinfo() below, and stores the size of an IPv4/IPv6
            sockaddr structure. 
            */
            char ap[100];
            const int family_size = family == AF_INET ? sizeof(struct sockaddr_in) : sizeof (struct sockaddr_in6);

            /*
            The function getnameinfo will be covered in more detail soon. 
            TODO: Explain this
            */
            getnameinfo(address->ifa_addr, family_size, ap, sizeof(ap), 0, 0, NI_NUMERICHOST);
            printf("\t%s\n", ap);
        }
        /*
        Moves on to the next address in the addresses linked list.
        */
        address = address->ifa_next;
    }
}