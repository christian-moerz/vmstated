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

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "parser_offsets.h"

bool parser_bool_reg;
uint64_t parser_uint64_reg;

/*
 * filter a memory string into uint64_t of megabytes
 */
void *
po_filter_memory(void *data)
{
	const char *val = data;
	char *memstring = 0;
	size_t memstring_len = 0;
	char sizing = 0;
	uint16_t multiplier = 0;

	if (!data) {
		errno = EINVAL;
		return NULL;
	}

	memstring = strdup(val);
	memstring_len = strlen(memstring);
	sizing = toupper(memstring[memstring_len-1]);

	switch (sizing) {
	case 'G':
		multiplier = 1024;
		break;
	case 'M':
		multiplier = 1;
	default:
		errno = EDOM;
		return NULL;
	}

	memstring[memstring_len-1] = 0;
	errno = 0;
	
	parser_uint64_reg = atoll(memstring);
	if (errno)
		return NULL;

	return &parser_uint64_reg;
}

/*
 * filter a boolean string into a boolean
 */
void *
po_filter_bool(void *data)
{
	const char *val = data;
	
	if (!data) {
		errno = EINVAL;
		return NULL;
	}

	if (!*val) {
		parser_bool_reg = false;
		return &parser_bool_reg;
	}

	parser_bool_reg = !strcmp("true", val);
	if (parser_bool_reg)
		return &parser_bool_reg;

	if (!strcmp("false", val)) {
		parser_bool_reg = false;
		return &parser_bool_reg;
	}

	errno = EDOM;
	return NULL;
}
