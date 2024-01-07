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

#include <sys/nv.h>

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>

#include "bhyve_command.h"
#include "command_sender.h"

/*
 * directly send a bhyve_usercommand struct
 */
int
bcs_sendcmd_raw(struct bhyve_usercommand *bcmd, struct bhyve_command_sender *bcs)
{
	if (!bcmd || !bcs) {
		errno = EINVAL;
		return -1;
	}

	nvlist_t *nvl = 0;
	void *buffer = 0;
	size_t bufferlen = 0;
	int result = 0;

	if (!(nvl = nvlist_create(0)))
		return -1;

	do {
		if (bcmd_encodenvlist_command(bcmd, nvl)) {
			result = -1;
			break;
		}
		
		if (!(buffer = nvlist_pack(nvl, &bufferlen))) {
			result = -1;
			break;
		}

		if (bcs->send_dynamic) {
			result = bcs->send_dynamic(bcs->ctx, buffer, bufferlen,
						   bcmd->reply, bcmd->replylen,
						   &bcmd->blob, &bcmd->bloblen);
		} else 
			result = bcs->send_fixed(bcs->ctx, buffer, bufferlen,
						 bcmd->reply, bcmd->replylen);

		free(buffer);
	} while (0);

	nvlist_destroy(nvl);
	return result;
}

/*
 * send a command
 */
int
bcs_sendcmd(const char *cmd, const char *vmname, struct bhyve_command_sender *bcs)
{
	struct bhyve_usercommand bcmd = {0};

	if (!cmd || !vmname || !bcs) {
		errno = EINVAL;
		return -1;
	}

	/* these are actually const, but we are not changing them, so it should be fine */
	/* (probably last famous words...) */
	bcmd.cmd = (char *) cmd;
	bcmd.vmname = (char *) vmname;
	return bcs_sendcmd_raw(&bcmd, bcs);
}
