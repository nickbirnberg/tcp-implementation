/*
 * sender_main.c
 *
 * Nick Birnberg (birnber2)
 * Anit Gandhi (aagandh2)
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

#define MAXPAYLOAD 1472 // max number of bytes we can get at once
#define MAXDATA 1468 // MAXPAYLOAD - 4
#define TIMEOUT 30 // milliseconds to timeout

// Connection related variables
struct addrinfo hints, *servinfo, *p;
int sockfd;
int rv;
int numbytes;

// Congestion Control
int cwnd = 1, ssthresh;
uint32_t num_packets;
int dupACKcount = 0;

// Packet struct
typedef struct packet {
	uint32_t seq_num;
	unsigned char data[MAXDATA];
} packet_t;

packet_t* packets;

bool* acked;

int make_connection(char*hostname, char* hostUDPport) {
	memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(hostname, hostUDPport, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("sender: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "sender: failed to create socket\n");
        return -1;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
    	perror("sender: connect");
    	return -1;
    }
    freeaddrinfo(servinfo);

    // create timeout for recv.
    struct timeval tv;
	tv.tv_sec = 0;  // 0 seconds
	tv.tv_usec = TIMEOUT * 1000;  // TIMEOUT milliseconds
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

	return 0;
}

void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, unsigned long long int bytesToTransfer)
{
    int ssthresh = cwnd;

    // Calculate number of packets
	num_packets = (bytesToTransfer + MAXDATA - 1) / MAXDATA; //ceiling division
	printf("sender: calculated number of packets: %u\n", num_packets);

    // packets
    packets = malloc(sizeof(packet_t) * num_packets);

    // acked
    acked = malloc(sizeof(bool) * num_packets);

    // Establish connection
    make_connection(hostname, hostUDPport);

    int cwnd_start = 0;

    // main loop
    while(1) {
		uint32_t num_packets_to_n = htonl(num_packets);
	    if ((numbytes = send(sockfd, &num_packets_to_n, sizeof(num_packets_to_n), 0)) == -1) {
	        perror("sender: sendto");
	        return;
	    }
	    printf("sender: sent calculated number (%d/4 bytes) to %s\n", numbytes, hostname);
	    printf("sender: waiting for ACK from receiver\n");

	    char ack_message[4];
	    int response_len = recv(sockfd, &ack_message, sizeof(ack_message), 0);
	    if (response_len >= 0) {
			ack_message[3] = '\0';
		    printf("sender: got %s\n", ack_message);
		    break;
		}
		else if (response_len == -1) {
		    perror("sender: receive ACK");
		}
		printf("sender: timeout, retrying.\n");
    }

    // Connection Established on Both Ends, can now use send()/recv() on both ends.

    // Read file and convert to packets.
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
    	perror("sender: can't open file");
    }
    
	uint32_t i;
	size_t last_packet_size;
	for (i = 0; i < num_packets; ++i)
	{
		packets[i].seq_num_to_n = htonl(i);
		last_packet_size = fread(packets[i].data, 1, sizeof(packets[i].data), fp);
	}

	fclose(fp);

	// Send last packet
	printf("sender: sending the final packet.\n");
	send(sockfd, &packets[num_packets - 1], last_packet_size + sizeof(uint32_t), 0);

    close(sockfd);
}

int main(int argc, char** argv)
{
	char* udpPort;
	unsigned long long int numBytes;
	
	if(argc != 5)
	{
		fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
		exit(1);
	}
	udpPort = argv[2];
	numBytes = atoll(argv[4]);
	
	reliablyTransfer(argv[1], udpPort, argv[3], numBytes);
	return 0;
} 
