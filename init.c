#include "ping.h"

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

static int init_sock_addr(struct sockinfo *si)
{
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_RAW,
		.ai_protocol = IPPROTO_ICMP
	};
	struct addrinfo *tmp;

	if (getaddrinfo(si->host, NULL, &hints, &tmp) != 0) {
		printf("ft_ping: unknown host\n");
		return -1;
	}
	si->remote_addr = *(struct sockaddr_in *)tmp->ai_addr;
	freeaddrinfo(tmp);

	if (inet_ntop(AF_INET, &si->remote_addr.sin_addr, si->str_sin_addr,
	    INET_ADDRSTRLEN) == NULL) {
		printf("inet_ntop: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

static int create_socket(uint8_t ttl)
{
	int sock_fd;

	if ((sock_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1) {
		printf("socket: %s\n", strerror(errno));
		return -1;
	}
	if (setsockopt(sock_fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == -1) {
		printf("setsockopt: %s\n", strerror(errno));
		close(sock_fd);
		return -1;
	}
	return sock_fd;
}

int init_sock(int *sock_fd, struct sockinfo *si, char *host, int ttl)
{
	si->host = host;
	if (init_sock_addr(si) == -1)
		return -1;
	if ((*sock_fd = create_socket(ttl)) == -1)
		return -1;
	return 0;
}