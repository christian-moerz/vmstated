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

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "../libsocket/socket_handle.h"
#include "../libprocwatch/bhyve_director.h"
#include "../libprocwatch/bhyve_messagesub_object.h"

int
vmsms_subscribe_ondata(void *obj, void *ctx,
		       int(*on_data)(void*, uid_t, pid_t, const char *, const char*, size_t,
				     struct bhyve_messagesub_replymgr*));
int
vmsms_onrecv_data(void *ctx, uid_t uid, pid_t pid, const char *cmd, const char *buffer, size_t bufferlen,
		  struct socket_reply_collector *src);

struct vmstated_message_subscriber {
	/* reference to bhyve_director */
	struct bhyve_director *bd;

	/* socket handle doing the message lifting */
	struct socket_handle *sh;
	
	/* generic part of message subscriber */
	struct bhyve_messagesub_obj bmo;

	int(*on_data)(void*, uid_t, pid_t, const char *, const char*, size_t,
		      struct bhyve_messagesub_replymgr*);
};

/*
 * get message subscriber object, usable for bhyve_director
 */
struct bhyve_messagesub_obj *
vmsms_getmessagesub_obj(struct vmstated_message_subscriber *vmsms)
{
	if (!vmsms) {
		errno = EINVAL;
		return NULL;
	}
	return &vmsms->bmo;
}

/*
 * instantiate a new message subscriber helper
 */
struct vmstated_message_subscriber *
vmsms_new(struct socket_handle *sh)
{
	struct vmstated_message_subscriber *vmsms = 0;

	if (!(vmsms = malloc(sizeof(struct vmstated_message_subscriber))))
		return NULL;

	bzero(vmsms, sizeof(struct vmstated_message_subscriber));
	vmsms->sh = sh;
	vmsms->bmo.obj = vmsms;
	vmsms->bmo.subscribe_ondata = vmsms_subscribe_ondata;

	/* we now subscribe vmsms to socket handle */
	if (sh_subscribe_ondata(sh, vmsms, vmsms_onrecv_data)) {
		free(vmsms);
		return NULL;
	}

	return vmsms;
}

/*
 * release the helper object
 */
void
vmsms_free(struct vmstated_message_subscriber *vmsms)
{
	free(vmsms);
}

/*
 * called when data is received
 */
int
vmsms_onrecv_data(void *ctx, uid_t uid, pid_t pid, const char *cmd, const char *buffer, size_t bufferlen,
		  struct socket_reply_collector *src)
{
	struct vmstated_message_subscriber *vmsms = ctx;
	
	struct bhyve_messagesub_replymgr bmr = {
		.ctx = src,
		.short_reply = (int (*)(void *, const char *)) src_short_reply,
		.reply = (int (*)(void *, const void *, size_t)) src_reply
	};

	return vmsms->on_data(vmsms->bd, uid, pid, cmd, buffer, bufferlen, &bmr);
}

/*
 * subscribe bhyve_director
 */
int
vmsms_subscribe_ondata(void *obj, void *ctx,
		       int(*on_data)(void*, uid_t, pid_t, const char *, const char*, size_t,
				     struct bhyve_messagesub_replymgr*)) {
	struct vmstated_message_subscriber *vms = obj;

	if (!vms) {
		errno = EINVAL;
		return -1;
	}

	vms->bd = ctx;
	vms->on_data = on_data;

	return 0;	
}
