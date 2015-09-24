/* The GOTR pidgin plugin
 * Copyright (C) 2015 Markus Teich
 *
 * This file is part of the GOTR pidgin plugin.
 *
 * The GOTR pidgin plugin is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * The GOTR pidgin plugin is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * the GOTR pidgin plugin.  If not, see <http://www.gnu.org/licenses/>.
 */

#define PURPLE_PLUGINS

#include <glib.h>

#include "notify.h"
#include "plugin.h"
#include "version.h"

static gboolean
plugin_load(PurplePlugin *plugin)
{
	purple_notify_message(plugin, PURPLE_NOTIFY_MSG_INFO, "Hi there!",
	                      "This is the gotr plugin", NULL, NULL, NULL);

	return TRUE;
}

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	NULL,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,

	"core-gotr",
	"Group OTR",
	GOTR_VERSION_MAJOR "." GOTR_VERSION_MINOR,

	"GOTR Plugin",
	"GOTR Plugin",
	"Markus Teich <teichm@in.tum.de>",
	"https://github.com/schachmat/gotr",

	plugin_load,
	NULL,
	NULL,

	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(PurplePlugin *plugin)
{
}

PURPLE_INIT_PLUGIN(hello_world, init_plugin, info)
