/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2023 Christian Moerz. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/queue.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct string_list_item {
	char *string;

	SLIST_ENTRY(string_list_item) entries;
};

struct string_list {
	SLIST_HEAD(, string_list_item) items;
};

/*
 * create new string list
 */
struct string_list *
sl_new()
{
	struct string_list *sl = 0;

	if (!(sl = malloc(sizeof(struct string_list))))
		return NULL;

	SLIST_INIT(&sl->items);

	return sl;	
}

/*
 * check if string has a string
 */
bool
sl_has(const struct string_list *sl, const char *string)
{
	if (!sl || !string) {
		errno = EINVAL;
		return false;
	}

	struct string_list_item *sli = 0;

	SLIST_FOREACH(sli, &sl->items, entries) {
		if (!strcmp(sli->string, string))
			return true;
	}

	return false;
}

/*
 * adds a string into the string list
 */
int
sl_add(struct string_list *sl, const char *string)
{
	struct string_list_item *sli = 0;

	if (!sl || !string) {
		errno = EINVAL;
		return -1;
	}

	if (!(sli = malloc(sizeof(struct string_list_item))))
		return -1;

	if (!(sli->string = strdup(string))) {
		free(sli);
		return -1;
	}

	SLIST_INSERT_HEAD(&sl->items, sli, entries);

	return 0;
}

/*
 * release previously allocated string list
 */
void
sl_free(struct string_list *sl)
{
	if (!sl) {
		errno = EINVAL;
		return;
	}

	struct string_list_item *sli = 0;
	
	while (!SLIST_EMPTY(&sl->items)) {
		sli = SLIST_FIRST(&sl->items);
		SLIST_REMOVE_HEAD(&sl->items, entries);
		free(sli->string);
		free(sli);
	}

	free(sl);
}
