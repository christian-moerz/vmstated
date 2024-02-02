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

#include "check_isabridge.h"

size_t
check_bpc_countisabridge(struct bhyve_parameters_core *bpc)
{
	struct bhyve_parameters_pcislot_iter *bppi = 0;
	const struct bhyve_parameters_pcislot *bpp = 0;
	size_t counter = 0;

	if (!(bppi = bpc_iter_pcislots(bpc)))
		return 0;

	while (bppi_next(bppi)) {
		bpp = bppi_item(bppi);
		if (TYPE_ISABRIDGE == bpp_get_slottype(bpp)) {
			counter++;
		}
	}

	return counter;
}

int
check_bpc_singleisabridge(struct bhyve_parameters_core *bpc)
{
	struct bhyve_parameters_pcislot_iter *bppi = 0;
	const struct bhyve_parameters_pcislot *bpp = 0;
	size_t counter = 0;

	if (!(bppi = bpc_iter_pcislots(bpc)))
		return -1;

	if (check_bpc_countisabridge(bpc) > 1)
		return -1;
	
	return 0;
	
}

/*
 * check if we have an isa bridge if a com port is enabled
 */
int
check_bpc_comwithisa(struct bhyve_parameters_core *bpc)
{
	bool any_enabled = false;
	size_t counter=0;

	for (counter = 0; counter < 4; counter++)
		any_enabled |= bpc_get_comport(bpc, counter)->enabled;

	if (!any_enabled)
		return 0;

	if (0 == check_bpc_countisabridge(bpc))
		return -1;
	
	return 0;
}

/*
 * check if we have a bootrom set
 */
int
check_bpc_bootrom(struct bhyve_parameters_core *bpc)
{
	if (!*bpc_get_bootrom(bpc)->bootrom)
		return -1;
	return 0;
}

/*
 * check if we have an isa bridge if we have a boot rom
 */
int
check_bpc_bootromwithisa(struct bhyve_parameters_core *bpc)
{
	/* if no bootrom, we're fine either way */
	if (check_bpc_bootrom(bpc))
		return 0;

	if (check_bpc_countisabridge(bpc) == 0)
		return -1;
	
	return 0;
}

/*
 * check whether isa bridge is on bus zero
 */
int
check_bpc_isabridgebus(struct bhyve_parameters_core *bpc)
{
	struct bhyve_parameters_pcislot_iter *bppi = 0;
	const struct bhyve_parameters_pcislot *bpp = 0;
	size_t counter = 0;
	uint8_t bus = 0, pcislot = 0, function = 0;

	if (!(bppi = bpc_iter_pcislots(bpc)))
		return 0;

	while (bppi_next(bppi)) {
		bpp = bppi_item(bppi);
		if (TYPE_ISABRIDGE == bpp_get_slottype(bpp)) {
			if (bpp_get_pciid(bpp, &bus, &pcislot, &function))
				return -1;

			if (0 != bus)
				return -1;
		}
	}

	return 0;
}
