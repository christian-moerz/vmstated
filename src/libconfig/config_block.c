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

#include <string.h>

#include "config_block.h"

int
bpp_new_block_generic(struct bhyve_parameters_block *block, const char *storage_path,
		      bhyve_parameters_block_t block_type)
{
	if (!block || !storage_path) {
		errno = EINVAL;
		return -1;
	}

	bzero(block, sizeof(struct bhyve_parameters_block));
	block->block_type = block_type;
	switch (block_type) {
	case TYPE_BLOCK_VIRTIO:
		strncpy(block->data.virtio_blk.storage_path,
			storage_path,
			PATH_MAX);
		break;
	case TYPE_BLOCK_NVME:
		strncpy(block->data.nvme.storage_path,
			storage_path,
			PATH_MAX);
		break;
	case TYPE_BLOCK_AHCI_HD:
		strncpy(block->data.ahci_hd.storage_path,
			storage_path,
			PATH_MAX);
		break;
	default:
		errno = EINVAL;
		return -1;
	}

	return 0;
}

/*
 * set new virtio-blk device
 */
int
bpp_new_block_virtioblk(struct bhyve_parameters_block *block, const char *storage_path)
{
	return bpp_new_block_generic(block, storage_path, TYPE_BLOCK_VIRTIO);
}

/*
 * set new nvme device
 */
int
bpp_new_block_nvme(struct bhyve_parameters_block *block, const char *storage_path)
{
	return bpp_new_block_generic(block, storage_path, TYPE_BLOCK_NVME);
}

/*
 * set new ahci hd device
 */
int
bpp_new_block_ahcihd(struct bhyve_parameters_block *block, const char *storage_path)
{
	return bpp_new_block_generic(block, storage_path, TYPE_BLOCK_AHCI_HD);
}
