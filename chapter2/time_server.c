/* time_console.c */

/*
CHAPTER 2:  Getting to Grips with Socket APIs
            Our First Program (pg. 53 - 68)

To execute: gcc time_server.c -o time_server
            ./time_server

            The use a browser to send a request to 127.0.0.1:8080

This program creates a web server that responds with the current time. It
is intended to work on both windows and Unix-like systems (including MacOS),
cleverly using macros to account for the differences in systems.
*/

/*
This block of headers determines if the program is running on Windows or not,
including the relevant headers based on OS. 
TODO: Look more into the Windows headers.. _WIN32_WINNT 0x0600?
*/
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

/*
These macros abstract out some of the difference between Berkeley sockets and
Winsock APIs. The first block checks if _WIN32 (Windows API) is defined, and if
so, defines aliases for the following functions:
    
    ISVALIDSOCKET(s) returns true/false based on a comparison of the socket
    value and comparison against the INVALID_SOCKET value (defined as 0xffff).

    CLOSESOCKET(s) is simply an abstraction for the Winsock function 
    closesocket(s) which closes the specified socket.

    GETSOCKETERRNO() is an abstraction for WSAGetLastError().


On Unix systems, Berkeley sockets are somewhat more intuitive.

    ISVALIDSOCKET(s) checks if the socket value is anything other than the "OK"
    value.

    CLOSESOCKET(s) is an abstraction for close(s) which closes the specified 
    socket.

    SOCKET int is unusual. In Unix, a socket descriptor is represented by a 
    standard file descriptor which allows for any standard I/O functions to be
    performed on it. In Windows, a socket handle can be anything Additionally, 
    the socket() function returns an integer, while in Windows it returns a 
    SOCKET (a typedef (alias) for an unsigned int). To compensate for this 
    difference, this program uses #define SOCKET int (though typedef int 
    SOCKET) would also work so a socket descriptor can be stored as a SOCKET
    regardless of platform.

    GETSOCKETERRNO() reads from the thread-global errno variable. Whenever a
    socket function, such as socket(), bind() or accept() encounter an error 
    on the Unix platform, the error number is stored in errno.

The "if defined(IPV6_V6ONLY)" is part of a later addition on pg. 66 to add 
support for dual-stack sockets which can support both IPv4 and IPv6.
    
*/
#if defined(_WIN32)
#define ISVALIDSOCKET(s) ((s) != INVALID_SOCKET)
#define CLOSESOCKET(s) closesocket(s)
#define GETSOCKETERRNO() (WSAGetLastError())

#if !defined(IPV6_V6ONLY)
#define IPV6_V6ONLY 27
#endif

#else
#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)
#endif

/*
Standard C headers.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

int main() {
    /*
    If this program is being compiled on Windows, Winsock must first be 
    defined. An instance of the WSADATA structure, which contains information
    about the Windows Sockets implementation, is created, and initialized with
    WSAStartup. MAKEWORD(2, 2) indicates that Winsock 2.2 is the requested 
    version of Winsock. Note that MAKEWORD is a niche macro that functions as 
    a way to build a 16 bit word from two one-byte words specifically to 
    generate a versioning word for Winsock.
    */
    #if defined(_WIN32)
        WSADATA d;
        if (WSAStartup(MAKEWORD(2, 2), &d)) {
            fprintf(stderr, "Failed to initialize.\n");
            return 1;
        }
    #endif

    /*
    Next, we must figure out the local address that our web server should bind
    to. We first create a addrinfo structure and zero it using memset(). Next,
    we set several fields of the hints structure:

        int ai_family   (Address family, AF_INET for IPv4 or AF_INET6 for IPv6)
        int socktype    (Preferred socket type, SOCK_DGRAM for UDP or 
                        SOCK_STREAM for TCP. 0 In this field signifies socket
                        addresses of any type can be returned by getaddrinfo())
        int ai flags    (Ought to read more about this. It appears that the
                        AI_PASSIVE flag signifies that the caller intends to 
                        use the returned socket address to bind() a socket that
                        will accept() connectons. The summary in the textbook
                        says that this line actually informs getaddrinfo() that
                        we want it to bind to the wildcard address, that is,
                        asking getaddrinfo() to set up the address, so we listen
                        on any available network interface. 
                        TODO: Read more on this)
    */
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    /*
    Upon properly setting up the addrinfo structure "hints", we declare a 
    pointer to another addrinfo structure, which holds the return information
    from getaddrinfo(). getaddrinfo() has many uses, and in this case we are 
    using it to generate an address that is suitable for bind(). 
    
    To make sure  it generates this, we pass in the first parameter as NULL 
    and have the AI_PASSIVE flag set in hints.ai_flags.

    The second parameter is the port we listen for connections on, chosen
    arbitrarily. However, since only one program can bind to a particular
    port at a time, the call to bind() may fail. If this occurs, the number
    should be manually changed (Or, a smarter strategy should be used to pick
    a port, that isn't hardcoding.)

    There are cases wher a call to getaddrinfo() is unnecessary, and an
    addrinfo struct can instead just be filled directly. The advantage of using
    getaddrinfo() is that it is protocol-independent, and as a result can 
    easily be converted between IPv4 and IPv6, just by changing AF_INET to
    AF_INET6. If the addrinfo structure was filled in manually, many changes
    would have to be done to convert to IPv6.
    */

    struct addrinfo *bind_address;
    getaddrinfo(0, "8080", &hints, &bind_address);

    /*
    socket_listen is defined as a SOCKET type--recall that this is a Winsock
    type on Windows, and that we have a macro defining it as an int on other
    platforms, to account for the fact that Windows socket handles are unsigned
    ints that can be anything, while on Unix they are small non-negative ints.

    The socket itself is generated with a call to socket(), and is provided 
    three parameters--the socket family, socket type, and socket protocol. The
    reason that getaddrinfo() was called before socket() was to allow for the 
    passing in of fields from bind_address as arguments to socket(), making
    it easier to change our programs protocol between UDP/TCP without needing 
    significant reworking.

    In most programs, socket() is called before getaddrinfo(), which is fine
    but complicates the program as information must be entered multiple times.

    Finally, we check if the call to socket() was successful. We can check that
    socket_listen is valid by using the ISVALIDSOCKET() marco defined above.
    The GETSOCKETERRNO() macro is used to retrieve the error number in a 
    cross-platform manner.
    */

    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, 
    bind_address->ai_protocol);

    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    /*
    This is a later addition to the program defined on pg. 66 as a method for 
    supporting both IPv4 and IPv6.

    After the call to socket() and before the call to bind(), we must clear
    the IPV6_V6ONLY flag on the socket (which is not preserved in this code, 
    and was actually done as an experimental modification when changing 
    ai_family->AF_INET to ai_family->AF_INET6... so I suppose in this case, 
    it's actually clearing the IPv4 flag?). This is done with the setsockopt()
    function.

    We must first declare option as an integer and set it to 0. IPV6_V6ONLY is
    enabled by default, so we clear it by setting it to 0. setsockop() is then
    called on the listening socket. We pass in IPPROTO_IPV6 to tell it what
    part of the socket we are operating on, and IPV6_V6ONLY to tell it which 
    flag we are setting. We then pass in a pointer to our option and its 
    length. setsockopt() retusn 0 on success, so we use this convoluted method
    to check for that.

    */

   int option = 0;
   if (setsockopt(socket_listen, IPPROTO_IPV6, IPV6_V6ONLY, (void*) &option, 
   sizeof(option))) {
        fprintf(stderr, "setsockopt() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
   }


    /*
    Next, the socket must be bound to a local address, the address we received
    earlier from getaddrinfo(). The bind() function requires a file descriptor
    for the socket, an address and address length, to assign the provided 
    address to the socket. bind() will fail if the provided port is already in
    use, and the port must manually be freed or this program must be changed to
    use another port.

    After we have bound the socket and address, we can call freeaddrinfo() to
    release the memory being used to store the address information.
    */

    printf("Binding socket to local address...\n");
    if(bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    freeaddrinfo(bind_address);

    /*
    With the socket created and bound to a local address,  we can start
    listening for connections using the listen() function. The 10 value is
    arbitrary and tells listen() how many connections it can queue at a time,
    so if many users are attempting to connect to the server at once, then
    the operating system will only queue 10 and reject any additional 
    connections.
    */

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    /*
    As preparation for incoming connections, we initialize a sockaddr_storage
    structure, which contains the following a single field of type sa_family_t.
    This structure stores the address information of the connection client, 
    and is designed to be large enough to store the largest supported address 
    on the system. The size of the address stored in this structure is also
    saved.

    The accept() function has multiple uses. Upon being called, it blocks the
    program until a new connection is made, essenially forcing it to sleep 
    until a new connection is made to the listening socket. When a new 
    connection is made, accept() will create a new socket, while the original
    socket remains open and listening for new connections. The new socket
    returned by accept() can only be used to send and receive data over the 
    newly established connection with a client. accept() also fills in address
    info of the client that connected. When accept() returns, 

    When accept() returns, it will have stored the connected client's address
    in client_address, and filled in client_len with the length of that
    address (variable depending on if IPv4 or IPv6 is used).

    The return value of accept() is stored in socket_client, and we check it
    for errors in the same way as we do with socket() above.
    */

    printf("Waiting for connection...\n");
    struct sockaddr_storage client_address;
    socklen_t client_len = sizeof(client_address);
    SOCKET socket_client = accept(socket_listen, (struct sockaddr*) 
        &client_address, &client_len);
    if (!ISVALIDSOCKET(socket_client)) {
        fprintf(stderr, "accept() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    /*
    At this point, a TCP connection has been established, and we print the 
    client's address to the sonsole. This isn't necessary to the functionality
    of the program, but it is good practice to log connections somewhere.

    getnameinfo() takes the client's address and address length. The address
    length is needed because getnameinfo() can work with both IPv6 and IPv6
    addresses. We pass in an output buffer and buffer length. This is the 
    buffer than getnameinfo() writes its hostname output to. The next two
    arguments specify a second buffer and its length, which getnameinfo() 
    outputs the service name to. Since this is irrelevant to us, we simply
    pass in two zeros to these parameters. Finally, we pass in the 
    NI_NUMERICHOST flag, which specifies that we want the hostname as an IP
    address.
    */

    printf("Client is connected...\n");
    char address_buffer[100];
    getnameinfo((struct sockaddr*) &client_address, client_len, address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
    printf("%s\n", address_buffer);

    /*
    Since we are programming a web server, we expect the client (such as a
    web browser) to send us an HTTP request. We read this request using the
    recv() function, and store it in the request buffer.

    recv() returns the number of bytes that are received, and if no bytes 
    have been received, it blocks until it has something. If connection is
    prematurely terminated by the client, recv() returns 0 or -1 (depending
    on the circumstances). For the sake of this code, that situation is 
    ignored, but in production code it must always be accounted for.

    Similarly, in production code, the request recieved from the client should
    be properly processed and examine which resource the client is requesting.
    For now, we ignore the body of the request altogether.
    */

    printf("Reading request...\n");
    char request[1024];
    int bytes_received = recv(socket_client, request, 1024, 0);
    printf("Received %d bytes\n", bytes_received);

    /*
    Print the browser's request to the console. Note that "%.*s" is used instead
    of the typical "%s" to signify we want to print bytes_received amount of 
    characters. There is no guarantee that the data received from recv() is 
    null terminated, and printing it with printf(request) or 
    printf("%s", request) may lead to a segmentation fault!
    */
    printf("%.*s", bytes_received, request);

    /*
    When responding to an HTTP request, we must prefix our content with an HTTP
    response header. The first three lines ("HTTP, Connection, Content-Type")
    compose this header, while the last ("Local time is") is the beginning of
    our response message. The response header contains everything the client
    needs to know about the oncoming data:

        HTTP/1.1 200 OK             (The request is okay)
        Connection: close           (The server will terminate the connection 
                                    after sending all the data)
        Content-Type: text/plain    (Received data will be plain text)

    The HTTP response header ends with a blank line which partitions it from 
    the content of the response. HTTP requires line endings to take the form 
    of a carriage return followed by a newline, so a blank line is "\r\n."
    The part of the response that comes after the blank line is treated by 
    browsers as blank text.

    The send() function is what is used to actually send data to clients.
    This function takes a client's socket, a pointer to the data to be sent,
    and the length of the data to send, as well as some flags (unused, so 
    we send 0). send() also returns the amount of bytes sent, and as good
    practice we check that this is the same as what we expected to send.
    */

    printf("Sending response...\n");
    const char *response = 
        "HTTP/1.1 200 OK\r\n"
        "Connection: close\r\n"
        "Content-Type: text/plain\r\n\r\n"
        "Local time is "; 
    int bytes_sent = send(socket_client, response, strlen(response), 0);
    printf("Sent %d of %d bytes\n", bytes_sent, (int)strlen(response));


    /*
    After the HTTP header and beginning of our message is sent, we can send
    the rest of our message, which features the actual time. We get the local
    time simply, using the same method we used in the initial version of this
    program (pg. 54). We send it using send().
    */
    time_t timer;
    time(&timer);
    char *time_msg = ctime(&timer);
    bytes_sent = send(socket_client, time_msg, strlen(time_msg), 0);
    printf("Sent %d of %d bytes.\n", bytes_sent, (int)strlen(time_msg));

    /*
    We then close the client connection to indicate to the client that we have
    sent all the data they requested. If not closed, the client will wait for
    more data until it times out. 

    In a real server, at this point we could call accept() on socket_listen to
    accept additional connections, but for a simple program like this we simply
    terminate. On Windows, a small amount of cleanup is required, which is 
    performed by WSACleanup().
    */

    printf("Closing connection...\n");
    CLOSESOCKET(socket_client);

    #if defined(_WIN32)
        WSACleanup();
    #endif

    printf("Finished.\n");

    return 0;
    
}