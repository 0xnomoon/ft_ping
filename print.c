#include "ping.h"

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>


void print_help()
{
	printf("Usage: ping [OPTION...] HOST ...\n"
	       "Send ICMP ECHO_REQUEST packets to network hosts.\n\n"
	       " Options:\n"
	       "  -h                 Show help\n"
	       "  -q                 Quiet output\n"
	       "  -v                 Verbose output\n");
}

void print_start_info(const struct sockinfo *si, const struct options *opts)
{
	int pid;

	printf("PING %s (%s): %d data bytes", si->host, si->str_sin_addr, ICMP_BODY_SIZE);
	if (opts->verb) 
	{
		pid = getpid();
		printf(", id 0x%04x = %d", pid, pid);
	}
	printf("\n");
}

static void print_icmp_err(int type, int code) {
	switch (type) {
	case ICMP_DEST_UNREACH:
		switch(code) {
		case ICMP_NET_UNREACH:
			printf("Destination Net Unreachable\n");
			break;
		case ICMP_HOST_UNREACH:
			printf("Destination Host Unreachable\n");
			break;
		case ICMP_PROT_UNREACH:
			printf("Destination Protocol Unreachable\n");
			break;
		case ICMP_PORT_UNREACH:
			printf("Destination Port Unreachable\n");
			break;
		case ICMP_FRAG_NEEDED:
			printf("Frag needed\n");
			break;
		case ICMP_SR_FAILED:
			printf("Source Route Failed\n");
			break;
		case ICMP_NET_UNKNOWN:
			printf("Destination Net Unknown\n");
			break;
		case ICMP_HOST_UNKNOWN:
			printf("Destination Host Unknown\n");
			break;
		case ICMP_HOST_ISOLATED:
			printf("Source Host Isolated\n");
			break;
		case ICMP_NET_ANO:
			printf("Destination Net Prohibited\n");
			break;
		case ICMP_HOST_ANO:
			printf("Destination Host Prohibited\n");
			break;
		case ICMP_NET_UNR_TOS:
			printf("Destination Net Unreachable for Type of Service\n");
			break;
		case ICMP_HOST_UNR_TOS:
			printf("Destination Host Unreachable for Type of Service\n");
			break;
		case ICMP_PKT_FILTERED:
			printf("Packet filtered\n");
			break;
		case ICMP_PREC_VIOLATION:
			printf("Precedence Violation\n");
			break;
		case ICMP_PREC_CUTOFF:
			printf("Precedence Cutoff\n");
			break;
		default:
			printf("Dest Unreachable, Bad Code: %d\n", code);
			break;
		}
		break;
	case ICMP_SOURCE_QUENCH:
		printf("Source Quench\n");
		break;
	case ICMP_REDIRECT:
		switch(code) {
		case ICMP_REDIR_NET:
			printf("Redirect Network");
			break;
		case ICMP_REDIR_HOST:
			printf("Redirect Host");
			break;
		case ICMP_REDIR_NETTOS:
			printf("Redirect Type of Service and Network");
			break;
		case ICMP_REDIR_HOSTTOS:
			printf("Redirect Type of Service and Host");
			break;
		default:
			printf("Redirect, Bad Code: %d", code);
			break;
		}
		break;
	}
}

static void print_format(const struct timeval *rtt)
{
	long msec;
	long usec;

	msec = rtt->tv_sec * 1000 + rtt->tv_usec / 1000;
	usec = rtt->tv_usec % 1000;
	usec %= 1000;
	printf("%ld,%03ld", msec, usec);
}

static void print_err_icmp_body(uint8_t *buf)
{
	struct iphdr *ipb = skip_icmphdr((struct icmphdr *)buf);
	struct icmphdr *icmpb = skip_iphdr(ipb);
	uint8_t *bytes = (uint8_t *)ipb;
	char str[INET_ADDRSTRLEN];

	printf("IP Hdr Dump:\n");
	for (size_t i = 0; i < sizeof(struct iphdr); i += 2) {
		printf(" %02x%02x", *bytes, *(bytes + 1));
		bytes += 2;
	}
	printf("\nVr HL TOS  Len   ID Flg  off TTL Pro  cks      Src	"
	       "Dst	Data\n");
	printf(" %x  %x  %02x %04x %04x   %x %04x  %02x  %02x %04x ",
	       ipb->version, ipb->ihl, ipb->tos, ntohs(ipb->tot_len),
	       ntohs(ipb->id), ntohs(ipb->frag_off) >> 13,
	       ntohs(ipb->frag_off) & 0x1FFF, ipb->ttl, ipb->protocol,
	       ntohs(ipb->check));
	inet_ntop(AF_INET, &ipb->saddr, str, sizeof(str));
	printf("%s  ", str);
	inet_ntop(AF_INET, &ipb->daddr, str, sizeof(str));
	printf("%s\n", str);
	printf("ICMP: type %x, code %x, size %zu, id %#04x, seq 0x%04x\n",
	       icmpb->type, icmpb->code, ICMP_BODY_SIZE + sizeof(*icmpb),
	       icmpb->un.echo.id, icmpb->un.echo.sequence);
}

/**
 * Print information of a received packet following a ICMP echo request:
 *    - If it's an echo reply, print number of bytes received, source IP
 *      address, sequence number, ttl value and rtt.
 *    - If it's an error packet, print source IP address, the appropriate
 *      error message, and the detailed content of packet is verbose option is
 *      on.
 * @buf: Buffer fill with the received packet (IP header + ICMP).
 * @nb_bytes: Number of bytes received.
 * @opts: ft_ping enabled options.
 * @pi: Filled with packets' statistics.
 *
 * Return: 0 on success, -1 on error.
 */
int print_recv_info(void *buf, ssize_t nb_bytes, const struct options *opts,
                    const struct packinfo *pi)
{
	char addr[INET_ADDRSTRLEN] = {};
	struct iphdr *iph = buf;
	struct icmphdr *icmph = skip_iphdr(iph);

	inet_ntop(AF_INET, &iph->saddr, addr, INET_ADDRSTRLEN);
	if (!opts->quiet && icmph->type == ICMP_ECHOREPLY) {
		printf("%ld bytes from %s: ", nb_bytes - IP_HDR_SIZE, addr);
		printf("icmp_seq=%d ttl=%d time=", icmph->un.echo.sequence,
		        iph->ttl);
		print_format(&pi->rtt_last->val);
		printf(" ms\n");
	} else if (icmph->type != ICMP_ECHOREPLY) {
		printf("%ld bytes from %s: ", nb_bytes - IP_HDR_SIZE, addr);
		print_icmp_err(icmph->type, icmph->code);
		if (opts->verb)
			print_err_icmp_body((uint8_t *)icmph);
	}
	return 0;
}

/**
 * Calculate the percentage of lost packets.
 */
static inline float calc_packet_loss(const struct packinfo *pi)
{
	return (1.0 - (float)(pi->nb_ok) / (float)pi->nb_send) * 100.0;
}

/**
 * Print the different statistics for all send packets at the end of ft_ping
 * command.
 */
void print_end_info(const struct sockinfo *si, struct packinfo *pi)
{
	printf("\n--- %s ping statistics ---\n", si->host);
	printf("%d packets transmitted, %d packets received, "
	       "%d%% packet loss\n", pi->nb_send, pi->nb_ok,
	       (int)calc_packet_loss(pi));
	if (pi->nb_ok) {
		rtts_calc_stats(pi);
		printf("round-trip min/avg/max/stddev = ");
		print_format(pi->min);
		printf("/");
		print_format(&pi->avg);
		printf("/");
		print_format(pi->max);
		printf("/");
		print_format(&pi->stddev);
		printf(" ms\n");
	}
}