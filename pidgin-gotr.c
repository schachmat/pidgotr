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
#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <pidgin/gtkconv.h>
#include <pidgin/gtkconvwin.h>
#include <pidgin/gtkplugin.h>
#include <libpurple/account.h>
#include <libpurple/conversation.h>
#include <libpurple/debug.h>
#include <libpurple/version.h>

#include <gotr.h>
#include "gtk-ui.h"

static PidginPluginUiInfo ui_config = { gotrg_ui_create_conf_widget };

/** GOTR plugin handle */
static PurplePlugin *gotrph = NULL;

static void
gotrp_log_cb(const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	purple_debug_warning(PLUGIN_ID, format, ap);
	va_end(ap);
}

static int
gotrp_send_all_cb(void *room_closure,
                  const char *b64_msg)
{
	return 1;
}

static int
gotrp_send_user_cb(void *room_closure,
                   void *user_closure,
                   const char *b64_msg)
{
	return 1;
}

static void
gotrp_receive_user_cb(void *room_closure,
                      void *user_closure,
                      const char *plain_msg)
{
}

static void
sending_im(PurpleAccount *account,
           const char *receiver,
           char **message)
{
	purple_debug_info(PLUGIN_ID, "sending_im\n");
}

static gboolean
receiving_im(PurpleAccount *account,
             char **sender,
             char **message,
             PurpleConversation *conv,
             int *flags)
{
	purple_debug_info(PLUGIN_ID, "receiving_im: (flags %d) %s->%s: %s\n",
	                  *flags, conv ? conv->title : "(NULL)", *sender, *message);
	/**TODO: decrypt if possible */

	return FALSE;
}
static void
sending_chat(PurpleAccount *account,
             char **message,
             int id)
{
	purple_debug_info(PLUGIN_ID, "sending_chat\n");
}

static gboolean
receiving_chat(PurpleAccount *account,
               char **sender,
               char **message,
               PurpleConversation *conv,
               int *flags)
{
	if (*flags & PURPLE_MESSAGE_DELAYED) {
		purple_debug_misc(PLUGIN_ID,
		                  "ignoring received delayed message from %s in %s "
		                  "(probably replay from pidgin history): %s\n",
		                  *sender,
		                  conv->title,
		                  *message);
		return FALSE;
	}

	/* ignore own messages */
	if (*flags & PURPLE_MESSAGE_SEND)
		return FALSE;

	purple_debug_info(PLUGIN_ID, "receiving_chat: (flags %d) %s->%s: %s\n",
	                  *flags, conv->title, *sender, *message);
	/**TODO: decrypt if possible */

	return FALSE;
}

static void
chat_user_joined(PurpleConversation *conv,
                 const char *name,
                 PurpleConvChatBuddyFlags flags,
                 gboolean new_arrival)
{
	const size_t blen = 2048;
	char buf[blen];
	PurpleConversation *im_conv;
//	PidginConversation *pg_conv;
//	PidginWindow *pg_win;

	/* we don't need to handle ourselves joining the chat -> ignore signal */
	/**TODO: check if we can improve key exchange if the joining user also
	 * starts sending messages instead of waiting for other users. Use the
	 * strcmp version of the line to accept this signal for already existing
	 * other users in a chat we just joined ourselves. */
//	if (0 == strcmp(PURPLE_CONV_CHAT(conv)->nick, name))
	if (!new_arrival)
		return;

	if (blen <= snprintf(buf, blen, "%s/%s", conv->name, name)) {
		purple_debug_error(PLUGIN_ID, "chat_user_joined: buffer too small\n");
		return;
	}

	im_conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
	                                                buf,
	                                                conv->account);
	if (!im_conv) {
		/* We create a new IM (=1on1) conversation, but don't want it to be
		 * shown to the user. Thus we hide the conversation by temporarily
		 * changing the hide_new setting and emitting the received-im-msg which
		 * will create the conversation for us inside the hidden window. */
		const char *old = purple_prefs_get_string(PIDGIN_PREFS_ROOT
		                                          "/conversations/im/hide_new");
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new",
		                        "always");
		purple_signal_emit(purple_conversations_get_handle(),
		                   "received-im-msg",
		                   conv->account,
		                   buf,
		                   "lol ignore me",
		                   NULL,
		                   PURPLE_MESSAGE_INVISIBLE);
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new",
		                        old);
//		im_conv = purple_conversation_new(PURPLE_CONV_TYPE_IM,
//		                                  conv->account,
//		                                  buf);

		im_conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
		                                                buf,
		                                                conv->account);
		if (!im_conv) {
			purple_debug_error(PLUGIN_ID,
			                   "failed to create hidden conversation with %s\n",
			                   buf);
			return;
		}

//		if (PIDGIN_IS_PIDGIN_CONVERSATION(im_conv) &&
//		    (pg_conv = PIDGIN_CONVERSATION(im_conv)) &&
//		    (pg_win = pidgin_conv_get_window(pg_conv))) {
//			purple_debug_info(PLUGIN_ID, "hiding new conversation window\n");
//			pidgin_conv_window_hide(pg_win);
//		}
	}
	purple_conversation_set_logging(im_conv, FALSE);
	purple_conv_im_send_with_flags(PURPLE_CONV_IM(im_conv),
	                               new_arrival ? "hi newcomer" : "ancient one",
	                               PURPLE_MESSAGE_INVISIBLE);

	purple_debug_info(PLUGIN_ID, "chat_user_joined: %s->%s\n",
	                  conv->title, name);
}

static void
chat_user_left(PurpleConversation *conv,
               const char *name,
               const char *reason)
{
	purple_debug_info(PLUGIN_ID, "chat_user_left: %s->%s\n", conv->title, name);
}

static void
chat_joined(PurpleConversation *conv)
{
	purple_debug_info(PLUGIN_ID, "chat_joined: %s\n", conv->title);
}

static void
chat_left(PurpleConversation *conv)
{
	purple_debug_info(PLUGIN_ID, "chat_left: %s\n", conv->title);
}

static void
conv_created(PurpleConversation *conv)
{
	purple_debug_warning(PLUGIN_ID, "conv_created: name: %s\n", conv->name);
	purple_debug_warning(PLUGIN_ID, "conv_created: title: %s\n", conv->title);
	purple_debug_warning(PLUGIN_ID, "conv_created: type: %d\n", conv->type);
}

static void
conv_deleting(PurpleConversation *conv)
{
	purple_debug_warning(PLUGIN_ID, "conv_deleting: name: %s\n", conv->name);
	purple_debug_warning(PLUGIN_ID, "conv_deleting: title: %s\n", conv->title);
	purple_debug_warning(PLUGIN_ID, "conv_deleting: type: %d\n", conv->type);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	void *conv = purple_conversations_get_handle();

	gotr_set_log_fn(&gotrp_log_cb);
	if (!gotr_init())
		return FALSE;

	purple_signal_connect(conv, "sending-im-msg", plugin,
	                      PURPLE_CALLBACK(sending_im), NULL);
	purple_signal_connect(conv, "receiving-im-msg", plugin,
	                      PURPLE_CALLBACK(receiving_im), NULL);
	purple_signal_connect(conv, "sending-chat-msg", plugin,
	                      PURPLE_CALLBACK(sending_chat), NULL);
	purple_signal_connect(conv, "receiving-chat-msg", plugin,
	                      PURPLE_CALLBACK(receiving_chat), NULL);
	purple_signal_connect(conv, "chat-buddy-joined", plugin,
	                      PURPLE_CALLBACK(chat_user_joined), NULL);
	purple_signal_connect(conv, "chat-buddy-left", plugin,
	                      PURPLE_CALLBACK(chat_user_left), NULL);
	purple_signal_connect(conv, "chat-joined", plugin,
	                      PURPLE_CALLBACK(chat_joined), NULL);
	purple_signal_connect(conv, "chat-left", plugin,
	                      PURPLE_CALLBACK(chat_left), NULL);
	purple_signal_connect(conv, "conversation-created", plugin,
	                      PURPLE_CALLBACK(conv_created), NULL);
	purple_signal_connect(conv, "deleting-conversation", plugin,
	                      PURPLE_CALLBACK(conv_deleting), NULL);

	gotrph = plugin;
	purple_debug_info(PLUGIN_ID, "plugin loaded\n");
	return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
	void *conv = purple_conversations_get_handle();

	purple_signal_disconnect(conv, "sending-im-msg", gotrph,
	                         PURPLE_CALLBACK(sending_im));
	purple_signal_disconnect(conv, "receiving-im-msg", gotrph,
	                         PURPLE_CALLBACK(receiving_im));
	purple_signal_disconnect(conv, "sending-chat-msg", gotrph,
	                         PURPLE_CALLBACK(sending_chat));
	purple_signal_disconnect(conv, "receiving-chat-msg", gotrph,
	                         PURPLE_CALLBACK(receiving_chat));
	purple_signal_disconnect(conv, "chat-buddy-joined", plugin,
	                         PURPLE_CALLBACK(chat_user_joined));
	purple_signal_disconnect(conv, "chat-buddy-left", plugin,
	                         PURPLE_CALLBACK(chat_user_left));
	purple_signal_disconnect(conv, "chat-joined", plugin,
	                         PURPLE_CALLBACK(chat_joined));
	purple_signal_disconnect(conv, "chat-left", plugin,
	                         PURPLE_CALLBACK(chat_left));
	purple_signal_disconnect(conv, "conversation-created", plugin,
	                         PURPLE_CALLBACK(conv_created));
	purple_signal_disconnect(conv, "deleting-conversation", plugin,
	                         PURPLE_CALLBACK(conv_deleting));

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
