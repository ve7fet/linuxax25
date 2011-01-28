/* ax25ipd.c     main entrypoint
 *
 * Copyright 1991, Michael Westerhof, Sun Microsystems, Inc.
 * This software may be freely used, distributed, or modified, providing
 * this header is not removed.
 *
 */

/*
 * cleaned up and prototyped for inclusion into the standard linux ax25
 * toolset in january 1997 by rob mayfield, vk5xxx/vk5zeu
 */

#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <syslog.h>

#include <netax25/daemon.h>
#include <config.h>
#include <getopt.h>

#include "../pathnames.h"
#include "ax25ipd.h"

jmp_buf restart_env;

/* Prototypes */
void hupper(int);

int opt_version = 0;
int opt_loglevel = 0;
int opt_nofork = 0;
int opt_help = 0;
char opt_configfile[PATH_MAX];
char opt_ttydevice[PATH_MAX];

struct option options[] = {
	{"version", 0, NULL, 'v'},
	{"loglevel", 1, NULL, 'l'},
	{"help", 0, NULL, 'h'},
	{"configfile", 1, NULL, 'c'},
	{"ttydevice", 1, NULL, 'd'},
        {"nofork", 0, NULL, 'f'},
	{0, 0, 0, 0}
};

int main(int argc, char **argv)
{
	if (setjmp(restart_env) == 0) {
		signal(SIGHUP, hupper);
	}

	*opt_configfile = 0;
	*opt_ttydevice = 0;

	/* set up the handler for statistics reporting */
	signal(SIGUSR1, usr1_handler);
	signal(SIGINT, int_handler);
	signal(SIGTERM, term_handler);

	while (1) {
		int c;

		c = getopt_long(argc, argv, "c:d:fhl:v", options, NULL);
		if (c == -1)
			break;

		switch (c) {
		case 'c':
			strncpy(opt_configfile, optarg, sizeof(opt_configfile)-1);
			opt_configfile[sizeof(opt_configfile)-1] = 0;
			break;
		case 'd':
			strncpy(opt_ttydevice, optarg, sizeof(opt_ttydevice)-1);
			opt_ttydevice[sizeof(opt_ttydevice)-1] = 0;
			break;
                case 'f':
                        opt_nofork = 1;
			break;
		case 'h':
			opt_help = 1;
			break;
		case 'v':
			opt_version = 1;
			break;
		case 'l':
			opt_loglevel = atoi(optarg);
			break;
		default:
			opt_help = 1;
			break;
		}
	}

	if (optind < argc) {
		printf("Unknown argument '%s' ...\n\n", argv[optind++]);
		opt_help = 1;
	}

	if (opt_version == 1) {
		greet_world();
		exit(0);
	}
	if (opt_help == 1) {
		greet_world();
		printf("Usage:\n");
		printf("%s [flags]\n", argv[0]);
		printf("\nFlags:\n");
		printf
		    ("  --version, -v                 Print version of program\n");
		printf("  --help, -h                    This help screen\n");
		printf
		    ("  --loglevel NUM, -l NUM        Set logging level to NUM\n");
		printf
		    ("  --configfile FILE, -c FILE    Set configuration file to FILE\n");
		printf
		    ("  --ttydevice TTYDEV, -d TTYDEV Set device parameter to TTYDEV\n");
                printf
                    ("  --nofork, -f                  Do not put daemon in background\n");
		exit(0);
	}

	/* Initialize all routines */
	config_init();
	kiss_init();
	route_init();
	process_init();
	io_init();

	/* read config file */
	config_read(opt_configfile);

	if (opt_ttydevice[0] != '\0') {
		strncpy(ttydevice, opt_ttydevice, sizeof(ttydevice)-1);
		ttydevice[sizeof(ttydevice)-1] = '\0';
	}

	/* print the current config and route info */
	dump_config();
	dump_routes();
	dump_params();

	/* Open the IO stuff */
	io_open();

	/* if we get this far without error, let's fork off ! :-) */
        if (opt_nofork == 0) {
	    if (!daemon_start(TRUE)) {
	    	    syslog(LOG_DAEMON | LOG_CRIT, "ax25ipd: cannot become a daemon\n");
		    return 1;
	    }
        }

	/* we need to close stdin, stdout, stderr: because otherwise
	 * scripting like ttyname=$(ax25ipd | tail -1) does not work
	 */
	if (!isatty(1)) {
		fflush(stdout);
		fflush(stderr);
		close(0);
		close(1);
		close(2);
	}

	/* and let the games begin */
	io_start();

	return (0);
}


void greet_world(void)
{
	printf("\nax25ipd %s / %s\n", VERS2, VERSION);
	printf
	    ("Copyright 1991, Michael Westerhof, Sun Microsystems, Inc.\n");
	printf
	    ("This software may be freely used, distributed, or modified, providing\nthis header is not removed\n\n");
	fflush(stdout);
}

void do_stats(void)
{
	int save_loglevel;

/* save the old loglevel, and force at least loglevel 1 */
	save_loglevel = loglevel;
	loglevel = 1;

	printf("\nSIGUSR1 signal: statistics and configuration report\n");

	greet_world();

	dump_config();
	dump_routes();
	dump_params();

	printf("\nInput stats:\n");
	printf("KISS input packets:  %d\n", stats.kiss_in);
	printf("           too big:  %d\n", stats.kiss_toobig);
	printf("          bad type:  %d\n", stats.kiss_badtype);
	printf("         too short:  %d\n", stats.kiss_tooshort);
	printf("        not for me:  %d\n", stats.kiss_not_for_me);
	printf("  I am destination:  %d\n", stats.kiss_i_am_dest);
	printf("    no route found:  %d\n", stats.kiss_no_ip_addr);
	printf("UDP  input packets:  %d\n", stats.udp_in);
	printf("IP   input packets:  %d\n", stats.ip_in);
	printf("   failed CRC test:  %d\n", stats.ip_failed_crc);
	printf("         too short:  %d\n", stats.ip_tooshort);
	printf("        not for me:  %d\n", stats.ip_not_for_me);
	printf("  I am destination:  %d\n", stats.ip_i_am_dest);
	printf("\nOutput stats:\n");
	printf("KISS output packets: %d\n", stats.kiss_out);
	printf("            beacons: %d\n", stats.kiss_beacon_outs);
	printf("UDP  output packets: %d\n", stats.udp_out);
	printf("IP   output packets: %d\n", stats.ip_out);
	printf("\n");

	fflush(stdout);

/* restore the old loglevel */
	loglevel = save_loglevel;
}

void hupper(int i)
{
	printf("\nSIGHUP!\n");
	longjmp(restart_env, 1);
}

void usr1_handler(int i)
{
	printf("\nSIGUSR1!\n");
	do_stats();
}

void int_handler(int i)
{
	printf("\nSIGINT!\n");
	do_stats();
	exit(1);
}

void term_handler(int i)
{
	printf("\nSIGTERM!\n");
	do_stats();
	exit(1);
}
