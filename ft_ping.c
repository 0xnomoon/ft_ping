#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/ip_icmp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

// Define maximum packet size and other constants
#define PACKET_SIZE 64
#define TIMEOUT 1

// Function to calculate checksum
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

// Function to handle the help message (-?)
void print_usage() {
    printf("Usage: ft_ping [options] <destination>\n");
    printf("Options:\n");
    printf("  -v          Verbose output.\n");
    printf("  -?          Show this help message.\n");
    exit(EXIT_SUCCESS);
}

// Main function
int main(int argc, char **argv) {
    int sockfd;
    char packet[PACKET_SIZE];
    struct icmp *icmp_hdr;
    struct sockaddr_in addr;
    struct timeval timeout;
    int verbose = 0;
    int ttl = 64;
    struct hostent *host;
    int opt;

    // Parse command-line arguments
    while ((opt = getopt(argc, argv, "v?")) != -1) {
        switch (opt) {
            case 'v':
                verbose = 1;
                break;
            case '?':
                print_usage();
                break;
            default:
                print_usage();
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Expected destination address after options.\n");
        exit(EXIT_FAILURE);
    }

    char *destination = argv[optind];

    // Get host by name (without DNS resolution in return packet)
    if ((host = gethostbyname(destination)) == NULL) {
        fprintf(stderr, "Unknown host: %s\n", destination);
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr, host->h_addr, host->h_length);

    // Create raw socket
    if ((sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options (TTL)
    setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // Prepare ICMP header
    icmp_hdr = (struct icmp *) packet;
    icmp_hdr->icmp_type = ICMP_ECHO;
    icmp_hdr->icmp_code = 0;
    icmp_hdr->icmp_id = getpid();
    icmp_hdr->icmp_seq = 1;
    icmp_hdr->icmp_cksum = checksum(icmp_hdr, PACKET_SIZE);

    // Send packet
    if (sendto(sockfd, packet, PACKET_SIZE, 0, (struct sockaddr *) &addr, sizeof(addr)) <= 0) {
        perror("Packet sending failed");
        exit(EXIT_FAILURE);
    }

    // Receive packet
    int addr_len = sizeof(addr);
    if (recvfrom(sockfd, packet, PACKET_SIZE, 0, (struct sockaddr *) &addr, &addr_len) <= 0) {
        if (verbose) {
            fprintf(stderr, "Failed to receive packet: %s\n", strerror(errno));
        }
    } else {
        printf("Received reply from %s\n", inet_ntoa(addr.sin_addr));
    }

    close(sockfd);
    return 0;
}

