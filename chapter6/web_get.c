#include "chap06.h"

#define TIMEOUT 5.0

void parse_url(char* url, char** hostname, char** port, char** path){

    printf("URL: %s\n", url);

    // URL example: http://example.com:80/res/page1.php?user=linda#account

    /*
    Start by identifying the "://" in the url, which is where the protocol
    portion stops.
    */

    char* p;
    p = strstr(url, "://");
    char* protocol = 0;
    if (p) {
        /* If a protocol is found, protocol is set to the start of the url 
        (where the protocol starts), and p is moved to the beginning of the 
        hostname. */
        protocol = url;
        *p = 0;
        p += 3;   
    } else {
        /* If no protocol is found, p is set back to the start of the URL. */
        p = url;
    }
    /* Check that the protocol is http. No other protocol is supported. */
    if (protocol) {
        if (strcmp(protocol, "http")) {
            fprintf(stderr, "ERROR: Protocol %s is unsupported.\n", protocol);
            exit(1);
        }
    }

    /* With protocol determined, now we can save the hostname into the 
    hostname return variable. This is done by looking for the first colon, 
    slash or hash. */
    *hostname = p;
    while (*p && *p != ':' && *p != '/' && *p != '#') ++p;

    /* After the hostname, check for a port number. */
    *port = "80";
    if(*p == ';') {
        *p++ = 0;
        *port = p;
    }
    while (*p && *p != ':' && *p != '/' && *p != '#') ++p;

    /* After the port number, check for document path. */
    *path = p;
    if(*p == '/') {
        *path = p + 1;
    }
    *p = 0;

    /* Next, check for a hash. If one exists, overwrite it with a null 
    terminator, since the hash is not intended to be sent to the server. */
    while (*p && *p != '#') ++p;
    if (*p == '#') *p = 0;

    /* Having parsed the hostname, port number and document path, print these 
    values out for debugging info: */
    printf("Hostname: %s\n", *hostname);
    printf("Port num: %s\n", *port);
    printf("Path: %s\n", *path);

}

/* This is a helper function intended to assemble the header and store it in a 
buffer, including a blank line required for terminating a header. It 
then sends this to the server. */
void send_request(SOCKET s, char* hostname, char* port, char* path) {
    char buffer[2048];

    sprintf(buffer, "GET /%s HTTP/1.1\r\n", path);
    sprintf(buffer + strlen(buffer), "Host: %s:%s\r\n", hostname, port);
    sprintf(buffer + strlen(buffer), "Connection: close\r\n");
    sprintf(buffer + strlen(buffer), "User-Agent: honpwc web_get 1.0\r\n");
    sprintf(buffer + strlen(buffer), "\r\n");

    send(s, buffer, strlen(buffer), 0);
    printf("Sent Headers:\n%s", buffer); /* For debugging. */
}

SOCKET connect_to_host(char* hostname, char* port) {
    /* Pretty much the same as the previous chapters. */
    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* peer_address;
    if(getaddrinfo(hostname, port, &hints, &peer_address)) {
        fprintf(stderr, "ERROR: Issue with getaddrinfo(). (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    printf("Remote address is...\n");
    char address_buffer[100];
    char service_buffer[100];
    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen, 
        address_buffer, sizeof(address_buffer), service_buffer, 
        sizeof(service_buffer), NI_NUMERICHOST);
    printf("%s %s", address_buffer, service_buffer);

    printf("Creating socket...\n");
    SOCKET server;
    server = socket(peer_address->ai_family, peer_address->ai_socktype, 
        peer_address->ai_protocol);
    if(!ISVALIDSOCKET(server)) {
        fprintf(stderr, "ERROR: Issue with socket creation. (%d)\n", 
            GETSOCKETERRNO());
            exit(1);
    }

    printf("Connecting...\n");
    if(connect(server, peer_address->ai_addr, peer_address->ai_addrlen)) {
        fprintf(stderr, "ERROR: Issue with connection. (%d)\n", 
            GETSOCKETERRNO());
            exit(1);
    }

    freeaddrinfo(peer_address);

    printf("Connected.\n");
    return server;

}

int main(int argc, char* argv[]){

    /* Windows stuff. */
#if defined(_WIN32)
    WSADATA d;
    if(WSAStartup(MAKEWORD(2, 2), &d)){
        fprintf(stderr, "ERROR: Issue with Windows initialization.\n");
        return 1;
    }
#endif

    if (argc < 2) {
        fprintf(stderr, "Usage: ./web_get url\n");
        return 1;
    }

    char* url = argv[1];
    char *hostname, *port, *path;
    parse_url(url, &hostname, &port, &path);

    SOCKET server = connect_to_host(hostname, port);
    send_request(server, hostname, port, path);

    const clock_t start_time = clock();

    /* Bunch of stuff here for parsing the HTTP response. Breakdown follows: 
        -   RESPONSE_SIZE:  The maximum length of response this program can
                            deal with.
        -   response:   A character array that will hold the entire response.
        -   p, q:       Pointers to walk the response. p points to where we 
                        have written so far, q is used later and will be used 
                        in determining the length of the response body.
        -   end:        A pointer to the end of response.
        -   body:       A pointer to the start of the response body.
    
    Recall that the lenth of the HTTP response body is not always given, so 
    we accomodate for that by using multiple methods to determine if.

     */
#define RESPONSE_SIZE 8192
    char response[RESPONSE_SIZE +1];
    char *p = response, *q; 
    char *end = response + RESPONSE_SIZE;
    char* body = 0;

    enum {length, chunked, connection};
    int encoding = 0;
    int remaining = 0;

    /* Time to process the response! */
    while(1) {
        /* Finish processing if the timeout limit has been passed. */
        if ((clock() - start_time) / CLOCKS_PER_SEC > TIMEOUT) {
            fprintf(stderr, "ERROR: Timeout after %.2f seconds.\n", TIMEOUT);
            return 1;
        }
        /* Finish processing if p has reached the end of the buffer. */
        if (p == end) {
            fprintf(stderr, "ERROR: out of buffer space.\n");
            return 1;
        }
        
        /*
        Since we're going to be using select() to read from our socket with 
        a 0.2 second timeout, we also have to create an fd_set to iterate 
        over (even though we will only be iterating over a single socket).
        */
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(server, &reads);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;

        if (select(server+1, &reads, 0, 0, &timeout) < 0) {
            fprintf(stderr, "ERROR: Issue with select()\n");
            return 1;
        }

        /* If there is data to read, read it with recv(). */
        if(FD_ISSET(server, &reads)) {
            int bytes_received = recv(server, p, end - p, 0);
            /*
            If no data is read (indicating a closed connection), check if 
            the encoding is of type "connection." For a connection encoding, 
            no data indicates that the transmission is complete and that the 
            HTTP body can be printed.
            */
            if (bytes_received < 1) {
                if (encoding == connection && body) {
                    printf("%.*s", (int)(end - body), body);
                }
                printf("Connection closed by peer.\n");
                break;
            }

            /* 
            The p pointer is always advanced to the end of the received data, 
            and is used to terminate the data with a null terminator. This is 
            not necessary, but a useful feature since most functions that deal 
            with strings expect them to be null terminated.
            */
            p += bytes_received;
            *p = 0;

            /*
            If the HTTP body hasn't been found yet, find it here. Note the 
            formatting of this line--it will only enter this if statement if 
            body has not been found already but is then found with strstr().
            */
            if (!body && (body = strstr(response, "\r\n\r\n"))) {
                /* Body pointer is updated to the beginning of the body. */
                *body = 0;
                body += 4;

                /* Not necessary, but useful for debugging. */
                printf("Received Headers:\n%s\n", response);

                /* 
                Now comes the issue of determining how the HTTP server 
                indicates the length of the body--Content-Length or 
                Transfer-Encoding: chunked. Alternatively, if neither are 
                given, we assume that the entire body has already been 
                received once the connection is closed. 
                
                If Content-Length is found, then update the encoding variable 
                with that information, and store the body length in the 
                remaining variable. The length itself is read with strtol() 
                (string to long).
                */
                q = strstr(response, "\nContent-Length: ");
                if (q) {
                    encoding = length;
                    q = strchr(q, ' ');
                    q += 1;
                    remaining = strtol(q, 0, 10);
                /*
                If Transfer-Encoding is chunked, then we cannot be sure of the 
                remaining length of the body, so we set the encoding variable 
                but not the remaining variable.
                */
                } else {
                    q = strstr(response, "\nTransfer-Encoding: chunked");
                    if (q) {
                        encoding = chunked;
                    }
                }
                printf("\nReceived body.\n");
            }
            /* 
            If the body has been found and the encoding == length, then 
            the program simply needs to wait for the remaining bytes are 
            received
            */
            if (body) {
                if (encoding == length) {
                    if (p - body >= remaining) {
                        printf("%.*s", remaining, body);
                        break;
                    }

                /* 
                The receiving logic is a little more complicated when the 
                encoding type is chunked. When remaining == 0, this indicates 
                that the program is waiting to receive a new chunk length. 
                Since each chunk length ends with a newline (strstr() is 
                used to check for one), if a newline is found, that indicates 
                that an entire chunk length has been received. That chunk 
                length is then read using strtol(), and remaining is set to 
                the expected chunk length. Since a chunked message is 
                terminated by a 0 length chunk, if remaining is ever 0 after 
                a read, then a goto is used to break out of the loop.

                If remaining != 0, the program checks if there are at least 
                remaining bytes of data that have been received. If so, that 
                chunk is printed, and the body pointer is advanced to the end 
                of the current chunk.
                */
                } else if (encoding == chunked) {
                    do {
                        if (remaining == 0) {
                            if ((q = strstr(body, "\r\n"))) {
                                remaining = strtol(body, 0, 16);
                                if (!remaining) goto finish;
                                body = q + 2;
                            }
                        } if (remaining && p - body >= remaining) {
                            printf("%.*s", remaining, body);
                            body += remaining + 2;
                            remaining = 0;
                        }
                    } while (!remaining);
                }
            }   
        }
    }
    /* Destination of the goto statement above, socket closing and cleanup. */
finish:
    printf("\nClosing socket..\n");
    CLOSESOCKET(server);

# if defined(_WIN32)
    WSACleanup();
# endif

    printf("Finished.\n");
    return 0;
}