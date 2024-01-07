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

#ifndef __BHYVE_CONFIG_CONSOLE_H__
#define __BHYVE_CONFIG_CONSOLE_H__

#include "../libcommand/nvlist_mapping.h"

struct bhyve_configuration_console;
struct bhyve_configuration_console_list;

size_t bcc_get_mapping_count();
struct nvlistitem_mapping * bcc_get_mapping();

struct bhyve_configuration_console *bcc_new(const char *name, bool enabled);
struct bhyve_configuration_console *bcc_new_empty();
void bcc_free(struct bhyve_configuration_console *bcc);

struct bhyve_configuration_console_list *bccl_new();
void bccl_free(struct bhyve_configuration_console_list *bccl);
size_t bccl_count(struct bhyve_configuration_console_list *bccl);
int
bccl_add(struct bhyve_configuration_console_list *bccl,
	 struct bhyve_configuration_console *bcc);

#endif /* __BHYVE_CONFIG_CONSOLE_H__ */
