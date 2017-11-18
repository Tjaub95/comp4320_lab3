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

// Struct that will be used to recieve unverified incoming packets.
struct incoming_unverified_packet {
    unsigned int should_be_magic_number;
    unsigned char extra_char[7];
} __attribute__((__packed__));

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

typedef struct incoming_unverified_packet iu_packet;

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
        printf("Received response from server...\n");
        // Create a tcp server that waits
    }
        // Found waiting client case
    else if (numbytes_rx == 11) {
        printf("Received response from server...\n");
        // Create a tcp client
        // Start chat session with server
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
