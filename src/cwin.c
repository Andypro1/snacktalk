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
	struct _ywin *next;		/* next ywin in linked list */
	yuser *user;			/* user pointer */
	WINDOW *win;			/* window pointer */
	int height, width;		/* height and width in characters */
	int row, col;			/* row and column position on screen */
	int fgcolor, bgcolor;	//  Current color draw attributes
	char *title;			/* window title string */
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
	w->fgcolor = -1;  //  curses defaults
	w->bgcolor = -1;

	//  Set new window curses options
	scrollok(w->win, FALSE);
	idlok(w->win, TRUE);  //  Added by ap: let's allow curses to use the line editing commands

	wmove(w->win, 0, 0);  //  position cursor
}

//  Updated to use the curses line-drawing constants instead of hyphens
//  for terminals that support it
static void
draw_title(w)
	ywin *w;
{
	register int pad, x;
	register char *t;

	if ((int) strlen(w->title) > w->width) {
		for (x = 0; x < w->width; x++)
			addch(ACS_HLINE);
		return;
	}
	pad = (w->width - strlen(w->title)) / 2;
	move(w->row - 1, w->col);
	x = 0;
	for (; x < pad - 2; x++)
		addch(ACS_HLINE);
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
		addch(ACS_HLINE);
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

	if(def_flags & FL_COLOR) {
		start_color();  //  Added by ap - need to start here if we want to have a chance of showing colors
		use_default_colors();  //  avoids curses color silliness

		//  Set curses color pairs up and map them to
		//  the xterm Character Attributes (SGR) codes received by the shell
		init_pair(30, COLOR_BLACK, -1);
		init_pair(31, COLOR_RED, -1);
		init_pair(32, COLOR_GREEN, -1);
		init_pair(33, COLOR_YELLOW, -1);
		init_pair(34, COLOR_BLUE, -1);
		init_pair(35, COLOR_MAGENTA, -1);
		init_pair(36, COLOR_CYAN, -1);
		init_pair(37, COLOR_WHITE, -1);
		init_pair(39, -1, -1);
		init_pair(40, -1, COLOR_BLACK);
		init_pair(41, -1, COLOR_RED);
		init_pair(42, -1, COLOR_GREEN);
		init_pair(43, -1, COLOR_YELLOW);
		init_pair(44, -1, COLOR_BLUE);
		init_pair(45, -1, COLOR_MAGENTA);
		init_pair(46, -1, COLOR_CYAN);
		init_pair(47, -1, COLOR_WHITE);
		init_pair(49, -1, -1);
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

//  Returns the 0-based index in the forcedorder snacktalk option of the passed-in username
int get_forceduser_index(char* name) {
	int nameindex = 0;
	forceduser_atindex(nameindex);

	while(((int)strlen(tempOrderUser) > 0) && (strcmp(tempOrderUser, name) != 0)) {
		nameindex++;
		forceduser_atindex(nameindex);
	}

	return nameindex;
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
		if (head == NULL) { //only one place the new user can go
			w = head = new_ywin(user, title);
		}
		else { //check the order and place incoming user accordingly
			//  1.  Identify incoming user_name's spot in the forceorder list
			int incominguser_nameindex = get_forceduser_index(user->user_name);

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
				//  2.  For each person present, make sure said person is before us in the
				//  forceorder list (< incominguser_nameindex).  If not, insert us here and break.
				for (w = head; w; w = w->next) {
					if(w == head) {  //  we need to test against this first user the first time through.  All subsequent tests will be against the w->next user.
						//  Identify this user's spot in the forceorder list
						int thisguyindex = get_forceduser_index(w->user->user_name);

						if(incominguser_nameindex <= thisguyindex) { //the incoming user belongs before this guy!  Put him in
							temp = w;
							w = new_ywin(user, title);
							w->next = temp;  //  w needs to be set for the term assignment below!
							head = w;  //  The incoming user is now the leader

							break;
						}
					}

					if(w->next == NULL) { //We're at end-of-list.  No where else to go but at the end
						w->next = new_ywin(user, title);
						w = w->next;
						break;
					}

					//  Identify the next user's spot in the forceorder list
					int nextguyindex = get_forceduser_index(w->next->user->user_name);

					if(incominguser_nameindex <= nextguyindex) { //the incoming user belongs before the next guy!  Put him in
						temp = w->next;  //  Save rest of chain
						w->next = new_ywin(user, title);  //  Next user is incoming user
						w->next->next = temp;  //  Reattach chain after incoming user
						w = w->next;  //  w needs to be set for the term assignment below!

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
insert_line_curses(user, step)
	register yuser *user;
	int step;
{
	register ywin *w;

	w = (ywin *)(user->term);
	winsdelln(w->win, step);  //  Inserts lines if step is positive and deletes lines if step is negative
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
erase_curses(user)
	register yuser *user;
{
	register ywin *w;

	w = (ywin *) (user->term);
	werase(w->win);
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

void
color_curses(user, colorID, isBg)
	register yuser *user;
	register int colorID;
	register int isBg;
{
	register ywin *w;

	w = (ywin *) (user->term);

	if(isBg)
		w->bgcolor = colorID;
	else
		w->fgcolor = colorID;

	if(w->fgcolor == -1 || w->bgcolor == -1) { //use a color pair preset defined on init
		if(isBg)
			wattron(w->win, COLOR_PAIR(colorID+40));
		else
			wattron(w->win, COLOR_PAIR(colorID+30));
	}
	else { //init a custom color pair with the window's current fg and bg colors
		init_pair(29, w->fgcolor, w->bgcolor);
		wattron(w->win, COLOR_PAIR(29));
	}
}

void
format_curses(user, sgrID)
	register yuser *user;
	register int sgrID;
{
	register ywin *w;

	w = (ywin *) (user->term);

	switch(sgrID) {
	case 0: //Normal
		w->fgcolor = -1;
		w->bgcolor = -1;
		wattrset(w->win, A_NORMAL);
		break;
	case 1: //Bold
		wattron(w->win, A_BOLD);
		break;
	case 4: //Underlined
		wattron(w->win, A_UNDERLINE);
		break;
	case 5: //Blink
		wattron(w->win, A_BLINK);
		break;
	case 7: //Inverse
		wattron(w->win, A_REVERSE);
		break;
	case 8: //Invisible (VT300)
		wattron(w->win, A_INVIS);
		break;
	case 22: //Normal (neither bold nor faint)
		wattroff(w->win, A_BOLD);
		break;
	case 24: //Not underlined
		wattroff(w->win, A_UNDERLINE);
		break;
	case 25: //Steady (not blinking)
		wattroff(w->win, A_BLINK);
		break;
	case 27: //Positive (not inverse)
		wattroff(w->win, A_REVERSE);
		break;
	case 28: //Visible (not hidden) (VT300)
		wattroff(w->win, A_INVIS);
		break;
	default:
		break;
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
