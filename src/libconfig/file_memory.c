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

#include "file_memory.h"

/*
 * Copies contents of a file into memory buffer This is used for
 * reading a bhyve config file to combine it with generated config
 * output.
 */
struct file_memory {
	char filename[PATH_MAX];
	int filefd;
	size_t filesize;
	char *memory;
};

struct file_memory *
fm_new(const char *filename)
{
	struct file_memory *fm = 0;
	off_t offset = 0;

	if (!filename) {
		errno = EINVAL;
		return NULL;
	}

	if (!(fm = malloc(sizeof(struct file_memory))))
		return NULL;

	bzero(fm, sizeof(struct file_memory));
	strncpy(fm->filename, filename, PATH_MAX);

	if ((fm->filefd = open(filename, O_RDONLY)) < 0) {
		free(fm->filename);
		free(fm);
		return NULL;
	}

	if ((offset = lseek(fm->filefd, 0, SEEK_END)) < 0) {
		close(fm->filefd);
		free(fm->filename);
		free(fm);
		return NULL;
	}
	fm->filesize = offset;

	fm->memory = mmap(NULL, fm->filesize, PROT_READ, MAP_PRIVATE,
			  fm->filefd, 0);
	if (MAP_FAILED == fm->memory) {
		close(fm->filefd);
		free(fm->filename);
		free(fm);
		return NULL;
	}
	
	return fm;
}

/*
 * get file contents
 */
const char *
fm_get_memory(const struct file_memory *fm)
{
	return fm->memory;
}

/*
 * free previously allocated file memory
 */
void
fm_free(struct file_memory *fm)
{
	if (!fm)
		return;

	munmap(fm->memory, fm->filesize);

	close(fm->filefd);
	free(fm);
}
