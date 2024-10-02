#include "ping.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

/**
 * Calculate RTT for a packet (time to travel source -> host)
 */
static int calc_packet_rtt(struct icmphdr *icmph, struct rtt_node *new_rtt)
{
	struct timeval *send;
	struct timeval recv_time;

	send = ((struct timeval *)skip_icmphdr(icmph));
	if (gettimeofday(&recv_time, NULL) == -1) 
	{
		printf("gettimeofday err: %s\n", strerror(errno));
		return -1;
	}
	timersub(&recv_time, send, &new_rtt->val);
	return 0;
}

/**
 * Store all the previous RTT
 */
struct rtt_node * RTT_store(struct packinfo *pi, struct icmphdr *icmph)
{
	struct rtt_node *element = pi->rtt_list;
	struct rtt_node *new_rtt = NULL;

	if ((new_rtt = malloc(sizeof(*new_rtt))) == NULL)
	{
		return NULL;
	}
	if (calc_packet_rtt(icmph, new_rtt) == -1)
	{
		return NULL;
	}
	new_rtt->next = NULL;
	if (element != NULL)
	{
		while (element->next) {
			element = element->next;
		}
		element->next = new_rtt;
	} else {
		pi->rtt_list = new_rtt;
	}
	pi->rtt_last = new_rtt;
	return new_rtt;
}

/**
 * Free all nodes of the list containing RTT.
 */
void clean(struct packinfo *pi)
{
	struct rtt_node *element = pi->rtt_list;
	struct rtt_node *tmp;

	while (element) 
	{
		tmp = element;
		element = element->next;
		free(tmp);
	}
}

/**
 * Calculate the standard deviation based on average rtt.
 */
void calc_stddev(struct packinfo *pi, long nb_elem)
{
	struct rtt_node *elem = pi->rtt_list;
	struct timeval *avg = &pi->avg;
	long sec_dev = 0;
	long usec_dev = 0;
	long total_sec_dev = 0;
	long total_usec_dev = 0;

	while (elem) {
		sec_dev = elem->val.tv_sec - avg->tv_sec;
		sec_dev *= sec_dev;
		total_sec_dev += sec_dev;
		usec_dev = elem->val.tv_usec - avg->tv_usec;
		usec_dev *= usec_dev;
		total_usec_dev += usec_dev;
		elem = elem->next;
	}
	if (nb_elem - 1 > 0) {
		total_sec_dev /= nb_elem - 1;
		total_usec_dev /= nb_elem - 1;
		pi->stddev.tv_sec = (long)sqrt(total_sec_dev);
		pi->stddev.tv_usec = (long)sqrt(total_usec_dev);
	} else {
		pi->stddev.tv_sec = 0;
		pi->stddev.tv_usec = 0;
	}
}

/**
 * Fill the struct with stats
 */
void rtts_stats(struct packinfo *pi)
{
	struct rtt_node *elem = pi->rtt_list;
	long nb_element = 0;
	long total_seconds = 0;
	long total_usec = 0;

	pi->min = &elem->val;
	pi->max = &elem->val;
	while (elem) {
		if (timercmp(pi->min, &elem->val, >))
			pi->min = &elem->val;
		else if (timercmp(pi->max, &elem->val, <))
			pi->max = &elem->val;

		total_seconds += elem->val.tv_sec;
		total_usec += elem->val.tv_usec;
		if (total_usec > 100000) {
			total_usec -= 100000;
			++total_seconds;
		}
		++nb_element;
		elem = elem->next;
	}
	pi->avg.tv_sec = total_seconds / nb_element;
	pi->avg.tv_usec = total_usec / nb_element;
	calc_stddev(pi, nb_element);
}