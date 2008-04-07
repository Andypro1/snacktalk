/*
 * src/menu.h
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

/*
 * The following structure defines a menu item.  It will be displayed to the
 * user as the _key_ followed by the _item_.  If an item's _key_ is pressed,
 * the _func_ for that item is called with one argument: the _key_ pressed.
 */
typedef struct {
	char *item;		/* item string, ie: "add a user" */
	void (*func) ();	/* function to call */
	ychar key;		/* activating keypress, ie: "a" */
} menu_item;

extern menu_item *menu_ptr;	/* if non-NULL, current menu in processing */

/* global functions */

extern void kill_menu();	/* menu.c */
extern void update_menu();	/* menu.c */
extern int show_menu( /* menuptr, len */ );	/* menu.c */
extern int show_text( /* prompt, func */ );	/* menu.c */
extern int show_mesg();		/* menu.c */
extern int show_main_menu();	/* menu.c */
extern int show_option_menu();	/* menu.c */
extern int show_user_menu( /* title, func */ );	/* menu.c */
extern int show_error_menu( /* str1, str2 */ );	/* menu.c */
extern int yes_no( /* prompt */ );	/* menu.c */
extern void update_user_menu();	/* menu.c */
extern void stalkversion( /* yuser, buf, len */ ); /* menu.c */

/* EOF */
