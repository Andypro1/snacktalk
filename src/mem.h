/*
 * src/mem.h
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

#ifdef YTALK_DEBUG
typedef struct __mem_list {
	yaddr addr;
	size_t size;
	char *file;
	int line;
	struct __mem_list *prev;
	struct __mem_list *next;
} mem_list;

#define get_mem(size) (real_get_mem(size, __LINE__, __FILE__))
#define realloc_mem(addr, size) (real_realloc_mem(addr, size, __LINE__, __FILE__))
#define free_mem(addr) (real_free_mem(addr, __LINE__, __FILE__))

extern yaddr real_get_mem( /* size_t, __LINE__, __FILE__ */ );
extern yaddr real_realloc_mem( /* yaddr, size_t, __LINE__, __FILE__ */ );
extern void real_free_mem( /* yaddr, __LINE__, __FILE__ */ );
extern void change_area( /* mem_list *, yaddr, yaddr, size_t */ );
extern mem_list *find_area( /* mem_list *, yaddr */ );
extern void clear_all();
extern mem_list *add_area( /* mem_list *, yaddr, size_t, int, char * */ );
extern mem_list *del_area( /* mem_list *, mem_list * */ );
#else
extern yaddr get_mem( /* size_t */ );
extern yaddr realloc_mem( /* yaddr, size_t */ );
extern void free_mem( /* yaddr */ );
#endif				/* YTALK_DEBUG */
