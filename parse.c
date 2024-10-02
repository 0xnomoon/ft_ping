#include "ping.h"

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

static const char supported_opts[] = "hqv";

int check_right()
{
	if (getuid() != 0) {
		printf("ft_ping: usage error: need to be run as root\n");
		return -1;
	}
	return 0;
}

static int parse_arg(char *arg, struct options *opts)
{
	char *match = NULL;
	size_t len = strlen(arg);

	/* i = 1 because of '-' character before any option */
	for (size_t i = 1; i < len; ++i) {
		if ((match = strchr(supported_opts, arg[i])) != NULL) {
			switch (*match) {
			case 'h':
				opts->help = 1;
				break;
			case 'q':
				opts->quiet = 1;
				break;
			case 'v':
				opts->verb = 1;
				break;
			default:
				printf("ping: unknown option\n");
			}
		} else {
			printf("ft_ping: invalid option -- '%c'\n", arg[i]);
			printf("Try 'ft_ping -h' for more information.\n");
			return -1;
		}
	}
	return 0;
}

int check_args(int ac, char **av, char **host, struct options *opts)
{
	int nb_host = 0;

	for (int i = 1; i < ac; ++i) {
		if (av[i][0] == '-' && strlen (av[i]) > 1) {
			if (parse_arg(av[i], opts) == -1)
				return -1;
		} else {
			*host = av[i];
			++nb_host;
		}
		if (opts->help) {
			print_help();
			return 1;
		}
	}
	if (!nb_host) {
		printf("ft_ping: missing host operand\n");
		printf("Try 'ft_ping -h' for more information.\n");
		return -1;
	} else if (nb_host > 1) {
		printf("ft_ping: only one host is needed\n");
		printf("Try 'ft_ping -h' for more information.\n");
		return -1;
	}
	return 0;
}