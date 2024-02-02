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
#include <stdio.h>
#include <string.h>

#include "check_core.h"
#include "config_core.h"
#include "string_list.h"

/*
 * check that we have valid memory
 */
int
check_bpc_nonnull(struct bhyve_parameters_core *bpc)
{
	return NULL == bpc;
}

/*
 * check that a non-empty name is set
 */
int
check_bpc_name(struct bhyve_parameters_core *bpc)
{
	if (0 == *(bpc_get_vmname(bpc)))
		return -1;

	return 0;
}

/*
 * helper method counting the number of hostbridge
 */
int
check_bpc_counthostbridge(struct bhyve_parameters_core *bpc)
{
	struct bhyve_parameters_pcislot_iter *bppi = 0;
	const struct bhyve_parameters_pcislot *bpp = 0;
	size_t counter = 0;

	if (!(bppi = bpc_iter_pcislots(bpc)))
		return -1;

	while (bppi_next(bppi)) {
		bpp = bppi_item(bppi);
		if (TYPE_HOSTBRIDGE == bpp_get_slottype(bpp))
			counter++;
	}

	return counter;
}

/*
 * check that host bridge is at 0:0:0
 */
int
check_bpc_hostbridgeslot(struct bhyve_parameters_core *bpc)
{
	struct bhyve_parameters_pcislot_iter *bppi = 0;
	const struct bhyve_parameters_pcislot *bpp = 0;
	size_t counter = 0;
	uint8_t bus = 0;
	uint8_t pcislot = 0;
	uint8_t function = 0;
	int result = 0;

	if (!(bppi = bpc_iter_pcislots(bpc)))
		return -1;

	while (bppi_next(bppi)) {
		bpp = bppi_item(bppi);
		if (TYPE_HOSTBRIDGE == bpp_get_slottype(bpp)) {
			if (bpp_get_pciid(bpp, &bus, &pcislot, &function))
				return -1;

			if ((0 != bus) || (0 != pcislot) || (0 != function))
				return -1;
			
			break;
		}
	}

	return result;

}


/*
 * check that we don't have more than one host bridge
 */
int
check_bpc_singlehostbridge(struct bhyve_parameters_core *bpc)
{
	int counter = check_bpc_counthostbridge(bpc);

	if (counter > 1)
		return -1;

	return 0;
}

/*
 * check that we have at least one host bridge
 */
int
check_bpc_hostbridge(struct bhyve_parameters_core *bpc)
{
	int counter = check_bpc_counthostbridge(bpc);
	
	if (0 == counter)
		return -1;

	return 0;
}

/*
 * Check if there is two pci slots wanting the same idents
 */
int
check_bpc_pciidconflict(struct bhyve_parameters_core *bpc)
{
	struct bhyve_parameters_pcislot_iter *iter = 0;
	const struct bhyve_parameters_pcislot *bpp = 0;
	struct string_list *sl = 0;
	uint8_t bus = 0, pcislot = 0, function = 0;
	char ident[16] = {0};
	int result = 0;

	if (!bpc) {
		errno = EINVAL;
		return -1;
	}
	
	if (!(iter = bpc_iter_pcislots(bpc)))
		return -1;

	sl = sl_new();

	while(bppi_next(iter)) {
		bpp = bppi_item(iter);
		if (bpp_get_pciid(bpp, &bus, &pcislot, &function))
			break;

		snprintf(ident, 16, "%d:%d:%d", bus, pcislot, function);
		if (sl_has(sl, ident)) {
			result = -1;
			break;
		}

		if (sl_add(sl, ident)) {
			result = -1;
			break;
		}
	}

	bppi_free(iter);
	sl_free(sl);

	return result;
}
