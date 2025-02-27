/* tcp_client.c */

/*
CHAPTER 3:  An In-Depth Overview of TCP Connections
            A TCP Client (pg. 78 - 87)

To execute: gcc tcp_client.c -o tcp_client
            ./tcp_client <IP address> <port number>

This program creates a TCP client which can connect to any TCP server. It will 
take a hostname (IP address) and port (service number) from the user and 
attempt to connect to the TCP server at that address. If successful, any data
received from that server will be relayed to the terminal, and any data input
by the user into the terminal will be sent to the server. This will continue 
until the connection is terminated by the user (with CTRL/CMD + C) or the 
server closes the connection.
*/

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
    /*
    Winsock initialization for Windows systems, same as chapter 2 example.
    */
    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d)) {
            fprintf(stderr, "Failed to initialize.\n");
            return 1;
        }
    #endif

    /*
    Checks if the user has supplied a hostname and port. argc includes
    both the executable name ("./tcp_client") and fields after it. Otherwise, 
    prints an error and returns.
    */
   if (argc < 3) {
        fprintf(stderr, "Usage: tcp_client hostname port\n");
        return 1;
   }

    /*
    Configure a remote address for connection. This is similar to what was done
    in the chapter 2 example, but in this case, we configure a remote address
    instead of a local addess!

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
    Create our socket. Done almost exactly the same way as it was in the 
    chapter 2 example program, although we use the peer address's fields like 
    ai_family rather than that of the address we are binding to (as that was
    done because the program was for a server, not a client). 
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
    Connect socket. This is also done similarly to the program from chapter 2, 
    but instead of bind(), connect() is used. The bind() function associates a
    socket with a local address, while connect() associates a socket with a 
    remote address and initiates a TCP connection.
    */
    printf("Connecting...\n");
    if(connect(socket_peer, peer_address->ai_addr, peer_address->ai_addrlen)) {
        fprintf(stderr, "ERROR: connect() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    freeaddrinfo(peer_address);

    /*
    If a connection is established, this will be printed.
    */

    printf("Connected.\n");
    printf("To send data, enter text followed by enter:\n");

    /*
    Now, the client enters an infinite loop where it repeatedly checks the 
    terminal (for new data to send to the socket) and socket (for new data to 
    display on the terminal).

    In the interest of portability, it is recommended to use the provided 
    macros FD_ZERO, FD_SET, FD_CLR, and FD_ISSET rather than manipulate the
    fd_set directly.

    Note that we cannot use recv() here--recv() blocks until there is data to
    read, meaning that the user would not be able to enter data.
    */

    while(1) {
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(socket_peer, &reads);

        /*
        On non-Windows systems, we add stdin's file descriptor (0) to the 
        fd_set reads to monitor the terminal for user input. This can also be
        done with FD_SET(fileno(stdin), &reads), which is simple a more clear
        way of indicating what we are doing. 
        */

        #if !defined(_WIN32)
            FD_SET(0, &reads);
        #endif

        /*
        On Windows systems, select() is only capable of monitoring sockets, 
        not console input. To accomodate this, a 100 millisecond timeout
        value is used--if there is no socket activity after 100 milliseconds, 
        select() returns and we manually check for input from the terminal.
        */

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        if(select(socket_peer+1, &reads, 0, 0, &timeout) < 0) {
            fprintf(stderr, "ERROR: select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }

        /*
        After select() returns, we check if the socket is set in the reads 
        fd_set. If so, that indicates there is data to read. Here is where we 
        can call recv() and be confident that it will not block.

        Recall that recv returns the number of bytes read, and stores the 
        contents of what it has read in a buffer (read). Also recall that as 
        the data from recv() is not null terminated, we use the %.*s format
        specifier to avoid printing beyond the buffer. If recv() returns less 
        than 1, then the connection has ended and the loop is broken out of.
        */

        if(FD_ISSET(socket_peer, &reads)) {
            char read[4096];
            int bytes_received = recv(socket_peer, read, 4096, 0);
            if(bytes_received < 1) {
                printf("Connection closed by peer.\n");
                break;
            }
            printf("Received (%d bytes): %.*s", bytes_received, bytes_received, read);
        }

    /*
    After having read our new data, the program checks for terminal input to 
    send to the server. On Windows, _kbhit() is used to check if there are any 
    unhandled key press events in the queue (if so, it will return non-zero). 
    On non-Windows systems, we can check if file descriptor 0 (represents 
    stdin) is in the reads fd_set--if so, we use fgets to read the next line 
    of input. The input is then sent over the socket with send().

    Note that the terminal input that is sent over the socket will always end 
    with a newline (since one is required to terminate a line on the console).

    In the case that the socket has closed, send() returns -1. This is ignored 
    here, but on the next call to recv() the closed socket is noticed 
    immediately upon the next call to recv() on the next iteration of the 
    loop. This is a common paradigm in TCP programming, to ignore the errors
    from send() and to instead to detect and handle them on recv().

    This select() based terminal works very well on Unix-based systems, and 
    even allows for piping input, such as the following: 
    
        cat input_file.txt | tcp_client 192.168.54.122 8080

    However, the Windows implementation is shaky. Windows does not provide an 
    easy way to check stdin for input without blocking, so _kbhit() is used as 
    a shoddy proxy--_kbhit() will trigger in the event of any key press, 
    including those of non-printable keys such as arrow keys. Additionally, 
    after the first key press, the program will block on fgets() until the 
    user presses the Enter key. Received TCP data will also not display until 
    after that point.
    */

    #if defined(_WIN32)
        if(kbhit()) {
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

    /*
    Close socket, do final program cleanup.
    */
    printf("Closing socket...\n");
    CLOSESOCKET(socket_peer);

#if defined(_WIN32)
    WSACleanup()
#endif
    printf("Finished.\n");
    return 0;

}
