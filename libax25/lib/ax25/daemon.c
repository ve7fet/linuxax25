#include <fcntl.h>
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
	/* Code to initialize a daemon process. Taken from _UNIX Network	*/
	/* Programming_ pp.72-85, by W. Richard Stephens, Prentice		*/
	/* Hall PTR, 1990							*/

	int childpid;

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

	/*
	 * Disassociate from controlling terminal and process group and
	 * ensure the process can't reacquire a new controlling terminal.
	 * We're freshly forked, so setsid can't fail.
	 */
	(void) setsid();

out:
	/* Move the current directory to root, to make sure we aren't on a	*/
	/* mounted filesystem.							*/
	if (chdir("/") < 0)
		return 0;

	/* Clear any inherited file mode creation mask.	*/
	umask(0);

	if (ignsigcld)
		signal(SIGCHLD, SIG_IGN);

	/* That's it, we're a "daemon" process now */
	return 1;
}
