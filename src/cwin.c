/*
 * src/cwin.c
 * curses interface
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
#include "mem.h"

#ifdef HAVE_NCURSES_H
# include <ncurses.h>
#else
# include <curses.h>
#endif

/* Some systems, notably Solaris, don't have sys/signal.h */
#include <signal.h>

#include "cwin.h"

typedef struct _ywin {
	struct _ywin *next;	/* next ywin in linked list */
	yuser *user;		/* user pointer */
	WINDOW *win;		/* window pointer */
	int height, width;	/* height and width in characters */
	int row, col;		/* row and column position on screen */
	char *title;		/* window title string */
} ywin;

static ywin *head;		/* head of linked list */

char tempOrderUser[V3_NAMELEN+1];		/* stack memory for holding usernames from the forceorder option */

/*
 * Take input from the user.
 */
static void
curses_input(fd)
	int fd;
{
	register int rc;
	static ychar buf[MAXBUF];

	if ((rc = read(fd, buf, MAXBUF)) <= 0) {
		if (rc == 0)
			bail(YTE_SUCCESS);
		bail(YTE_ERROR);
	}
	my_input(me, buf, rc);
}

static ywin *
new_ywin(user, title)
	yuser *user;
	char *title;
{
	register ywin *out;
	register int len;

	len = strlen(title);
	out = (ywin *) get_mem(sizeof(ywin) + len + 1);
	memset(out, 0, sizeof(ywin));
	out->user = user;
	out->title = ((char *) out) + sizeof(ywin);
	strcpy(out->title, title);
	return out;
}

static void
make_win(w, height, width, row, col)
	ywin *w;
	int height, width, row, col;
{
	if ((w->win = newwin(height, width, row, col)) == NULL) {
		w = (ywin *) (me->term);
		if (w->win != NULL)
			show_error("make_win: newwin() failed");
		bail(YTE_ERROR);
	}
	w->height = height;
	w->width = width;
	w->row = row;
	w->col = col;
	scrollok(w->win, FALSE);
	wmove(w->win, 0, 0);
}

static void
draw_title(w)
	ywin *w;
{
	register int pad, x;
	register char *t;

	if ((int) strlen(w->title) > w->width) {
		for (x = 0; x < w->width; x++)
			addch('-');
		return;
	}
	pad = (w->width - strlen(w->title)) / 2;
	move(w->row - 1, w->col);
	x = 0;
	for (; x < pad - 2; x++)
		addch('-');
	if (pad >= 2) {
		addch('=');
		addch(' ');
		x += 2;
	}
	for (t = w->title; *t && x < w->width; x++, t++)
		addch(*t);
	if (pad >= 2) {
		addch(' ');
		addch('=');
		x += 2;
	}
	for (; x < w->width; x++)
		addch('-');
}

/*
 * Return number of lines per window, given "wins" windows.
 */
static int
win_size(wins)
	int wins;
{
	if (wins == 0)
		return 0;
	return (LINES - 1) / wins;
}

/*
 * Break down and redraw all user windows.
 */
static void
curses_redraw()
{
	register ywin *w;
	register int row, wins, wsize;
	int curwin;

	/* kill old windows */

	wins = 0;
	for (w = head; w; w = w->next) {
		if (w->win) {
			delwin(w->win);
			w->win = NULL;
		}
		wins++;
	}
	if ((wsize = win_size(wins)) < 3) {
		end_term();
		errno = 0;
		show_error("curses_redraw: window size too small");
		bail(YTE_ERROR);
	}

	/* make new windows */

	clear();
	refresh();
	row = 0;
	curwin = 0;
	for (w = head; w; w = w->next) {
		//  2008-04-08 Andypro: Improved algorithm for divvying up the terminal
		if(curwin < wins - (LINES % wins)) { //do not allocate an extra line
		   make_win(w, (LINES / wins)-1, COLS, row+1, 0);
		   resize_win(w->user, (LINES / wins)-1, COLS);
		   row += (LINES / wins);
		}
		else { //give this user one of the leftover lines in LINES%wins
		   make_win(w, (LINES / wins), COLS, row+1, 0);
		   resize_win(w->user, (LINES / wins), COLS);
		   row += (LINES / wins)+1;
		}

		draw_title(w);
		curwin++;
		//row += wsize;
		refresh();
		wrefresh(w->win);
	}
}

/*
 * Start curses and set all options.
 */
static void
curses_start()
{
	char *term;
	if (initscr() == NULL) {
		term = getenv("TERM");
		fprintf(stderr, "Error opening terminal: %s.\n", (term ? term : "(null)"));
		bail(YTE_INIT);
	}
	noraw();
	crmode();
	noecho();
	clear();
}

/*
 * Restart curses... window size has changed.
 */
static RETSIGTYPE
curses_restart()
{
	register ywin *w;

	/* kill old windows */

	for (w = head; w; w = w->next)
		if (w->win) {
			delwin(w->win);
			w->win = NULL;
		}

	/* restart curses */

	endwin();
	curses_start();

	/* fix for ncurses from Max <Marc.Espie@liafa.jussieu.fr> */

	refresh();
	curses_redraw();

#ifdef SIGWINCH
	/* some systems require we do this again */
	signal(SIGWINCH, (void (*)())curses_restart);
#endif
}

/* ---- global functions ---- */

void
init_curses()
{
	curses_start();
	refresh();
	head = NULL;
	add_fd(0, curses_input);/* set up user's input function */

#ifdef SIGWINCH
	/* set up SIGWINCH signal handler */
	signal(SIGWINCH, (void (*)())curses_restart);
#endif
}

void
end_curses()
{
	move(LINES - 1, 0);
	clrtoeol();
	refresh();
	endwin();
}

//  Returns a null-terminated string reference to the user at index "index"
//  in the forceorder list option if present.  Otherwise returns an empty string
void forceduser_atindex(int index) {
	//forceorder[MAXOPT]			//  The pipe-separated user list
	//tempOrderUser[V3_NAMELEN+1]	//  the stack mem to return our result

	int i, pipecount, copycount, listlength;

	pipecount = 0;
	copycount = 0;
	listlength = (int)strlen(forceorder);

	for(i=0; (i < listlength) && (copycount <= V3_NAMELEN); ++i) {
		if((pipecount == index) && (forceorder[i] != '|')) {
			tempOrderUser[copycount] = forceorder[i];
			copycount++;
		}

		if(forceorder[i] == '|')
			pipecount++;
	}

	tempOrderUser[copycount] = '\0';
}

/*
 * Open a new window.
 */
int
open_curses(user, title)
	yuser *user;
	char *title;
{
	register ywin *w;
	ywin* temp;
	register int wins;
	char* incoming_username;

	/*
	 * count the open windows.  We want to ensure at least three lines
	 * per window.
	 */
	wins = 0;
	for (w = head; w; w = w->next)
		wins++;
	if (win_size(wins + 1) < 3)
		return -1;

	/* add the new user */  //  ap: This is where the user gets added to the window list!!!!
	incoming_username = user->user_name;

	//  Added by ap: If we have a forced order set, then enforce it here
	//  as we're adding this new user to make sure we always conform to our user order.
	if(def_flags & FL_FORCEORDER) {
		if (head == NULL) {
			w = head = new_ywin(user, title);
		}
		else { //if there is me and another dude, start adding in reverse order for testing
			w = head->next;
			head->next = new_ywin(user, title);
			head->next->next = w;
			w = head->next;  //  this needs to be set for the term assignment below!

			//  Pseudo code for force order processing
			//
			//  1.  Identify incoming user_name's spot in the forceorder list
			int nameindex = 0;
			forceduser_atindex(nameindex);

			while(((int)strlen(tempOrderUser) > 0) && (strcmp(tempOrderUser, user->user_name) != 0)) {
				nameindex++;
				forceduser_atindex(nameindex);
			}

			if((int)strlen(tempOrderUser) <= 0) { //incoming user not found in the order list; default snacktalk behavior.
				for (w = head; w; w = w->next) {
					if (w->next == NULL) {
						w->next = new_ywin(user, title);
						w = w->next;
						break;
					}
				}
			}
			else {
				//  2.  For each person present, make sure
				//		said person is before us in the forceorder list (< nameindex)
				//			if not, insert us here and break.
				int thisguyindex = 0;

				for (w = head; w; w = w->next) {
					if(thisguyindex < nameindex) {
						//  Identify this user's spot in the forceorder list
						forceduser_atindex(thisguyindex);

						while(((int)strlen(tempOrderUser) > 0) && (strcmp(tempOrderUser, w->user->user_name) != 0) && (thisguyindex <= nameindex)) {
							thisguyindex++;
							forceduser_atindex(thisguyindex);
						}
					}

					if(nameindex <= thisguyindex) { //the incoming user belongs before this guy!  Put him in.
						temp = w;
						w = new_ywin(user, title);
						w->next = temp;
						break;
					}

					if(w->next == NULL) {
						w->next = new_ywin(user, title);
						w = w->next;
						break;
					}
				}
			}
		}
	}
	else { //default snacktalk behavior -> user goes to end of list (bottom of screen)
		if (head == NULL) {
			w = head = new_ywin(user, title);
		}
		else {
			for (w = head; w; w = w->next) {
				if (w->next == NULL) {
					w->next = new_ywin(user, title);
					w = w->next;
					break;
				}
			}
		}
	}

	user->term = w;  //  assign the terminal pointer from the newly-created window
	curses_redraw();

	return 0;
}

/*
 * Close a window.
 */
void
close_curses(user)
	yuser *user;
{
	register ywin *w, *p;

	/* zap the old user */

	w = (ywin *) (user->term);
	if (w == head)
		head = w->next;
	else {
		for (p = head; p; p = p->next)
			if (w == p->next) {
				p->next = w->next;
				break;
			}
		if (p == NULL) {
			show_error("close_curses: user not found");
			return;
		}
	}
	delwin(w->win);
	free_mem(w);
	w = NULL;
	user->term = NULL;
	curses_redraw();
}

void
addch_curses(user, c)
	yuser *user;
	register ychar c;
{
	register ywin *w;
	register int x, y;

	w = (ywin *) (user->term);
	getyx(w->win, y, x);
	waddch(w->win, c);
	if (x >= COLS - 1)
		wmove(w->win, y, x);
}

void
move_curses(user, y, x)
	yuser *user;
	register int y, x;
{
	register ywin *w;

	w = (ywin *) (user->term);
	wmove(w->win, y, x);
}

void
clreol_curses(user)
	register yuser *user;
{
	register ywin *w;

	w = (ywin *) (user->term);
	wclrtoeol(w->win);
}

void
clreos_curses(user)
	register yuser *user;
{
	register ywin *w;

	w = (ywin *) (user->term);
	wclrtobot(w->win);
}

void
scroll_curses(user)
	register yuser *user;
{
	register ywin *w;

	/*
	 * Curses uses busted scrolling.  In order to call scroll()
	 * effectively, scrollok() must be TRUE.  However, if scrollok() is
	 * TRUE, then placing a character in the lower right corner will
	 * cause an auto-scroll.  *sigh*
	 */
	w = (ywin *) (user->term);
	scrollok(w->win, TRUE);
	scroll(w->win);
	scrollok(w->win, FALSE);

	/*
	 * Some curses won't leave the cursor in the same place, and some
	 * curses programs won't erase the bottom line properly.
	 */
	wmove(w->win, user->t_rows - 1, 0);
	wclrtoeol(w->win);
	wmove(w->win, user->y, user->x);
}

void
keypad_curses(bf)
	bool bf;
{
#ifdef HAVE_KEYPAD
	keypad(((ywin *) (me->term))->win, bf);
#endif
}

void
flush_curses(user)
	register yuser *user;
{
	register ywin *w;

	w = (ywin *) (user->term);
	wrefresh(w->win);
}

/*
 * Clear and redisplay.
 */
void
redisplay_curses()
{
	register ywin *w;

	clear();
	refresh();
	for (w = head; w; w = w->next) {
		redraw_term(w->user, 0);
		draw_title(w);
		refresh();
		wrefresh(w->win);
	}
}

/*
 * Set raw mode.
 */
void
set_raw_curses()
{
	raw();
}

/*
 * Set cooked mode.
 */
void
set_cooked_curses()
{
	noraw();
	crmode();
	noecho();
}
