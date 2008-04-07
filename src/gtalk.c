/*
 * src/gtalk.c
 * some stuff to interface with GNU talk
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
#include "cwin.h"
#include "mem.h"
#include "gtalk.h"

void
gtalk_process(user, data)
	yuser *user;
	ychar data;
{
	if (user->gt.len >= (MAXBUF - 1)) {
		/* If we run out of buffer space, just forget it.
		 * There's no reason reason to read >4K of version string.
		 */
		user->gt.got_gt = 0;
		return;
	}

	if (user->gt.type == 0) {
		user->gt.type = data;
		return;
	}

	if (user->gt.buf == NULL)
		user->gt.buf = get_mem(MAXBUF);

	if (data == user->KILL || data == '\n') {
		user->gt.got_gt = 0;
		user->gt.buf[user->gt.len] = 0;
		switch (user->gt.type) {
		case GTALK_VERSION_MESSAGE:
			if (user->gt.version != NULL)
				free_mem(user->gt.version);
			user->gt.version = gtalk_parse_version(user->gt.buf, user->KILL);
			/* let the main loop know there's been a change in user data */
			user_winch = 1;
			break;
		}
		return;
	}

	user->gt.buf[user->gt.len++] = data;
}

char *
gtalk_parse_version(str, ukill)
	char *str;
	ychar ukill;
{
	char *p, *e;
	p = strchr(str, ' ');
	if (p != NULL)
		p = strchr(p + 1, ' ');
	if (p != NULL) {
		p++;
		for (e = p; *e; e++)
			if (*e == ukill || *e == '\n') {
				*e = 0;
				break;
			}
		return str_copy(p);
	}
	return NULL;
}
