/*
 * src/exec.c
 * run a command in a window
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
#include <pwd.h>

#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#include <signal.h>
#include <sys/wait.h>

#ifdef HAVE_STROPTS_H
# include <stropts.h>
#endif

#ifdef HAVE_SYS_CONF_H
# include <sys/conf.h>
#endif

#ifdef HAVE_TERMIOS_H
# include <termios.h>
#else
# ifdef HAVE_SGTTY_H
#  include <sgtty.h>
#  ifdef hpux
#   include <sys/bsdtty.h>
#  endif
# endif
#endif

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#ifdef HAVE_OPENPTY
# ifdef HAVE_UTIL_H
#  include <util.h>
# else
int openpty(int *, int *, char *, struct termios *, struct winsize *);
# endif
#endif

#if defined(HAVE_PTSNAME) && defined(HAVE_GRANTPT) && defined(HAVE_UNLOCKPT)
# define USE_DEV_PTMX
#endif

int running_process = 0;	/* flag: is process running? */
static int pid;			/* currently executing process id */
static int pfd;			/* currently executing process fd */
static int prows, pcols;	/* saved rows, cols */

/* ---- local functions ---- */

#ifndef HAVE_SETSID
static int
setsid()
{
#ifdef TIOCNOTTY
	register int fd;

	if ((fd = open("/dev/tty", O_RDWR)) >= 0) {
		ioctl(fd, TIOCNOTTY);
		close(fd);
	}
	return fd;
#endif
}
#endif

#if defined(USE_DEV_PTMX) && defined(YTALK_SUNOS)
int needtopush = 0;
#endif

#ifndef SIGCHLD
# define SIGCLD SIGCHLD
#endif

static void
exec_input(fd)
	int fd;
{
	register int rc;
	static ychar buf[MAXBUF];

	if ((rc = read(fd, buf, MAXBUF)) <= 0) {
		kill_exec();
		errno = 0;
		show_error("command shell terminated");
		return;
	}
	show_input(me, buf, rc);
	send_users(me, buf, rc, buf, rc);
}

static void
calculate_size(rows, cols)
	int *rows, *cols;
{
	register yuser *u;

	*rows = me->t_rows;
	*cols = me->t_cols;

	for (u = connect_list; u; u = u->next)
		if (u->remote.vmajor > 2) {
			if (u->remote.my_rows > 1 && u->remote.my_rows < *rows)
				*rows = u->remote.my_rows;
			if (u->remote.my_cols > 1 && u->remote.my_cols < *cols)
				*cols = u->remote.my_cols;
		}
}

/* ---- global functions ---- */

/*
 * Execute a command inside my window.  If command is NULL, then execute a
 * shell.
 */
void
execute(command)
	char *command;
{
	int fd;
#ifdef HAVE_PUTENV
	char yvenv[25];
#endif
	char name[20], *shell;
	struct stat sbuf;
	struct passwd *pw = NULL;
#ifdef HAVE_OPENPTY
	int fds;
#endif

	if (me->flags & FL_LOCKED) {
		errno = 0;
		show_error("alternate mode already running");
		return;
	}
#ifdef HAVE_OPENPTY
	if (openpty(&fd, &fds, name, NULL, NULL) < 0) {
		msg_term(me, "cannot get pseudo terminal");
		return;
	}
	close(fds);
#else
	if ((fd = getpty(name)) < 0) {
		msg_term(me, "cannot get pseudo terminal");
		return;
	}
#endif				/* HAVE_OPENPTY */

	/* init the pty a bit (inspired from the screen(1) sources, pty.c) */

#if defined(HAVE_TCFLUSH) && defined(TCIOFLUSH)
	tcflush(fd, TCIOFLUSH);
#else
#ifdef TIOCFLUSH
	ioctl(fd, TIOCFLUSH, NULL);
#else
#ifdef TIOCEXCL
	ioctl(fd, TIOCEXCL, NULL);
#endif
#endif
#endif

   pw = getpwuid(myuid);
   shell=getenv("SHELL");
   if(shell==NULL){
      if (pw != NULL) {
         shell = pw->pw_shell;
      } else {
         shell = "/bin/sh";
      }
   }

	calculate_size(&prows, &pcols);

#ifdef SIGCHLD
	/*
	 * Modified by P. Maragakis (Maragakis@mpq.mpg.de) Aug 10, 1999,
	 * following hints by Jason Gunthorpe. This closes two Debian bugs
	 * (#42625, #2196).
	 */
	signal(SIGCHLD, SIG_DFL);
#else
# ifdef SIGCLD
	signal(SIGCLD, SIG_DFL);
# endif
#endif

	if ((pid = fork()) == 0) {
		close(fd);
		close_all();
		if (setsid() < 0)
			exit(-1);
		if ((fd = open(name, O_RDWR)) < 0)
			exit(-1);

#if defined(USE_DEV_PTMX) && defined(YTALK_SUNOS)
# if defined(HAVE_STROPTS_H) && defined(I_PUSH)
		/*
		 * This will really mess up the shell on OSF1/Tru64 UNIX,
		 * so we only do it on SunOS/Solaris
		 */
		if (needtopush) {
			ioctl(fd, I_PUSH, "ptem");
			ioctl(fd, I_PUSH, "ldterm");
		}
# endif
#endif

		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);

#ifdef SIGWINCH
		/* tricky bit -- ignore WINCH */
		signal(SIGWINCH, SIG_IGN);
#endif

		/* set terminal characteristics */

		set_terminal_flags(fd);
		set_terminal_size(fd, prows, pcols);

#ifdef HAVE_PUTENV
# ifndef NeXT
		putenv("TERM=vt100");
# endif
# ifdef HAVE_SNPRINTF
		snprintf(yvenv, sizeof(yvenv), "YTALK_VERSION=%s", PACKAGE_VERSION);
# else
		sprintf(yvenv, "YTALK_VERSION=%s", PACKAGE_VERSION);
# endif
		putenv(yvenv);
#endif

#ifdef TIOCSCTTY
		/*
		 * Mark the new pty as a controlling terminal to enable
		 * BSD-style job control.
		 */
		ioctl(fd, TIOCSCTTY);
#endif

		/* Check pty permissions and warn about potential risks. */
		if (stat(name, &sbuf) == 0) {
			if (sbuf.st_mode & 0004) {
				write(1, "Warning: This pseudo-terminal is world-readable.\n", 49);
			}
		}
		/* execute the command */

		if (command)
			execl(shell, shell, "-c", command, (char *) NULL);
		else
			execl(shell, shell, (char *) NULL);
		perror("execl");
		exit(-1);
	}
	/* Modified by P. Maragakis (Maragakis@mpq.mpg.de) Aug 10, 1999. */
#ifdef SIGCHLD
	signal(SIGCHLD, SIG_IGN);
#else
# ifdef SIGCLD
	signal(SIGCLD, SIG_IGN);
# endif
#endif

	if (pid < 0) {
		show_error("fork() failed");
		return;
	}
	set_win_region(me, prows, pcols);
	sleep(1);
	pfd = fd;
	running_process = 1;
	lock_flags((ylong) (FL_RAW | FL_SCROLL));
	set_raw_term();
	add_fd(fd, exec_input);
}

/*
 * Send input to the command shell.
 */
void
update_exec()
{
	write(pfd, io_ptr, (size_t) io_len);
	io_len = 0;
}

/*
 * Kill the command shell.
 */
void
kill_exec()
{
	if (!running_process)
		return;
	remove_fd(pfd);
	close(pfd);
	running_process = 0;
	unlock_flags();
	set_cooked_term();
	end_win_region(me);
}

/*
 * Send a SIGWINCH to the process.
 */
void
winch_exec()
{
	int rows, cols;

	if (!running_process)
		return;

	/* if the winch has no effect, return now */

	calculate_size(&rows, &cols);
	if (rows == prows && cols == pcols) {
		if (prows != me->rows || pcols != me->cols)
			set_win_region(me, prows, pcols);
		return;
	}
	/* oh well -- redo everything */

	prows = rows;
	pcols = cols;
	set_terminal_size(pfd, prows, pcols);
	set_win_region(me, prows, pcols);
#ifdef SIGWINCH
	kill(pid, SIGWINCH);
#endif
}
