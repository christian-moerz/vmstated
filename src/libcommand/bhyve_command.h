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

#ifndef __LIBCMD_BHYVE_COMMAND_H__
#define __LIBCMD_BHYVE_COMMAND_H__

#include <sys/nv.h>

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include "nvlist_mapping.h"

typedef enum {
	OK                 = 0,
	UNAUTHORIZED       = 1,
	NO_SUCH_VM         = 2,
	VM_ALREADY_RUNNING = 3,
	VM_IN_FAILURESTATE = 4
} bhyve_command_result_t;

/*
 * represents a bhyve command to be created, sent, parsed
 */
struct bhyve_usercommand {
	char *cmd;               /* command name */
	char *vmname;            /* name of vm to work on */

	int result;       /* return code to send back / received */
	char *reply;      /* reply data to send back / received */
	size_t replylen;  /* size of reply buffer */

	void *blob;       /* blob reply data */
	size_t bloblen;   /* blob length */
};

int bcmd_parse_nvlistcmd(const char *buffer, size_t bufferlen, struct bhyve_usercommand *bc);
void bcmd_free(struct bhyve_usercommand *bcf);
void bcmd_freestatic(struct bhyve_usercommand *bcmd);
int bcmd_encodenvlist(struct nvlistitem_mapping *mapping, size_t mapping_count,
		      void *buffer, nvlist_t *nvl);
int bcmd_encodenvlist_command(struct bhyve_usercommand *buffer, nvlist_t *nvl);
int bcmd_decodenvlist(struct nvlistitem_mapping *mapping, size_t mapping_count,
		      void *buffer, nvlist_t *nvl);

#endif /* __LIBCMD_BHYVE_COMMAND_H__ */
