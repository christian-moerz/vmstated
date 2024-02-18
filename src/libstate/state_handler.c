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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "state_handler.h"
#include "state_node.h"

/*
 * a helper structure that combines state nodes with
 * transitions to simplify lookups
 */
struct state_node_expanded {
	/* TODO implement */
	/* we could simplify this by building this a list of
	   states that directly link the available transitions.
	   We would then no longer need to walk through the
	   whole array to find the right transitions */
};

/* TODO we might need a "reset" function */

struct state_handler {
	struct state_node *current;
	/* array of transition pointers */
	struct state_transition *vec;
	size_t vecsize;
	void *ctx;
};

/*
 * look up a transition from current state to desired target state
 * this only supports moving to a neighboring state.
 *
 * returns NULL if no transition exists
 */
struct state_transition *
sth_findtransition(struct state_handler *sth, uint64_t target_state)
{
	if (!sth)
		return NULL;

	struct state_transition *transition = sth->vec;
	size_t counter = 0;
	
	for(counter = 0; counter < sth->vecsize; counter++, transition++) {
		if (transition->from) {
			/* if transition has a from property, check it */
			if (transition->from->id != sth->current->id)
				continue;
		}
		if (transition->to) {
			/* we match on the target state */
			if (transition->to->id == target_state)
				return transition;
		}			    
	}

	return NULL;
}

/*
 * transition to a new state
 *
 * looks up transition to get to target state. If there is no transition,
 * it fails with -1. If there is a transition, it calls on_exit on
 * current state and on_enter on target state, then moves state to the
 * new desired target state. If any of the on_exit of on_enter calls
 * fails with <0, the transition will fail with -1.
 *
 * - sth: state handler
 * - target_state: numeric id of target state
 */
int
sth_transitionto(struct state_handler *sth, uint64_t target_state)
{
	if (!sth)
		return -1;

	struct state_transition *st = sth_findtransition(sth, target_state);
	if (!st) {
		syslog(LOG_ERR, "Failed to find transition to target_state = %lu", target_state);
		return -1;
	}

	/* attempt on_exit on current state */
	if (sth->current->on_exit) {
		if (sth->current->on_exit(sth->current, sth->ctx, st->to, st->to->id) != 0) {
			syslog(LOG_ERR, "on_exit returned error");
			return -1;
		}
	}

	if (st->to->on_enter) {
		if (st->to->on_enter(st->to, sth->ctx, sth->current, sth->current->id) != 0) {
			syslog(LOG_ERR, "on_enter returned error");
			return -1;
		}
	}

	sth->current = st->to;
	
	return 0;
}

/*
 * get id of current state; sets errno if an error occurrs.
 */
uint64_t
sth_getcurrentid(struct state_handler *sth)
{
	if (!sth) {
		errno = EINVAL;
		return 0;
	}
	
	return sth->current->id;
}

/*
 * look up index of node with specific id
 */
int
sth_lookupstate(struct state_node *vec, size_t vecsize, uint64_t id)
{
	if (!vec)
		return -1;

	for (size_t i = 0; i < vecsize; i++) {
		if (vec[i].id == id)
			return i;
	}

	return -1;
}

/*
 * instantiate a new state handler
 *
 * - vec: the list of transitions
 * - vecsize: size of the transition list (number of items)
 * - current: the initial state to start from
 */
struct state_handler *
sth_new(struct state_transition *vec, size_t vecsize, struct state_node *current, void *ctx)
{
	if (!vec)
		return NULL;

	struct state_handler *sth = malloc(sizeof(struct state_handler));
	if (!sth)
		return NULL;

	sth->vec = vec;
	sth->vecsize = vecsize;
	sth->current = current;
	sth->ctx = ctx;

	return sth;
}

/*
 * free a previously allocated state handler
 */
void
sth_free(struct state_handler *sth)
{
	free(sth);
}
