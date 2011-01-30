#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <netax25/daemon.h>

int daemon_start(int ignsigcld)
{
	/* Code to initialiaze a daemon process. Taken from _UNIX Network	*/
	/* Programming_ pp.72-85, by W. Richard Stephens, Prentice		*/
	/* Hall PTR, 1990							*/

	int childpid, fd;

	/* If started by init, don't bother */
	if (getppid() == 1)
		goto out;

	/* Ignore the terminal stop signals */
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);

	/* Fork and let parent exit, insures we're not a process group leader */
	if ((childpid = fork()) < 0) {
		return 0;
	} else if (childpid > 0) {
		exit(0);
	}

	/* Disassociate from controlling terminal and process group.		*/
	/* Ensure the process can't reacquire a new controlling terminal.	*/
	if (setpgrp() == -1)
		return 0;

	if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
		/* loose controlling tty */
		ioctl(fd, TIOCNOTTY, NULL);
		close(fd);
	}

out:
	/* Move the current directory to root, to make sure we aren't on a	*/
	/* mounted filesystem.							*/
	chdir("/");

	/* Clear any inherited file mode creation mask.	*/
	umask(0);

	if (ignsigcld)
		signal(SIGCHLD, SIG_IGN);

	/* That's it, we're a "daemon" process now */
	return 1;
}
