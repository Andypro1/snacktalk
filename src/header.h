/*
 * src/header.h
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

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>

#ifdef SNACKTALK_HPUX
# define _XOPEN_SOURCE_EXTENDED
# include <sys/socket.h>
# undef _XOPEN_SOURCE_EXTENDED
#else
# include <sys/socket.h>
#endif

#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* We may not have prototypes for these functions. */
#if defined(HAVE_PTSNAME) && defined(HAVE_GRANTPT) && defined(HAVE_UNLOCKPT)
extern int grantpt();
extern int unlockpt();
extern char *ptsname();
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#define VMAJOR	3		/* major version number */
#define VMINOR	3		/* minor version number */
#define VPATCH	0		/* patch level */

#define EIGHT_BIT_CLEAN		/* take this one out if you don't want it */

#ifdef EIGHT_BIT_CLEAN
# define is_printable(x)  ( (((x) >= ' ' && (x) <= '~') || \
			    (unsigned char)(x) >= 0xa0) \
			  && (x) != RUBDEF )
#else
# define is_printable(x) ( (x) >= ' ' && (x) <= '~' )
#endif

/* ---- YTalk protocols ---- */

/*
 * These protocol numbers are a MAJOR HACK designed to get around the fact
 * that old versions of ytalk didn't send any version information during
 * handshaking.  Nor did they bzero() the unused portions of the handshaking
 * structures.  Argh!  These two protocol numbers were very carefully
 * picked... do not add any others and expect them to work. Instead, use the
 * "vmajor" and "vminor" fields of the y_parm structure.
 */
#define YTP_OLD	20		/* YTalk versions before 3.0 */
#define YTP_NEW	27		/* YTalk versions 3.0 and up */

/* ---- types ---- */

typedef void *yaddr;		/* any pointer address */
typedef yaddr yterm;		/* terminal cookie */
typedef u_char ychar;		/* we use unsigned chars */

typedef u_int ylong;		/* this should work both on 32-bit and 64-bit
				 * machines  -Roger */

#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

typedef struct {
	u_char w_rows, w_cols;	/* window size FOR PROTOCOL YTP_OLD */
	char protocol;		/* ytalk protocol -- see above */
	char pad1;		/* zeroed out */
	short vmajor, vminor;	/* version numbers */
	u_short rows, cols;	/* his window size over there */
	u_short my_rows, my_cols;	/* my window size over there */
	ylong pid;		/* my process id */
	char pad[44];		/* zeroed out */
} y_parm;

#define MAXARG	100		/* max ESC sequence arg count */

typedef struct _yuser {
	struct _yuser *next;	/* next user in group lists */
	struct _yuser *unext;	/* next user in full user list */
	int fd;			/* file descriptor */
	int output_fd;		/* non-zero if output is going to a file */
	ylong flags;		/* active FL_* flags below */
	ychar edit[4];		/* edit characters */
	char crlf;		/* 1 if user wants CRLF newlines */
	u_short t_rows, t_cols;	/* his rows and cols on window over here */
	u_short rows, cols;	/* his active region rows and cols over here */
	y_parm remote;		/* remote parms */
	ychar **scr;		/* screen data */
	char *scr_tabs;		/* screen tab positions */
	char bump;		/* set if at far right */
	char onend;		/* set if stomping at far right */
	ychar old_rub;		/* my actual rub character */
	ychar key;		/* this user's ident letter for menus */
	int y, x;		/* current cursor position - y is the ROW position and x is the COLUMN (confusing convention) */
	int sy, sx;		/* saved cursor position */
	int sc_top, sc_bot;	/* scrolling region */
	char region_set;	/* set if using a screen region */
	char *full_name;	/* full name (up to 50 chars) */
	char *user_name;	/* user name */
	char *host_name;	/* host name */
	char *host_fqdn;	/* host fully qualified name */
	char *tty_name;		/* tty name */
	ylong host_addr;	/* host inet address */
	int daemon;		/* daemon type to use */
	ylong l_id, r_id;	/* local and remote talkd invite list index */
	ylong d_id;		/* talk daemon process id -- see socket.c */
	ylong last_invite;	/* timestamp of last invitation sent */
	struct sockaddr_in sock;/* communication socket */
	struct sockaddr_in orig_sock;	/* original socket -- another sick
					 * hack */
	/* out-of-band data */

	int dbuf_size;		/* current buffer size */
	ychar *dbuf, *dptr;	/* buffer base and current pointer */
	int drain;		/* remaining bytes to drain */
	void (*dfunc) ();	/* function to call with drained data */
	int got_oob;		/* got OOB flag */

	struct {
		char got_esc;	/* received an ESC */
		int av[MAXARG];	/* ESC sequence arguments */
		u_int ac;	/* ESC sequence arg count */
		char lparen;	/* lparen escape? */
		char hash;
	} vt;

	struct {
		char got_gt;	/* are we parsing a gtalk string? */
		char *buf;	/* gtalk string buffer */
		char type;	/* type of current gtalk string */
		int len;	/* length of current gtalk string */
		char *version;	/* user's GNUTALK version */
	} gt;

	/* anything below this is available for the terminal interface */

	yterm term;		/* terminal cookie */
} yuser;

#define FL_RAW		0x00000001L	/* raw input enabled */
#define FL_SCROLL	0x00000002L	/* scrolling enabled */
#define FL_WRAP		0x00000004L	/* word-wrap enabled */
#define FL_IMPORT	0x00000008L	/* auto-import enabled */
#define FL_INVITE	0x00000010L	/* auto-invite enabled */
#define FL_RING		0x00000020L	/* rering at all */
#define FL_CAPS		0x00000100L	/* want caps as answers */
#define FL_NOAUTO       0x00000200L	/* no auto-invite port */
#define FL_PROMPTRING	0x00000400L	/* prompt before reringing */
#define FL_BEEP		0x00000800L	/* allow snacktalk to beep? */
#define FL_IGNBRK	0x00001000L	/* don't die when ^C is pressed */
#define FL_PROMPTQUIT	0x00002000L	/* prompt before quitting */
#define FL_ESC_YN	0x00004000L	/* want ESC before y/n answers */
#define FL_LOCKED	0x40000000L	/* flags locked by other end */
#define FL_FORCEORDER	0x00008000L /* using the forceorder option for auto-ordering connected users */
#define FL_NOCOLOR	0x00010000L /* disable color */

/* ---- defines and short-cuts ---- */

#ifdef NOFILE
#define MAX_FILES	NOFILE	/* max open file descriptors */
#else
#define MAX_FILES	256	/* better to be too high than too low */
#endif
#define CLEAN_INTERVAL	16	/* seconds between calls to house_clean() */
#define MAXBUF		4096	/* buffer size for I/O operations */
#define MAXERR		132	/* error text buffer size */
#define MAXTEXT		50	/* text entry buffer */
#define MAXOPT		1024  /* max string buffer for options */

#define RUB	edit[0]
#define KILL	edit[1]
#define WORD	edit[2]
#define CLR	edit[3]
#define RUBDEF	0xfe

/* ---- Ytalk version 3.* out-of-band data ---- */

/* see comm.c for a description of Ytalk 3.* OOB protocol */

#define V3_OOB		0xfd	/* out-of-band marker -- see comm.c */
#define V3_MAXPACK	0xfc	/* max OOB packet size -- see comm.c */
#define V3_NAMELEN	16	/* max username length */
#define V3_HOSTLEN	64	/* max hostname length */

typedef struct {
	ychar code;		/* V3_EXPORT, V3_IMPORT, or V3_ACCEPT */
	char filler[3];
	ylong host_addr;	/* host address */
	ylong pid;		/* process id */
	char name[V3_NAMELEN];	/* user name */
	char host[V3_HOSTLEN];	/* host name */
} v3_pack;

#define V3_PACKLEN	sizeof(v3_pack)
#define V3_EXPORT	101	/* export a user */
#define V3_IMPORT	102	/* import a user */
#define V3_ACCEPT	103	/* accept a connection from a user */

typedef struct {
	ychar code;		/* V3_LOCKF or V3_UNLOCKF */
	char filler[3];
	ylong flags;		/* flags */
} v3_flags;

#define V3_FLAGSLEN	sizeof(v3_flags)
#define V3_LOCKF	111	/* lock my flags */
#define V3_UNLOCKF	112	/* unlock my flags */

typedef struct {
	ychar code;		/* V3_YOURWIN, V3_MYWIN, or V3_REGION */
	char filler[3];
	u_short rows, cols;	/* window size */
} v3_winch;

#define V3_WINCHLEN	sizeof(v3_winch)
#define V3_YOURWIN	121	/* your window size changed over here */
#define V3_MYWIN	122	/* my window size changed over here */
#define V3_REGION	123	/* my window region changed over here */

/* ---- Ytalk version 2.* out-of-band data ---- */

#define V2_NAMELEN	12
#define V2_HOSTLEN	64

typedef struct {
	ychar code;		/* one of the V2_?? codes below */
	char filler;
	char name[V2_NAMELEN];	/* user name */
	char host[V2_HOSTLEN];	/* user host */
} v2_pack;

#define V2_PACKLEN	sizeof(v2_pack)
#define V2_EXPORT	130	/* export a user */
#define V2_IMPORT	131	/* import a user */
#define V2_ACCEPT	132	/* accept a connection from a user */
#define V2_AUTO		133	/* accept auto invitation */

/* ---- exit codes ---- */

#define YTE_SUCCESS_PROMPT  -1	/* successful completion, prompt, return 0 */
#define YTE_SUCCESS	0	/* successful completion */
#define YTE_INIT	1	/* initialization error */
#define YTE_NO_MEM	2	/* out of memory */
#define YTE_SIGNAL	3	/* fatal signal received */
#define YTE_ERROR	4	/* unrecoverable error */

/* ---- global variables ---- */

#ifndef HAVE_STRERROR
extern char *sys_errlist[];	/* system errors */
#endif

extern yuser *me;		/* just lil' ol' me */
extern yuser *user_list;	/* full list of invited/connected users */
extern yuser *connect_list;	/* list of connected users */
extern yuser *wait_list;	/* list of invited users */
extern yuser *fd_to_user[MAX_FILES];	/* convert file descriptors to users */
extern yuser *key_to_user[128];	/* convert menu ident chars to users */
extern char errstr[MAXERR];	/* temporary string for errors */
extern ylong def_flags;		/* default FL_* flags */
extern char forceorder[MAXOPT];	/* 1k string buffer for the forceorder option */
extern int user_winch;		/* user window/status changed flag */

extern ychar *io_ptr;		/* user input pointer */
extern int io_len;		/* user input count */

extern int running_process;	/* flag: is process running? */
extern ylong myuid;		/* stores your uid */

/* ---- some machine compatibility definitions ---- */

extern int errno;

/* aliases -- added by Roger Espel Llima (borrowed from utalk) */

struct alias {
	char from[256], to[256];
	int type;
	struct alias *next;
};

#define ALIAS_ALL       0
#define ALIAS_BEFORE    1
#define ALIAS_AFTER     2


/* ---- global functions ---- */

extern void bail( /* int */ );	/* main.c */
extern char *str_copy( /* string */ );	/* main.c */
extern void show_error( /* str */ );	/* main.c */

extern void init_term();	/* term.c */
extern void set_terminal_size( /* fd, rows, cols */ );	/* term.c */
extern void set_terminal_flags( /* fd */ );	/* term.c */
extern int what_term();		/* term.c */
extern void end_term();		/* term.c */
extern int open_term( /* yuser, title */ );	/* term.c */
extern void close_term( /* yuser */ );	/* term.c */
extern void addch_term( /* yuser, ch */ );	/* term.c */
extern void move_term( /* yuser, y, x */ );	/* term.c */
extern void fill_term( /* yuser, y1, x1, y2, x2, c */ ); /* term.c */
extern void clreol_term( /* yuser */ );	/* term.c */
extern void clreos_term( /* yuser */ );	/* term.c */
extern void scroll_term( /* yuser */ );	/* term.c */
extern void rev_scroll_term( /* yuser */ );	/* term.c */
extern void flush_term( /* yuser */ );	/* term.c */
extern void rub_term( /* yuser */ );	/* term.c */
extern void word_term( /* yuser */ );	/* term.c */
extern void kill_term( /* yuser */ );	/* term.c */
extern void tab_term( /* yuser */ );	/* term.c */
extern void newline_term( /* yuser */ );	/* term.c */
extern void lf_term( /* yuser */ );	/* term.c */
extern void add_line_term( /* yuser, num */ );	/* term.c */
extern void del_line_term( /* yuser, num */ );	/* term.c */
extern void add_char_term( /* yuser, num */ );	/* term.c */
extern void del_char_term( /* yuser, num */ );	/* term.c */
extern void redraw_term( /* yuser, start_row */ );	/* term.c */
extern void keypad_term( /* yuser, int */ );	/* term.c */
extern void resize_win( /* yuser, h, w */ );	/* term.c */
extern void set_win_region( /* yuser, h, w */ );	/* term.c */
extern void end_win_region( /* yuser */ );	/* term.c */
extern void set_scroll_region( /* yuser, top, bottom */ );	/* term.c */
extern void msg_term( /* yuser, str */ );	/* term.c */
extern void spew_term( /* yuser, fd, rows, cols */ );	/* term.c */
extern int center( /* width, n */ );	/* term.c */
extern void redraw_all_terms();	/* term.c */
extern void set_raw_term();	/* term.c */
extern void set_cooked_term();	/* term.c */

extern void init_user();	/* user.c */
extern yuser *new_user( /* name, host, tty */ );	/* user.c */
extern void free_user( /* yuser */ );	/* user.c */
extern yuser *find_user( /* name, host_addr, pid */ );	/* user.c */
extern void generate_full_name( /* yuser */ );	/* user.c */
extern void free_users();	/* user.c */

extern void init_fd();		/* fd.c */
extern void add_fd( /* fd, func */ );	/* fd.c */
extern void remove_fd( /* fd */ );	/* fd.c */
extern int full_read( /* fd, buf, len */ );	/* fd.c */
extern void main_loop();	/* fd.c */
extern void input_loop();	/* fd.c */
extern void bail_loop();	/* fd.c */

extern yuser *invite( /* username, announce */ );	/* comm.c */
extern void house_clean();	/* comm.c */
extern void send_winch( /* yuser */ );	/* comm.c */
extern void send_region();	/* comm.c */
extern void send_end_region();	/* comm.c */
extern void send_users( /* buf, len */ );	/* comm.c */
extern void show_input( /* user, buf, len */ );	/* comm.c */
extern void my_input( /* buf, len */ );	/* comm.c */
extern void lock_flags( /* flags */ );	/* comm.c */
extern void unlock_flags();	/* comm.c */
extern void rering_all();	/* comm.c */

extern void init_socket();	/* socket.c */
extern void close_all();	/* socket.c */
extern int send_dgram( /* user, type */ );	/* socket.c */
extern int send_auto( /* type */ );	/* socket.c */
extern void kill_auto();	/* socket.c */
extern int newsock( /* yuser */ );	/* socket.c */
extern int connect_to( /* yuser */ );	/* socket.c */
extern ylong get_host_addr( /* hostname */ );	/* socket.c */
extern char *host_name( /* addr */ );	/* socket.c */
extern void readdress_host( /* from, to, on */ );	/* socket.c */

extern void read_ytalkrc();	/* rc.c */
extern char *resolve_alias( /* uh */ );	/* rc.c */

extern void execute( /* command */ );	/* exec.c */
extern void update_exec();	/* exec.c */
extern void kill_exec();	/* exec.c */
extern void winch_exec();	/* exec.c */

extern int getpty( /* char* */ );	/* getpty.c */

extern void vt100_process( /* yuser, char */ );	/* vt100.c */

/* EOF */
