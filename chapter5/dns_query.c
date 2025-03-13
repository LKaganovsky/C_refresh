/* dns_query.c */

/*
CHAPTER 5:  Hostname Resolution and DNS
            A DNS Query Program (pg. 146 - 160)

To execute: gcc dns_query.c -o dns_query
            ./dns_query

A small utility to send DNS queries to a DNS server and receive responses 
back. 
*/

#include "chap05.h"

/*
This function takes three pointers: one to the start of the message (msg), one 
to the name to print (p) and one to the end of the message (end). The msg 
pointer is required so we know where to start reading, and also to interpret 
name pointers. the end pointer is required so we do not read past the end of 
the message.

TODO: Is p required, or a convenience? Is end necessary?
*/
const unsigned char* print_name(const unsigned char* msg, const unsigned char* p, const unsigned char* end) {
    /*
    A name should consist of at least a length and some text. If p is within 
    two characters (16 bits) of the end, then we exit.
    */
    if (p + 2 > end) {
        fprintf(stderr, "End of message.\n");
        exit(1);
    }

    /*
    If the first two bits of the name are set (we write this as 0xC0, in binary 
    it is 0b11000000) then this indicates that p is actually a pointer to a 
    name stored elsewhere.
    */
    if ((*p & 0xC0) == 0xc0 ) {
        /*
        In this case, we take the lower 6 bits of p and all 8 bits of p[1] to 
        indicate the pointer. We can be confident that p[1] is still within 
        message boundaries due to our check above that assured that p was at 
        least 2 bytes from the end. 
        */
        const int k = ((*p & 0x3F) << 8) + p[1]; 
        p += 2;
        printf("    (Pointer %d):", k);
        /* 
        We can then call print_name recursively to print the name that p 
        points to. 
        
        TODO: 0x3F?
        */
        print_name(msg, msg+k, end);
        return p;

    } else {
        /*
        If p is not a pointer to a name stored elsewhere and instead is an 
        entire name itself, we can print it one label at a time.

        *p is read into len to store the name of the label.
        */
        const int len = *p++;
        /*
        Check if len + 1 past p puts us over the end of the buffer--if so, 
        exit. 
        */
        if (p + len + 1 > end) { 
            fprintf(stderr, "End of message\n");
            exit(1);
        }
        /*
        If p has at least len bytes before the end of the message, then we can 
        print the next len number of bytes without worry. So long as the next 
        byte isn't 0 (which would indicate the end of the name), we can print 
        the label, and then call print_name again to run the function again 
        from the next label.
        */
        printf("%.*s", len, p);
        p += len;
        if (*p) {
            printf(".");
            return print_name(msg, p, end);
        } else {
            return p + 1;
        }
    }
}

/*
This function prints an entire DNS message to the screen--helpfully, since 
requests and responses use the same format, both can be printed without any 
need for unique behaviour!

It takes a pointer to the start of the message and a an integer to represent 
the message length (since they can be variable).
*/
void print_dns_message(const char* message, int msg_length) {
    /*
    A DNS header is 12 bytes long, anything shorter than that is malformed and 
    should be rejected.
    */
    if (msg_length < 12) {
        fprintf(stderr, "ERROR: Message is too short to be valid.\n");
        exit(1);
    }

    /*
    Copy pointer to message into an unsigned char pointer variable to make 
    calculations further down easier.

    TODO: Elaborate
    */
    const unsigned char* msg = (const unsigned char*) message;

    /*
    Quick way to print the entire raw DNS message. This isn't optimal, but 
    is here anyway.
    */
    // int i;
    // for (i = 0; i < msg_length; ++i) {
    //     unsigned char r = msg[i];
    //     printf("%02d:   %02X  %03d  '%c'\n", i, r, r, r);
    // }
    // printf("\n");

    /*
    Print message ID. This is the first two bytes of the message, so it's easy.
    Take notice of the format specifiers in this program's code--%0X is used 
    to print unsigned hexadecimal integers in uppercase, left padded with 0s. 
    */
    printf("ID = %0X %0X\n", msg[0], msg[1]);

    /*
    Print the value of the QR bit which determines if the message is a 
    question or a response. The QR value is the most significant bit of 
    msg[2], so to isolate it we perform a logical AND on it with 128 (in 
    binary, that's 0b10000000), and then shift it right 7 bits to turn it into 
    either 1 (0b00000001) or 0 (0b00000000). This is a clever way to make use 
    of bitwise operations (and the ternary operator) to make nice, compact 
    code.
    */
    const int qr = (msg[2] & 0x80) >> 7;        // 128 (0b10000000)
    printf("QR = %d %s\n", qr, qr ? "response" : "query");

    /*
    Print the OPCODE value using the same method as above. OPCODE is stored 
    in bits 1-4 of the third octet in the message (msg[2]--be wary of one-off 
    errors).
    */
    const int opcode = (msg[2] & 0x78) >> 3;    // 120 (0b01111000) 
        printf("OPCODE = %d ", opcode);
        switch(opcode) {
            case 0: printf("standard\n"); break;
            case 1: printf("reverse\n"); break;
            case 2: printf("status\n"); break;
            default: printf("?\n"); break;
        }

    /* Same method as above for the following fields. AA, TC and RD are 
    stored in bits 5, 6, and 7 of the third octet, while RCODE is stored in 
    bits 4, 5 6 and 7 of the fourth octet, hence why msg[3] is used. */
    /* Authoritative? */
    const int aa = (msg[2] & 0x04) >> 2;        // 4 (0b00000100)
    printf("AA = %d %s\n", aa, aa ? "authoritative" : "");

    /* Truncated? */
    const int tc = (msg[2] & 0x02) >> 1;        // 2 (0b00000010)
    printf("TC = %d %s\n", tc, tc ? "message truncated" : "");

    /* Recursion desired? */
    const int rd = (msg[2] & 0x01);             // 1 (0b00000001)
    printf("RD = %d %s\n", rd, rd ? "recursion desired" : "");

    /* RCODE values. 4 bit field representing possible errors. Note that 
    while RCODE is 4 bits, and can have values from 0 to 15, only values 0 to 
    5 actually have interpretations available. For this reason, we only AND 
    the RCODE with 0x07 (0b00000111) instead of 0x0F (0b00001111), though the 
    code would still work with 0x0F. */
    if (qr) {
        const int rcode = msg[3] & 0x07;        // 7 (0b00000111)
        printf("RCODE = %d ", rcode);
        switch(rcode) {
            case 0: printf("success\n"); break;
            case 1: printf("format error\n"); break;
            case 2: printf("server failure\n"); break;
            case 3: printf("name error\n"); break;
            case 4: printf("not implemented\n"); break;
            case 5: printf("refused\n"); break;
            default: printf("?\n"); break;
        }
        if (rcode != 0) return;
    }

    /* QCOUNT, ANCOUNT, NSCOUNT, ARCOUNT. 
    Again, the same method. Each of these fields is 2 octets in length, 
    so we can access them easily by hopping to the appropriate octet in
    msg. 

    We left shift the bits in the first octet of each field by 8 spaces, and 
    then add the 

    TODO: Wait what? Doesn't left shifting the first octet by 8 spaces just 
    turn it into 0b00000000? Why even bother with this part?
    
    */
    const int qdcount = (msg[4] << 8) + msg[5];
    const int ancount = (msg[6] << 8) + msg[7];
    const int nscount = (msg[8] << 8) + msg[9];
    const int arcount = (msg[10] << 8) + msg[11];
    printf("QDCOUNT = %d\n", qdcount);
    printf("ANCOUNT = %d\n", ancount);
    printf("NSCOUNT = %d\n", nscount);
    printf("ARCOUNT = %d\n", arcount);

    /*
    With the DNS header done printing, next comes the rest of the message. To 
    read it, we define p to walk through the message starting right after the 
    header, and end which points to right after the end of the message.
    */
    const unsigned char *p = msg + 12;
    const unsigned char *end = msg + msg_length;

    /* 
    Here we print each question in the DNS message.
    */
    if(qdcount) {
        int i;
        /*
        There really is no situation in which a loop is actually required, 
        since no DNS message can have more than 1 question. However, RFC 1035 
        (the one for DNS) defines the format as being capable of encoding 
        multiple questions. That will never happen in practice, but we 
        accomodate it here just because.
        */
        for (i = 0; i < qdcount; ++i) {
            if (p >= end) {
                fprintf(stderr, "End of message.\n");
                exit(1);
            }
            printf("Query %2d\n", i + 1);
            printf("  name: ");

            /*
            Call on the print_name function to print the name from the 
            question. Cheating by getting the hostname from the user (who must 
            provide one as an argument to this program) may work, but defeats 
            the purpose of this exercise.

            TODO: Actually, what about the case of aliases? Does the DNS reply 
            always copy the name field from the question, or can it return a 
            record that maps the IP address to an alias of the given hostname? 
            */
            p = print_name(msg, p, end);
            printf("\n");

            /*
            If there is less than 4 whole octets between p and the end, that 
            space is unused and cannot hold a question. TODO: Double check.
            */
            if (p + 4 >= end) {
                fprintf(stderr, "End of message.\n");
                exit(1);
            }
            
            const int type = (p[0] << 8) + p[1];
            printf("  type: %d\n", type);
            p += 2;
            const int qclass = (p[0] << 8) + p[1];
            printf(" class: %d\n", qclass);
            p += 2;
        }
    }
    
    /*
    If there are answer resource records, name server resource records, or 
    additional resource records, print them.
    */
    if (ancount || nscount || arcount) {
        int i;
        /* If we are past the end of the message, exit. */
        for (i = 0; i < ancount + nscount + arcount; ++i) {
            if (p >= end) {
                fprintf(stderr, "End of message\n");
                exit(1);
            }
            printf("Answer %2d\n", i + 1);
            printf("  name: ");

            p = print_name(msg, p, end); 
            printf("\n");

            if (p + 10 > end) {
                fprintf(stderr, "End of message.\n"); 
                exit(1);
            }
            
            const int type = (p[0] << 8) + p[1];
            printf("  type: %d\n", type);
            p += 2;

            const int qclass = (p[0] << 8) + p[1];
            printf(" class: %d\n", qclass);
            p += 2;

            const unsigned int ttl = (p[0] << 24) + (p[1] << 16) + 
                (p[2] << 8) + p[3];
            printf("   ttl: %u\n", ttl);
            p += 4;

            const int rdlen = (p[0] << 8) + p[1];
            printf(" rdlen: %d\n", rdlen);
            p += 2;

            if (p + rdlen > end) {
                fprintf(stderr, "End of message.\n"); 
                exit(1);
            }

            if (rdlen == 4 && type == 1) { /* A Record */
                printf("Address ");
                printf("%d.%d.%d.%d\n", p[0], p[1], p[2], p[3]);
            } else if (type == 15 && rdlen > 3) { /* MX Record */
                const int preference = (p[0] << 8) + p[1];
                printf("  pref: %d\n", preference);
                printf("MX: ");
                print_name(msg, p+2, end); printf("\n");
            } else if (rdlen == 16 && type == 28) { /* AAAA Record */
                printf("Address ");
                int j;
                for (j = 0; j < rdlen; j+=2) {
                    printf("%02x%02x", p[j], p[j+1]);
                    if (j + 2 < rdlen) printf(":");
                }
                printf("\n");
            } else if (type == 16) { /* TXT Record */
                printf("TXT: '%.*s'\n", rdlen-1, p+1);
            } else if (type == 5) { /* CNAME Record */
                printf("CNAME: ");
                print_name(msg, p, end); printf("\n");
            }
            p += rdlen;
        }
    }
    if (p != end) {
           printf("There is some unread data left over.\n");
    }
    printf("\n");
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage:\n\tdns_query hostname type\n");
        printf("Example:\n\tdns_query example.com aaaa\n");
        exit(0);
    }

    /* Makes sure the hostname isn't too long. */
    if (strlen(argv[1]) > 255) {
        fprintf(stderr, "Hostname too long.");
        exit(1);
    }

    /* Check for requested record type */
    unsigned char type;
    if (strcmp(argv[2], "a") == 0) {
        type = 1;
    } else if (strcmp(argv[2], "mx") == 0) {
        type = 15;
    } else if (strcmp(argv[2], "txt") == 0) {
        type = 16;
    } else if (strcmp(argv[2], "aaaa") == 0) {
        type = 28;
    } else if (strcmp(argv[2], "any") == 0) {
        type = 255;
    } else {
        fprintf(stderr, "Unknown type '%s'. Use a, aaaa, txt, mx, or any.",
                argv[2]);
        exit(1); 
    }

    /* Usual Winsock initialization. */
#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    } 
#endif

    /* 
    Note the hardcoded IP address here--this is the primary DNS server of 
    Google, though any other DNS with a constant address (Like Cloudflare's
    1.1.1.1) would work. The rest of this is just regular initialization of a 
    UDP socket to send and recv with.
    */
    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo *peer_address;
    if (getaddrinfo("8.8.8.8", "53", &hints, &peer_address)) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }


    printf("Creating socket...\n");
    SOCKET socket_peer;
    socket_peer = socket(peer_address->ai_family,
            peer_address->ai_socktype, peer_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_peer)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    /*
    Build the query for the DNS. According to the textbook, "the first 12 
    bytes compose the header and are known at compile time." 
    */
    char query[1024] = {0xAB, 0xCD, /* ID */ 
                        0x01, 0x00, /* Set recursion */
                        0x00, 0x01, /* QDCOUNT */
                        0x00, 0x00, /* ANCOUNT */
                        0x00, 0x00, /* NSCOUNT */
                        0x00, 0x00  /* ARCOUNT */};

    /*
    *p is a pointer to the end of the header, *h will be used to loop 
    through the hostname. 
    */
    char *p = query + 12;
    char *h = argv[1];

    /* Encoding the user's desired hostname into the query. */
    while(*h) {
        char *len = p; /* beginning of label. */
        p++;
        if (h != argv[1]) ++h;

        while(*h && *h != '.') *p++ = *h++;
        *len = p - len - 1;
    }
    /* Add a terminating 0. */
    *p++ = 0;

    /* Question type and question class. Class is always 1 (Internet). */
    *p++ = 0x00; 
    *p++ = type; /* QTYPE */
    *p++ = 0x00; 
    *p++ = 0x01; /* QCLASS */

    /* Get query size. */
    const int query_size = p - query;

    /* With size and length known, the query can now be sent. */
    int bytes_sent = sendto(socket_peer, query, query_size, 0, 
        peer_address->ai_addr, peer_address->ai_addrlen);
    printf("Sent %d bytes.\n", bytes_sent);

    /* For debugging purposes, look at the query again. */
    print_dns_message(query, query_size);

    /* It'd be wise to use select() here, but this works for now. */
    char read[1024];
    int bytes_received = recvfrom(socket_peer,
            read, 1024, 0, 0, 0);

    printf("Received %d bytes.\n", bytes_received);

    /* Print the response. */
    print_dns_message(read, bytes_received);
    printf("\n");

    /* Usual closing procedures. */
    freeaddrinfo(peer_address);
    CLOSESOCKET(socket_peer);

#if defined(_WIN32)
    WSACleanup();
#endif

    return 0;

}