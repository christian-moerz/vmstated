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

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../state_handler.h"
#include "../state_node.h"

int node_counter_enter[4] = {0};
int node_counter_exit[4] = {0};

int
node_counter_incr_enter(struct state_node *ns, void *ctx, struct state_node *from, uint64_t from_state)
{
	node_counter_enter[ns->id]++;
	(*(int *)ctx)++;
	return 0;
}

int
node_counter_incr_exit(struct state_node *os, void *ctx, struct state_node *to, uint64_t to_state)
{
	node_counter_exit[os->id]++;
	(*(int *)ctx)++;
	return 0;
}

struct state_node nodes[] = {
	{
		.id = 0,
		.name = "Start node",
		.on_enter = node_counter_incr_enter,
		.on_exit = node_counter_incr_exit
	},
	{
		.id = 1,
		.name = "Middle node",
		.on_enter = node_counter_incr_enter,
		.on_exit = node_counter_incr_exit
	},
	{
		.id = 2,
		.name = "Ende node",
		.on_enter = node_counter_incr_enter,
		.on_exit = node_counter_incr_exit
	},
	{
		.id = 3,
		.name = "Failure",
		.on_enter = node_counter_incr_enter,
		.on_exit = node_counter_incr_exit
	}
};

struct state_transition transitions[] = {
	{
		.from = &nodes[0],
		.to = &nodes[1]
	},
	{
		.from = &nodes[1],
		.to = &nodes[2]
	},
	{
		.from = NULL,
		.to = &nodes[3]
	}
};


ATF_TC(tc_sth_simplerun);
ATF_TC_HEAD(tc_sth_simplerun, tc)
{
	bzero(node_counter_exit, sizeof(node_counter_exit));
	bzero(node_counter_enter, sizeof(node_counter_enter));
}
ATF_TC_BODY(tc_sth_simplerun, tc)
{
	size_t transition_count = sizeof(transitions) / sizeof(struct state_transition);
	int funccalls = 0;

	struct state_handler *sth = sth_new(transitions, transition_count, &nodes[0], &funccalls);
	ATF_REQUIRE(0 != sth);

	ATF_REQUIRE_EQ(0, sth_getcurrentid(sth));
	ATF_REQUIRE_EQ(-1, sth_transitionto(sth, 2));
	ATF_REQUIRE_EQ(0, sth_getcurrentid(sth));
	
	ATF_REQUIRE_EQ(0, sth_transitionto(sth, 1));	
	ATF_REQUIRE_EQ(1, sth_getcurrentid(sth));

	ATF_REQUIRE_EQ(-1, sth_transitionto(sth, 0));
	ATF_REQUIRE_EQ(1, sth_getcurrentid(sth));
	
	ATF_REQUIRE_EQ(0, sth_transitionto(sth, 2));	
	ATF_REQUIRE_EQ(2, sth_getcurrentid(sth));

	ATF_REQUIRE_EQ(0, sth_transitionto(sth, 3));
	ATF_REQUIRE_EQ(3, sth_getcurrentid(sth));

	printf("funccalls = %d\n", funccalls);
	ATF_REQUIRE_EQ(6, funccalls);
	
	ATF_REQUIRE_EQ(1, node_counter_exit[0]);
	ATF_REQUIRE_EQ(1, node_counter_exit[1]);
	ATF_REQUIRE_EQ(1, node_counter_exit[2]);
	ATF_REQUIRE_EQ(0, node_counter_exit[3]);
	ATF_REQUIRE_EQ(0, node_counter_enter[0]);
	ATF_REQUIRE_EQ(1, node_counter_enter[1]);
	ATF_REQUIRE_EQ(1, node_counter_enter[2]);
	ATF_REQUIRE_EQ(1, node_counter_enter[3]);
	
	sth_free(sth);
}
ATF_TC_CLEANUP(tc_sth_simplerun, tc)
{
}

ATF_TP_ADD_TCS(testplan)
{
	ATF_TP_ADD_TC(testplan, tc_sth_simplerun);

	return atf_no_error();
}
