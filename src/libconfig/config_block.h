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

#ifndef __CONFIG_BLOCK_H__
#define __CONFIG_BLOCK_H__

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <unistd.h>

#include "config_core.h"

/*
 * available block storage types
 */
typedef enum {
	TYPE_BLOCK_VIRTIO = 0,
	TYPE_BLOCK_VIRTIO_SCSI = 1,
	TYPE_BLOCK_VIRTIO_9P = 2,
	TYPE_BLOCK_AHCI_HD = 3,
	TYPE_BLOCK_NVME = 4
} bhyve_parameters_block_t;

/*
 * virtio-blk configuration
 */
struct bhyve_parameters_block_virtioblk {
	char storage_path[PATH_MAX];

	bool nocache;
	bool direct;
	bool read_only;
	uint32_t logical;
	uint32_t physical;
	bool nodelete;
};

/*
 * nvme configuration
 */
struct bhyve_parameters_block_nvme {
	char storage_path[PATH_MAX];

	uint16_t max_queues;
	uint16_t queue_size;
	uint16_t io_slots;
	uint32_t sector_size;
	char serial_number[21];
	char eui64[9];
	enum {
		NVME_DSA_AUTO = 0,
		NVME_DSA_ENABLE = 1,
		NVME_DSA_DISABLE = 2
	} dsm;
};

struct bhyve_parameters_block_scsi {
	uint16_t pp;
	uint16_t vp;
	char iid[BPC_PARM_MAX];
};

struct bhyve_parameters_block_ahcihd {
	char storage_path[PATH_MAX];
	
	uint16_t nmrr;
	char serial_number[21];
	char revision[9];
	char model[41];
};

/*
 * defines a block device and its controller
 */
struct bhyve_parameters_block {
	bhyve_parameters_block_t block_type;

	union {
		struct bhyve_parameters_block_virtioblk virtio_blk;
		struct bhyve_parameters_block_nvme nvme;
		struct bhyve_parameters_block_scsi scsi;
		struct bhyve_parameters_block_ahcihd ahci_hd;
	} data;
};

int bpp_new_block_virtioblk(struct bhyve_parameters_block *block, const char *storage_path);
int bpp_new_block_nvme(struct bhyve_parameters_block *block, const char *storage_path);
int bpp_new_block_ahcihd(struct bhyve_parameters_block *block, const char *storage_path);

#endif /* __CONFIG_BLOCK_H__ */
