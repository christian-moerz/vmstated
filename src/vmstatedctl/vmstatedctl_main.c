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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libcommand/bhyve_command.h"
#include "../libcommand/command_sender.h"
#include "../libcommand/vm_info.h"
#include "../libsocket/socket_connect.h"

#include "vmstatedctl_config.h"

/*
 * program options
 */
struct vmstatedctl_opts {
	char sockpath[PATH_MAX];     /* path to vmstated socket */
};

/*
 * directory structure for commands
 */
struct vmstatedctl_cmd {
	char *command;
	int (*func)(int, char**, struct bhyve_usercommand *);
	bool requires_vm_name;
};

/*
 * reply handler structure
 */
struct vmstatedctl_replyhandler {
	char *command;
	int (*func)(struct bhyve_usercommand *);
};

int cmd_start(int, char**, struct bhyve_usercommand *);
int cmd_stop(int, char**, struct bhyve_usercommand *);
int cmd_status(int argc, char **argv, struct bhyve_usercommand *buc);
int cmd_default_reply(struct bhyve_usercommand *buc);
int cmd_status_reply(struct bhyve_usercommand *buc);
int cmd_failreset(int argc, char **argv, struct bhyve_usercommand *buc);

/*
 * list of available commands
 */
struct vmstatedctl_cmd commands[] = {
	{
		.command = "start",
		.func = cmd_start,
		.requires_vm_name = true
	},
	{
		.command = "stop",
		.func = cmd_stop,
		.requires_vm_name = true
	},
	{
		.command = "status",
		.func = cmd_status,
		.requires_vm_name = false
	},
	{
		.command = "failreset",
		.func = cmd_failreset,
		.requires_vm_name = true
	}
};

struct vmstatedctl_replyhandler reply_handler[] = {
	{
		.command = "start",
		.func = cmd_default_reply
	},
	{
		.command = "stop",
		.func = cmd_default_reply
	},
	{
		.command = "status",
		.func = cmd_status_reply
	},
	{
		.command = "failreset",
		.func = cmd_default_reply
	}
};

/*
 * handle default replies
 */
int
cmd_default_reply(struct bhyve_usercommand *buc)
{
	if (!buc)
		return -1;
	
	/* print reply on screen */
	printf("%s\n", buc->reply);

	return 0;
}

void
cmd_printsep(char sepchar)
{
	size_t counter = 0;

	for (counter = 0; counter < 79; counter++)
		printf("%c", sepchar);
	printf("\n");
}

/*
 * handle status reply
 */
int
cmd_status_reply(struct bhyve_usercommand *buc)
{
	if (!buc)
		return -1;

	struct bhyve_vm_manager_info *bvmmi = 0;
	const struct bhyve_vm_info *bvi = 0;
	size_t vm_count = 0, counter = 0;

	if (bvmmi_decodebinary(buc->blob, buc->bloblen, &bvmmi)) {
		fprintf(stderr, "Failed to decode vm_info data");
		return -1;
	}

	vm_count = bvmmi_getvmcount(bvmmi);
	printf("vmstated running, managing %lu virtual machines\n",
	       vm_count);

	if (vm_count) {
		/* print header */
		printf("%-16s %-6s %-8s\n", "Name", "Status", "Owner");
	}
	cmd_printsep('=');

	for (counter = 0; counter < vm_count; counter++) {
		bvi = bvmmi_getvminfo_byidx(bvmmi, counter);
		
		printf("%-16s   %4s %-8s\n", bvmi_get_vmname(bvi),
		       bvmi_get_statestring(bvi),
		       bvmi_get_owner(bvi));
		       
	}

	return 0;
}

int
cmd_start(int argc, char **argv, struct bhyve_usercommand *buc)
{
	buc->cmd = strdup("startvm");
	buc->vmname = strdup(argv[0]);
	
	return 0;
}

int
cmd_stop(int argc, char **argv, struct bhyve_usercommand *buc)
{
	buc->cmd = strdup("stopvm");
	buc->vmname = strdup(argv[0]);

	return 0;
}

int
cmd_status(int argc, char **argv, struct bhyve_usercommand *buc)
{
	buc->cmd = strdup("status");
	buc->vmname = NULL;

	return 0;
}

int
cmd_failreset(int argc, char **argv, struct bhyve_usercommand *buc)
{
	buc->cmd = strdup("resetfail");
	buc->vmname = strdup(argv[0]);

	return 0;
}

/*
 * transmit data via socket function
 *
 * deprecated
 */
int
send_bhyvecmd_raw(struct socket_connection *sc,
		  const void *data,
		  size_t datalen,
		  char *retbuffer,
		  size_t retbuffer_len)
{
	return sc_sendrecv_len(sc, "BHYV", data, datalen, retbuffer, retbuffer_len);
}

int
send_bhyvecmd_dyanmic(void *ctx,
		      const void *data,
		      size_t datalen,
		      char *retbuffer,
		      size_t retbuffer_len,
		      void **blob_reply,
		      size_t *blob_reply_len)
{
	struct socket_connection *sc = ctx;
	return sc_sendrecv_dynamic(sc, "BHYV", data, datalen, retbuffer, retbuffer_len,
				   blob_reply, blob_reply_len);
}

/*
 * send user command over socket
 */
int
send_bhyvecmd(struct socket_connection *sc, struct bhyve_usercommand *buc)
{
	struct bhyve_command_sender bcs = {
		.ctx = sc,
		.send_fixed = (int (*)(void *, const void *, size_t, char *, size_t)) send_bhyvecmd_raw,
		.send_dynamic = send_bhyvecmd_dyanmic
	};

	/* bcs_snedmail_raw translates the bhvye_usercommand into a binary blob to send */
	return bcs_sendcmd_raw(buc, &bcs);
}

void
print_usage()
{
	printf("Usage: vmstatedctl [command] <vmname>\n\n");
	printf("Following vm commands are supported and require a vmname parameter:\n");
	printf(" - start\n - stop\n - failreset\n\n");
	printf("Following general commands are supported and do not require a vmname:\n");
	printf(" - status\n\n");
	exit(0);
}

/*
 * program entry point
 */
int
main(int argc, char **argv)
{
	struct bhyve_usercommand usrcmd = {0};
	struct vmstatedctl_opts opts = {0};
	struct socket_connection *sc = 0;
	void *blob = 0;
	size_t bloblen = 0;
	
	size_t total = sizeof(commands)/sizeof(struct vmstatedctl_cmd);
	size_t counter = 0;
	char **ptr = argv;
	
	int result = 0;
	nvlist_t *nvl = 0;
	void *buffer;
	size_t bufferlen = 0;
	size_t minargcount = 3;
	char *command_name = 0;
	bool found_command = false;

	/* set sane default options */
	strncpy(opts.sockpath, DEFAULTPATH_SOCKET, PATH_MAX);

	/* allocate reply buffer */
	if (!(usrcmd.reply = malloc(DEFAULT_BUFFERSIZE)))
		err(ENOMEM, "Failed to allocate reply buffer");
	usrcmd.replylen = DEFAULT_BUFFERSIZE;
	usrcmd.blob = NULL;
	usrcmd.bloblen = 0;

	if (!(nvl = nvlist_create(0))) {
		err(ENOMEM, "Failed to allocate nvlist");
	}

	if (argc < 2) {
		print_usage();
	}

	command_name = argv[1];

	for (counter = 0; counter < total; counter++) {
		/* find first argument in command directory */
		if (!strcmp(commands[counter].command, command_name)) {
			/* check if we have sufficient argumements */
			if (!commands[counter].requires_vm_name)
				minargcount = 2;
			
			if (argc < minargcount) {
				errno = EINVAL;
				err(EINVAL, "Missing arguments");
			}
			
			/* move two arguments forward
			   1. program name
			   2. command parameter we just processed
			*/
			ptr+=2;
			result = commands[counter].func(argc - 1,
							ptr,
							&usrcmd);


			found_command = true;
		}
	}

	if (!found_command) {
		/* unknown command */
		printf("Unknown command: %s\n", command_name);
		exit(1);
	}

	if (!result) {
		/* TODO consider replacing this with bcs_sendcmd instead */

		/* connect to socket */
		sc = sc_new(opts.sockpath);
		if (!sc) {
			err(errno, "Failed to create socket");
		}
		if (sc_connect(sc))
			err(errno, "Failed to connect to socket \"%s\"",
				opts.sockpath);

		if (send_bhyvecmd(sc, &usrcmd)) {
			err(errno, "Failed to transmit command");
		}

		/* handle reply data */
		total = sizeof(reply_handler) / sizeof(struct vmstatedctl_replyhandler);
		for (counter = 0; counter < total; counter++) {
			if (!strcmp(reply_handler[counter].command, command_name)) {
				if (reply_handler[counter].func) {
					result = reply_handler[counter].func(&usrcmd);
				}
			}
		}
		
		/* release memory */
		sc_free(sc);
	}

	nvlist_destroy(nvl);

	/* release any allocated memory */
	free(usrcmd.cmd);
	free(usrcmd.vmname);
	free(usrcmd.reply);
	
	return 0;
}
