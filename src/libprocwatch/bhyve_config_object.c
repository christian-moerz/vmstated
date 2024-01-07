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

#include "bhyve_config_object.h"
#include "bhyve_config.h"

#include <stdlib.h>

/*
 * the default function table to use
 */
struct bhyve_configuration_store_funcs bcs_funcs = {
	.getconfig_byname = (void *)bcs_getconfig_byname,
	.getiterator = (void *)bcs_iterate_configs
};

/*
 * create a new object
 */
struct bhyve_configuration_store_obj *
bcsobj_frombcs(struct bhyve_configuration_store *bcs) {
	struct bhyve_configuration_store_obj *bcso = 0;

	bcso = malloc(sizeof(struct bhyve_configuration_store_obj));
	if (!bcso)
		return NULL;

	bcso->ctx = bcs;
	bcso->funcs = &bcs_funcs;

	return bcso;
}

/*
 * release a previously allocated object
 */
void
bcsobj_free(struct bhyve_configuration_store_obj *bcso)
{
	free(bcso);
}
