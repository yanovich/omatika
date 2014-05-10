/* logical-xor.c -- calculate logical xor
 * Copyright (C) 2014 Sergei Ianovich <ynvich@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "includes.h"

#include "block.h"
#include "logical-xor.h"
#include "list.h"
#include "map.h"
#include "state.h"

#define PCS_BLOCK	"logical-xor"

struct logical_xor_state {
	struct list_head	component_list;
};

struct data_component {
	struct list_head	component_entry;
	long			*data;
};

static void
logical_xor_run(struct block *b, struct server_state *s)
{
	struct logical_xor_state *d = b->data;
	struct data_component *c;
	long res = 0;

	list_for_each_entry(c, &d->component_list, component_entry) {
		res += *c->data;
	}
	debug3("%s: %li\n", PCS_BLOCK, res);
	*b->outputs = res % 2;
}

static int
set_input(void *data, const char const *key, long *input)
{
	struct logical_xor_state *d = data;
	struct data_component *c = xzalloc(sizeof(*c));
	c->data = input;
	list_add_tail(&c->component_entry, &d->component_list);
	return 0;
}

static const char *outputs[] = {
	NULL
};

static struct pcs_map inputs[] = {
	{
		.key			= NULL,
		.value			= set_input,
	}
};

static void *
alloc(void)
{
	struct logical_xor_state *d = xzalloc(sizeof(*d));
	INIT_LIST_HEAD(&d->component_list);
	return d;
}

static struct block_ops ops = {
	.run		= logical_xor_run,
};

static struct block_ops *
init(void *data)
{
	struct logical_xor_state *d = data;
	if (&d->component_list == d->component_list.prev) {
		error("%s: input not defined\n", PCS_BLOCK);
		return NULL;
	}
	return &ops;
}

static struct block_builder builder = {
	.alloc		= alloc,
	.ops		= init,
	.outputs	= outputs,
	.inputs		= inputs,
};

struct block_builder *
load_logical_xor_builder(void)
{
	return &builder;
}
