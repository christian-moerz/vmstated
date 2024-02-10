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

#ifndef __BHYVE_DIRECTOR_ERRORS_H__
#define __BHYVE_DIRECTOR_ERRORS_H__

#define BD_ERR_UNLOCKFAILURE 195 /* failed to unlock structure */
#define BD_ERR_MUTEXLOCKFAIL 192
#define BD_ERR_MUTEXDESTFAIL 191 /* mutex destruction failed */
#define BD_ERR_KQUQUERFAILED 186 /* kqueue query failed */

/* 150-169 reserved for bhyve_director */
#define BD_ERR_VMSTATEISFAIL 169 /* vm is already in failed state */
#define BD_ERR_VMSTARTFAILED 168 /* failed to start vm */
#define BD_ERR_KEVENTREGFAIL 167 /* failed to register pid with queue */
#define BD_ERR_TRANSITCHFAIL 166 /* failed to transition to new state */
#define BD_ERR_VMSTARTDIEDIM 165 /* vm started but immediately died */
#define BD_ERR_VMSTATENOFAIL 164 /* vm is not in failed state */
#define BD_ERR_VMCONFGENFAIL 163 /* failed to generate vm config */

#endif /* __BHYVE_DIRECTOR_ERRORS_H__ */
