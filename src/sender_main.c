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
#include <time.h>
#include <sys/stat.h>

#define MAXPAYLOAD 1472 // max number of bytes we can get at once
#define MAXDATA 1468 // MAXPAYLOAD - 4
#define TIMEOUT 30 // milliseconds to timeout
#define CWND_START 1
#define SSTHRESH_START 50 // an arbitrarily high value

typedef enum { false, true } bool;

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

	// Get file size
    struct stat st;
    stat(filename, &st);

    // Either send the requsted number of bytes, or the entire file
    if (st.st_size < bytesToTransfer) {
    	bytesToTransfer = st.st_size;
    }

	// Calculate number of packets
	uint32_t num_packets = (bytesToTransfer + MAXDATA - 1) / MAXDATA; //ceiling division
	printf("sender: calculated number of packets: %u\n", num_packets);

    // loop for initial SYN/ACK 
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

    // Packet should contain sequence number as well as data.
    // Sequence number needs to be transformed from host to network endian
    struct Packet
    {
    	uint32_t seq_num_to_n;
    	char data[MAXDATA];
    } packets[num_packets];

    // Read file and convert to packets.
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
    	perror("sender: can't open file");
    }
    
	uint32_t i, ack_response;
	size_t last_packet_size;
	for (i = 0; i < num_packets; ++i)
	{
		packets[i].seq_num_to_n = htonl(i);
		last_packet_size = fread(packets[i].data, 1, sizeof(packets[i].data), fp);
	}

	// TODO: only read in the number of bytes as limited by numberOfBytes

	fclose(fp);


	// // window = 1
	// // 
	// struct Packet_Info {
	// 	time_t time_sent;
	// 	bool received;
	// } packet_info[num_packets];
 	//    memset(&packet_info, 0, sizeof(Packet_Info) * num_packets);

	// whi
	// send base
	// record time sent
	// send packets until base + window_size
	// recv ack
	// window_size++
	// base = ack + 1
	// if timeout
	// 		next_seq = base;
	//		window_size = 1;
	//

	uint32_t base = 0;
	uint32_t next_seq = 0;
	int window_size = CWND_START;

	int cwnd = CWND_START;
	int ssthresh = SSTHRESH_START;

	while (num_packets != 1) {
		printf("New cwnd: %d \n", cwnd);
		while (next_seq < base + cwnd && next_seq < num_packets - 1) {
			send(sockfd, &packets[next_seq], sizeof(struct Packet), 0);
			// printf("sender: sent packet %u/%u\n", next_seq, num_packets - 1);
			++next_seq;
		}
		ssize_t recv_length = recv(sockfd, &ack_response, sizeof(ack_response), 0);
		if (recv_length < -1) {
			printf("packet dropped\n");
			next_seq = base;
			ssthresh = cwnd >> 1;
			cwnd = 1;
			continue;
		}
		ack_response = ntohl(ack_response);
		// printf("sender: got ack %u/%u\n", ack_response, num_packets - 1);
		// Break out of loop when you got last second to last ack.
		if (ack_response == num_packets - 2) {
			break;
		}
		base = ack_response + 1;
		// In exponential slow start, increment window size for each ack received
		if (cwnd < ssthresh)
			cwnd += 1;
		// if we're past the threshold, linearly increase it
		else
			cwnd += 1/cwnd;
	}

	// Send last packet
	i = num_packets - 1;
	send(sockfd, &packets[i], last_packet_size + sizeof(uint32_t), 0);
	printf("sender: sent packet %u/%u\n", i, num_packets - 1);
	while (i < num_packets)
	{
		send(sockfd, &packets[i], last_packet_size + sizeof(uint32_t), 0);
		printf("sender: sent packet %u/%u\n", i, num_packets - 1);
		recv(sockfd, &ack_response, sizeof(ack_response), 0);
		ack_response = ntohl(ack_response);
		printf("sender: got ack %u/%u\n", ack_response, num_packets - 1);
		if (ack_response == i)
		{
			++i;
		}
	}

	// Send kill packets
	for (i = 0; i < 100; ++i)
	{
		send(sockfd, &ack_response, sizeof(uint32_t), 0);
	}

    close(sockfd);
}