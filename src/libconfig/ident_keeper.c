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

#include <sys/queue.h> 

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h> 
#include <stddef.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 

#include "ident_keeper.h"

/*
 * reservation handle
 */
struct ident_keeper_reservation {
	uint64_t reservation_id;
	uint64_t ident;

	TAILQ_ENTRY(ident_keeper_reservation) entries;
};

/*
 * keeps record of available identifiers that can be reserved
 */
struct ident_keeper {
	pthread_mutex_t mtx;
	void *ctx;

	uint64_t ident_min;
	uint64_t ident_max;
	uint64_t reservation_counter;
	uint64_t ident_next;

	/* checker method whether an identifier is still valid */
	/* method is expected to return true, if still valid */
	bool(*check_ident)(void *ctx, uint64_t ident);
	
	TAILQ_HEAD(, ident_keeper_reservation) reservations;
};

/*
 * get next reservation id
 */
uint64_t
ik_next_resid(struct ident_keeper *ik, bool locked)
{
	uint64_t res_id = 0;

	if (!locked)
		if (pthread_mutex_lock(&ik->mtx))
			return -1;
	
	res_id = ik->reservation_counter++;

	if (!locked)
		if (pthread_mutex_unlock(&ik->mtx))
			return -1;

	return res_id;
}

/*
 * get numeric identifier
 */
uint64_t
ikr_get_ident(struct ident_keeper_reservation *ikr)
{
	if (!ikr) {
		errno = EINVAL;
		return -1;
	}

	return ikr->ident;
}

/*
 * release a previously allocated identifier reservation
 */
void
ikr_free(struct ident_keeper_reservation *ikr)
{
	free(ikr);
}

/*
 * look up next ident
 */
uint64_t
ik_next_ident_locked(struct ident_keeper *ik)
{
	if (!ik) {
		errno = EINVAL;
		return -1;
	}

	if (ik->ident_next <= ik->ident_max) {
		return ik->ident_next++;
	}

	/* we have no more free identifiers */
	/* now we walk through reservations and look for gaps */

	uint64_t ident_start = ik->ident_min;
	struct ident_keeper_reservation *ikr = 0;
	
	TAILQ_FOREACH(ikr, &ik->reservations, entries) {
		/* if identifier does not match, we found a gap */
		if (ident_start != ikr->ident)
			return ident_start;
		
		ident_start++;
	}

	/* no more free identifiers */
	errno = ENOENT;
	return -1;
}

/*
 * inserts a reservation into the list
 */
int
ik_insert_reservation_locked(struct ident_keeper *ik,
			     struct ident_keeper_reservation *ikr)
{
	struct ident_keeper_reservation *ikr_item = 0;

	if (!ik || !ikr) {
		errno = EINVAL;
		return -1;
	}
	
	/* insert that reservation into the list */
	TAILQ_FOREACH(ikr_item, &ik->reservations, entries) {
		/* insert before next larger item */
		if (ikr_item->ident > ikr->ident)
			break;
	}

	if (NULL == ikr_item) {
		/* insert at the end */
		TAILQ_INSERT_TAIL(&ik->reservations, ikr, entries);
	} else {
		/* insert before ikr_item */
		TAILQ_INSERT_BEFORE(ikr_item, ikr, entries);
	}

	return 0;
}

/*
 * check validity of a reservation
 * 
 * attempts to move the reservation to another identifier if it turns
 * out to be invalid. If that fails, the reservation becomes
 * invalidated and is free'd.
 */
int
ik_validate_reservation(struct ident_keeper *ik,
			struct ident_keeper_reservation *ikr)
{
	int result = 0;
	
	if (!ik || !ikr) {
		errno = EINVAL;
		return -1;
	}

	if (ik->check_ident && (ik->check_ident(ik->ctx, ikr->ident)))
		return 0;

	if (pthread_mutex_lock(&ik->mtx))
		return -1;
	
	/* identifier is no longer valid - remove from list */
	TAILQ_REMOVE(&ik->reservations, ikr, entries);

	errno = 0;
	ikr->ident = ik_next_ident_locked(ik);

	if ((ikr->ident != -1) && (0 == errno)) {
		result = ik_insert_reservation_locked(ik, ikr);
	} else {
		ikr_free(ikr);
		result = -1;
	}

	if (pthread_mutex_unlock(&ik->mtx))
		return -1;
	
	return result;
}

/*
 * dispose of a previously used reservation
 */
int
ik_dispose(struct ident_keeper *ik,
	   struct ident_keeper_reservation *ikr)
{
	if (!ik || !ikr) {
		errno = EINVAL;
		return -1;
	}

	if (pthread_mutex_lock(&ik->mtx))
		return -1;

	TAILQ_REMOVE(&ik->reservations, ikr, entries);
	if ((ik->ident_next - 1) == ikr->ident)
		ik->ident_next--;
	ikr_free(ikr);
	
	if (pthread_mutex_unlock(&ik->mtx))
		return -1;

	return 0;
}

/*
 * get a new reservation
 */
struct ident_keeper_reservation *
ik_reserve(struct ident_keeper *ik)
{
	struct ident_keeper_reservation *ikr = 0;

	if (!ik) {
		errno = EINVAL;
		return NULL;
	}

	if (pthread_mutex_lock(&ik->mtx)) {
		return NULL;
	}

	errno = 0;
	
	/* first check if we have a free identifier */
	uint64_t new_ident = ik_next_ident_locked(ik);
	uint64_t invalid = -1;

	if ((invalid == new_ident) && (errno == ENOENT)) {
		/* no more free identifiers */
		pthread_mutex_unlock(&ik->mtx);
		return NULL;
	}

	/* allocate and set new reservation */
	if (!(ikr = malloc(sizeof(struct ident_keeper_reservation))))
		return NULL;
		
	ikr->reservation_id = ik_next_resid(ik, true);
	ikr->ident = new_ident;

	if (ik_insert_reservation_locked(ik, ikr)) {
		ikr_free(ikr);
		ikr = NULL;
	}

	pthread_mutex_unlock(&ik->mtx);

	return ikr;
}

/*
 * construct a new identifier keeper
 */
struct ident_keeper *
ik_new(void *ctx, uint64_t ident_min, uint64_t ident_max,
       bool(*check_ident)(void *, uint64_t))
{
	struct ident_keeper *ik = 0;

	if (ident_min > ident_max) {
		errno = EDOM;
		return NULL;
	}

	if (!(ik = malloc(sizeof(struct ident_keeper))))
		return NULL;

	bzero(ik, sizeof(struct ident_keeper));
	TAILQ_INIT(&ik->reservations);

	if (pthread_mutex_init(&ik->mtx, NULL)) {
		free(ik);
		return NULL;
	}

	ik->ctx = ctx;
	ik->ident_min = ident_min;
	ik->ident_max = ident_max;
	ik->ident_next = ident_min;

	ik->check_ident = check_ident;

	return ik;
}

/*
 * release a prevously allocated ident keeper and
 * invalidate all its reservations
 */
void
ik_free(struct ident_keeper *ik)
{
	if (!ik) {
		errno = EINVAL;
		return;
	}

	struct ident_keeper_reservation *ikr = 0, *next_item = 0;

	if (pthread_mutex_lock(&ik->mtx))
		return;

	ikr = TAILQ_FIRST(&ik->reservations);
	while (NULL != ikr) {
		next_item = TAILQ_NEXT(ikr, entries);
		ikr_free(ikr);
		ikr = next_item;
	}

	pthread_mutex_unlock(&ik->mtx);
	pthread_mutex_destroy(&ik->mtx);

	free(ik);
}

/*
 * a helper method that always validates an identifier
 */
bool
ik_always_valid(void *ctx, uint64_t ident)
{
	return true;
}
