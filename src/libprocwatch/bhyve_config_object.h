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

#ifndef __BHYVE_CONFIG_OBJECT_H__
#define __BHYVE_CONFIG_OBJECT_H__

#include "bhyve_config.h"

/*
 * function table for configuration store
 */
struct bhyve_configuration_store_funcs {
	/* looks up a configuration by name */
	struct bhyve_configuration *(*getconfig_byname)(void *ctx, const char *name);
	/* get an iterator over configuration objects */
	struct bhyve_configuration_iterator *(*getiterator)(void *ctx);
};

/*
 * combined context pointer with functions
 */
struct bhyve_configuration_store_obj {
	/* context pointer that should also be provided when calling funcs */
	void *ctx;

	/*
	 * funcs is in a separate structure because there is a default
	 * implementation `bcs_funcs` that can be referenced
	 */
	struct bhyve_configuration_store_funcs *funcs;
};

struct bhyve_configuration_store_obj *bcsobj_frombcs(struct bhyve_configuration_store *bcs);
void bcsobj_free(struct bhyve_configuration_store_obj *bcso);

#endif /* __BHYVE_CONFIG_OBJECT_H__ */
