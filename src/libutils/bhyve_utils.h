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

#ifndef __BHYVE_UTILS_H__
#define __BHYVE_UTILS_H__

#define CREATE_GETTERFUNC_STR(structname, shortcode, varname)   \
	const char * shortcode##_get_##varname (const struct structname *shortcode) { \
		return shortcode->varname; \
	}

#define CREATE_GETTERFUNC_NUMERIC(typename, structname, shortcode, varname) \
	typename shortcode##_get_##varname (const struct structname *shortcode) { \
		if (!shortcode) { \
			errno = EINVAL; \
			return 0; \
		} \
                return shortcode->varname; \
	}

#define CREATE_GETTERFUNC_UINT16(structname, shortcode, varname)   \
	CREATE_GETTERFUNC_NUMERIC(uint16_t, structname, shortcode, varname)

#define CREATE_GETTERFUNC_UINT32(structname, shortcode, varname) \
	CREATE_GETTERFUNC_NUMERIC(uint32_t, structname, shortcode, varname)

#endif /* __BHYVE_UTILS_H__ */
