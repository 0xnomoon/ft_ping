#include "ping.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/ip_icmp.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#define RECV_PACK_SIZE ((IP_HDR_SIZE + ICMP_HDR_SIZE) * 2 + ICMP_BODY_SIZE + 1)

static unsigned short checksum(unsigned short *ptr, int nbytes) {
	unsigned long sum;
	unsigned short oddbyte;

	sum = 0;
	while (nbytes > 1) {
		sum += *ptr++;
		nbytes -= 2;
	}
	if (nbytes == 1) {
		oddbyte = 0;
		*((unsigned char *)&oddbyte) = *(unsigned char *)ptr;
		sum += oddbyte;
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return (unsigned short) ~sum;
}

static int fill_icmp_echo_packet(uint8_t *buf, int packet_len)
{
	static int seq = 0;
	struct icmphdr *hdr = (struct icmphdr *)buf;
	struct timeval *timestamp = skip_icmphdr(buf);

	if (gettimeofday(timestamp, NULL) == -1) {
		printf("gettimeofday err: %s\n", strerror(errno));
		return -1;
	}
	hdr->type = ICMP_ECHO;
	hdr->un.echo.id = getpid();
	hdr->un.echo.sequence = seq++;
	hdr->checksum = checksum((unsigned short *)buf, packet_len);
	return 0;
}

int icmp_send_ping(int sock_fd, const struct sockinfo *si, struct packinfo *pi)
{
	ssize_t nb_bytes;
	uint8_t buf[sizeof(struct icmphdr) + ICMP_BODY_SIZE] = {};

	if (fill_icmp_echo_packet(buf, sizeof(buf)) == -1)
		return -1;

	nb_bytes = sendto(sock_fd, buf, sizeof(buf), 0,
			  (const struct sockaddr *)&si->remote_addr,
			  sizeof(si->remote_addr));
	if (nb_bytes == -1)
		goto err;
	pi->nb_send++;
	return 0;

err:
	if (errno == EACCES) {
		printf("ft_ping: socket access error. Are you trying "
		       "to ping broadcast ?\n");
	} else {
		printf("sendto err: %s\n", strerror(errno));
	}
	return -1;
}

static _Bool is_addressed_to_us(uint8_t *buf)
{
	struct icmphdr *hdr_sent;
	struct icmphdr *hdr_rep = (struct icmphdr *)buf;

	/* To discard our own ICMP echo request when pinging localhost */
	if (hdr_rep->type == ICMP_ECHO)
		return 0;

	/* If error, jumping to ICMP sent packet header stored in body */
	if (hdr_rep->type != ICMP_ECHOREPLY)
		buf += ICMP_HDR_SIZE + IP_HDR_SIZE;
	hdr_sent = (struct icmphdr *)buf;

	return hdr_sent->un.echo.id == getpid();
}

int icmp_recv_ping(int sock_fd, struct packinfo *pi, const struct options *opts)
{
	uint8_t buf[RECV_PACK_SIZE] = {};
	ssize_t nb_bytes;
	struct icmphdr *icmph;
	struct iovec iov[1] = {
		[0] = { .iov_base = buf, .iov_len = sizeof(buf)}
	};
	struct msghdr msg = { .msg_iov = iov, .msg_iovlen = 1 };

	nb_bytes = recvmsg(sock_fd, &msg, MSG_DONTWAIT);
	if (errno != EAGAIN && errno != EWOULDBLOCK && nb_bytes == -1) {
		printf("recvmsg err: %s\n", strerror(errno));
		return -1;
	} else if (nb_bytes == -1) {
		return 0;
	}
	icmph = skip_iphdr(buf);
	if (!is_addressed_to_us((uint8_t *)icmph))
		return 0;

	if (icmph->type == ICMP_ECHOREPLY) {
		pi->nb_ok++;
		if (rtts_save_new(pi, icmph) == NULL)
			return -1;
	}
	if (print_recv_info(buf, nb_bytes, opts, pi) == -1)
		return -1;
	return 1;
}