/* servconf.h -- load configuration from a YAML file
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

#ifndef PCS_SERVCONF_H
#define PCS_SERVCONF_H

#include <yaml.h>
#include "list.h"

struct config_node {
	int			(*parse)(yaml_event_t *event,
		       			struct site_config *conf,
					struct config_node *node);
	struct list_head 	node_entry;
};

void
load_server_config(const char *filename, struct site_config *conf);

#endif /* PCS_SERVCONF_H */