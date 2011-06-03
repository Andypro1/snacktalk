/*
 * src/vt100.c
 * VT100 terminal emulation
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

void
vt100_process(user, data)
	yuser *user;
	char data;
{
	int i;

	if (data >= '0' && data <= '9') {
		if (user->vt.got_esc > 1) {
			user->vt.av[user->vt.ac] = (user->vt.av[user->vt.ac] * 10) + (data - '0');
			return;
		}
		if (user->vt.hash == 1 && data == '8') { //DECALN - This sequence fills the screen with uppercase E's
			fill_term(user, 0, 0, user->rows - 1, user->cols - 1, 'E');
			redraw_term(user, 0);  //  This redraw_term() call is fine
			user->vt.got_esc = 0;
			return;
		}
	}

	switch (data) {
	case ';':		/* arg separator */
		if (user->vt.ac < MAXARG - 1)
			user->vt.av[++(user->vt.ac)] = 0;
		break;
	case '[':
		user->vt.got_esc = 2;
		break;
	case '?':
		if (user->vt.got_esc == 2)
			user->vt.got_esc = 3;
		else
			user->vt.got_esc = 0;
		break;
	case 'E':
		newline_term(user);
		user->vt.got_esc = 0;
		break;
	case '#':
		user->vt.hash = 1;
		break;
	case 'c':
		//  I am a service class 2 terminal (62) with 132 columns (1), printer port (2), selective erase (6),
		//  DRCS (7), UDK (8), and I support 7-bit national replacement character sets (9).
		write(user->fd, "\033[62;1;2;6;7;8;9c", 4);
		user->vt.got_esc = 0;
		break;
	case 8:
		if (user->x > 0)
			move_term(user, user->y, user->x - 1);
		break;
	case 11:
		lf_term(user);
		break;
	case 13:
		move_term(user, user->y, 0);
		break;
	case 's':		/* save cursor */
		if (user->vt.got_esc != 2) {
			user->vt.got_esc = 0;
			break;
		}
		user->sy = user->y;
		user->sx = user->x;
		user->vt.got_esc = 0;
		break;
	case 'u':		/* restore cursor */
		if (user->vt.got_esc != 2) {
			user->vt.got_esc = 0;
			break;
		}
		move_term(user, user->sy, user->sx);
		user->vt.got_esc = 0;
		break;
	case 'h':		/* set modes */
		switch (user->vt.av[0]) {
		case 1:	/* keypad "application" mode */
			keypad_term(user, 1);
			break;
		}
		user->vt.got_esc = 0;
		break;
	case 'l':		/* clear modes */
		switch (user->vt.av[0]) {
		case 1:	/* keypad "normal" mode */
			keypad_term(user, 0);
			break;
		}
		user->vt.got_esc = 0;
		break;
	case '=':
		keypad_term(user, 1);
		user->vt.got_esc = 0;
		break;
	case '>':
		keypad_term(user, 0);
		user->vt.got_esc = 0;
		break;
	case '@':
		if (user->vt.got_esc == 2) {	/* add char */
			if (user->vt.av[0] == 0)
				add_char_term(user, 1);
			else
				add_char_term(user, user->vt.av[0]);
		}
		user->vt.got_esc = 0;
		break;
	case 'A':		/* move up */
		if (user->vt.av[0] == 0)
			move_term(user, user->y - 1, user->x);
		else if ((int) user->vt.av[0] > user->y)
			move_term(user, 0, user->x);
		else
			move_term(user, user->y - user->vt.av[0], user->x);
		user->vt.got_esc = 0;
		break;
	case 'B':		/* move down */
		if (user->vt.lparen) {
			user->vt.lparen = 0;
		} else {
			if (user->y != user->rows) {
				if (user->vt.av[0] == 0)
					move_term(user, user->y + 1, user->x);
				else
					move_term(user, user->y + user->vt.av[0], user->x);
			}
		}
		user->vt.got_esc = 0;
		break;
	case 'C':		/* move right */
		if (user->vt.got_esc == 2) {
			if (user->x != user->cols) {
				if (user->vt.av[0] == 0)
					move_term(user, user->y, user->x + 1);
				else
					move_term(user, user->y, user->x + user->vt.av[0]);
			}
		}
		user->vt.got_esc = 0;
		break;
	case 'D':		/* move left */
		if (user->vt.got_esc == 2) {
			if (user->vt.av[0] == 0)
				move_term(user, user->y, user->x - 1);
			else if (/*(int)*/ user->vt.av[0] > user->x)
				move_term(user, user->y, 0);
			else
				move_term(user, user->y, user->x - user->vt.av[0]);
		} else {
			if (user->y < user->sc_bot)
				user->y++;
			else
				scroll_term(user);
		}
		user->vt.got_esc = 0;
		break;
	case 'f':		/* force cursor */
	case 'H':		/* move */
		if (user->vt.got_esc == 2) {
			if (user->vt.av[0] > 0)
				user->vt.av[0]--;
			if (user->vt.av[1] > 0)
				user->vt.av[1]--;
			move_term(user, user->vt.av[0], user->vt.av[1]);
		} else {
			if (data == 'H')	/* <esc>H is set tab */
				user->scr_tabs[user->x] = 1;
		}
		user->vt.got_esc = 0;
		break;
	case 'G':
		if (user->vt.got_esc == 2) {
			if (user->vt.av[0] > 0)
				user->vt.av[0]--;
			move_term(user, user->y, user->vt.av[0]);
		}
		user->vt.got_esc = 0;
		break;
	case 'g':
		if (user->vt.got_esc == 2) {
			switch (user->vt.av[0]) {
			case 3:		/* clear all tabs */
				for (i = 0; i < user->t_cols; i++)
					user->scr_tabs[i] = 0;
				break;
			case 0:		/* clear tab at x */
				user->scr_tabs[user->x] = 0;
				break;
			}
		}
		user->vt.got_esc = 0;
		break;
	case 'J': //ED
		if(user->vt.got_esc == 2) {
			switch(user->vt.av[0]) {
			case 1: //Erase Above
				//  *Note: the below condition is most probably a typo.
				//  Should be checking if user->y > 0 instead.  consider correcting later.
				if(user->x > 0)  //fill all lines above with blanks
					fill_term(user, 0, 0, user->y - 1, user->cols - 1, ' ');

				//fill current line with blanks up to and including cursor column
				fill_term(user, user->y, 0, user->y, user->x, ' ');

				erase_above_curses(user);
				break;
			case 2: //Erase All
				fill_term(user, 0, 0, user->rows - 1, user->cols - 1, ' ');  //  Clear snacktalk's user screen buffer
				erase_curses(user);											 //  But issue simple clear call to curses
				break;
			case 0:
			default: //Erase Below
				clreos_term(user);
			}
		}

		user->vt.got_esc = 0;
		break;
	case 'K':
		if(user->vt.got_esc == 2) {
			switch (user->vt.av[0]) {
			case 0:		/* clear to end of line */
				clreol_term(user);
				break;
			case 1:		/* clear to beginning of line */
				fill_term(user, user->y, 0, user->y, user->x, ' ');
				clear_line_curses(user, 0);
				break;
			case 2:		/* clear entire line */
				fill_term(user, user->y, 0, user->y, user->cols - 1, ' ');
				clear_line_curses(user, 1);
				break;
			}
		}
		user->vt.got_esc = 0;
		break;
	case 'L': //Insert Lines (IL)
		if(user->vt.got_esc == 2) {
			if(user->vt.av[0] == 0)
				insert_lines_term(user, 1);
			else
				insert_lines_term(user, user->vt.av[0]);
		}

		user->vt.got_esc = 0;
		break;
	case 'M':
		if(user->vt.got_esc == 2) {	//Delete Lines (DL)
			if(user->vt.av[0] == 0)
				delete_lines_term(user, 1);
			else
				delete_lines_term(user, user->vt.av[0]);
		}
		else { //Reverse Index (RI)
			if(user->y > user->sc_top)
				user->y--;
			else
				rev_scroll_term(user);
		}

		user->vt.got_esc = 0;
		break;
	case 'P':
		if (user->vt.got_esc == 2) {	/* del char */
			if (user->vt.av[0] == 0)
				del_char_term(user, 1);
			else
				del_char_term(user, user->vt.av[0]);
		}
		user->vt.got_esc = 0;
		break;
	case 'S':		/* forward scroll */
		scroll_term(user);
		user->vt.got_esc = 0;
		break;
	case 'm':  //Character Attributes (SGR) - Added by ap 2011-05-18
		if(user->vt.got_esc == 2) {
			if(user->vt.av[0] >= 30 && user->vt.av[0] <= 37) { //foreground color
				color_term(user, user->vt.av[0]-30, 0);
			}
			else if(user->vt.av[0] >= 40 && user->vt.av[0] <= 47) { //background color
				color_term(user, user->vt.av[0]-40, 1);
			}
			else if(user->vt.av[0] >= 1 && user->vt.av[0] <= 28) {
				format_term(user, user->vt.av[0]);
			}
			else if(user->vt.av[0] == 39) { //default foreground
				color_term(user, -1, 0);
			}
			else if(user->vt.av[0] == 49) { //default background
				color_term(user, -1, 1);
			}
			else if(user->vt.av[0] == 0) { //SGR() (no arguments - reset formatting)
				format_term(user, 0);
			}

			//  Handle second SGR() argument, if any
			if(user->vt.av[1] >= 30 && user->vt.av[1] <= 37) { //foreground color
				color_term(user, user->vt.av[1]-30, 0);
			}
			else if(user->vt.av[1] >= 40 && user->vt.av[1] <= 47) { //background color
				color_term(user, user->vt.av[1]-40, 1);
			}
			else if(user->vt.av[1] == 39) { //default foreground
				color_term(user, -1, 0);
			}
			else if(user->vt.av[1] == 49) { //default background
				color_term(user, -1, 1);
			}
			else if(user->vt.av[1] >= 1 && user->vt.av[1] <= 28) {
				format_term(user, user->vt.av[1]);
			}
		}

		user->vt.got_esc = 0;
		break;
	case 'r':		/* set scroll region */
		if (user->vt.got_esc == 2) {
			if (user->vt.av[0] > 0)
				user->vt.av[0]--;
			if (user->vt.av[1] > 0)
				user->vt.av[1]--;
			set_scroll_region(user, user->vt.av[0], user->vt.av[1]);
			move_term(user, 0, 0);
		}
		user->vt.got_esc = 0;
		break;
	case '(':		/* skip over lparens for now */
		user->vt.lparen = 1;
		break;
	case '0':
		if (user->vt.lparen)
			user->vt.lparen = 0;
		user->vt.got_esc = 0;
		break;
	case '7':		/* save cursor and attributes */
		user->sy = user->y;
		user->sx = user->x;
		user->vt.got_esc = 0;
		break;
	case '8':		/* restore cursor and attributes */
		move_term(user, user->sy, user->sx);
		user->vt.got_esc = 0;
		break;
	default:
		user->vt.got_esc = 0;
	}
}
