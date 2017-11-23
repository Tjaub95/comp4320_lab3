/*
** talker.c -- a datagram "client" demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_MESSAGE_LEN 1000
#define GROUP_ID 12
#define MAGIC_NUMBER 0x4A6F7921
#define BACKLOG 10

// Struct that will be used to send data to the Server
struct transmitted_packet {
    unsigned int magic_num;
    unsigned short port_num;
    unsigned char gid_client;
} __attribute__((__packed__));

typedef struct transmitted_packet tx_packet;

struct waiting_client {
    unsigned int magic_num;
    unsigned char gid_server;
    unsigned short port_num;
} __attribute__((__packed__));

typedef struct waiting_client wc_packet;

struct waiting_client_found {
    unsigned int magic_num;
    unsigned int ip_addr_waiting_client;
    unsigned short port_num_waiting_client;
    unsigned char gid_server;
} __attribute__((__packed__));

typedef struct waiting_client_found wcf_packet;

struct error_received {
    unsigned int magic_num;
    unsigned char gid_server;
    unsigned short error_code;
} __attribute__((__packed__));

typedef struct error_received er_packet;

// Struct that will be used to receive unverified incoming packets.
struct incoming_unverified_packet {
    unsigned int should_be_magic_number;
    unsigned char extra_char[7];
} __attribute__((__packed__));

typedef struct incoming_unverified_packet iu_packet;

struct client_message {
    const char *client_message;
} __attribute__((__packed__));

typedef struct client_message cm_packet;

/*
* This function combines four bytes to get an int.
*
*/
unsigned int make_int(unsigned char a, unsigned char b,
                      unsigned char c, unsigned char d);

/*
* This function combines two bytes to get a short.
*
*/
unsigned short make_short(unsigned char a, unsigned char b);

int tcp_server(wc_packet server_info);

int tcp_client(wcf_packet server_info_in);

void sigchld_handler(int s) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes_tx;
    int numbytes_rx;
    struct sockaddr_in their_addr;
    socklen_t addr_len;

    char *my_server;    // The host server name
    char *server_port;    // The port we will be using
    char *my_port;      // The port we are willing to play on

    // The packet we will send
    tx_packet packet_out;

    if (argc != 4) {
        fprintf(stderr, "Invalid args, expected serverName, serverPort, myPort.\n");
        exit(1);
    }

    // Get the params from the command line
    my_server = argv[1];
    server_port = argv[2];
    my_port = argv[3];

    // Check to make sure the Port Number is in the correct range
    if (atoi(my_port) < (10010 + (GROUP_ID * 5))
        || atoi(my_port) > (10010 + (GROUP_ID * 5) + 4)) {
        printf("Error: Port number was '%s' this is not in range of [",
               my_port);
        printf("%d, %d]\n", 10010 + GROUP_ID * 5,
               10010 + GROUP_ID * 5 + 4);
        exit(1);
    }

    // Get the Packet Ready to Send
    packet_out.magic_num = (int) htonl(MAGIC_NUMBER);
    packet_out.gid_client = GROUP_ID;
    packet_out.port_num = htons((unsigned short) strtoul(my_port, NULL, 0));

    memset(&hints, 0, sizeof hints);    // put 0's in all the mem space for hints (clearing hints)
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(my_server, server_port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("Error: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Error: failed to bind socket\n");
        return 2;
    }

    // Send the data to the server
    if ((numbytes_tx = sendto(sockfd, (char *) &packet_out, sizeof(packet_out), 0,
                              p->ai_addr, p->ai_addrlen)) == -1) {
        perror("Error: sendto");
        exit(1);
    }

    addr_len = sizeof their_addr;

    // Create the structs needed for receiving a packet
    iu_packet inc_packet;

    if ((numbytes_rx = recvfrom(sockfd, (char *) &inc_packet, MAX_MESSAGE_LEN, 0,
                                (struct sockaddr *) &their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
    }

    // Error case
    if (numbytes_rx == 7 && inc_packet.extra_char[1] == 0x00) {
        er_packet error;

        error.magic_num = inc_packet.should_be_magic_number;
        error.gid_server = inc_packet.extra_char[0];
        error.error_code = make_short(inc_packet.extra_char[1], inc_packet.extra_char[2]);

        if (error.error_code == 1) {
            printf("Error: The magic number in the sent request was incorrect.\n");
            printf("Error code: %X\n", error.error_code);
            exit(1);
        } else if (error.error_code == 2) {
            printf("Error: The packet length of the sent request was incorrect.\n");
            printf("Error code: %X\n", error.error_code);
            exit(1);
        } else if (error.error_code == 4) {
            printf("Error: The port in the request was not a member of the correct range.\n");
            printf("Error code: %X\n", error.error_code);
            exit(1);
        } else {
            printf("Error: There was an unknown issue");
            exit(1);
        }
    }
        // Waiting case
    else if (numbytes_rx == 7) {
        printf("Received response from server...\n\n");
        // Create a tcp server that waits
        wc_packet wait_packet;

        wait_packet.magic_num = inc_packet.should_be_magic_number;
        wait_packet.gid_server = inc_packet.extra_char[0];
        wait_packet.port_num = make_short(inc_packet.extra_char[1], inc_packet.extra_char[2]);

        wait_packet.magic_num = (unsigned int) htonl(wait_packet.magic_num);

        printf("Received Magic Number is: %X\n", wait_packet.magic_num);
        printf("Received Server GID is: %d\n", wait_packet.gid_server);
        printf("Received Port Number is: %d\n\n", wait_packet.port_num);

        printf("Waiting for partner to connect\n\n");

        tcp_server(wait_packet);
    }
        // Found waiting client case
    else if (numbytes_rx == 11) {
        printf("Received response from server...\n");
        // Create a tcp client
        // Start chat session with server
        wcf_packet wait_found_packet;

        wait_found_packet.magic_num = inc_packet.should_be_magic_number;
        wait_found_packet.ip_addr_waiting_client = make_int(inc_packet.extra_char[0], inc_packet.extra_char[1],
                                                            inc_packet.extra_char[2], inc_packet.extra_char[3]);
        wait_found_packet.port_num_waiting_client = make_short(inc_packet.extra_char[4], inc_packet.extra_char[5]);
        wait_found_packet.gid_server = inc_packet.extra_char[6];

        wait_found_packet.magic_num = (unsigned int) htonl(wait_found_packet.magic_num);

        printf("Received Magic Number is: %X\n", wait_found_packet.magic_num);
        printf("Received Server GID is: %d\n", wait_found_packet.gid_server);
        printf("Received Port Number is: %d\n", wait_found_packet.port_num_waiting_client);
        printf("Received IP address is: %d.%d.%d.%d\n", (int) (their_addr.sin_addr.s_addr & 0xFF),
               (int) ((their_addr.sin_addr.s_addr & 0xFF00) >> 8),
               (int) ((their_addr.sin_addr.s_addr & 0xFF0000) >> 16),
               (int) ((their_addr.sin_addr.s_addr & 0xFF000000) >> 24));


        tcp_client(wait_found_packet);
    }

    freeaddrinfo(servinfo);
    close(sockfd);

    return 0;
}

/*
* This function combines four bytes to get an int.
*
*/
unsigned int make_int(unsigned char a, unsigned char b,
                      unsigned char c, unsigned char d) {
    unsigned int val = 0;
    val = a;
    val <<= 8;
    val |= b;

    val <<= 8;
    val |= c;

    val <<= 8;
    val |= d;

    return val;
}

/*
* This function combines two bytes to get a short.
*
*/
unsigned short make_short(unsigned char a, unsigned char b) {
    unsigned short val = 0;
    val = a;
    val <<= 8;
    val |= b;
    return val;
}

int tcp_server(wc_packet server_info) {
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints_tcp, *servinfo_tcp, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s[INET6_ADDRSTRLEN];
    int rv;


    char port_num[5] = {0};      // The port we are willing to play on

    // Converts the short back to a char*
    sprintf(port_num, "%d", ntohs(server_info.port_num));

    memset(&hints_tcp, 0, sizeof hints_tcp);
    hints_tcp.ai_family = AF_UNSPEC;
    hints_tcp.ai_socktype = SOCK_STREAM;
    hints_tcp.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, port_num, &hints_tcp, &servinfo_tcp)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for (p = servinfo_tcp; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(servinfo_tcp); // all done with this structure

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);
    if (new_fd == -1) {
        perror("accept");
        //	continue;
    }

    inet_ntop(their_addr.ss_family,
              get_in_addr((struct sockaddr *) &their_addr),
              s, sizeof s);

    cm_packet client_mess;

    char bye_bye_birdie[15] = "Bye Bye Birdie";

    do {
        if ((recv(new_fd, (char *) &client_mess, sizeof(client_mess), 0)) == -1) {
            perror("recv_error");
            exit(1);
        }
        printf("Connection received!\n");
        printf("Client message is %s\n", client_mess.client_message);
    } while (strcmp(client_mess.client_message, bye_bye_birdie) != 0);

    return 0;
}

int tcp_client(wcf_packet server_info_in) {
    int sockfd, numbytes;
    char buf[MAX_MESSAGE_LEN];
    struct addrinfo hints, *servinfo, *p;
    int status;
    char s[INET6_ADDRSTRLEN];

    char *hostname;
    char port[5] = {0};      // The port we are willing to play on

    // Converts the short back to a char*
    sprintf(port, "%d", ntohs(server_info_in.port_num_waiting_client));

    // Converts the hex IP address into a char* using dotted notation
    struct in_addr addr;
    addr.s_addr = htonl(server_info_in.ip_addr_waiting_client);
    hostname = inet_ntoa(addr);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    // Loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("Socket error");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Connect error");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "Failed to connect!\n");
        return 2;
    }

    freeaddrinfo(servinfo);    // All done with this structure

    cm_packet client_mess;

    client_mess.client_message = "Hello World!\0";

    if (send(sockfd, (char *) &client_mess, sizeof(client_mess), 0) == -1) {
        perror("Send Error");
    }
}