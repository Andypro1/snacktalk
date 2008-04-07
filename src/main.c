/*
 * src/main.c
 *
 * YTalk
 *
 * Copyright (C) 1990,1992,1993 Britt Yenne
 * Currently maintained by Andreas Kling
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "header.h"
#include "menu.h"
#include "mem.h"

#include <signal.h>

char errstr[MAXERR];		/* temporary string for errors */
char *vhost = NULL;		/* specified virtual host */
ylong myuid;			/* global uid */

/*
 * Clean up and exit.
 */
void
bail(n)
	int n;
{
	kill_auto();
	if (n == YTE_SUCCESS_PROMPT && (def_flags & FL_PROMPTQUIT))
		if (show_mesg("Press any key to quit.", NULL) == 0) {
			update_menu();
			bail_loop();
		}
	end_term();
	free_users();
#ifdef YTALK_DEBUG
	clear_all();
#endif
	exit(n == YTE_SUCCESS_PROMPT ? YTE_SUCCESS : n);
}

#ifndef HAVE_STRERROR
# define strerror(n)	(sys_errlist[(n)])
#endif

/*
 * Display an error.
 */
void
show_error(str)
	register char *str;
{
	register char *syserr;
	static int in_error = 0;

	if (errno == 0)
		syserr = NULL;
	else
		syserr = strerror(errno);

	if (def_flags & FL_BEEP)
		putc(7, stderr);
	if (in_error == 0 && what_term() != 0) {
		in_error = 1;
		if (show_error_menu(str, syserr) < 0) {
			show_error("show_error: show_error_menu() failed");
			show_error(str);
		} else
			update_menu();
		in_error = 0;
	} else {
		if (syserr != NULL)
			fprintf(stderr, "%s: %s\n", str, syserr);
		else
			fprintf(stderr, "%s\n", str);
		sleep(2);
	}
}

/*
 * Copy a string.
 */
char *
str_copy(str)
	register char *str;
{
	register char *out;
	register size_t len;

	if (str == NULL)
		return NULL;
	len = strlen(str) + 1;
	out = get_mem(len);
	memcpy(out, str, len);
	return out;
}

/*
 * Process signals.
 */
static RETSIGTYPE
got_sig(n)
	int n;
{
	if (n == SIGINT) {
		if (def_flags & FL_IGNBRK)
			return;
		bail(YTE_SUCCESS);
	}
	bail(YTE_SIGNAL);
}

/* MAIN  */
int
main(argc, argv)
	int argc;
	char **argv;
{
	int sflg = 0, yflg = 0, iflg = 0, vflg = 0, qflg = 0, eflg = 0;
	char *prog;

#if SIZEOF_U_INT != 4
	/* check for a 64-bit mis-compile */
	fprintf(stderr, "The definition for ylong in header.h is wrong;\n\
please change it to an unsigned 32-bit type that works on your computer,\n\
then type 'make clean' and 'make'.\n");
	exit(YTE_INIT);
#endif

	/* search for options */

	prog = *argv;
	argv++, argc--;
	while (argc > 0 && **argv == '-') {
		if (strcmp(*argv, "-Y") == 0) {
			yflg++;
			argv++, argc--;
		} else if (strcmp(*argv, "-E") == 0) {
			eflg++;
			argv++, argc--;
		} else if (strcmp(*argv, "-i") == 0) {
			iflg++;
			argv++, argc--;
		} else if (strcmp(*argv, "-v") == 0) {
			vflg++;
			argv++, argc--;
		} else if (strcmp(*argv, "-h") == 0) {
			argv++;
			vhost = *argv++;
			argc -= 2;
		} else if (strcmp(*argv, "-s") == 0) {
			sflg++;	/* immediately start a shell */
			argv++, argc--;
		} else if (strcmp(*argv, "-q") == 0) {
			qflg++;
			argv++, argc--;
		} else
			argc = 0;	/* force a Usage error */
	}

	if (vflg) {
		/* print version and exit */
		fprintf(stderr, "YTalk %s\n", PACKAGE_VERSION);
		exit(YTE_SUCCESS);
	}
	/* check for users */

	if (argc <= 0) {
		fprintf(stderr, "Usage:    %s [options] user[@host][#tty]...\n", prog);
		fprintf(stderr, "\
Options:     -i             --    no auto-invite port\n\
             -Y             --    require caps on all y/n answers\n\
             -E             --    require <esc> before y/n answers\n\
             -s             --    start a shell\n\
             -q             --    prompt before quitting\n\
             -v             --    print program version\n\
             -h host_or_ip  --    select interface or virtual host\n");
		exit(YTE_INIT);
	}

	/* set up signals */
	signal(SIGINT, got_sig);
	signal(SIGHUP, got_sig);
	signal(SIGQUIT, got_sig);
	signal(SIGABRT, got_sig);
	signal(SIGPIPE, SIG_IGN);

	/* save the uid for later use */
	myuid = getuid();

	/* set default options */
	def_flags = FL_PROMPTRING | FL_RING | FL_BEEP | FL_SCROLL;

	/* go for it! */
	errno = 0;
	init_fd();
	read_ytalkrc();
	init_user(vhost);
	if (yflg)
		def_flags |= FL_CAPS;
	if (eflg)
		def_flags |= FL_ESC_YN;
	if (iflg)
		def_flags |= FL_NOAUTO;
	if (qflg)
		def_flags |= FL_PROMPTQUIT;

	init_term();
	init_socket();
	for (; argc > 0; argc--, argv++)
		invite(*argv, 1);
	if (sflg)
		execute(NULL);
	else
		msg_term(me, "Waiting for connection...");
	main_loop();
	bail(YTE_SUCCESS_PROMPT);

	return 0;
}
