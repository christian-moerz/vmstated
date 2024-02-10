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

#include <private/ucl/ucl.h>

#include <sys/mman.h>
#include <sys/nv.h>
#include <sys/queue.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "bhyve_config.h"
#include "bhyve_config_console.h"
#include "bhyve_uclparser.h"
#include "bhyve_uclparser_funcs.h"

#include "../libcommand/nvlist_mapping.h"
#include "../libcommand/bhyve_command.h"
#include "../libutils/bhyve_utils.h"

/*
 * represents a bhyve configuration to be handled by vmstated. This
 * focuses on vmstated related information and does not include any
 * bhyve configuration details but is more of a meta data structure.
 *
 * Handling and working bhyve configuration data is managed by
 * structures in libconfig.
 */
struct bhyve_configuration {
	char name[PATH_MAX];
	char configfile[PATH_MAX];
	char scriptpath[PATH_MAX];
	char *os;
	char *osversion;
	char *owner;
	char *group;
	char *description;

	/* number of maximum restarts within maxrestarttime */
	uint32_t maxrestart;
	/* number of seconds to allow maxrestarts to occur before failure */
	time_t maxrestarttime;

	/* console configuration options */
	struct bhyve_configuration_console_list *consoles;

	bool autostart;
	/* a bootrom file */
	char *bootrom;

	/* vm memory setting in megabytes */
	uint32_t memory;
	/* cpu configuration */
	uint16_t numcpus;
	uint16_t sockets;
	uint16_t cores;

	/* transient cached values */
	uint32_t *uid;
	uint32_t *gid;
	/* stores the filename, which created this instance */
	char *backing_filepath;
	/* stores the filename of the generated config */
	char *generated_config;

	/* core config variables */
	bool generate_acpi_tables;
	bool wire_memory;
	bool vmexit_on_halt;
	
	LIST_ENTRY(bhyve_configuration) entries;
};

/*
 * this maps configuration variable names in our UCL config files into
 * the memory offsets in struct bhyve_configuration
 */
struct nvlistitem_mapping bc_nvlist2config[] = {
	{
		.offset = offsetof(struct bhyve_configuration, name),
		.value_type = FIXEDSTRING,
		.size = PATH_MAX,
		.varname = "name"
	},
	{
		.offset = offsetof(struct bhyve_configuration, configfile),
		.value_type = FIXEDSTRING,
		.size = PATH_MAX,
		.varname = "configfile"
	},
	{
		.offset = offsetof(struct bhyve_configuration, scriptpath),
		.value_type = FIXEDSTRING,
		.size = PATH_MAX,
		.varname = "scriptpath"
	},
	{
		.offset = offsetof(struct bhyve_configuration, os),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char *),
		.varname = "os"
	},
	{
		.offset = offsetof(struct bhyve_configuration, osversion),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char *),
		.varname = "osversion"
	},
	{
		.offset = offsetof(struct bhyve_configuration, owner),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char *),
		.varname = "owner"
	},
	{
		.offset = offsetof(struct bhyve_configuration, group),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char *),
		.varname = "group"
	},
	{
		.offset = offsetof(struct bhyve_configuration, description),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char *),
		.varname = "description"
	},
	{
		.offset = offsetof(struct bhyve_configuration, maxrestart),
		.value_type = UINT32,
		.size = sizeof(uint32_t),
		.varname = "maxrestart"
	},
	{
		.offset = offsetof(struct bhyve_configuration, maxrestarttime),
		.value_type = UINT64,
		.size = sizeof(time_t),
		.varname = "maxrestarttime"
	},
	{ /* TODO implement feature */
		.offset = offsetof(struct bhyve_configuration, autostart),
		.value_type = BOOLEAN,
		.size = sizeof(bool),
		.varname = "autostart"
	},
	{
		.offset = offsetof(struct bhyve_configuration, bootrom),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char *),
		.varname = "bootrom"
	},
	{
		.offset = offsetof(struct bhyve_configuration, memory),
		.value_type = UINT32,
		.size = sizeof(uint32_t),
		.varname = "memory"
	},
	{
		.offset = offsetof(struct bhyve_configuration, numcpus),
		.value_type = UINT16,
		.size = sizeof(uint16_t),
		.varname = "numcpus"
	},
	{
		.offset = offsetof(struct bhyve_configuration, sockets),
		.value_type = UINT16,
		.size = sizeof(uint16_t),
		.varname = "sockets"
	},
	{
		.offset = offsetof(struct bhyve_configuration, cores),
		.value_type = DYNAMICSTRING,
		.size = sizeof(char *),
		.varname = "cores"
	},
	{
		.offset = offsetof(struct bhyve_configuration, generate_acpi_tables),
		.value_type = BOOLEAN,
		.size = sizeof(bool),
		.varname = "generate_acpi_tables"
	},
	{
		.offset = offsetof(struct bhyve_configuration, wire_memory),
		.value_type = BOOLEAN,
		.size = sizeof(bool),
		.varname = "wire_memory"
	},
	{
		.offset = offsetof(struct bhyve_configuration, vmexit_on_halt),
		.value_type = BOOLEAN,
		.size = sizeof(bool),
		.varname = "vmexit_on_halt"
	}
};

/*
 * definition of sub-parsing methods
 */
struct bhyve_uclparser_item bc_ucl2subparser_items[] = {
	{
		.varname = "consoles",
		.offset = offsetof(struct bhyve_configuration, consoles),
		.delegate_func = buf_parse_console_list
	}
};

/*
 * iterator for walking through available configurations
 */
struct bhyve_configuration_iterator {
	struct bhyve_configuration *current;
	struct bhyve_configuration_store *bcs;
};

/*
 * keeps available configurations together
 */
struct bhyve_configuration_store {
	char searchpath[PATH_MAX];
	LIST_HEAD(,bhyve_configuration) configs;
};

/*
 * get the configuration from the iterator
 */
const struct bhyve_configuration *
bci_getconfig(struct bhyve_configuration_iterator *bci)
{
	if (!bci)
		return NULL;

	return bci->current;
}

/*
 * move iterator to next item
 *
 * returns TRUE if successful; FALSE if no more items.
 */
bool
bci_next(struct bhyve_configuration_iterator *bci)
{
	if (!bci)
		return 0;

	if (!bci->current) {
		if (LIST_EMPTY(&bci->bcs->configs))
			return false;
		
		bci->current = LIST_FIRST(&bci->bcs->configs);
		return true;
	}
	
	struct bhyve_configuration *bc = LIST_NEXT(bci->current, entries);
	bci->current = bc;

	return (bci->current != NULL);
}

/*
 * release the perviously allocated iterator
 */
void
bci_free(struct bhyve_configuration_iterator *bci)
{
	if (!bci)
		return;

	free(bci);
}

/*
 * get an iterator walking over available config items
 */
struct bhyve_configuration_iterator *
bcs_iterate_configs(struct bhyve_configuration_store *bcs)
{
	if (!bcs)
		return NULL;

	struct bhyve_configuration_iterator *bci = malloc(sizeof(struct bhyve_configuration_iterator));
	if (!bci)
		return NULL;

	bci->current = NULL;
	bci->bcs = bcs;

	return bci;	
}

struct bhyve_configuration *
bc_new(const char *name,
       const char *configfile,
       const char *os,
       const char *osversion,
       const char *owner,
       const char *group)
{
	if (!name || !configfile) {
		errno = EINVAL;
		return NULL;
	}

	struct bhyve_configuration *bc = malloc(sizeof(struct bhyve_configuration));
	size_t size = 0;

	bzero(bc, sizeof(struct bhyve_configuration));

	/* set default values for restart counting */
	bc->maxrestart = 3;
	bc->maxrestarttime = 30;

	if (!bc)
		return NULL;

	strncpy(bc->name, name, PATH_MAX);
	strncpy(bc->configfile, configfile, PATH_MAX);
	if (os) {
		size = strlen(os);
		bc->os = malloc(size+1);
		strncpy(bc->os, os, size+1);
	}

	if (osversion) {
		size = strlen(osversion);
		bc->osversion = malloc(size+1);
		strncpy(bc->osversion, osversion, size+1);
	}

	if (owner) {
		size = strlen(owner);
		bc->owner = malloc(size+1);
		strncpy(bc->owner, owner, size+1);
	}

	if (group) {
		size = strlen(group);
		bc->group = malloc(size+1);
		strncpy(bc->group, group, size+1);
	}

	return bc;
}

/* INTERNAL API */
int
bcmd_encodenvlist(struct nvlistitem_mapping *mapping, size_t mapping_count,
		  void *buffer, nvlist_t *nvl);

/*
 * convert data in bhyve_configuration structure into nvlist format
 */
int
bc_tonvlist(struct bhyve_configuration *bc, nvlist_t *nvl)
{
	return bcmd_encodenvlist(bc_nvlist2config,
				 sizeof(bc_nvlist2config) / sizeof(struct nvlistitem_mapping),
				 bc, nvl);
}

/*
 * attempt looking up mapping via variable name
 *
 * returns NULL if nothing matches
 */
struct nvlistitem_mapping *
bc_findmapping(const char *varname)
{
	if (!varname)
		return NULL;

	size_t configcount = sizeof(bc_nvlist2config) / sizeof(struct nvlistitem_mapping);
	size_t counter = 0;

	for (counter = 0; counter < configcount; counter++) {
		if (!strcmp(bc_nvlist2config[counter].varname, varname))
			return &bc_nvlist2config[counter];
	}

	return NULL;
}

/*
 * read configuration from ucl object
 */
int
bc_parsefromucl(struct bhyve_configuration *bc, const ucl_object_t *confobj)
{
	return bup_parsefromucl(bc, confobj,
				bc_nvlist2config,
				sizeof(bc_nvlist2config)/sizeof(struct nvlistitem_mapping),
				bc_ucl2subparser_items,
				sizeof(bc_ucl2subparser_items)/sizeof(struct bhyve_uclparser_item));
}

/*
 * read content from nvlist into bhyve_configuration structure
 */
int
bc_fromnvlist(struct bhyve_configuration *bc, nvlist_t *nvl)
{
	if (!bc || !nvl) {
		errno = EINVAL;
		return -1;
	}

	return bcmd_decodenvlist(bc_nvlist2config,
				 sizeof(bc_nvlist2config) /
				 sizeof(struct nvlistitem_mapping),
				 bc, nvl);
}

/*
 * get owner's user id
 */
int
bc_getuid(struct bhyve_configuration *bc)
{
	if (bc->uid)
		return *bc->uid;

	if (!bc->owner)
		return -1;

	bc->uid = malloc(sizeof(uint32_t));
	if (!bc->uid)
		return -1;

	struct passwd *pwd = getpwnam(bc->owner);
	if (pwd) {
		*bc->uid = pwd->pw_uid;
	} else {
		free(bc->uid);
		bc->uid = NULL;
		return -1;
	}

	return *bc->uid;
}

/*
 * get group's group id
 */
int
bc_getgid(struct bhyve_configuration *bc)
{
	if (bc->gid)
		return *bc->gid;

	if (!bc->group)
		return -1;

	bc->gid = malloc(sizeof(uint32_t));
	if (!bc->gid)
		return -1;

	struct group *gr = getgrnam(bc->group);
	if (gr) {
		*bc->gid = gr->gr_gid;
	} else {
		free(bc->gid);
		bc->gid = NULL;
		return -1;
	}

	return *bc->gid;
}

/*
 * release a previously allocated bhyve configuration
 */
void
bc_free(struct bhyve_configuration *bc)
{
	if (!bc)
		return;

	free(bc->os);
	free(bc->osversion);

	if (bc->consoles)
		bccl_free(bc->consoles);

	free(bc->uid);
	free(bc->gid);

	free(bc->backing_filepath);
	free(bc->generated_config);
	
	free(bc);
}

/*
 * look up config by name
 *
 * returns NULL if nothing matches.
 */
struct bhyve_configuration *
bcs_getconfig_byname(struct bhyve_configuration_store *bcs, const char *name)
{
	if (!name)
		return NULL;

	struct bhyve_configuration *bc = 0;

	LIST_FOREACH(bc, &bcs->configs, entries) {
		if (!strcmp(bc->name, name))
			return bc;
	}

	return NULL;
}

/*
 * initialize a new configuration store
 *
 * returns NULL on error.
 */
struct bhyve_configuration_store *
bcs_new(const char *searchpath)
{
	if (!searchpath)
		return NULL;

	struct bhyve_configuration_store *bcs = malloc(sizeof(struct bhyve_configuration_store));
	if (!bcs)
		return NULL;

	strncpy(bcs->searchpath, searchpath, PATH_MAX);

	LIST_INIT(&bcs->configs);

	return bcs;
}

/*
 * parses a config file into the configuration store; since a config file may contain
 * multiple configurations, we are not returning a single object but immediately put
 * results into the bhyve_configuration_store bcs
 *
 * returns 0 on success.
 */
int
bcs_parseucl(struct bhyve_configuration_store *bcs, const char *configfile)
{
	struct ucl_parser *uclp = 0;
	ucl_object_t *root = 0;
	const ucl_object_t *obj = NULL;
	ucl_object_iter_t it = NULL, it_obj = NULL;
	const ucl_object_t *cur, *tmp;
	const char *configname = NULL;
	struct bhyve_configuration *bc;
	size_t pathlen = strlen(configfile);

	syslog(LOG_INFO, "Parsing UCL config file \"%s\"", configfile);

	uclp = ucl_parser_new(UCL_PARSER_KEY_LOWERCASE);
	if (!uclp) {
		syslog(LOG_ERR, "Failed to construct UCL parser");
		return -1;
	}

	if (!ucl_parser_add_file(uclp, configfile)) {
		if (ucl_parser_get_error(uclp)) {
			syslog(LOG_ERR, "%s",
			       ucl_parser_get_error(uclp));
		}
		syslog(LOG_ERR, "Failed to parse \"%s\"", configfile);
		return -1;
	}

	root = ucl_parser_get_object(uclp);
	do {
		if (!root) {
			syslog(LOG_WARNING, "No config node found in \"%s\"", configfile);
			break;
		}

		while ((cur = ucl_iterate_object(root, &it, true))) {
			configname = ucl_object_key(cur);
						
			/* handle parsing the contents of the object within bc */
			bc = malloc(sizeof(struct bhyve_configuration));
			if (!bc)
				break;
			
			bzero(bc, sizeof(struct bhyve_configuration));
			
			/* put configname into name as default */
			strncpy(bc->name, configname, sizeof(bc->name));
			
			if (bc_parsefromucl(bc, cur)) {
				syslog(LOG_WARNING, "Failed to parse \"%s\"", configfile);
				break;
			}

			/* remember file name */
			bc->backing_filepath = strdup(configfile);
			if (!bc->backing_filepath) {
				bc_free(bc);
				break;
			}
			
			LIST_INSERT_HEAD(&bcs->configs, bc, entries);
		}
	} while (0);

	syslog(LOG_INFO, "Parsing completed for \"%s\"", configfile);

	if (ucl_parser_get_error(uclp)) {
		syslog(LOG_ERR, "%s",
		       ucl_parser_get_error(uclp));
	}

	ucl_parser_free(uclp);
	ucl_object_unref(root);

	return 0;
}

/*
 * internal method doing the actual walking of the directory
 */
int
bcs_parseconfdir(struct bhyve_configuration_store *bcs, const char *path)
{
	char *configpath = 0;
	const char *configname = "config";
	size_t pathlen = 0, configlen = 0;
	int result = 0;

	if (!bcs || !path)
		return -1;

	pathlen = strlen(path);
	configlen = strlen(configname) + pathlen + 2;
	configpath = malloc(configlen);
	if (!configpath)
		return -1;

	snprintf(configpath, configlen, "%s/%s", path, configname);
	syslog(LOG_INFO, "Checking for config at \"%s\"", configpath);

	result = bcs_parseucl(bcs, configpath);

	free(configpath);	    
	
	return result;
}

/*
 * collect configuration files from config directory
 *
 * returns 0 on success.
 */
int
bcs_walkdir(struct bhyve_configuration_store *bcs)
{
	if (!bcs)
		return -1;

	DIR *d = NULL;
	struct dirent *de = NULL;
	char *subpath = NULL;
	size_t pathlen = strlen(bcs->searchpath);
	size_t maxbuf = 0;
	int result = 0;
	bool found_one = false;

	syslog(LOG_INFO, "Checking configuration dir \"%s\" for config files",
	       bcs->searchpath);

	if (NULL != (d = opendir(bcs->searchpath))) {
		while (1) {
			/* failed to open directory */
			de = readdir(d);
			if (!de)
				break;
			
			/* if directory or dot file, skip */
			if ('.' == *de->d_name)
				continue;
			
			if (DT_DIR & de->d_type) {
				/* construct sub path */
				free(subpath);
				maxbuf = pathlen + strlen(de->d_name) + 2;
				subpath = malloc(maxbuf);
				if (!subpath) {
					result = -1;
					syslog(LOG_ERR, "Memory allocation error");
					break;
				}
				snprintf(subpath, maxbuf, "%s/%s", bcs->searchpath, de->d_name);
				syslog(LOG_INFO, "Looking for config in \"%s\"",
				       subpath);
				if (!bcs_parseconfdir(bcs, subpath)) {
					found_one = true;
				} else {
					syslog(LOG_INFO,
					       "No configuration found in \"%s/%s\" - ignoring",
					       bcs->searchpath,
					       de->d_name);
				}
			}
		}
	} 
	free(subpath);

	if (found_one)
		result = 0;
	else {
		errno = ENOENT;
		result = -1;
	}

	if (!d) {
		/* we failed to access the directory */
		syslog(LOG_ERR, "Configuration directory \"%s\" could not be "
		       "accessed", bcs->searchpath);
		result = -1;
	}

	return (d ? closedir(d) : 0) || result;
}

/*
 * release a previously allocated configuration store
 */
void
bcs_free(struct bhyve_configuration_store *bcs)
{
	if (!bcs)
		return;

	struct bhyve_configuration *bc = 0;
	
	while (!LIST_EMPTY(&bcs->configs)) {
		bc = LIST_FIRST(&bcs->configs);
		LIST_REMOVE(bc, entries);
		bc_free(bc);
	}

	free(bcs);
}

/* generate getter functions */
CREATE_GETTERFUNC_STR(bhyve_configuration, bc, name);
CREATE_GETTERFUNC_STR(bhyve_configuration, bc, configfile);
CREATE_GETTERFUNC_STR(bhyve_configuration, bc, scriptpath);
CREATE_GETTERFUNC_STR(bhyve_configuration, bc, os);
CREATE_GETTERFUNC_STR(bhyve_configuration, bc, osversion);
CREATE_GETTERFUNC_STR(bhyve_configuration, bc, owner);
CREATE_GETTERFUNC_STR(bhyve_configuration, bc, group);
CREATE_GETTERFUNC_STR(bhyve_configuration, bc, description);
CREATE_GETTERFUNC_STR(bhyve_configuration, bc, bootrom);
CREATE_GETTERFUNC_STR(bhyve_configuration, bc, generated_config);

/*
 * get number of consoles
 */
size_t
bc_get_consolecount(const struct bhyve_configuration *bc)
{
	return bccl_count(bc->consoles);
}

/*
 * get console list
 */
const struct bhyve_configuration_console_list *
bc_get_consolelist(const struct bhyve_configuration *bc)
{
	return bc->consoles;
}

/*
 * get maximum restart count before failure state is assumed
 * when it happens within maxrestarttime seconds
 */
uint32_t
bc_get_maxrestart(const struct bhyve_configuration *bc)
{
	return bc->maxrestart;
}

/*
 * get number of cpus
 */
uint16_t
bc_get_numcpus(const struct bhyve_configuration *bc)
{
	return bc->numcpus;
}

/*
 * get number of sockets
 */
uint16_t
bc_get_sockets(const struct bhyve_configuration *bc)
{
	return bc->sockets;
}

/*
 * get number of cores
 */
uint16_t
bc_get_cores(const struct bhyve_configuration *bc)
{
	return bc->cores;
}

/*
 * get memory amount in megabytes
 */
uint32_t
bc_get_memory(const struct bhyve_configuration *bc)
{
	return bc->memory;
}

time_t
bc_get_maxrestarttime(const struct bhyve_configuration *bc)
{
	return bc->maxrestarttime;
}

/*
 * get path to filename that originated configuration
 */
const char *
bc_get_backingfile(const struct bhyve_configuration *bc)
{
	if (!bc)
		return NULL;

	return bc->backing_filepath;
}

/*
 * store the path to the generated config file
 */
int
bc_set_generated_config(struct bhyve_configuration *bc,
			const char *generated_config)
{
	if (!bc) {
		errno = EINVAL;
		return -1;
	}

	if (!generated_config) {
		free(bc->generated_config);
		bc->generated_config = NULL;
		return 0;
	}

	if (!(bc->generated_config = strdup(generated_config)))
		return -1;
	
	return 0;
}

/*
 * get vmexit on hlt flag
 */
bool
bc_get_vmexithlt(const struct bhyve_configuration *bc)
{
	if (!bc) {
		errno = EINVAL;
		return false;
	}

	return bc->vmexit_on_halt;
}

/*
 * get memory wired flag
 */
bool
bc_get_wired(const struct bhyve_configuration *bc)
{
	if (!bc) {
		errno = EINVAL;
		return false;
	}

	return bc->wire_memory;
}

/*
 * get generate acpi tables flag
 */
bool
bc_get_generateacpi(const struct bhyve_configuration *bc)
{
	if (!bc) {
		errno = EINVAL;
		return false;
	}

	return bc->generate_acpi_tables;
}
