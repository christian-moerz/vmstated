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

#include <sys/mman.h>
#include <sys/queue.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser_reader.h"

/*
 * a line that was not parsed or converted
 */
struct parser_reader_unparsed {
	char *line;
};

/*
 * encapsulates a reading parser for a bhyve config file
 */
struct parser_reader {
	char filename[PATH_MAX];
	int filefd;
	size_t filesize;
	void *memory;
};

/*
 * allocate a new parser reader
 */
struct parser_reader *
pr_new(const char *filename)
{
	struct parser_reader *pr = 0;
	off_t offset = 0;

	if (!filename) {
		errno = EINVAL;
		return NULL;
	}

	if (!(pr = malloc(sizeof(struct parser_reader))))
		return NULL;

	bzero(pr, sizeof(struct parser_reader));
	strncpy(pr->filename, filename, PATH_MAX);

	if ((pr->filefd = open(filename, O_RDONLY | O_CLOEXEC)) < 0) {
		free(pr->filename);
		free(pr);
		return NULL;
	}

	if ((offset = lseek(pr->filefd, 0, SEEK_END)) < 0) {
		close(pr->filefd);
		free(pr->filename);
		free(pr);
		return NULL;
	}
	pr->filesize = offset;

	pr->memory = mmap(NULL, pr->filesize, PROT_READ, 0,
			  pr->filefd, 0);
	if (!pr->memory) {
		close(pr->filefd);
		free(pr->filename);
		free(pr);
		return NULL;
	}
	
	return pr;
}

/*
 * release a previously allocated parser reader
 */
void
pr_free(struct parser_reader *pr)
{
	if (!pr) {
		errno = EINVAL;
		return;
	}

	munmap(pr->memory, pr->filesize);
	
	close(pr->filefd);
	free(pr->filename);
	free(pr);
}

/*
 * parses key and value
 */
int
pr_parsekeyval(struct parser_reader *pr, const char *key, const char *value)
{
	return 0;
}

/*
 * parses a single line
 */
int
pr_parseline(struct parser_reader *pr, const char *line)
{
	const char *equals = 0;
	char *key = 0, *value = 0;
	size_t line_len = 0;
	int result = 0;

	if (!pr || !line) {
		errno = EINVAL;
		return -1;
	}

	line_len = strlen(line);
	equals = strchr(line, '=');
	if (!equals) {
		errno = EFTYPE;
		return -1;
	}

	key = malloc(equals - line + 1);
	value = malloc(line + line_len - equals + 1);
	if (!key || !value) {
		free(key);
		free(value);
		return -1;
	}
	bzero(key, equals - line + 1);
	bzero(value, line + line_len - equals + 1);
	strncpy(key, line, equals - line);
	strncpy(value, equals + 1, line + line_len - equals - 1);

	result = pr_parsekeyval(pr, key, value);

	free(key);
	free(value);
	return result;
}

/*
 * parses the contents of the file
 */
int
pr_parsefile(struct parser_reader *pr)
{
	const char *start, *end, *next;
	char *linebuf = 0;
	size_t linebuf_len = 0;
	int result = 0;
	
	if (!pr) {
		errno = EINVAL;
		return -1;
	}

	start = end = next = 0;
	start = pr->memory;

	while(start) {
		end = strchr(start, '\n');
		if (!end) {
			/* last line without newline */

			start = 0;
			break;
		}

		if ((end - start) > linebuf_len) {
			linebuf_len = (end - start) * 2;
			if (linebuf)
				free(linebuf);
			
			linebuf = malloc(linebuf_len);
			if (!linebuf) {
				result = -1;
				break;
			}

			if (pr_parseline(pr, linebuf)) {
				result = -1;
				break;
			}
		}
		strncpy(linebuf, start, end-start);
		linebuf[end-start] = 0;
	}

	free(linebuf);

	return result;
}
