/*
 * receiver_main.c
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
#define ACKBUFFERSIZE 


void reliablyReceive(char* myUDPport, char* destinationFile);

int main(int argc, char** argv)
{
	char* udpPort;
	
	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}
	
	udpPort = argv[1];
	
	reliablyReceive(udpPort, argv[2]);
	return 0;
}

void *get_in_addr(struct sockaddr *sa) {
  return sa->sa_family == AF_INET
    ? (void *) &(((struct sockaddr_in*)sa)->sin_addr)
    : (void *) &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void reliablyReceive(char* myUDPport, char* destinationFile)
{
	int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, myUDPport, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("receiver: socket");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("receiver: bind");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "receiver: failed to bind socket\n");
        return;
    }

    printf("receiver: waiting to receive number of packets...\n");
    uint32_t number_of_packets;

    addr_len = sizeof their_addr;
    if ((numbytes = recvfrom(sockfd, &number_of_packets, sizeof(number_of_packets), 0,
        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        return;
    }
    number_of_packets = ntohl(number_of_packets);

    printf("receiver: got packet from %s\n",
        inet_ntop(their_addr.ss_family,
            get_in_addr((struct sockaddr *)&their_addr),
            s, sizeof s));
    printf("receiver: file will come in %u packets\n", number_of_packets);

    if (connect(sockfd, (struct sockaddr*)&their_addr, addr_len) == -1)
    {
    	perror("receiver: connect");
    }

    freeaddrinfo(servinfo);

    char ack[] = "ACK";

    printf("receiver: sending ACK packet to sender.\n");
    send(sockfd, ack, strlen(ack), 0);

    /* Receive data */
    struct Packet
    {
    	uint32_t seq_num_from_n;
    	char data[MAXDATA];
    } packets[number_of_packets], tmp_packet;

    // Make sure the ACK/SIN pockets werent dropped.
    uint32_t last_packet_size;
    ssize_t recv_size;
    uint32_t cumulative_recv = 0;
    // TODO: first packet drop

    while (1) {
        printf("receiver: waiting for data or another SYN.\n");
        last_packet_size = recv(sockfd, &tmp_packet, sizeof(struct Packet), 0);
        if (last_packet_size == 3) {
            // There was a pocket drop, receiving SYN again.
            printf("receiver: received SYN, sending ACK packet to sender.\n");
            send(sockfd, ack, strlen(ack), 0);
            continue;
        } else if (last_packet_size == -1) {
            perror("receiver: recv");
            return;
        } else {
            uint32_t seq_number = ntohl(tmp_packet.seq_num_from_n);
            printf("receiver: received packet %u/%u. Sending ACK %u.\n", seq_number, number_of_packets - 1, 
                cumulative_recv);
            packets[seq_number] = tmp_packet;
            uint32_t cumul_to_n = htonl(cumulative_recv);
            send(sockfd, &cumul_to_n, sizeof(uint32_t), 0);
            break;
        }
    }

    while (1) {
        recv_size = recv(sockfd, &tmp_packet, sizeof(struct Packet), 0);
        if (recv_size == 4) {
            // kill packet received
            break;
        }
        uint32_t seq_number = ntohl(tmp_packet.seq_num_from_n);
        if (seq_number == cumulative_recv + 1)
        {
            ++cumulative_recv;
        }
        printf("receiver: received packet %u/%u. Sending ACK %u.\n", seq_number, number_of_packets - 1, 
            cumulative_recv);
        packets[seq_number] = tmp_packet;
        uint32_t cumul_to_n = htonl(cumulative_recv);
        send(sockfd, &cumul_to_n, sizeof(uint32_t), 0);
        if (seq_number == number_of_packets - 1) {
            last_packet_size = recv_size;
        }
    }

    // Write packets to file
    FILE *fp = fopen(destinationFile, "w");
    if (fp == NULL) {
    	perror("receiver: can't create file");
    }

    // write all but the last packet
    uint32_t i;
    for (i = 0; i < number_of_packets - 1; ++i)
    {
    	fwrite(packets[i].data, 1, sizeof(packets[i].data), fp);
    }
    // write last packet
    fwrite(packets[i].data, 1, last_packet_size - 4, fp);
    fclose(fp);

    printf("receiver: wrote data contained in packet %u to %s\n", i, destinationFile);

    close(sockfd);
}