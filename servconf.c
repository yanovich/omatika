/* servconf.c -- load configuration from a YAML file
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

#include <stdio.h>
#include <errno.h>
#include <sys/time.h>
#include <yaml.h>

#include "log.h"

#include "includes.h"
#include "list.h"
#include "process.h"
#include "icp.h"
#include "fuzzy.h"
#include "evaporation.h"
#include "hot_water.h"
#include "heating.h"
#include "cascade.h"
#include "reference.h"
#include "relay.h"
#include "counter.h"
#include "servconf.h"

struct modules_parser {
	struct config_node	node;
	int			in_a_seq;
	int			in_a_map;
	int			level[3];
	int			last_mod;
	int			last_type;
};

#define NI1000_TK5000		"ni1000 tk5000"
#define PT1000			"pt1000"
#define DIRECT			"direct"
#define P_0_13BAR_4_20_MA	"0-13 bar 4-20 mA"
#define P_0_16BAR_4_20_MA	"0-16 bar 4-20 mA"
#define P_0_31BAR_4_20_MA	"0-31 bar 4-20 mA"
#define VOLTS_TO_HEX		"0-8000 0-10 V"
#define VOLTS_TO_HEX_2		"1600-8000 2-10 V"

static int
direct(int x)
{
	return x;
}

void
configure_module(struct site_config *conf, struct modules_parser *data,
		const char *text)
{
	int type, more = 0;
	int i, j = 0;
	int offset = 0;
	void *mod;
	switch (data->in_a_seq) {
	case 1:
		break;
	case 2:
		do {
			mod = icp_init_module(text, j, &type, &more);
			j++;
			data->last_type = type;
			switch (type) {
			case DO_MODULE:
				for (i = 0; conf->DO_mod[i].count > 0; i++)
					offset += conf->DO_mod[i].count;
				data->last_mod = i;
				conf->DO_mod[i] = *(struct DO_mod *) mod;
				conf->DO_mod[i].block = data->level[0];
				conf->DO_mod[i].slot = data->level[1] + 1;
				break;
			case AI_MODULE:
				for (i = 0; conf->AI_mod[i].count > 0; i++)
					offset += conf->AI_mod[i].count;
				data->last_mod = i;
				conf->AI_mod[i] = *(struct AI_mod *) mod;
				conf->AI_mod[i].block = data->level[0];
				conf->AI_mod[i].slot = data->level[1] + 1;
				conf->AI_mod[i].first = offset;
				break;
			case AO_MODULE:
				for (i = 0; conf->AO_mod[i].count > 0; i++)
					offset += conf->AO_mod[i].count;
				data->last_mod = i;
				conf->AO_mod[i] = *(struct AO_mod *) mod;
				conf->AO_mod[i].block = data->level[0];
				conf->AO_mod[i].slot = data->level[1] + 1;
				conf->AO_mod[i].first = offset;
				break;
			case DI_MODULE:
				for (i = 0; conf->DI_mod[i].count > 0; i++)
					offset += conf->DI_mod[i].count;
				data->last_mod = i;
				conf->DI_mod[i] = *(struct DI_mod *) mod;
				conf->DI_mod[i].block = data->level[0];
				conf->DI_mod[i].slot = data->level[1] + 1;
				break;
			case NULL_MODULE_TYPE:
			default:
				if (text && text[0])
					fatal("module %s is unsupported\n",
						       	text);
				break;
			};
		} while (more);
		break;
	case 3:
		if (!text || !text[0])
			break;
		switch (data->last_type) {
		case AI_MODULE:
			i = conf->AI_mod[data->last_mod].first + data->level[2];
			conf->AI[i].mod = data->last_mod;
			if (!strcmp(text, DIRECT)) {
				conf->AI[i].convert = direct;
				break;
			}
			if (!strcmp(text, NI1000_TK5000)) {
				conf->AI[i].convert = ni1000;
				break;
			}
			if (!strcmp(text, PT1000)) {
				conf->AI[i].convert = pt1000;
				break;
			}
			if (!strcmp(text, P_0_13BAR_4_20_MA)) {
				conf->AI[i].convert = b013;
				break;
			}
			if (!strcmp(text, P_0_16BAR_4_20_MA)) {
				conf->AI[i].convert = b016;
				break;
			}
			if (!strcmp(text, P_0_31BAR_4_20_MA)) {
				conf->AI[i].convert = b031;
				break;
			}
			if (!strcmp(text, VOLTS_TO_HEX)) {
				conf->AI[i].convert = v2h;
				break;
			}
			if (!strcmp(text, VOLTS_TO_HEX_2)) {
				conf->AI[i].convert = v2h2;
				break;
			}
			fatal("conversion '%s' is unsupported for "
					"module %i-%i\n", text,
					data->level[0],
					data->level[1]);
			break;
		case DO_MODULE:
		case DI_MODULE:
		case NULL_MODULE_TYPE:
		default:
			if (text && text[0])
				fatal("sensor '%s' is unsupported for "
						"module %i-%i\n", text,
					       	data->level[0],
						data->level[1]);
			break;
		};
		break;
	default:
		break;
	}
}

int
parse_modules(yaml_event_t *event, struct site_config *conf,
	       struct config_node *node)
{
	struct modules_parser *data = container_of(node, typeof(*data), node);
	char spaces[8] = "       ";

	switch (event->type) {
	case YAML_NO_EVENT:
	case YAML_STREAM_START_EVENT:
	case YAML_STREAM_END_EVENT:
	case YAML_DOCUMENT_START_EVENT:
	case YAML_DOCUMENT_END_EVENT:
	case YAML_ALIAS_EVENT:
		fatal("unexpected element at line %zu column %zu\n",
			       	event->start_mark.line,
				event->start_mark.column);
		break;
	case YAML_SCALAR_EVENT:
		if (!data->in_a_seq)
			fatal("unexpected element at line %zu column %zu\n",
					event->start_mark.line,
					event->start_mark.column);
		if (data->in_a_seq < 8)
			spaces[data->in_a_seq] = 0;
		debug("%s%i:%s\n", spaces, data->level[data->in_a_seq - 1] + 1,
			       	event->data.scalar.value);
		configure_module(conf, data, (const char *)event->data.scalar.value);
		data->level[data->in_a_seq - 1]++;
		break;
	case YAML_SEQUENCE_START_EVENT:
		data->in_a_seq++;
		if (data->in_a_seq > 3)
			fatal("unexpected element at line %zu column %zu\n",
					event->start_mark.line,
					event->start_mark.column);
		break;
	case YAML_MAPPING_START_EVENT:
		if (!data->in_a_seq)
			fatal("unexpected element at line %zu column %zu\n",
					event->start_mark.line,
					event->start_mark.column);
		data->in_a_map++;
		break;
	case YAML_SEQUENCE_END_EVENT:
		data->in_a_seq--;
		data->level[data->in_a_seq] = 0;
		break;
	case YAML_MAPPING_END_EVENT:
		data->in_a_map--;
		break;
	};

	yaml_event_delete(event);
	if (data->in_a_seq)
		return 0;
	list_del(&node->node_entry);
	xfree(data);
	return 0;
}

struct top_level_parser {
	struct config_node	node;
	int			in_a_map;
};

int
parse_do_nothing(yaml_event_t *event, struct site_config *conf,
	       struct config_node *node)
{
	struct top_level_parser *data = container_of(node, typeof(*data), node);

	switch (event->type) {
	case YAML_NO_EVENT:
	case YAML_STREAM_START_EVENT:
	case YAML_STREAM_END_EVENT:
	case YAML_DOCUMENT_START_EVENT:
	case YAML_DOCUMENT_END_EVENT:
	case YAML_ALIAS_EVENT:
		fatal("unexpected element at line %zu column %zu\n",
			       	event->start_mark.line,
				event->start_mark.column);
		break;
	case YAML_SCALAR_EVENT:
		break;
	case YAML_SEQUENCE_START_EVENT:
	case YAML_MAPPING_START_EVENT:
		data->in_a_map++;
		break;
	case YAML_SEQUENCE_END_EVENT:
	case YAML_MAPPING_END_EVENT:
		data->in_a_map--;
		break;
	};

	yaml_event_delete(event);
	if (!data->in_a_map)
		list_del(&node->node_entry);
	return 0;
}

struct top_level_parser empty = {
	.node		= {
		.parse		= parse_do_nothing,
	},
};

struct setpoints_parser {
	struct config_node	node;
	int			index;
	int			in_a_map;
	struct process_builder	*builder;
	void			*conf;
	char			buff[256];
};

int
parse_setpoints(yaml_event_t *event, struct site_config *conf,
	       struct config_node *node)
{
	struct setpoints_parser *data = container_of(node, typeof(*data), node);
	struct setpoint_map *map = NULL;
	int value;
	int i;
	const char *text;
       	char *bad;

	switch (event->type) {
	case YAML_NO_EVENT:
	case YAML_STREAM_START_EVENT:
	case YAML_STREAM_END_EVENT:
	case YAML_DOCUMENT_START_EVENT:
	case YAML_DOCUMENT_END_EVENT:
	case YAML_ALIAS_EVENT:
	case YAML_SEQUENCE_START_EVENT:
	case YAML_SEQUENCE_END_EVENT:
		fatal("unexpected element at line %zu column %zu\n",
			       	event->start_mark.line,
				event->start_mark.column);
		break;
	case YAML_SCALAR_EVENT:
		text = (const char *) event->data.scalar.value;
		if ((data->index % 2) == 0) {
			strncpy(data->buff, text, 255);
			data->index++;
			break;
		}
		value = (int) strtol(text, &bad, 0);
		if (bad[0] != 0)
			fatal("bad integer near %s\n", bad);
		for (i = 0; data->builder->setpoint[i].name; i++) {
			if (strcmp(data->builder->setpoint[i].name, data->buff))
				continue;
			map = &data->builder->setpoint[i];
			break;
		}
		if(!map)
			fatal("process does not expect setpoint '%s'\n",
				       data->buff);
		map->set(data->conf, value);
		data->index++;
		break;
	case YAML_MAPPING_START_EVENT:
		if (!data->builder || !data->builder->setpoint)
			fatal("process does not expect setpoints\n");
		data->in_a_map++;
		break;
	case YAML_MAPPING_END_EVENT:
		data->in_a_map--;
		break;
	};

	yaml_event_delete(event);
	if (data->in_a_map)
		return 0;
	list_del(&node->node_entry);
	xfree(data);
	return 0;
}

struct io_parser {
	struct config_node	node;
	int			index;
	int			in_a_map;
	struct process_builder	*builder;
	void			*conf;
	char			buff[256];
};

static int
find_io(const char *text, struct site_config *conf, int *type, int *index)
{
	char buff[16];
	int block;
	int slot;
	int i;
	int j;
	int offset = 0;
	sscanf(text, "%15[^][:]:%d:%d:%d", buff, &block, &slot, &i);
	debug("%s:%d:%d:%d\n", buff, block, slot, i);
	if (!strcmp("do", buff)) {
		*type = DO_MODULE;
		for (j = 0; conf->DO_mod[j].count > 0; j++) {
			if (conf->DO_mod[j].slot == slot) {
				if (i > conf->DO_mod[j].count)
					return 1;
				*index = offset + i;
				return 0;
			}
			offset += conf->DO_mod[j].count;
		}

	} else if (!strcmp("tr", buff)) {
		*type = AI_MODULE;
		for (j = 0; conf->AI_mod[j].count > 0; j++) {
			if (conf->AI_mod[j].slot == slot) {
				*index = offset + i - 1;
				return 0;
			}
			offset += conf->AI_mod[j].count;
		}
	} else if (!strcmp("ai", buff)) {
		*type = AI_MODULE;
		for (j = 0; conf->AI_mod[j].count > 0; j++) {
			if (conf->AI_mod[j].slot == slot) {
				*index = offset + i - 1;
				return 0;
			}
			offset += conf->AI_mod[j].count;
		}
	} else if (!strcmp("ao", buff)) {
		*type = AO_MODULE;
		for (j = 0; conf->AO_mod[j].count > 0; j++) {
			if (conf->AO_mod[j].slot == slot) {
				*index = offset + i - 1;
				return 0;
			}
			offset += conf->AO_mod[j].count;
		}
	} else if (!strcmp("di", buff)) {
		*type = DI_MODULE;
		for (j = 0; conf->DI_mod[j].count > 0; j++) {
			if (conf->DI_mod[j].slot == slot) {
				if (i > conf->DI_mod[j].count)
					return 1;
				*index = offset + i;
				return 0;
			}
			offset += conf->DI_mod[j].count;
		}
	} else if (!strcmp("ds", buff)) {
		*type = DI_STATUS;
		*index = block;
		return 0;
	}
	return 1;
}

int
parse_io(yaml_event_t *event, struct site_config *conf,
	       struct config_node *node)
{
	struct io_parser *data = container_of(node, typeof(*data), node);
	int type, index;
	int i;
	const char *text;
	int err;

	switch (event->type) {
	case YAML_NO_EVENT:
	case YAML_STREAM_START_EVENT:
	case YAML_STREAM_END_EVENT:
	case YAML_DOCUMENT_START_EVENT:
	case YAML_DOCUMENT_END_EVENT:
	case YAML_ALIAS_EVENT:
	case YAML_SEQUENCE_START_EVENT:
	case YAML_SEQUENCE_END_EVENT:
		fatal("unexpected element at line %zu column %zu\n",
			       	event->start_mark.line,
				event->start_mark.column);
		break;
	case YAML_SCALAR_EVENT:
		text = (const char *) event->data.scalar.value;
		if ((data->index % 2) == 0) {
			strncpy(data->buff, text, 255);
			data->index++;
			break;
		}
		err = find_io(text, conf, &type, &index);
		if (0 != err)
			fatal("bad address %s\n", text);
		for (i = 0; data->builder->io[i].name; i++) {
			if (strcmp(data->builder->io[i].name, data->buff))
				continue;
			break;
		}
		if(!data->builder->io[i].name)
			fatal("process does not expect io '%s'\n",
				       data->buff);
		data->builder->io[i].set(data->conf, type, index);
		data->index++;
		break;
	case YAML_MAPPING_START_EVENT:
		if (!data->builder || !data->builder->setpoint)
			fatal("process does not expect setpoints\n");
		data->in_a_map++;
		break;
	case YAML_MAPPING_END_EVENT:
		data->in_a_map--;
		break;
	};

	yaml_event_delete(event);
	if (data->in_a_map)
		return 0;
	list_del(&node->node_entry);
	xfree(data);
	return 0;
}

int
parse_fuzzy_if(struct fuzzy_clause *fcl, const char *key, const char *value)
{
	char *bad;
	int i;
	debug2("   %s:%s\n", key, value);
	switch (key[0]) {
	case 'v':
		i = (int) strtol(value, &bad, 0);
		if (0 != bad[0])
			return 1;
		fcl->var = i;
		break;
	case 'f':
		switch (value[0]) {
		case 'A':
			i = 0;
			break;
		case 'S':
			i = 1;
			break;
		case 'Z':
			i = 2;
			break;
		default:
			return 1;
		};
		fcl->h_f = i;
		break;
	case 'a':
		i = (int) strtol(value, &bad, 0);
		if (0 != bad[0])
			return 1;
		fcl->h_a = i;
		break;
	case 'b':
		i = (int) strtol(value, &bad, 0);
		if (0 != bad[0])
			return 1;
		fcl->h_b = i;
		break;
	case 'c':
		i = (int) strtol(value, &bad, 0);
		if (0 != bad[0])
			return 1;
		fcl->h_c = i;
		break;
	case 0:
	default:
		return 1;
	};

	return 0;
}

int
parse_fuzzy_then(struct fuzzy_clause *fcl, const char *key, const char *value)
{
	char *bad;
	int i;
	debug2("   %s:%s\n", key, value);
	switch (key[0]) {
	case 'f':
		switch (value[0]) {
		case 'A':
			i = 0;
			break;
		default:
			return 1;
		};
		fcl->m_f = i;
		break;
	case 'a':
		i = (int) strtol(value, &bad, 0);
		if (0 != bad[0])
			return 1;
		fcl->m_a = i;
		break;
	case 'b':
		i = (int) strtol(value, &bad, 0);
		if (0 != bad[0])
			return 1;
		fcl->m_b = i;
		break;
	case 'c':
		i = (int) strtol(value, &bad, 0);
		if (0 != bad[0])
			return 1;
		fcl->m_c = i;
		break;
	case 0:
	default:
		return 1;
	};
	return 0;
}

struct fuzzy_parser {
	struct config_node	node;
	int			in_a_map;
	int			in_a_seq;
	int			part;
	int			index;
	struct list_head	*fuzzy_list;
	struct fuzzy_clause	*fuzzy_clause;
	char			buff[256];
};

int
parse_fuzzy(yaml_event_t *event, struct site_config *conf,
	       struct config_node *node)
{
	struct fuzzy_parser *data = container_of(node, typeof(*data), node);
	const char *text;
	int err;

	switch (event->type) {
	case YAML_NO_EVENT:
	case YAML_STREAM_START_EVENT:
	case YAML_STREAM_END_EVENT:
	case YAML_DOCUMENT_START_EVENT:
	case YAML_DOCUMENT_END_EVENT:
	case YAML_ALIAS_EVENT:
		fatal("unexpected element at line %zu column %zu\n",
			       	event->start_mark.line,
				event->start_mark.column);
		break;
	case YAML_SCALAR_EVENT:
		text = (const char *) event->data.scalar.value;
		if ((data->index % 2) == 0) {
			strncpy(data->buff, text, 255);
			data->index++;
			break;
		}
		debug3("   %i\n", data->part);
		if ((data->part % 2) == 1) {
			err = parse_fuzzy_if(data->fuzzy_clause, data->buff,
				       	text);
		} else {
			err = parse_fuzzy_then(data->fuzzy_clause, data->buff,
				       	text);
		}
		if (0 != err)
			fatal("unexpected element at line %zu column %zu\n",
					event->start_mark.line,
					event->start_mark.column);
		data->index++;
		break;
	case YAML_MAPPING_START_EVENT:
		data->in_a_map++;
		if (data->in_a_map == 2) {
			data->part++;
			break;
		} else if (data->in_a_map == 1)
			data->fuzzy_clause =
				xzalloc(sizeof(*data->fuzzy_clause));
		else
			fatal("unexpected element at line %zu column %zu\n",
					event->start_mark.line,
					event->start_mark.column);
		break;
	case YAML_MAPPING_END_EVENT:
		data->in_a_map--;
		if (!data->in_a_map)
			list_add_tail(&data->fuzzy_clause->fuzzy_entry,
					data->fuzzy_list);
		break;
	case YAML_SEQUENCE_START_EVENT:
		data->in_a_seq++;
		break;
	case YAML_SEQUENCE_END_EVENT:
		data->in_a_seq--;
		break;
	};

	yaml_event_delete(event);
	if (data->in_a_seq)
		return 0;
	list_del(&node->node_entry);
	xfree(data);
	return 0;
}

struct process_map {
	const char		*name;
	struct process_builder	*(*builder)(void);
};

struct process_map v_builders[] = {
	{
		.name		= "2way",
		.builder	= load_2way_valve,
	},
	{
		.name		= "analog",
		.builder	= load_analog_valve,
	},
	{
	}
};

struct valve_parser {
	struct config_node	node;
	int			in_a_map;
	struct process_builder	*builder;
	struct process_builder	*v_builder;
	void			*conf;
	void			*p_conf;
};

int
parse_valve(yaml_event_t *event, struct site_config *conf,
	       struct config_node *node)
{
	struct valve_parser *data = container_of(node, typeof(*data), node);
	struct config_node *next;
	struct valve *v;
	const char *text;
	int i;

	switch (event->type) {
	case YAML_NO_EVENT:
	case YAML_STREAM_START_EVENT:
	case YAML_STREAM_END_EVENT:
	case YAML_DOCUMENT_START_EVENT:
	case YAML_DOCUMENT_END_EVENT:
	case YAML_SEQUENCE_START_EVENT:
	case YAML_SEQUENCE_END_EVENT:
	case YAML_ALIAS_EVENT:
		fatal("unexpected element at line %zu column %zu\n",
			       	event->start_mark.line,
				event->start_mark.column);
		break;
	case YAML_SCALAR_EVENT:
		text = (const char *)event->data.scalar.value;
		if (!data->v_builder) {
			if (data->in_a_map > 1)
				fatal("unexpected element at line %zu"
						" column %zu\n",
						event->start_mark.line,
						event->start_mark.column);
			for (i = 0; v_builders[i].name; i++) {
				if (strcmp(text, v_builders[i].name))
					continue;
				data->v_builder = v_builders[i].builder();
				data->conf = data->v_builder->alloc();
				break;
			}
			if (!data->v_builder) {
				fatal("unexpected element at line %zu"
						" column %zu\n",
						event->start_mark.line,
						event->start_mark.column);
			}
			break;
		}
		if (!strcmp(text, "setpoints")) {
			struct setpoints_parser *p = xzalloc(sizeof(*p));
			p->builder = data->v_builder;
			p->conf = data->conf;
			p->node.parse = parse_setpoints;
			next = &p->node; 
		} else if (!strcmp(text, "IO")) {
			struct io_parser *p = xzalloc(sizeof(*p));
			p->builder = data->v_builder;
			p->conf = data->conf;
			p->node.parse = parse_io;
			next = &p->node; 
		} else {
			empty.in_a_map = 0;
			next = &empty.node;
		}
		list_add(&next->node_entry, &node->node_entry);
		break;
	case YAML_MAPPING_START_EVENT:
		data->in_a_map++;
		break;
	case YAML_MAPPING_END_EVENT:
		data->in_a_map--;
		break;
	};

	yaml_event_delete(event);
	if (data->in_a_map)
		return 0;
	list_del(&node->node_entry);
	v = xzalloc(sizeof(*v));
	v->data = data->conf;
	v->ops = data->v_builder->ops(data->conf);
	if (!v->ops || !v->ops->adjust)
		fatal("bad valve ops at line %zu column %zu\n",
			       	event->start_mark.line,
				event->start_mark.column);
	data->builder->set_valve(data->p_conf, v);
	xfree(data);
	return 0;
}

struct process_parser {
	struct config_node	node;
	int			in_a_map;
	struct process_builder	*builder;
	void			*conf;
	char			buff[256];
	int			index;
	int			interval;
};

static void
set_process_interval(void *data, int value)
{
	struct process_parser *p = data;
	debug("  interval = %i\n", value);
	p->interval = value * 1000;
}

static struct setpoint_map process_setpoint[] = {
	{
		.name	= "interval",
		.set	= set_process_interval,
	},
};

int
parse_process(yaml_event_t *event, struct site_config *conf,
	       struct config_node *node)
{
	struct process_parser *data = container_of(node, typeof(*data), node);
	struct config_node *next = NULL;
	struct setpoint_map *map = NULL;
	struct process *pr;
	const char *text;
	int value;
	int i;
       	char *bad;

	switch (event->type) {
	case YAML_NO_EVENT:
	case YAML_STREAM_START_EVENT:
	case YAML_STREAM_END_EVENT:
	case YAML_DOCUMENT_START_EVENT:
	case YAML_DOCUMENT_END_EVENT:
	case YAML_SEQUENCE_START_EVENT:
	case YAML_SEQUENCE_END_EVENT:
	case YAML_ALIAS_EVENT:
		fatal("unexpected element at line %zu column %zu\n",
			       	event->start_mark.line,
				event->start_mark.column);
		break;
	case YAML_SCALAR_EVENT:
		text = (const char *)event->data.scalar.value;
		if (!strcmp(text, "setpoints")) {
			struct setpoints_parser *p = xzalloc(sizeof(*p));
			p->builder = data->builder;
			p->conf = data->conf;
			p->node.parse = parse_setpoints;
			next = &p->node; 
		} else if (!strcmp(text, "IO")) {
			struct io_parser *p = xzalloc(sizeof(*p));
			p->builder = data->builder;
			p->conf = data->conf;
			p->node.parse = parse_io;
			next = &p->node; 
		} else if (!strcmp(text, "fuzzy")) {
			struct fuzzy_parser *p = xzalloc(sizeof(*p));
			if (!data->builder->fuzzy)
				fatal("process doesn't support fuzzy logic\n");
			p->fuzzy_list = data->builder->fuzzy(data->conf);
			p->node.parse = parse_fuzzy;
			next = &p->node; 
		} else if (!strcmp(text, "valve")) {
			struct valve_parser *p = xzalloc(sizeof(*p));
			p->builder = data->builder;
			p->node.parse = parse_valve;
			p->p_conf = data->conf;
			next = &p->node; 
		}
		if (next) {
			list_add(&next->node_entry, &node->node_entry);
			break;
		}
		data->index++;
		if ((data->index % 2) == 1) {
			strncpy(data->buff, text, 255);
			break;
		}
		value = (int) strtol(text, &bad, 0);
		if (bad[0] != 0)
			fatal("bad integer near %s\n", bad);
		for (i = 0; process_setpoint[i].name; i++) {
			if (strcmp(process_setpoint[i].name, data->buff))
				continue;
			map = &process_setpoint[i];
			break;
		}
		if(!map)
			fatal("process does not expect setting '%s'\n",
				       data->buff);
		map->set(data, value);
		break;
	case YAML_MAPPING_START_EVENT:
		data->in_a_map++;
		if (data->in_a_map > 1)
			fatal("unexpected element at line %zu column %zu\n",
					event->start_mark.line,
					event->start_mark.column);
		data->index = 0;
		break;
	case YAML_MAPPING_END_EVENT:
		data->in_a_map--;
		break;
	};

	yaml_event_delete(event);
	if (data->in_a_map)
		return 0;
	list_del(&node->node_entry);
	pr = xzalloc(sizeof(*pr));
	pr->config = data->conf;
	pr->ops = data->builder->ops(data->conf);
	if (!pr->ops || !pr->ops->run)
		fatal("bad process ops at line %zu column %zu\n",
			       	event->start_mark.line,
				event->start_mark.column);
	memcpy(&pr->start, &conf->start, sizeof(pr->start));
	pr->interval = data->interval;
	if (pr->interval == 0)
		pr->interval = conf->interval;
	else if (pr->interval < conf->input_interval)
		conf->input_interval = pr->interval;
	else if (pr->interval > conf->log_interval)
		conf->log_interval = pr->interval;
	pr->type = PROCESS;
	queue_process(pr);
	xfree(data);
	return 0;
}

struct processes_parser {
	struct config_node	node;
	int			in_a_seq;
	int			in_a_map;
};

struct process_map builders[] = {
	{
		.name		= "heating",
		.builder	= load_heating_builder,
	},
	{
		.name		= "hot water",
		.builder	= load_hot_water_builder,
	},
	{
		.name		= "evaporation",
		.builder	= load_evaporation_builder,
	},
	{
		.name		= "cascade",
		.builder	= load_cascade_builder,
	},
	{
		.name		= "reference",
		.builder	= load_reference_builder,
	},
	{
		.name		= "relay",
		.builder	= load_relay_builder,
	},
	{
		.name		= "counter",
		.builder	= load_counter_builder,
	},
	{
	}
};

int
parse_processes(yaml_event_t *event, struct site_config *conf,
	       struct config_node *node)
{
	struct processes_parser *data = container_of(node, typeof(*data), node);
	struct process_parser *p;
	struct config_node *next = NULL;
	int i;

	switch (event->type) {
	case YAML_NO_EVENT:
	case YAML_STREAM_START_EVENT:
	case YAML_STREAM_END_EVENT:
	case YAML_DOCUMENT_START_EVENT:
	case YAML_DOCUMENT_END_EVENT:
	case YAML_ALIAS_EVENT:
		fatal("unexpected element at line %zu column %zu\n",
			       	event->start_mark.line,
				event->start_mark.column);
		break;
	case YAML_SCALAR_EVENT:
		if (data->in_a_map > 1 || data->in_a_seq > 1)
			fatal("unexpected element at line %zu column %zu\n",
					event->start_mark.line,
					event->start_mark.column);
		for (i = 0; builders[i].name; i++) {
			if (strcmp((const char *)event->data.scalar.value, 
						builders[i].name))
				continue;
			p = xzalloc(sizeof(*p));
			p->node.parse = parse_process;
			p->builder = builders[i].builder();
			p->conf = p->builder->alloc();
			next = &p->node;
			debug("configuring %s\n", event->data.scalar.value);
			break;
		}
		if (!next) {
			empty.in_a_map = 0;
			next = &empty.node;
		}
		list_add(&next->node_entry, &node->node_entry);
		break;
	case YAML_SEQUENCE_START_EVENT:
		data->in_a_seq++;
		break;
	case YAML_MAPPING_START_EVENT:
		if (data->in_a_seq != 1)
			fatal("unexpected element at line %zu column %zu\n",
					event->start_mark.line,
					event->start_mark.column);
		data->in_a_map++;
		break;
	case YAML_SEQUENCE_END_EVENT:
		data->in_a_seq--;
		break;
	case YAML_MAPPING_END_EVENT:
		data->in_a_map--;
		break;
	};

	yaml_event_delete(event);
	if (data->in_a_seq)
		return 0;
	list_del(&node->node_entry);
	xfree(data);
	return 0;
}

int
parse_top_level(yaml_event_t *event, struct site_config *conf,
	       struct config_node *node)
{
	int done = 0;
	struct top_level_parser *data = container_of(node, typeof(*data), node);
	struct config_node *next;
	switch (event->type) {
	case YAML_NO_EVENT:
	case YAML_STREAM_START_EVENT:
	case YAML_DOCUMENT_START_EVENT:
	case YAML_DOCUMENT_END_EVENT:
		break;
	case YAML_STREAM_END_EVENT:
		done = 1;
		break;
	case YAML_ALIAS_EVENT:
	case YAML_SEQUENCE_START_EVENT:
	case YAML_SEQUENCE_END_EVENT:
		fatal("unexpected element at line %zu column %zu\n",
			       	event->start_mark.line,
				event->start_mark.column);
		break;
	case YAML_SCALAR_EVENT:
		debug("configuring %s\n", event->data.scalar.value);
		if (!strcmp((const char *)event->data.scalar.value,
				       	"modules")) {
			struct modules_parser *m = xzalloc(sizeof(*m));
			m->node.parse = parse_modules;
			next = &m->node; 
		} else if (!strcmp((const char *)event->data.scalar.value,
				       	"processes")) {
			struct processes_parser *p = xzalloc(sizeof(*p));
			p->node.parse = parse_processes;
			next = &p->node; 
		} else {
			empty.in_a_map = 0;
			next = &empty.node;
		}
		list_add(&next->node_entry, &node->node_entry);
		break;
	case YAML_MAPPING_START_EVENT:
		if (data->in_a_map)
			fatal("unexpected element at line %zu column %zu\n",
					event->start_mark.line,
					event->start_mark.column);
		data->in_a_map = 1;
		break;
	case YAML_MAPPING_END_EVENT:
		data->in_a_map = 0;
		break;
	};

	yaml_event_delete(event);
	return done;
}


void
load_server_config(const char *filename, struct site_config *conf)
{
	yaml_parser_t parser;
	yaml_event_t event;
	FILE *f = fopen(filename, "r");
	struct top_level_parser top = {
		.node		= {
			.parse		= parse_top_level,
		},
		.in_a_map	= 0,
	};
	struct config_node *node;

	conf->interval = 10000000;
	conf->input_interval = 10000000;
	conf->log_interval = 10000000;
	gettimeofday(&conf->start, NULL);
	INIT_LIST_HEAD(&top.node.node_entry);

	if (NULL == f)
		fatal("failed to open %s (%s)\n", filename, strerror(errno));

	yaml_parser_initialize(&parser);

	yaml_parser_set_input_file(&parser, f);

	while (1) {
		if (!yaml_parser_parse(&parser, &event))
			fatal("failed to parse %s\n", filename);
		node = list_entry(top.node.node_entry.prev,
			       	struct config_node, node_entry);
		if (!node->parse)
			fatal("%s:%i internal error\n", __FILE__, __LINE__);

		if (node->parse(&event, conf, node))
			break;
	}

	yaml_parser_delete(&parser);
	fclose(f);
}
