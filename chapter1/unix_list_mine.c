#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>

int main(){
    struct ifaddrs *all_addresses;

    if(getifaddrs(&all_addresses) == -1){
        fprintf(stderr, "ERROR: No addresses found.\n");
        exit(EXIT_FAILURE);
    }

    struct ifaddrs *address = all_addresses;
    while(address){
        int family = address->ifa_addr->sa_family;
        if (family == AF_INET || family == AF_INET6) {
            printf("%s\t%s\t", address->ifa_name, family == AF_INET ? "IPv4" : "IPv6");
            const int family_size = family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
            char ap[100];
            getnameinfo(address->ifa_addr, family_size, ap, sizeof(ap), 0, 0, NI_NUMERICHOST);
            printf("\t%s\n", ap);

        }
        address = address->ifa_next;
    }
    freeifaddrs(all_addresses);
    return 1;
}