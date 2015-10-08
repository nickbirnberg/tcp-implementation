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
#define MAXDATA MAXPAYLOAD - 4
#define TIMEOUT 30 // milliseconds to timeout

void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, unsigned long long int bytesToTransfer);

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

void reliablyTransfer(char* hostname, char* hostUDPport, char* filename, unsigned long long int bytesToTransfer)
{
	int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(hostname, hostUDPport, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return;
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
        return;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
    	perror("sender: connect");
    }
    freeaddrinfo(servinfo);

    // create timeout for recv.
    struct timeval tv;
	tv.tv_sec = 0;  // 0 seconds
	tv.tv_usec = TIMEOUT * 1000;  // TIMEOUT milliseconds

	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    // loop for initial SYN/ACK 
    while(1) {
    	char message[] = "SYN";
	    if ((numbytes = send(sockfd, message, strlen(message), 0)) == -1) {
	        perror("sender: sendto");
	        return;
	    }
	    printf("sender: sent %d bytes to %s\n", numbytes, hostname);
	    printf("sender: waiting for ACK from receiver\n");
	    int response_len = recv(sockfd, &message, strlen(message), 0);
	    if (response_len >= 0) {
		    printf("sender: packet contains \"%s\"\n", message);
		    break;
		}
		else if (response_len == -1) {
		    perror("sender: receive ACK");
		}
		printf("sender: timeout, retrying.\n");
    }

    // Connection Established on Both Ends, can now use send()/recv() on both ends.

    // Packet should contain sequence number as well as data.
    // Sequence number needs to be transformed from host to network endian
    uint32_t seq_num = 0;
    struct Packet
    {
    	uint32_t seq_num_to_n;
    	char data[MAXDATA];
    } packet;

    // Read file and start sending.
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
    	perror("sender: can't open file");
    }
    
	size_t data_len = fread(packet.data, 1, sizeof(packet.data), fp);
	packet.seq_num_to_n = htonl(seq_num);

	send(sockfd, &packet, data_len + 4, 0);

	fclose(fp);

    printf("Bytes to Transfer: %llu\n", bytesToTransfer);

    close(sockfd);
}