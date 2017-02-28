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

#define PLUGIN_ID "gtk-gotr"

#include <stdarg.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <pidgin/gtkplugin.h>
#include <libpurple/debug.h>
#include <libpurple/version.h>

#include <gotr.h>
#include "gtk-ui.h"

static PidginPluginUiInfo ui_config = { gotrg_ui_create_conf_widget };

static void
gotrp_log(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	purple_debug_info(PLUGIN_ID, format, ap);
	va_end(ap);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	gotr_set_log_fn(&gotrp_log);
	if (!gotr_init())
		return FALSE;

	//TODO: setup signal handlers.
	purple_debug_info(PLUGIN_ID, "plugin loaded\n");
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	purple_debug_info(PLUGIN_ID, "plugin unloaded\n");
	return TRUE;
}

static PurplePluginInfo info = {
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	PURPLE_MINOR_VERSION,
	PURPLE_PLUGIN_STANDARD,
	PIDGIN_UI,
	0,
	NULL,
	PURPLE_PRIORITY_DEFAULT,

	PLUGIN_ID,
	"Group OTR",
	GOTRP_VERSION_MAJOR "." GOTRP_VERSION_MINOR,

	"GOTR Plugin",
	"GOTR Plugin",
	"Markus Teich <teichm@in.tum.de>",
	"https://github.com/schachmat/gotr",

	plugin_load,
	plugin_unload,
	NULL,

	&ui_config,
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
