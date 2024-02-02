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

#ifndef __CONFIG_NETWORK_H__
#define __CONFIG_NETWORK_H__

#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>

typedef enum {
	TYPE_NET_VIRTIO = 0,
	TYPE_NET_E1000 = 1
} bhyve_parameters_network_t;

typedef enum {
	TYPE_NET_BACKEND_TAP = 0,
	TYPE_NET_BACKEND_VMNET = 1,
	TYPE_NET_BACKEND_NETGRAPH = 2
} bhyve_parameters_network_backend_t;

struct bhyve_parameters_network_tap {
	uint16_t tap_id;
};

struct bhyve_parameters_network_vmnet {
	uint16_t vmnet_id;
};

struct bhyve_parameters_network_netgraph {
	char path[PATH_MAX];
	char peerhook[PATH_MAX];
	char socket[PATH_MAX];
	char hook[PATH_MAX];
};

/*
 * represents a network interface
 */
struct bhyve_parameters_network {
	bhyve_parameters_network_t network_type;
	bhyve_parameters_network_backend_t backend_type;
	
	unsigned char mac_address[6];
	uint8_t mtu;

	union {
		struct bhyve_parameters_network_tap tap;
		struct bhyve_parameters_network_vmnet vmnet;
		struct bhyve_parameters_network_netgraph netgraph;
	} data;
};


int bpp_new_network_tap(struct bhyve_parameters_network *network,
			bhyve_parameters_network_t interface_type,
			uint16_t tap_id);
int bpp_new_network_vmnet(struct bhyve_parameters_network *network,
			  bhyve_parameters_network_t interface_type,
			  uint16_t tap_id);

#endif /* __CONFIG_NETWORK_H__ */
