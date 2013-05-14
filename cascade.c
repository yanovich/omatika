/* cascade.c -- control cascade system temperature
 * Copyright (C) 2013 Sergey Yanovich <ynvich@gmail.com>
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

#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>

#include "includes.h"
#include "log.h"
#include "list.h"
#include "process.h"
#include "cascade.h"

#define HAS_BLOCK_SP		0x01
#define HAS_UNBLOCK_SP		0x02
#define HAS_BLOCK_IO		0x04
#define HAS_BLOCK		0x07

#define HAS_BEFORE_IO		0x01
#define HAS_AFTER_IO		0x02
#define HAS_STAGE_SP		0x04
#define HAS_UNSTAGE_SP		0x08
#define HAS_TARGET		0x0E

struct cascade_config {
	int			block;
	int			block_sp;
	int			unblock_sp;
	int			stage;
	int			unstage;
	int			stage_wait;
	int			unstage_wait;
	int			entry;
	int			entry_type;
	int			feed;
	int			feed_type;
	int			first_run;
	int			motor[256];
	int			mark[256];
	int			motor_count;
	unsigned		has_analog_block;
	unsigned		has_target;
	int			m11_fail;
	int			m11_int;
	int			cycle;
};

static void
cascade_run(struct site_status *s, void *conf)
{
	struct cascade_config *c = conf;
	struct timeval tv;
	int go = 1;
	int i;
	int j = 0;
	int on = 0;
	int val;
	
	c->cycle++;

	debug("running cascade\n");
	if ((c->has_analog_block & HAS_BLOCK) == HAS_BLOCK) {
		if (c->block_sp > c->unblock_sp) {
		       if (c->unblock_sp > s->T[c->block])
			       go = 1;
		       else if (s->T[c->block] > c->block_sp)
			       go = -1;
		       else
			       go = 0;
		} else if (c->block_sp < c->unblock_sp) {
		       if (c->unblock_sp < s->T[c->block])
			       go = 1;
		       else if (s->T[c->block] < c->block_sp)
			       go = -1;
		       else
			       go = 0;
		}
	}
	debug2("  cascade: after block go %i\n", go);
	if (go == 0)
		return;

	if (go == -1) {
		for (i = 0; i < c->motor_count; i++)
			if (get_DO(c->motor[i])) {
				if (c->unstage_wait && c->mark[i] > (c->cycle
							- c->unstage_wait))
					return;
				set_DO(c->motor[i], 0, 0);
				c->mark[i] = c->cycle;
			}
		return;
	}

	if ((c->has_target & HAS_TARGET) != HAS_TARGET) {
		for (i = 0; i < c->motor_count; i++)
			if (!get_DO(c->motor[i])) {
				debug2("  motor %i[%i]: %i\n", i,
					       	c->motor[i],
					       	get_DO(c->motor[i]));
				if (c->stage_wait && c->mark[i] > (c->cycle
							- c->stage_wait))
					return;
				set_DO(c->motor[i], 1, 0);
				c->mark[i] = c->cycle;
			}
		return;
	}

	if (c->feed_type == AI_MODULE)
		val = s->AI[c->feed];
	else if (c->feed_type == TR_MODULE)
		val = s->T[c->feed];
	else {
		error("cascade: bad feed type");
		return;
	}
	debug2("  cascade: target processing %i (%i)\n", val, c->feed_type);
	if (c->has_target & HAS_BEFORE_IO)
		val -= s->AI[c->entry];

	if (c->unstage > c->stage) {
		if (c->stage > val)
			go = 1;
		else if (val > c->unstage)
			go = -1;
		else
			go = 0;
	} else if (c->unstage < c->stage) {
		if (c->stage < val)
			go = 1;
		else if (val < c->unstage)
			go = -1;
		else
			go = 0;
	}

	if (go == 0)
		return;

	for (i = 0; i < c->motor_count; i++)
		if (get_DO(c->motor[i])) {
			on++;
			j = i;
		}

	if (go == -1) {
		if (on == 1)
			return;
		for (i = 0; i < c->motor_count; i++)
			if (get_DO(c->motor[i])) {
				if (c->unstage_wait && c->mark[i] > (c->cycle
							- c->unstage_wait))
					return;
				set_DO(c->motor[i], 0, 0);
				c->mark[i] = c->cycle;
				return;
			}
	}

	if (on == c->motor_count)
		return;
	if (on == 0) {
		gettimeofday(&tv, NULL);
		j = tv.tv_sec;
	}
	i = (j + 1) % c->motor_count;
	if (c->stage_wait && c->mark[i] > (c->cycle
				- c->stage_wait))
		return;
	set_DO(c->motor[i], 1, 0);
	c->mark[i] = c->cycle;
	return;
}

static int
cascade_log(struct site_status *s, void *conf, char *buff,
		const int sz, int o)
{
	struct cascade_config *c = conf;
	int b = o;
	int i;
	int val;

	if (o == sz)
		return 0;
	if (o) {
		buff[o] = ',';
		b++;
	}
	for (i = 0; (i < c->motor_count) && (b < sz); i++, b++) {
		buff[b] = get_DO(c->motor[i]) ? 'M' : '_';
	}
	buff[b] = 0;
	if ((c->has_target & HAS_TARGET) != HAS_TARGET)
		return b - o;
	if (c->feed_type == AI_MODULE)
		val = s->AI[c->feed];
	else if (c->feed_type == TR_MODULE)
		val = s->T[c->feed];
	else {
		error("cascade: bad feed type");
		return b - o;
	}
	b += snprintf(&buff[b], sz - b, " O %3i", val);

	if ((c->has_target & HAS_BEFORE_IO) != HAS_BEFORE_IO)
		return b - o;

	if (c->entry_type == AI_MODULE)
		val = s->AI[c->entry];
	else if (c->entry_type == TR_MODULE)
		val = s->T[c->entry];
	else {
		error("cascade: bad entry type");
		return b - o;
	}

	b += snprintf(&buff[b], sz - b, " I %3i", val);

	return b - o;
}

struct process_ops cascade_ops = {
	.run = cascade_run,
	.log = cascade_log,
};

static void *
cascade_init(void *conf)
{
	struct cascade_config *c = conf;

	c->first_run = 1;
	c->cycle = c->stage_wait + 1;
	return &cascade_ops;
}

static void
set_stage(void *conf, int value)
{
	struct cascade_config *c = conf;
	c->stage = value;
	c->has_target |= HAS_STAGE_SP;
	debug("  cascade: stage = %i\n", value);
}

static void
set_unstage(void *conf, int value)
{
	struct cascade_config *c = conf;
	c->unstage = value;
	c->has_target |= HAS_UNSTAGE_SP;
	debug("  cascade: unstage = %i\n", value);
}

static void
set_stage_wait(void *conf, int value)
{
	struct cascade_config *c = conf;
	c->stage_wait = value;
	c->has_target |= HAS_STAGE_SP;
	debug("  cascade: stage = %i\n", value);
}

static void
set_unstage_wait(void *conf, int value)
{
	struct cascade_config *c = conf;
	c->unstage_wait = value;
	c->has_target |= HAS_UNSTAGE_SP;
	debug("  cascade: unstage = %i\n", value);
}

static void
set_block(void *conf, int value)
{
	struct cascade_config *c = conf;
	c->block_sp = value;
	c->has_analog_block |= HAS_BLOCK_SP;
	debug("  cascade: block_sp = %i\n", value);
}

static void
set_unblock(void *conf, int value)
{
	struct cascade_config *c = conf;
	c->unblock_sp = value;
	c->has_analog_block |= HAS_UNBLOCK_SP;
	debug("  cascade: unblock_sp = %i\n", value);
}

struct setpoint_map cascade_setpoints[] = {
	{
		.name 		= "stage",
		.set		= set_stage,
	},
	{
		.name 		= "unstage",
		.set		= set_unstage,
	},
	{
		.name 		= "stage wait",
		.set		= set_stage_wait,
	},
	{
		.name 		= "unstage wait",
		.set		= set_unstage_wait,
	},
	{
		.name 		= "block",
		.set		= set_block,
	},
	{
		.name 		= "unblock",
		.set		= set_unblock,
	},
	{
	}
};

static void
set_block_io(void *conf, int type, int value)
{
	struct cascade_config *c = conf;
	if (type != TR_MODULE)
		fatal("cascade: wrong type of feed sensor\n");
	c->block = value;
	c->has_analog_block |= HAS_BLOCK_IO;
	debug("  cascade: block io = %i\n", value);
}

static void
set_p_in_io(void *conf, int type, int value)
{
	struct cascade_config *c = conf;
	if (type != AI_MODULE && type != TR_MODULE)
		fatal("cascade: wrong type of entry sensor\n");
	c->entry = value;
	c->entry_type = type;
	c->has_target |= HAS_BEFORE_IO;
	debug("  cascade: entry io = %i\n", value);
}

static void
set_p_out_io(void *conf, int type, int value)
{
	struct cascade_config *c = conf;
	if (type != AI_MODULE && type != TR_MODULE)
		fatal("cascade: wrong type of feed sensor\n");
	c->feed = value;
	c->feed_type = type;
	c->has_target |= HAS_AFTER_IO;
	debug("  cascade: feed io = %i\n", value);
}

static void
add_motor(void *conf, int type, int value)
{
	struct cascade_config *c = conf;
	if (type != DO_MODULE)
		fatal("cascade: wrong type of motor output\n");
	c->motor[c->motor_count++] = value;
	debug("  cascade: motor[%i] = %i\n", c->motor_count - 1, value);
}

struct io_map cascade_io[] = {
	{
		.name 		= "block",
		.set		= set_block_io,
	},
	{
		.name 		= "entry",
		.set		= set_p_in_io,
	},
	{
		.name 		= "feed",
		.set		= set_p_out_io,
	},
	{
		.name 		= "motor",
		.set		= add_motor,
	},
	{
	}
};

static void *
c_alloc(void)
{
	struct cascade_config *c = xzalloc(sizeof(*c));
	return c;
}

struct process_builder cascade_builder = {
	.alloc			= c_alloc,
	.setpoint		= cascade_setpoints,
	.io			= cascade_io,
	.ops			= cascade_init,
};

struct process_builder *
load_cascade_builder(void)
{
	return &cascade_builder;
}
