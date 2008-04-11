/*
 * src/user.c
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

#include <pwd.h>
#include <string.h>

extern int dont_change_my_addr;

yuser *me;			/* my user information */
yuser *user_list;		/* list of invited/connected users */
yuser *connect_list;		/* list of connected users */
yuser *wait_list;		/* list of connected users */
yuser *fd_to_user[MAX_FILES];	/* convert file descriptors to users */
yuser *key_to_user[128];	/* convert menu ident chars to users */
ylong def_flags = 0L;		/* default FL_* flags */
static ylong daemon_id;		/* running daemon ID counter */

/* ---- local functions ----- */

static int passwd_opened = 0;

static char *
user_name(uid)
	ylong uid;
{
	register struct passwd *pw;
	passwd_opened = 1;
	if ((pw = getpwuid(uid)) == NULL)
		return NULL;
	return pw->pw_name;
}

static void
close_passwd()
{
	if (passwd_opened) {
		endpwent();
		passwd_opened = 0;
	}
}

void
generate_full_name(user)
	yuser *user;
{
	register char *c, *d, *ce;

	if (user->full_name == NULL)
		user->full_name = get_mem(50);
	c = user->full_name, ce = user->full_name + 49;

	for (d = user->user_name; *d && c < ce; d++)
		*(c++) = *d;

	if (c < ce)
		*(c++) = '@';
	for (d = user->host_fqdn; *d && c < ce; d++)
		*(c++) = *d;

	if (user->tty_name[0]) {
		if (c < ce)
			*(c++) = '#';
		for (d = user->tty_name; *d && c < ce; d++)
			*(c++) = *d;
	}
	*c = '\0';
}

static void
assign_key(user)
	yuser *user;
{
	register ychar old;
	static ychar key = 'a';

	if (user->key != '\0' || user == me || user_list == NULL)
		return;
	old = key;
	do {
		if (key_to_user[key] == NULL) {
			key_to_user[key] = user;
			user->key = key;
			return;
		}
		if (key == 'z')
			key = 'A';
		else if (key == 'Z')
			key = 'a';
		else
			key++;
	} while (old != key);
	user->key = '\0';
}

/* ---- global functions ----- */

/*
 * Initialize user data structures.
 */
void
init_user(vhost)
	char *vhost;
{
	char *my_name, *my_vhost;
	char my_host[100];

	user_list = NULL;
	connect_list = NULL;
	wait_list = NULL;
	daemon_id = getpid() << 10;
	memset(fd_to_user, 0, MAX_FILES * sizeof(yuser *));
	memset(key_to_user, 0, 128 * sizeof(yuser *));

	/* get my username */
	my_name = user_name(myuid);
	if (my_name == NULL || my_name[0] == '\0') {
		show_error("Who are you?");
		bail(YTE_ERROR);
	}

	/* get my hostname */
	if ((my_vhost = vhost) || (my_vhost = getenv("LOCALHOST"))) {
		strncpy(my_host, my_vhost, 99);
		dont_change_my_addr = 1;
	} else {
		if (gethostname(my_host, 100) < 0) {
			show_error("init_user: gethostname() failed");
			bail(YTE_ERROR);
		} else {
			/* try to find the fqdn */
			if (strchr(my_host, '.') == NULL) {
				ylong adr = get_host_addr(my_host);
				if (adr != 0 && adr != (ylong) -1) {
					char *name = host_name(adr);
					if (name && strchr(name, '.'))
						strncpy(my_host, name, 99);
				}
			}
		}
	}
	my_host[99] = 0;

	/* get my user record */
	if ((me = new_user(my_name, my_host, NULL)) == NULL)
		bail(YTE_ERROR);

	me->key = '@';
	me->remote.protocol = YTP_NEW;
	me->remote.vmajor = VMAJOR;
	me->remote.vminor = VMINOR;
	me->remote.pid = getpid();

	/* make sure we send CR/LF data to ourselves */
	me->crlf = 1;

	close_passwd();
}

/*
 * Create a new user record.
 */
yuser *
new_user(name, hostname, tty)
	char *name, *hostname, *tty;
{
	register yuser *out, *u;
	ylong addr;

	/* find the host address */

	if (hostname == NULL || *hostname == '\0') {
		hostname = me->host_name;
		addr = me->host_addr;
	} else if ((addr = get_host_addr(hostname)) == (ylong) -1) {
#ifdef HAVE_SNPRINTF
		snprintf(errstr, MAXERR, "new_user: bad host: '%s'", hostname);
#else
		sprintf(errstr, "new_user: bad host: '%s'", hostname);
#endif
		show_error(errstr);
		return NULL;
	}

	/* create the user record */

	out = (yuser *) get_mem(sizeof(yuser));
	memset(out, 0, sizeof(yuser));
	out->user_name = str_copy(name);
	out->host_name = str_copy(hostname);
	if (strchr(hostname, '.'))
		out->host_fqdn = str_copy(hostname);
	else
		out->host_fqdn = str_copy(host_name(addr));
	out->host_addr = addr;
	if (tty)
		out->tty_name = str_copy(tty);
	else
		out->tty_name = str_copy("");
	out->d_id = daemon_id++;
	generate_full_name(out);
	assign_key(out);
	out->flags = def_flags;

	/* Actually make an effort to keep the user list in order */

	if (user_list == NULL || out->key <= user_list->key) {
		out->unext = user_list;
		user_list = out;
	} else {
		for (u = user_list; u->unext != NULL; u = u->unext)
			if (out->key <= u->unext->key)
				break;
		out->unext = u->unext;
		u->unext = out;
	}
	return out;
}

static void
clear_user(user)
	yuser *user;
{
	unsigned int i;
	free_mem(user->user_name);
	free_mem(user->host_name);
	free_mem(user->host_fqdn);
	free_mem(user->tty_name);
	free_mem(user->full_name);
	if (user->gt.buf != NULL)
		free_mem(user->gt.buf);
	if (user->gt.version != NULL)
		free_mem(user->gt.version);
	if (user->dbuf != NULL)
		free_mem(user->dbuf);
	if (user->term != NULL)
		free_mem(user->term);
	if (user->output_fd > 0)
		close(user->output_fd);
	if (user->fd) {
		remove_fd(user->fd);
		fd_to_user[user->fd] = NULL;
		close(user->fd);
	}
	if (user->key != '\0')
		key_to_user[user->key] = NULL;
	if (user->scr != NULL) {
		for (i = 0; i < user->rows; i++)
			free_mem(user->scr[i]);
		free_mem(user->scr);
		free_mem(user->scr_tabs);
	}
	free_mem(user);
}

void
free_user(user)
	yuser *user;
{
	register yuser *u;

	/* remove him from the various blacklists */

	if (user == user_list)
		user_list = user->unext;
	else
		for (u = user_list; u; u = u->unext)
			if (u->unext == user) {
				u->unext = user->unext;
				break;
			}
	if (user == connect_list)
		connect_list = user->next;
	else
		for (u = connect_list; u; u = u->next)
			if (u->next == user) {
				u->next = user->next;
				break;
			}
	if (user == wait_list)
		wait_list = user->next;
	else
		for (u = wait_list; u; u = u->next)
			if (u->next == user) {
				u->next = user->next;
				break;
			}

	/* close him down */

	if (connect_list == NULL
	    && wait_list == NULL
	    && menu_ptr == NULL
	    && running_process == 0)
		bail(YTE_SUCCESS_PROMPT);

	close_term(user);
	clear_user(user);

	if (connect_list == NULL && wait_list != NULL)
		msg_term(me, "Waiting for connection...");
	user_winch = 1;
}

/*
 * Find a user by name/host/pid.  If name is NULL, then it is not checked. If
 * host_addr is (ylong)-1 then it is not checked.  If pid is (ylong)-1 then
 * it is not checked.
 */
yuser *
find_user(name, host_addr, pid)
	char *name;
	ylong host_addr, pid;
{
	register yuser *u;

	for (u = user_list; u; u = u->unext)
		if (name == NULL || strcmp(u->user_name, name) == 0)
			if (host_addr == (ylong) -1 || u->host_addr == host_addr)
				if (pid == (ylong) -1 || u->remote.pid == pid)
					return u;

	/* it could be _me_! */

	if (name == NULL || strcmp(me->user_name, name) == 0)
		if (host_addr == (ylong) -1 || me->host_addr == host_addr)
			if (pid == (ylong) -1 || me->remote.pid == pid)
				return me;

	/* nobody I know */

	return NULL;
}

/*
 * Clear out the user list, freeing memory as we go.
 */
void
free_users()
{
	yuser *u, *un;
	for (u = user_list; u != NULL;) {
		un = u->unext;
		clear_user(u);
		u = un;
	}
}
