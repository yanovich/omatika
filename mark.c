/* mark.c -- mark block and its builder
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
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "block.h"
#include "mark.h"
#include "state.h"

static void
mark_run(struct block *b, struct server_state *s)
{
	char buff[24];
	struct tm tm = *localtime(&s->start.tv_sec);
	strftime(&buff[0], sizeof(buff) - 1, "%b %e %H:%M:%S", &tm);
	logit("%s %s: Greetings, world!\n", buff, b->name);
}

static struct block_ops mark_ops = {
	.run		= mark_run,
};

static struct block_ops *
mark_init(void)
{
	return &mark_ops;
}

static struct block_builder mark_builder = {
	.ops		= mark_init,
};

struct block_builder *
load_mark_builder(void)
{
	return &mark_builder;
}
