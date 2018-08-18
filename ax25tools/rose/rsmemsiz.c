#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../pathnames.h"

enum meminfo_row { meminfo_main = 0,
		   meminfo_swap };

enum meminfo_col { meminfo_total = 0, meminfo_used, meminfo_free,
		   meminfo_shared, meminfo_buffers, meminfo_cached
};

unsigned read_total_main(void);

static int getuptime(double *uptime_secs)
{
	struct sysinfo si;

	if (sysinfo(&si) < 0)
		return -1;

	*uptime_secs = si.uptime;

	return 0;
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

static unsigned **meminfo(void)
{
	static unsigned *row[MAX_ROW + 1];	/* row pointers */
	static unsigned num[MAX_ROW * MAX_COL];	/* number storage */
	static int n, fd = -1;
	char buf[300];
	char *p;
	int i, j, k, l;

	/*
	 * Open FILE only if necessary and seek to 0 so that successive calls
	 * to the functions are more efficient.  It also reads the current
	 * contents of the file into the global buf.
	 */
	if (fd == -1 && (fd = open(PROC_MEMINFO_FILE, O_RDONLY)) == -1) {
		fprintf(stdout, "ERROR: file %s, %s\r",
		        PROC_MEMINFO_FILE, strerror(errno));

		return NULL;
	}
	lseek(fd, 0L, SEEK_SET);
	n = read(fd, buf, sizeof buf - 1);
	if (n < 0) {
		fprintf(stdout, "ERROR: file %s, %s\r",
		        PROC_MEMINFO_FILE, strerror(errno));
		close(fd);
		fd = -1;

		return NULL;
	}
	buf[n] = '\0';

	if (!row[0])				/* init ptrs 1st time through */
	for (i=0; i < MAX_ROW; i++)		/* std column major order: */
		row[i] = num + MAX_COL*i;	/* A[i][j] = A + COLS*i + j */
	p = buf;
	for (i=0; i < MAX_ROW; i++)		/* zero unassigned fields */
		for (j=0; j < MAX_COL; j++)
			row[i][j] = 0;
	for (i=0; i < MAX_ROW && *p; i++) {	/* loop over rows */
		while (*p && !isdigit(*p)) p++;	/* skip chars until a digit */
		for (j=0; j < MAX_COL && *p; j++) {	/* scanf column-by-column */
		l = sscanf(p, "%u%n", row[i] + j, &k);
		p += k;				/* step over used buffer */
		if (*p == '\n' || l < 1)	/* end of line/buffer */
			break;
		}
    	}
/*    row[i+1] = NULL;	terminate the row list, currently unnecessary */
	return row;				/* NULL return ==> error */
}


/*
 * by Heikki Hannikainen <oh7lzb@sral.fi>
 * The following was mostly learnt from the procps package and the
 * gnu sh-utils (mainly uname).
 */

int main(int argc, char **argv)
{
	int upminutes, uphours, updays;
	double uptime_secs;
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
	if (getuptime(&uptime_secs) < 0) {
		fprintf(stdout, "Cannot get system uptime\r");
		uptime_secs = 0;
	}

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

	getloadavg(av, 3);
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
