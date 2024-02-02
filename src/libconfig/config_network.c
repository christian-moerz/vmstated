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

#include "config_network.h"

int
bpp_new_network_generic(struct bhyve_parameters_network *network,
			bhyve_parameters_network_t interface_type,
			bhyve_parameters_network_backend_t backend_type,
			uint16_t ident)
{
	if (!network) {
		errno = EINVAL;
		return -1;
	}

	network->network_type = interface_type;
	network->backend_type = backend_type;

	switch(backend_type) {
	case TYPE_NET_BACKEND_TAP:
		network->data.tap.tap_id = ident;
		break;
	case TYPE_NET_BACKEND_VMNET:
		network->data.vmnet.vmnet_id = ident;
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	return 0;
}


/*
 * set up new tap device
 */
int
bpp_new_network_tap(struct bhyve_parameters_network *network,
		    bhyve_parameters_network_t interface_type,
		    uint16_t tap_id)
{
	return bpp_new_network_generic(network, interface_type, TYPE_NET_BACKEND_TAP, tap_id);
}

/*
 * set new vmnet interface
 */
int
bpp_new_network_vmnet(struct bhyve_parameters_network *network,
		    bhyve_parameters_network_t interface_type,
		    uint16_t tap_id)
{
	return bpp_new_network_generic(network, interface_type, TYPE_NET_BACKEND_VMNET, tap_id);
}
