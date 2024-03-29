/*
 * src/term.c
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

#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#ifdef HAVE_TERMIOS_H
# include <termios.h>
#else
# ifdef HAVE_SGTTY
#  include <sgtty.h>
#  ifdef hpux
#   include <sys/bsdtty.h>
#  endif
#  define USE_SGTTY
# endif
#endif

#include "cwin.h"
#include "menu.h"

static int term_type = 0;

#ifdef USE_SGTTY
static int line_discipline;
static int local_mode;
static struct sgttyb sgttyb;
static struct tchars tchars;
static struct ltchars ltchars;
#else
static struct termios tio;
#endif

#ifdef USE_SGTTY
static void
init_sgtty()
{
	if (ioctl(0, TIOCGETD, &line_discipline) < 0) {
		show_error("TIOCGETD");
		bail(YTE_INIT);
	}
	if (ioctl(0, TIOCLGET, &local_mode) < 0) {
		show_error("TIOCGETP");
		bail(YTE_INIT);
	}
	if (ioctl(0, TIOCGETP, &sgttyb) < 0) {
		show_error("TIOCGETP");
		bail(YTE_INIT);
	}
	if (ioctl(0, TIOCGETC, &tchars) < 0) {
		show_error("TIOCGETC");
		bail(YTE_INIT);
	}
	if (ioctl(0, TIOCGLTC, &ltchars) < 0) {
		show_error("TIOCGLTC");
		bail(YTE_INIT);
	}
	me->old_rub = sgttyb.sg_erase;
	me->RUB = RUBDEF;
	me->KILL = sgttyb.sg_kill;
	me->WORD = ltchars.t_werasc;
	me->CLR = '\024';	/* ^T */
}
#else
static void
init_termios()
{
	/* get edit chars */

	if (tcgetattr(0, &tio) < 0) {
		show_error("tcgetattr failed");
		bail(YTE_INIT);
	}
	me->old_rub = tio.c_cc[VERASE];
	me->RUB = RUBDEF;
#ifdef VKILL
	me->KILL = tio.c_cc[VKILL];
#else
	me->KILL = '\025';	/* ^U */
#endif
#ifdef VWERASE
	me->WORD = tio.c_cc[VWERASE];
	if (me->WORD == 0xff)
		me->WORD = '\027';	/* ^W */
#else
	me->WORD = '\027';	/* ^W */
#endif
	me->CLR = '\024';	/* ^T */
}
#endif

/*
 * Initialize terminal and input characteristics.
 */
void
init_term()
{
	char tmpstr[64];

#ifdef USE_SGTTY
	init_sgtty();
#else
	init_termios();
#endif

	init_curses();
	term_type = 1;		/* using curses */

	/* set me up a terminal */

#ifdef HAVE_SNPRINTF
	snprintf(tmpstr, sizeof(tmpstr), "Snacktalk version %s", PACKAGE_VERSION);
#else
	sprintf(tmpstr, "Snacktalk version %s", PACKAGE_VERSION);
#endif
	if (open_term(me, tmpstr) < 0) {
		end_term();
		show_error("init_term: open_term() failed");
		bail(YTE_SUCCESS);
	}
}

/*
 * Set terminal size.
 */
void
set_terminal_size(fd, rows, cols)
	int fd, rows, cols;
{
#ifdef TIOCSWINSZ
	struct winsize winsize;

	winsize.ws_row = rows;
	winsize.ws_col = cols;
	ioctl(fd, TIOCSWINSZ, &winsize);
#endif
}

/*
 * Set terminal and input characteristics for slave terminals.
 */
void
set_terminal_flags(fd)
	int fd;
{
#ifdef USE_SGTTY
	ioctl(fd, TIOCSETD, &line_discipline);
	ioctl(fd, TIOCLSET, &local_mode);
	ioctl(fd, TIOCSETP, &sgttyb);
	ioctl(fd, TIOCSETC, &tchars);
	ioctl(fd, TIOCSLTC, &ltchars);
#else
	if (tcsetattr(fd, TCSANOW, &tio) < 0)
		show_error("tcsetattr failed");
#endif
}

int
what_term()
{
	return term_type;
}

/*
 * Change terminal keypad mode (only for me, only with curses)
 */
void
keypad_term(user, bf)
	yuser *user;
	int bf;
{
	if (user != me)
		return;
	if (term_type == 1)
		keypad_curses(bf);
}

/*
 * Abort all terminal processing.
 */
void
end_term()
{
	if (term_type == 1)
		end_curses();
	term_type = 0;
}

/*
 * Open a new user window.
 */
int
open_term(user, title)
	register yuser *user;
	register char *title;
{
	if (open_curses(user, title) != 0)
		return -1;
	user->x = user->y = 0;
	if (user->scr == NULL)
		resize_win(user, 24, 80);
	return 0;
}

/*
 * Close a user window.
 */
void
close_term(user)
	register yuser *user;
{
	register int i;

	if (user->scr) {
		close_curses(user);
		for (i = 0; i < user->t_rows; i++)
			free_mem(user->scr[i]);
		free_mem(user->scr);
		user->scr = NULL;
		free_mem(user->scr_tabs);
		user->scr_tabs = NULL;
		user->t_rows = user->rows = 0;
		user->t_cols = user->cols = 0;
	}
}

/*
 * Place a character.
 */
void
addch_term(user, c)
	register yuser *user;
	register ychar c;
{
	if (is_printable(c)) {
		addch_curses(user, c);
		user->scr[user->y][user->x] = c;
		if (++(user->x) >= user->cols) {
			user->bump = 1;
			user->x = user->cols - 1;
			if (user->cols < user->t_cols)
				move_curses(user, user->y, user->x);
		}
	}
}

/*
 * Move the cursor.
 */
void
move_term(user, y, x)
	register yuser *user;
	register int y, x;
{
	if (y < 0 || y > user->sc_bot)
		y = user->sc_bot;
	if (x < 0 || x >= user->cols) {
		user->bump = 1;
		x = user->cols - 1;
	} else {
		user->bump = 0;
		user->onend = 0;
	}
	move_curses(user, y, x);
	user->y = y;
	user->x = x;
}

/*
 * Fill terminal region with char
 */
void
fill_term(user, y1, x1, y2, x2, c)
	yuser *user;
	int y1, x1, y2, x2;
	ychar c;
{
	int y, x;
	for (y = y1; y <= y2; y++)
		for (x = x1; x <= x2; x++)
			user->scr[y][x] = c;
	return;
}

/*
 * Clear to EOL.
 */
void
clreol_term(user)
	register yuser *user;
{
	register int j;
	register ychar *c;

	if (user->cols < user->t_cols) {
		c = user->scr[user->y] + user->x;
		for (j = user->x; j < user->cols; j++) {
			*(c++) = ' ';
			addch_curses(user, ' ');
		}
		move_term(user, user->y, user->x);
	} else {
		clreol_curses(user);
		c = user->scr[user->y] + user->x;
		for (j = user->x; j < user->cols; j++)
			*(c++) = ' ';
	}
}

/*
 * Clear to EOS.
 */
void
clreos_term(user)
	register yuser *user;
{
	register int j, i;
	register ychar *c;
	int x, y;

	if (user->cols < user->t_cols || user->rows < user->t_rows) {
		x = user->x;
		y = user->y;
		clreol_term(user);
		for (i = user->y + 1; i < user->rows; i++) {
			move_term(user, i, 0);
			clreol_term(user);
		}
		move_term(user, y, x);
	} else {
		clreos_curses(user);
		j = user->x;
		for (i = user->y; i < user->rows; i++) {
			c = user->scr[i] + j;
			for (; j < user->cols; j++)
				*(c++) = ' ';
			j = 0;
		}
	}
}

/*
 * Scroll window.
 */
void
scroll_term(user)
	register yuser *user;
{
	register int i;
	register ychar *c;
	int sy, sx;

	if (user->sc_bot > user->sc_top) { //scroll region sanity check
		c = user->scr[user->sc_top]; // points to top scrolling row of our screen buffer

		for(i = user->sc_top; i < user->sc_bot; i++) //each scrolling row, top to bottom-1
			user->scr[i] = user->scr[i + 1];  //  Move line pointer to the line below (effectively copy up)

		user->scr[user->sc_bot] = c;  //  Bottom row points to previous top line contents

		for(i = 0; i < user->cols; i++)
			*(c++) = ' ';  //  Blank out new bottom line

		//scroll_curses(user);
		scroll_curses_optimized(user);
	} else { //if scroll region is a single line or improperly set, just blank out the top scrolling line
		sy = user->y;  //  save cursor coordinates
		sx = user->x;
		move_term(user, user->sc_top, 0);
		clreol_term(user);
		move_term(user, sy, sx);  //  restore cursor
	}
}

/*
 * Reverse-scroll window.
 */
void
rev_scroll_term(user)
	register yuser *user;
{
	register int i;
	register ychar *c;
	int sy, sx;

	if(user->sc_bot > user->sc_top) { //scroll region sanity check
		c = user->scr[user->sc_bot]; // points to bottom scrolling row of our screen buffer

		for(i = user->sc_bot; i > user->sc_top; i--) //each scrolling row, bottom to top+1
			user->scr[i] = user->scr[i - 1];  //  Move line pointer to the line above (effectively copy down)

		user->scr[user->sc_top] = c;  //  Top row points to previous bottom line contents

		for(i = 0; i < user->cols; i++)
			*(c++) = ' ';  //  Blank out new top line

		rev_scroll_curses(user);
	} else { //if scroll region is a single line or improperly set, just blank out the top scrolling line
		sy = user->y;
		sx = user->x;
		move_term(user, user->sc_top, 0);
		clreol_term(user);
		move_term(user, sy, sx);
	}
}

/*
 * Flush window output.
 */
void
flush_term(user)
	register yuser *user;
{
	flush_curses(user);
}

/*
 * Rub one character.
 */
void
rub_term(user)
	register yuser *user;
{
	if (user->x > 0) {
		if (user->x == user->cols - 1)
			user->onend = 0;
		if (user->bump) {
			addch_term(user, ' ');
			user->bump = 0;
		} else {
			move_term(user, user->y, user->x - 1);
			addch_term(user, ' ');
			move_term(user, user->y, user->x - 1);
		}

	}
}

/*
 * Rub one word.
 */
void
word_term(user)
	register yuser *user;
{
	register int x;

	for (x = user->x - 1; x >= 0 && user->scr[user->y][x] == ' '; x--)
		continue;
	for (; x >= 0 && user->scr[user->y][x] != ' '; x--)
		continue;
	if ((user->x - (++x)) <= 0)
		return;
	move_term(user, user->y, x);
	clreol_term(user);
	return;
}

/*
 * Kill current line.
 */
void
kill_term(user)
	register yuser *user;
{
	if (user->x > 0) {
		move_term(user, user->y, 0);
		clreol_term(user);
	}
}

/*
 * Expand a tab.  We use non-destructive tabs.
 */
void
tab_term(user)
	register yuser *user;
{
	int i;
	/* Find nearest tab and jump to it. */
	if (user->x < user->t_cols) {
		for (i = (user->x + 1); i <= user->t_cols; i++) {
			if (user->scr_tabs[i] == 1) {
				move_term(user, user->y, i);
				break;
			}
		}
	}
}

/*
* Reverse tab.  We use non-destructive tabs.
*/
void
rev_tab_term(user)
register yuser *user;
{
	int i;
	/* Find nearest tab behind cursor and jump to it. */
	if(user->x > 0) {
		for(i = user->x; i > 0; --i) {
			if(user->scr_tabs[i] == 1) {
				move_term(user, user->y, i);
				break;
			}
		}
	}
}

/*
 * Process a line feed.
 */
void
lf_term(user)
	register yuser *user;
{
	register int new_y, next_y;

	new_y = user->y + 1;
	if (user->flags & FL_RAW) {
		if (new_y > user->sc_bot) {
			if (user->flags & FL_SCROLL)
				scroll_term(user);
		}
		move_term(user, new_y, user->x);
	} else {
		if (new_y > user->sc_bot) {
			if (user->flags & FL_SCROLL) {
				scroll_term(user);
				move_term(user, user->y, user->x);
				return;
			}
			new_y = 0;
		}
		next_y = new_y + 1;
		if (next_y >= user->rows)
			next_y = 0;
		if (next_y > 0 || !(user->flags & FL_SCROLL)) {
			move_term(user, next_y, user->x);
			clreol_term(user);
		}
		move_term(user, new_y, user->x);
	}
}

/*
 * Process a newline.
 */
void
newline_term(user)
	register yuser *user;
{
	register int new_y, next_y;

	new_y = user->y + 1;
	if (user->flags & FL_RAW) {
		if (new_y > user->sc_bot) {
			if (user->flags & FL_SCROLL)
				scroll_term(user);
		}
		move_term(user, new_y, 0);
	} else {
		if (new_y > user->sc_bot) {
			if (user->flags & FL_SCROLL) {
				scroll_term(user);
				move_term(user, user->y, 0);
				return;
			}
			new_y = 0;
		}
		next_y = new_y + 1;
		if (next_y >= user->rows)
			next_y = 0;
		if (next_y > 0 || !(user->flags & FL_SCROLL)) {
			move_term(user, next_y, 0);
			clreol_term(user);
		}
		move_term(user, new_y, 0);
		clreol_term(user);
	}
}

//  Updates the scr buffer and calls the proper curses function.
//  Since idlok() is called on window creation, this should invoke
//  the hardware IL command
void
insert_lines_term(user, num)
	register yuser *user;
	int num;
{
	register ychar *c;
	register int i;
	int count;

	count = num;  //  Set initial value to pass to curses function

	if (num == 1 && user->y == 0)
		rev_scroll_term(user);
	else {

		/* find number of remaining lines */
		i = user->rows - user->y - num;
		if (i <= 0) {
			i = user->x;
			move_term(user, user->y, 0);
			clreos_term(user);
			move_term(user, user->y, i);
			return;
		}

		/* swap the remaining lines to bottom */
		for (i--; i >= 0; i--) {
			c = user->scr[user->y + i];
			user->scr[user->y + i] = user->scr[user->y + i + num];
			user->scr[user->y + i + num] = c;
		}

		/* clear the added lines */
		for (num--; num >= 0; num--) {
			c = user->scr[user->y + num];
			for (i = 0; i < user->cols; i++)
				*(c++) = ' ';
		}

		insert_line_curses(user, count);
	}
}

void
delete_lines_term(user, num)
	register yuser *user;
	int num;
{
	register ychar *c;
	register int i;
	int count;

	count = 0-num;  //  Set initial value to pass to curses function

	if (num == 1 && user->y == 0)
		scroll_term(user);
	else {
		/* find number of remaining lines */
		i = user->rows - user->y - num;
		if (i <= 0) {
			i = user->x;
			move_term(user, user->y, 0);
			clreos_term(user);
			move_term(user, user->y, i);
			return;
		}

		/* swap the remaining lines to top */
		for (; i > 0; i--) {
			c = user->scr[user->rows - i];
			user->scr[user->rows - i] = user->scr[user->rows - i - num];
			user->scr[user->rows - i - num] = c;
		}

		/* clear the remaining bottom lines */
		for (; num > 0; num--) {
			c = user->scr[user->rows - num];
			for (i = 0; i < user->cols; i++)
				*(c++) = ' ';
		}

		insert_line_curses(user, count);
	}	
}

static void
copy_text(fr, to, count)
	register ychar *fr, *to;
	register int count;
{
	if (to < fr) {
		for (; count > 0; count--)
			*(to++) = *(fr++);
	} else {
		fr += count;
		to += count;
		for (; count > 0; count--)
			*(--to) = *(--fr);
	}
}

/*
 * Add chars.
 */
void
add_char_term(user, num)
	register yuser *user;
	int num;
{
	register ychar *c;
	register int i;

	/* find number of remaining non-blank chars */

	i = user->cols - user->x - num;
	c = user->scr[user->y] + user->cols - num - 1;
	while (i > 0 && *c == ' ')
		c--, i--;
	if (i <= 0) {
		clreol_term(user);
		return;
	}

	/* transfer the chars and clear the remaining */

	c++;
	copy_text(c - i, c - i + num, i);
	for (c -= i; num > 0; num--) {
		*(c++) = ' ';
		addch_curses(user, ' ');
	}
	for (; i > 0; i--)
		addch_curses(user, *(c++));
	move_curses(user, user->y, user->x);
}

/*
 * Delete chars.
 */
void
del_char_term(user, num)
	register yuser *user;
	int num;
{
	register ychar *c;
	register int i;

	/* find number of remaining non-blank chars */

	i = user->cols - user->x - num;
	c = user->scr[user->y] + user->cols - 1;
	while (i > 0 && *c == ' ')
		c--, i--;
	if (i <= 0) {
		clreol_term(user);
		return;
	}

	/* transfer the chars and clear the remaining */

	c++;
	copy_text(c - i, c - i - num, i);
	for (c -= (i + num); i > 0; i--)
		addch_curses(user, *(c++));
	for (; num > 0; num--) {
		*(c++) = ' ';
		addch_curses(user, ' ');
	}
	move_curses(user, user->y, user->x);
}

/*
 * Redraw a user's window.
 */
void
redraw_term(user, y)
	register yuser *user;
	register int y;
{
	register int x, spaces;
	register ychar *c;

	for(; y < user->t_rows; y++) {
		//  Clear this entire line and redraw it in the loop below
		move_curses(user, y, 0);
		clreol_curses(user);
		spaces = 0;
		c = user->scr[y];

		for(x = 0; x < user->t_cols; x++, c++) {
			//  Redraw this character.  Count consecutive spaces encoutered
			//  and issue a move_curses() command if there are more than three.
			//  Otherwise, just draw the spaces
			if(*c == ' ') {
				spaces++;
			}
			else {
				if(spaces) {
					if(spaces <= 3) {	/* arbitrary */
						for(; spaces > 0; spaces--)
							addch_curses(user, ' ');
					}
					else {
						move_curses(user, y, x);
						spaces = 0;
					}
				}

				addch_curses(user, *c);
			}
		}
	}

	/* redisplay any active menu */
	if(menu_ptr != NULL)
		update_menu();
	else
		move_curses(user, user->y, user->x);
}

/*
 * Return the first interesting row for a user with a window of the given
 * height and width.
 */
static int
first_interesting_row(user, height, width)
	yuser *user;
	int height, width;
{
	register int j, i;
	register ychar *c;

	if (height < user->t_rows) {
		j = (user->y + 1) - height;
		if (j < 0)
			j += user->t_rows;
	} else {
		j = user->y + 1;
		if (j >= user->t_rows)
			j = 0;
	}
	while (j != user->y) {
		i = (width > user->t_cols) ? user->t_cols : width;
		for (c = user->scr[j]; i > 0; i--, c++)
			if (*c != ' ')
				break;
		if (i > 0)
			break;
		if (++j >= user->t_rows)
			j = 0;
	}
	return j;
}

/*
 * Called when a user's window has been resized.
 */
void
resize_win(user, height, width)
	yuser *user;
	int height, width;
{
	register int j, i;
	register ychar *c, **new_scr;
	int new_y, y_pos;

	if (height == user->t_rows && width == user->t_cols)
		return;

	/* resize the user terminal buffer */

	new_y = -1;
	y_pos = 0;
	new_scr = (ychar **) get_mem(height * sizeof(ychar *));
	if (user->scr == NULL) {
		user->t_rows = user->rows = 0;
		user->t_cols = user->cols = 0;
	} else if (user->region_set) {
		/* save as many top lines as possible */

		for (j = 0; j < height && j < user->t_rows; j++)
			new_scr[j] = user->scr[j];
		new_y = j - 1;
		y_pos = user->y;
		for (; j < user->t_rows; j++)
			free_mem(user->scr[j]);
		free_mem(user->scr);
	} else {
		/* shift all recent lines to top of screen */

		j = first_interesting_row(user, height, width);
		for (i = 0; i < height; i++) {
			new_scr[++new_y] = user->scr[j];
			if (j == user->y)
				break;
			if (++j >= user->t_rows)
				j = 0;
		}
		for (i++; i < user->t_rows; i++) {
			if (++j >= user->t_rows)
				j = 0;
			free_mem(user->scr[j]);
		}
		y_pos = new_y;
		free_mem(user->scr);
	}
	user->scr = new_scr;

	/* fill in the missing portions */

	if (width > user->t_cols) {
		for (i = 0; i <= new_y; i++) {
			user->scr[i] = (ychar *) realloc_mem(user->scr[i], width);
			for (j = user->t_cols; j < width; j++)
				user->scr[i][j] = ' ';
		}

		user->scr_tabs = realloc_mem(user->scr_tabs, width * sizeof(char));
		for (j = user->t_cols; j < width; j++) {
			if (j % 8 == 0)
				user->scr_tabs[j] = 1;
			else
				user->scr_tabs[j] = 0;
		}
		/* rightmost column is always last tab on line */
		user->scr_tabs[width - 1] = 1;
	}
	for (i = new_y + 1; i < height; i++) {
		c = user->scr[i] = (ychar *) get_mem(width);
		for (j = 0; j < width; j++)
			*(c++) = ' ';
	}

	/* reset window values */

	user->t_rows = user->rows = height;
	user->t_cols = user->cols = width;
	user->sc_top = 0;
	user->sc_bot = height - 1;
	move_term(user, y_pos, user->x);
	send_winch(user);
	redraw_term(user, 0);
	flush_term(user);

	/* Restore raw mode if running a shell */
	if (running_process)
		set_raw_term();
}

/*
 * Draw a nice box.
 */
static void
draw_box(user, height, width, c)
	yuser *user;
	int height, width;
	char c;
{
	register int i;

	if (width < user->t_cols) {
		for (i = 0; i < height; i++) {
			move_term(user, i, width);
			addch_term(user, c);
			if (width + 1 < user->t_cols)
				clreol_term(user);
		}
	}
	if (height < user->t_rows) {
		move_term(user, height, 0);

		//  Added by ap:  Only change this draw_box code if it's being used
		//  to fill the virtual display area.  If there are any other calls with other chars,
		//  allow them to be processed traditionally.
		if(c == '%') {
			//  We want to first find the user who is to blame for this visible section
			//  of the display area rendered unusable due to that user's smaller term
			//  size restricting us.
			int minrows;  //  close enough to max signed int.
			int foundSomeone = 0;
			char* minuser_name;
			register yuser *u;
			minrows = me->t_rows;

			//  Loop through all connected users with supporting versions to report rows/cols.
			for(u = connect_list; u; u = u->next) {
				if (u->remote.vmajor > 2) {
					//  This user is (further) restricting my rows.  Note his user_name for display.
					if (u->remote.my_rows > 1 && u->remote.my_rows < minrows) {
						minrows = u->remote.my_rows;
						minuser_name = u->user_name;
						foundSomeone = 1;
					}
				}
			}

			if(!foundSomeone)
				return;  //  no blame found; do not write line/box.

			//  To write centered on blame line: " blame " + username + " for " + minrows + " lines! "
			register int pad, x;
			int lines_length;
			char* blame_literal = " blame \0";
			register char *t;
			char* for_literal = " for \0";
			char restricted_lines[10];  //  holds the itoa() conversion - use char[10] on the stack to hold any int value to be safe
			char* lines_literal = " lines! \0";

			for(i=0; i < 10; ++i)
				restricted_lines[i] = '\0';

			lines_length = sprintf(restricted_lines, "%d", minrows);  //  Get the char* representing the number of lines
			pad = (width - (int)strlen(blame_literal) - (int)strlen(minuser_name) - (int)strlen(for_literal) - lines_length - (int)strlen(lines_literal)) / 2;

			for (x=0; x < pad; x++)
				addch_term(user, c);

			for(t = blame_literal; *t; ++t)
				addch_term(user, *t);

			for(t = minuser_name; *t; ++t)
				addch_term(user, *t);

			for(t = for_literal; *t; ++t)
				addch_term(user, *t);

			for(t = restricted_lines; *t; ++t)
				addch_term(user, *t);

			for(t = lines_literal; *t; ++t)
				addch_term(user, *t);

			for (; x < width; x++)
				addch_term(user, c);
		}
		else {
			for (i = 0; i < width; i++)
				addch_term(user, c);
		}

		if (width < user->t_cols)
			addch_term(user, c);
		if (width + 1 < user->t_cols)
			clreol_term(user);
		if (height + 1 < user->t_rows) {
			move_term(user, height + 1, 0);
			clreos_term(user);
		}
	}
}

/*
 * Set the virtual terminal size, ie: the display region.
 */
void
set_win_region(user, height, width)
	yuser *user;
	int height, width;
{
	register int x, y;
	int old_height, old_width;

	if (height < 2 || height > user->t_rows)
		height = user->t_rows;
	if (width < 2 || width > user->t_cols)
		width = user->t_cols;

	/*
	 * Don't check if they're already equal; always perform processing.
	 * Just because it's equal over here doesn't mean it's equal for all
	 * ytalk connections.  We still need to clear the screen.
	 */

	old_height = user->rows;
	old_width = user->cols;
	user->rows = user->t_rows;
	user->cols = user->t_cols;
	if (user->region_set) {
		x = user->x;
		y = user->y;
		if (width > old_width || height > old_height)
			draw_box(user, old_height, old_width, ' ');
	} else {
		x = y = 0;
		move_term(user, 0, 0);
		clreos_term(user);
		user->region_set = 1;
	}
	draw_box(user, height, width, '%');	//  ap:  This seems to write percents to the entire window region.  Percents left over will only be outside
										//  the usable display region based on the fact that the rows/cols gets distributed to each user on all resizes.

	/* set the display region */

	user->rows = height;
	user->cols = width;
	user->sc_top = 0;
	user->sc_bot = height - 1;
	move_term(user, y, x);
	flush_term(user);				//  ap:  I assume this is where the usable window display region is blanked out.

	if (user == me)
		send_region();
}

/*
 * Set the virtual terminal size, ie: the display region.
 */
void
end_win_region(user)
	yuser *user;
{
	int old_height, old_width;

	old_height = user->rows;
	old_width = user->cols;
	user->rows = user->t_rows;
	user->cols = user->t_cols;
	user->sc_top = 0;
	user->sc_bot = user->rows - 1;
	if (old_height < user->t_rows || old_width < user->t_cols)
		draw_box(user, old_height, old_width, ' ');
	user->region_set = 0;
	if (user == me)
		send_end_region();
}

/*
 * Set the scrolling region.
 */
void
set_scroll_region(user, top, bottom)
	yuser *user;
	int top, bottom;
{
	if (top < 0 || top >= user->rows || bottom >= user->rows || bottom < top
	    || (bottom <= 0 && top <= 0)) {
		user->sc_top = 0;
		user->sc_bot = user->rows - 1;
	} else {
		user->sc_top = top;
		user->sc_bot = bottom;
	}
}

/*
 * Send a message to the terminal.
 */
void
msg_term(user, str)
	yuser *user;
	char *str;
{
	int y;

	if ((y = user->y + 1) >= user->rows)
		y = 0;
	move_curses(user, y, 0);
	addch_curses(user, '[');
	while (*str)
		addch_curses(user, *(str++));
	addch_curses(user, ']');
	clreol_curses(user);
	move_curses(user, user->y, user->x);
	flush_curses(user);
}

/*
 * Spew terminal contents to a file descriptor.
 */
void
spew_term(user, fd, rows, cols)
	yuser *user;
	int fd, rows, cols;
{
	register ychar *c, *e;
	register int len;
	int y;
	static char tmp[20];

	if (user->region_set) {
		y = 0;
		if (cols > user->cols)
			cols = user->cols;
		if (rows > user->rows)
			rows = user->rows;
		for (;;) {
			for (c = e = user->scr[y], len = cols; len > 0; len--, c++)
				if (*c != ' ')
					e = c + 1;
			if (e != user->scr[y])
				write(fd, user->scr[y], (size_t)(e - user->scr[y]));
			if (++y >= rows)
				break;
			if (user->crlf == 1)
				write(fd, "\r\n", 2);
			else
				write(fd, "\n", 1);
		}

		/* move the cursor to the correct place */

#ifdef HAVE_SNPRINTF
		snprintf(tmp, sizeof(tmp), "%c[%d;%dH", 27, user->y + 1, user->x + 1);
#else
		sprintf(tmp, "%c[%d;%dH", 27, user->y + 1, user->x + 1);
#endif
		write(fd, tmp, strlen(tmp));
	} else {
		y = first_interesting_row(user, rows, cols);
		for (;;) {
			if (y == user->y) {
				if (user->x > 0)
					write(fd, user->scr[y], (size_t)user->x);
				break;
			}
			for (c = e = user->scr[y], len = user->t_cols; len > 0; len--, c++)
				if (*c != ' ')
					e = c + 1;
			if (e != user->scr[y])
				write(fd, user->scr[y], (size_t)(e - user->scr[y]));
			if (user->crlf == 1)
				write(fd, "\r\n", 2);
			else
				write(fd, "\n", 1);
			if (++y >= user->t_rows)
				y = 0;
		}
	}
}

/*
 * Draw some raw characters to the screen without updating any buffers.
 * Whoever uses this should know what they're doing.  It should always be
 * followed by a redraw_term() before calling any of the normal term
 * functions again.
 *
 * If the given string is not as long as the given length, then the string is
 * repeated to fill the given length.
 *
 * This is an unadvertised function.
 */
void
raw_term(user, y, x, str, len)
	yuser *user;
	int y, x;
	ychar *str;
	int len;
{
	register ychar *c;

	if (y < 0 || y >= user->t_rows)
		return;
	if (x < 0 || x >= user->t_cols)
		return;
	move_curses(user, y, x);

	for (c = str; len > 0; len--, c++) {
		if (*c == '\0')
			c = str;
		if (!is_printable(*c))
			return;
		addch_curses(user, *c);
	}
}

//  Apply colors through curses color pairs
void
color_term(user, colorID, isBg)
	yuser *user;
	int colorID;
	int isBg;
{
	color_curses(user, colorID, isBg);
}

void
rawout_term(user, str)
	yuser *user;
	char *str;
{
	rawout_curses(user, str);
}

//  Apply curses formatting through SGR() commands
void
format_term(user, sgrID)
	yuser *user;
	int sgrID;
{
	format_curses(user, sgrID);
}

int
center(width, n)
	int width, n;
{
	if (n >= width)
		return 0;
	return (width - n) / 2;
}

void
redraw_all_terms()
{
	if (term_type == 1)
		redisplay_curses();
}

void
set_raw_term()
{
	if (term_type == 1)
		set_raw_curses();
}

void
set_cooked_term()
{
	if (term_type == 1)
		set_cooked_curses();
}
