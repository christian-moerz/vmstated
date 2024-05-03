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
#include <string.h>
#include <syslog.h>

#include "bhyve_uclparser.h"
#include "daemon_config.h"

#include "../libcommand/nvlist_mapping.h"
#include "../libutils/bhyve_utils.h"

/*
 * daemon configuration settings
 */
struct daemon_config {
	size_t tapid_min;
	size_t tapid_max;
	size_t nmdmid_min;
	size_t nmdmid_max;
};

/*
 * maps daemon_config variables for UCL parsing
 */
struct nvlistitem_mapping dconf_nvlist2config[] = {
	{
		.offset = offsetof(struct daemon_config, tapid_min),
		.value_type = UINT32,
		.size = sizeof(uint32_t),
		.varname = "tap_min"
	},
	{
		.offset = offsetof(struct daemon_config, tapid_max),
		.value_type = UINT32,
		.size = sizeof(uint32_t),
		.varname = "tap_max"
	},
	{
		.offset = offsetof(struct daemon_config, nmdmid_min),
		.value_type = UINT32,
		.size = sizeof(uint32_t),
		.varname = "nmdm_min"
	},
	{
		.offset = offsetof(struct daemon_config, nmdmid_max),
		.value_type = UINT32,
		.size = sizeof(uint32_t),
		.varname = "nmdm_max"
	}
};

/* define lookup method */
nvlistitem_mapping_lookupfunc(dconf_nvlist2config, dconf_findmapping);

/*
 * parse daemon_config from UCL config object
 */
int
dconf_parsefromucl(struct daemon_config *dc, const ucl_object_t *confobj)
{
	return bup_generic_parsefromucl(dc, confobj,
				dconf_nvlist2config,
				sizeof(dconf_nvlist2config) / sizeof(struct nvlistitem_mapping),
				NULL, 0);
}

/*
 * parses UCL config file into daemon_config structure
 */
int
dconf_parseucl(struct daemon_config *dc, const char *configfile)
{
	struct ucl_parser *uclp = 0;
	ucl_object_t *root = 0;
	const ucl_object_t *obj = NULL;
	ucl_object_iter_t it = NULL, it_obj = NULL;
	const ucl_object_t *cur, *tmp;
	const char *configname = NULL;
	size_t pathlen = strlen(configfile);
	int retcode = 0;

	syslog(LOG_INFO, "Parsing UCL daemon config file \"%s\"", configfile);

	uclp = ucl_parser_new(UCL_PARSER_KEY_LOWERCASE);
	if (!uclp) {
		syslog(LOG_ERR, "Failed to construct daemon UCL parser");
		return -1;
	}

	if (!ucl_parser_add_file(uclp, configfile)) {
		if (ucl_parser_get_error(uclp)) {
			syslog(LOG_ERR, "%s",
			       ucl_parser_get_error(uclp));
		}
		syslog(LOG_ERR, "Failed to parse \"%s\" daemon config file", configfile);
		return -1;
	}

	root = ucl_parser_get_object(uclp);
	do {
		if (!root) {
			syslog(LOG_WARNING, "No config node found in \"%s\"", configfile);
			retcode = -1;
			break;
		}

		while ((cur = ucl_iterate_object(root, &it, true))) {
			configname = ucl_object_key(cur);

			/* we expect vmstated config name */
			if (strcmp(configname, "vmstated"))
				continue;
			
			bzero(dc, sizeof(struct daemon_config));

			retcode = dconf_parsefromucl(dc, cur);
			if (retcode)
				syslog(LOG_WARNING, "Failed to parse \"%s\"", configfile);
		}
	} while (0);

	syslog(LOG_INFO, "Parsing completed for \"%s\"", configfile);

	if (ucl_parser_get_error(uclp)) {
		syslog(LOG_ERR, "%s",
		       ucl_parser_get_error(uclp));
	}

	ucl_parser_free(uclp);
	ucl_object_unref(root);

	return retcode;
}

/*
 * instantiate a new daemon_config structure
 */
struct daemon_config *
dconf_new()
{
	struct daemon_config *dc = 0;
	
	dc = malloc(sizeof(struct daemon_config));
	if (!dc)
		return NULL;

	bzero(dc, sizeof(struct daemon_config));
	return dc;
}

/*
 * release memory for previously allocated
 * daemon_config structure
 */
void dconf_free(struct daemon_config *dc)
{
	free(dc);
}

CREATE_GETTERFUNC_UINT32(daemon_config, dconf, tapid_min);
CREATE_GETTERFUNC_UINT32(daemon_config, dconf, tapid_max);
CREATE_GETTERFUNC_UINT32(daemon_config, dconf, nmdmid_min);
CREATE_GETTERFUNC_UINT32(daemon_config, dconf, nmdmid_max);
