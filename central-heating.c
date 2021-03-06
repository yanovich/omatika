/* central-heating.c -- calculate fuzzy logic measure with delta function
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

#include <stdio.h>

#include "block.h"
#include "central-heating.h"
#include "map.h"
#include "state.h"

#define PCS_BLOCK	"central-heating"

struct central_heating_state {
	long			*input_flowback;
	long			*input_street;
	long			*feed;
	long			*flowback;
	long			*inside;
	long			*stop;
	long			*street;
};

static void
central_heating_run(struct block *b, struct server_state *s)
{
	struct central_heating_state *d = b->data;
	long long t11;
	long long t12_excess;
	long street = *d->input_street;

	if (street > *d->stop)
		street = *d->stop;
	t11  = *d->feed - *d->inside;
	t11 *= *d->inside - street;
	t11 /= *d->inside - *d->street;
	t11 += *d->inside;

	t12_excess  = *d->feed - *d->flowback;
	t12_excess *= *d->inside - street;
	t12_excess /= *d->inside - *d->street;
	t12_excess += *d->input_flowback - t11;

	*b->outputs = (long) (t11 - t12_excess);
	debug3("%s: %lli %lli %li\n", PCS_BLOCK, t11, t12_excess, *b->outputs);
}

static void
set_input_flowback(void *data, const char const *key, long *input_flowback)
{
	struct central_heating_state *d = data;
	d->input_flowback = input_flowback;
}

static void
set_input_street(void *data, const char const *key, long *input_street)
{
	struct central_heating_state *d = data;
	d->input_street = input_street;
}

static int
set_feed(void *data, const char const *key, long *feed)
{
	struct central_heating_state *d = data;
	d->feed = feed;
	return 0;
}

static int
set_flowback(void *data, const char const *key, long *flowback)
{
	struct central_heating_state *d = data;
	d->flowback = flowback;
	return 0;
}

static int
set_inside(void *data, const char const *key, long *inside)
{
	struct central_heating_state *d = data;
	d->inside = inside;
	return 0;
}

static int
set_stop(void *data, const char const *key, long *stop)
{
	struct central_heating_state *d = data;
	d->stop = stop;
	return 0;
}

static int
set_street(void *data, const char const *key, long *street)
{
	struct central_heating_state *d = data;
	d->street = street;
	return 0;
}

static const char *outputs[] = {
	NULL
};

static struct pcs_map inputs[] = {
	{
		.key			= "flowback",
		.value			= set_input_flowback,
	}
	,{
		.key			= "street",
		.value			= set_input_street,
	}
	,{
		.key			= "feed setpoint",
		.value			= set_feed,
	}
	,{
		.key			= "flowback setpoint",
		.value			= set_flowback,
	}
	,{
		.key			= "inside setpoint",
		.value			= set_inside,
	}
	,{
		.key			= "stop setpoint",
		.value			= set_stop,
	}
	,{
		.key			= "street setpoint",
		.value			= set_street,
	}
	,{
	}
};

static void *
alloc(void)
{
	struct central_heating_state *d = xzalloc(sizeof(*d));
	return d;
}

static struct block_ops ops = {
	.run		= central_heating_run,
};

static struct block_ops *
init(struct block *b)
{
	return &ops;
}

static struct block_builder builder = {
	.alloc		= alloc,
	.ops		= init,
	.outputs	= outputs,
	.inputs		= inputs,
};

struct block_builder *
load_central_heating_builder(void)
{
	return &builder;
}
