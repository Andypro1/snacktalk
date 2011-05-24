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
	int is_color_set = 0;

	if (data >= '0' && data <= '9') {
		if (user->vt.got_esc > 1) {
			user->vt.av[user->vt.ac] = (user->vt.av[user->vt.ac] * 10) + (data - '0');
			return;
		}
		if (user->vt.hash == 1 && data == '8') {
			fill_term(user, 0, 0, user->rows - 1, user->cols - 1, 'E');
			redraw_term(user, 0);
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
		write(user->fd, "\033[0c", 4);
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
			else if ((int) user->vt.av[0] > user->x)
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
	case 'J':
		if (user->vt.got_esc == 2) {
			switch (user->vt.av[0]) {
			case 1:
				if (user->x > 0)
					fill_term(user, 0, 0, user->y - 1, user->cols - 1, ' ');
				fill_term(user, user->y, 0, user->y, user->x, ' ');
				redraw_term(user, 0);
				break;
			case 2:
				fill_term(user, 0, 0, user->rows - 1, user->cols - 1, ' ');
				redraw_term(user, 0);
				break;
			default:
				clreos_term(user);
			}
		}
		user->vt.got_esc = 0;
		break;
	case 'K':
		if (user->vt.got_esc == 2) {
			switch (user->vt.av[0]) {
			case 0:		/* clear to end of line */
				clreol_term(user);
				break;
			case 1:		/* clear to beginning of line */
				fill_term(user, user->y, 0, user->y, user->x, ' ');
				redraw_term(user, 0);
				break;
			case 2:		/* clear entire line */
				fill_term(user, user->y, 0, user->y, user->cols - 1, ' ');
				redraw_term(user, 0);
				break;
			}
		}
		user->vt.got_esc = 0;
		break;
	case 'L':
		if (user->vt.got_esc == 2) {	/* add line */
			if (user->vt.av[0] == 0)
				add_line_term(user, 1);
			else
				add_line_term(user, user->vt.av[0]);
		}
		user->vt.got_esc = 0;
		break;
	case 'M':
		if (user->vt.got_esc == 2) {	/* delete line */
			if (user->vt.av[0] == 0)
				del_line_term(user, 1);
			else
				del_line_term(user, user->vt.av[0]);
		} else		/* reverse scroll */
			if (user->y > user->sc_top)
				user->y--;
			else
				rev_scroll_term(user);
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
			if(user->vt.av[0] >= 30 && user->vt.av[0] <= 49) { //SGR(30) through SGR(49) turn color pair on
				attron(COLOR_PAIR(user->vt.av[0]));
				is_color_set = 1;
			}
			else if(user->vt.av[0] == 0) { //SGR() (no arguments - reset formatting)
				attron(COLOR_PAIR(39));
			}

			//  TODO: Find or create a term function (not write) that can simply write() non-printable
			//  messages like vtxxx control sequences as output in the correct location.  Then when ncurses' flush_term()
			//  is called, the non-printable messages will be written out in the correct order like the rest of the
			//  term commands / text.

			//write(user->fd, "\033[", 2);
			//add_raw_term_sequence_term(user, "\033[");

			//for(i=0; user->vt.av[i] > 0; ++i) {
			//	//////  Create string from number
			//	int number_length;
			//	int j;
			//	char restricted_lines[10];  //  holds the itoa() conversion - use char[10] on the stack to hold any int value to be safe

			//	for(j=0; j < 10; ++j)
			//		restricted_lines[j] = '\0';

			//	number_length = sprintf(restricted_lines, "%d", user->vt.av[i]);  //  Get the char* representing the number of lines
			//	//////  End string creation

			//	//write(user->fd, restricted_lines, number_length);
			//	add_raw_term_sequence_term(user, restricted_lines);

			//	if(user->vt.av[i+1] > 0) { //not the last argument
			//		//write(user->fd, ";", 1);
			//		add_raw_term_sequence_term(user, ";");
			//	}
			//}

			//write(user->fd, "m", 1);
			//add_raw_term_sequence_term(user, "m");
		}

		if(is_color_set == 1) { //instruct term that we used color
			is_color_set = 0;
			user->vt.got_esc = 39;
		}
		else {
			user->vt.got_esc = 0;
		}

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
