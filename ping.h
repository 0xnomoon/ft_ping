#ifndef PING_H
#define PING_H

#include <netinet/ip_icmp.h>

#define IP_TTL_VALUE 64

#define IP_HDR_SIZE (sizeof(struct iphdr))
#define ICMP_HDR_SIZE (sizeof(struct icmphdr))
#define ICMP_BODY_SIZE 56

enum e_exitcode {
	E_EXIT_OK,
	E_EXIT_ERR_HOST,
	E_EXIT_ERR_ARGS = 64
};

/**
 * The different available options for ft_ping.
 * @help: Help option (display help).
 * @quiet: Quiet option (no output when receiving a packet).
 * @verb: Verbose option (display content of each received packet).
 */
struct options {
	_Bool help;
	_Bool quiet;
	_Bool verb;
};

struct rtt_node {
	struct timeval val;
	struct rtt_node *next;
};

struct packinfo {
	int nb_send;
	int nb_ok;
	struct timeval *min;
	struct timeval *max;
	struct timeval avg;
	struct timeval stddev;
	struct rtt_node *rtt_list;
	struct rtt_node *rtt_last;
};

struct sockinfo {
	char *host;
	struct sockaddr_in remote_addr;
	char str_sin_addr[INET_ADDRSTRLEN];
};

static inline void * skip_iphdr(void *buf)
{
	return (void *)((uint8_t *)buf + IP_HDR_SIZE);
}

static inline void * skip_icmphdr(void *buf)
{
	return (void *)((uint8_t *)buf + ICMP_HDR_SIZE);
}

int check_right();
int check_args(int ac, char **av, char **host, struct options *opts);

int init_sock(int *sock_fd, struct sockinfo *si, char *host, int ttl);

int icmp_send_ping(int sock_fd, const struct sockinfo *si, struct packinfo *pi);
int icmp_recv_ping(int sock_fd, struct packinfo *pi,
		   const struct options *opts);

void print_help();
void print_start_info(const struct sockinfo *si, const struct options *opts);
int print_recv_info(void *buf, ssize_t nb_bytes, const struct options *opts,
                    const struct packinfo *pi);
void print_end_info(const struct sockinfo *si, struct packinfo *pi);

struct rtt_node * RTT_store(struct packinfo *pi, struct icmphdr *icmph);
void clean(struct packinfo *pi);
void rtts_stats(struct packinfo *pi);

#endif /* PING_H */