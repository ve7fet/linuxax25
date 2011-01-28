#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

enum meminfo_row { meminfo_main = 0,
		   meminfo_swap };

enum meminfo_col { meminfo_total = 0, meminfo_used, meminfo_free,
		   meminfo_shared, meminfo_buffers, meminfo_cached
};

unsigned read_total_main(void);

/*
 * This code is slightly modified from the procps package.
 */

#define UPTIME_FILE  "/proc/uptime"
#define LOADAVG_FILE "/proc/loadavg"
#define MEMINFO_FILE "/proc/meminfo"

static char buf[300];

/* This macro opens FILE only if necessary and seeks to 0 so that successive
   calls to the functions are more efficient.  It also reads the current
   contents of the file into the global buf.
*/
#define FILE_TO_BUF(FILE) {					\
    static int n, fd = -1;					\
    if (fd == -1 && (fd = open(FILE, O_RDONLY)) == -1) {	\
	fprintf(stdout, "ERROR: file %s, %s\r", FILE, strerror(errno));	\
	close(fd);						\
	return 0;						\
    }								\
    lseek(fd, 0L, SEEK_SET);					\
    if ((n = read(fd, buf, sizeof buf - 1)) < 0) {		\
	fprintf(stdout, "ERROR: file %s, %s\r", FILE, strerror(errno));	\
	close(fd);						\
	fd = -1;						\
	return 0;						\
    }								\
    buf[n] = '\0';						\
}

#define SET_IF_DESIRED(x,y)  if (x) *(x) = (y)	/* evals 'x' twice */

int uptime(double *uptime_secs, double *idle_secs) {
    double up=0, idle=0;
    
    FILE_TO_BUF(UPTIME_FILE)
    if (sscanf(buf, "%lf %lf", &up, &idle) < 2) {
	fprintf(stdout, "ERROR: Bad data in %s\r", UPTIME_FILE);
	return 0;
    }
    SET_IF_DESIRED(uptime_secs, up);
    SET_IF_DESIRED(idle_secs, idle);
    return up;	/* assume never be zero seconds in practice */
}

int loadavg(double *av1, double *av5, double *av15) {
    double avg_1=0, avg_5=0, avg_15=0;
    
    FILE_TO_BUF(LOADAVG_FILE)
    if (sscanf(buf, "%lf %lf %lf", &avg_1, &avg_5, &avg_15) < 3) {
	fprintf(stdout, "ERROR: Bad data in %s\r", LOADAVG_FILE);
	return 0;
    }
    SET_IF_DESIRED(av1,  avg_1);
    SET_IF_DESIRED(av5,  avg_5);
    SET_IF_DESIRED(av15, avg_15);
    return 1;
}

/* The following /proc/meminfo parsing routine assumes the following format:
   [ <label> ... ]				# header lines
   [ <label> ] <num> [ <num> ... ]		# table rows
   [ repeats of above line ]
   
   Any lines with fewer <num>s than <label>s get trailing <num>s set to zero.
   The return value is a NULL terminated unsigned** which is the table of
   numbers without labels.  Convenient enumeration constants for the major and
   minor dimensions are available in the header file.  Note that this version
   requires that labels do not contain digits.  It is readily extensible to
   labels which do not *begin* with digits, though.
*/

#define MAX_ROW 3	/* these are a little liberal for flexibility */
#define MAX_COL 7

unsigned** meminfo(void) {
    static unsigned *row[MAX_ROW + 1];		/* row pointers */
    static unsigned num[MAX_ROW * MAX_COL];	/* number storage */
    char *p;
    int i, j, k, l;
    
    FILE_TO_BUF(MEMINFO_FILE)
    if (!row[0])				/* init ptrs 1st time through */
	for (i=0; i < MAX_ROW; i++)		/* std column major order: */
	    row[i] = num + MAX_COL*i;		/* A[i][j] = A + COLS*i + j */
    p = buf;
    for (i=0; i < MAX_ROW; i++)			/* zero unassigned fields */
	for (j=0; j < MAX_COL; j++)
	    row[i][j] = 0;
    for (i=0; i < MAX_ROW && *p; i++) {		/* loop over rows */
	while(*p && !isdigit(*p)) p++;		/* skip chars until a digit */
	for (j=0; j < MAX_COL && *p; j++) {	/* scanf column-by-column */
	    l = sscanf(p, "%u%n", row[i] + j, &k);
	    p += k;				/* step over used buffer */
	    if (*p == '\n' || l < 1)		/* end of line/buffer */
		break;
	}
    }
/*    row[i+1] = NULL;	terminate the row list, currently unnecessary */
    return row;					/* NULL return ==> error */
}


/*
 * by Heikki Hannikainen <oh7lzb@sral.fi> 
 * The following was mostly learnt from the procps package and the
 * gnu sh-utils (mainly uname).
 */

int main(int argc, char **argv)
{
	int upminutes, uphours, updays;
	double uptime_secs, idle_secs;
	double av[3];
	unsigned **mem;
	char *p;
	struct utsname name;
	time_t t;

	fprintf(stdout, "Linux/ROSE 001. System parameters\r");

	time(&t);
	p = ctime(&t);
	p[24] = '\r';
	fprintf(stdout, "System time:       %s", p);

	if (uname(&name) == -1)
		fprintf(stdout, "Cannot get system name\r");
	else {
		fprintf(stdout, "Hostname:          %s\r", name.nodename);
		fprintf(stdout, "Operating system:  %s %s (%s)\r", name.sysname,
			name.release, name.machine);
	}

	/* read and calculate the amount of uptime and format it nicely */
	uptime(&uptime_secs, &idle_secs);
	updays = (int) uptime_secs / (60*60*24);
	upminutes = (int) uptime_secs / 60;
	uphours = upminutes / 60;
	uphours = uphours % 24;
	upminutes = upminutes % 60;
	fprintf(stdout, "Uptime:            ");

  	if (updays)
		fprintf(stdout, "%d day%s, ", updays, (updays != 1) ? "s" : "");

	if (uphours)
		fprintf(stdout, "%d hour%s ", uphours, (uphours != 1) ? "s" : "");
	fprintf(stdout, "%d minute%s\r", upminutes, (upminutes != 1) ? "s" : "");

	loadavg(&av[0], &av[1], &av[2]);
  	fprintf(stdout, "Load average:      %.2f, %.2f, %.2f\r", av[0], av[1], av[2]);

	if (!(mem = meminfo()) || mem[meminfo_main][meminfo_total] == 0) {
		/* cannot normalize mem usage */
		fprintf(stdout, "Cannot get memory information!\r");
	} else {
		fprintf(stdout, "Memory:            %5d KB available, %5d KB used, %5d KB free\r",
			mem[meminfo_main][meminfo_total]    >> 10,
			(mem[meminfo_main][meminfo_used] -
		 	mem[meminfo_main][meminfo_buffers] -
			mem[meminfo_total][meminfo_cached]) >> 10,
			(mem[meminfo_main][meminfo_free] +
		 	mem[meminfo_main][meminfo_buffers] +
			mem[meminfo_total][meminfo_cached]) >> 10);

		fprintf(stdout, "Swap:              %5d KB available, %5d KB used, %5d KB free\r",
			mem[meminfo_swap][meminfo_total]   >> 10,
			mem[meminfo_swap][meminfo_used]    >> 10,
			mem[meminfo_swap][meminfo_free]    >> 10);
	}

	fprintf(stdout, "\r");
	fflush(stdout);

	while (1) {
		if (read(STDIN_FILENO, av, 3) <= 0)
			break;
	}

	return 0;
}
