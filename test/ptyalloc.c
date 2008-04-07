/* ptyalloc.c
 *
 * Test the pseudo-terminal allocation methods in YTalk.
 */

#include "config.h"
#include <stdio.h>

int getpty();

#ifdef HAVE_OPENPTY
int openpty();
#endif

int main() {

	char buf[128];
	int fd, fds;
	int g = 0, o = 0;

	fd = getpty(buf);
	if (fd < 0)
		g++;

#ifdef HAVE_OPENPTY
	if (openpty(&fd, &fds, buf, 0, 0) < 0)
#endif
		o++;

	if (g && o) {
		fprintf(stderr, "Both getpty() and openpty() failed.\n");
		return 1;
	}

	return 0;
}
