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
#include <time.h>

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

struct gotrp_room {
	struct gotr_chatroom *room;
	GHashTable *users;
	GHashTable *userstrs;
};

/** internal room mapping from (PurpleConversation*) to (struct gotrp_room*) */
static GHashTable *gotrp_rooms = NULL;

static void
gotrp_log_cb(const char *format, ...)
{
	va_list ap;
	const size_t mlen = 2048;
	char msg[mlen];

	va_start(ap, format);
	vsnprintf(msg, mlen, format, ap);
	va_end(ap);
	msg[mlen-1] = '\0';

	purple_debug_warning(PLUGIN_ID, "%s", msg);
}

static int
gotrp_send_all_cb(void *room_closure,
                  const char *b64_msg)
{
	gboolean logging;
	PurpleConvChat *chat_conv;

	if (!(chat_conv = PURPLE_CONV_CHAT(room_closure))) {
		purple_debug_error(PLUGIN_ID, "send_all_cb: broken room_closure\n");
		return 0;
	}

	/* temporarily disable logging for hidden messages */
	logging = purple_conversation_is_logging(room_closure);
	purple_conversation_set_logging(room_closure, FALSE);
	purple_conv_chat_send_with_flags(chat_conv,
	                                 b64_msg,
	                                 PURPLE_MESSAGE_INVISIBLE);
	purple_conversation_set_logging(room_closure, logging);
	return 1;
}

static int
gotrp_send_user_cb(void *room_closure,
                   void *user_closure,
                   const char *b64_msg)
{
	const size_t blen = 2048;
	char buf[blen];
	gboolean logging;
	PurpleConversation *conv = room_closure;
	PurpleConvChat *chat_conv;
	PurpleConversation *im_conv;

	if (!(chat_conv = PURPLE_CONV_CHAT(conv))) {
		purple_debug_error(PLUGIN_ID, "send_user_cb: broken room_closure\n");
		return 0;
	}

	/* BEWARE: we rely on pidgin to use "chatroom/nickname" to refer to private
	 * conversations with users in a chat room */
	if (blen <= snprintf(buf,
	                     blen,
	                     "%s/%s",
	                     conv->name,
	                     (char *)user_closure)) {
		purple_debug_error(PLUGIN_ID, "send_user_cb: buffer too small\n");
		return 0;
	}

	im_conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
	                                                buf,
	                                                conv->account);
	if (!im_conv) {
		/* We create a new IM (=1on1) conversation, but don't want it to be
		 * shown to the user. Thus we hide the conversation by temporarily
		 * changing the hide_new setting and emitting the received-im-msg which
		 * will create the conversation for us inside the hidden window. */
		char *old;
		old = g_strdup(purple_prefs_get_string(PIDGIN_PREFS_ROOT
		                                       "/conversations/im/hide_new"));
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new",
		                        "always");
		purple_signal_emit(purple_conversations_get_handle(),
		                   "received-im-msg",
		                   conv->account,
		                   buf,
		                   "lol ignore me", /* unused setup message needed */
		                   NULL,
		                   PURPLE_MESSAGE_INVISIBLE);
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new",
		                        old);
		g_free(old);
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
			return 0;
		}
	}

	/* temporarily disable logging for hidden messages */
	logging = purple_conversation_is_logging(im_conv);
	purple_conversation_set_logging(im_conv, FALSE);
	purple_conv_im_send_with_flags(PURPLE_CONV_IM(im_conv),
	                               b64_msg,
	                               PURPLE_MESSAGE_INVISIBLE);
	purple_conversation_set_logging(im_conv, logging);

	return 1;
}

static void
gotrp_receive_user_cb(void *room_closure,
                      void *user_closure,
                      const char *plain_msg)
{
	PurpleConvChat *chat_conv;

	if (!(chat_conv = PURPLE_CONV_CHAT(room_closure))) {
		purple_debug_error(PLUGIN_ID, "recv_user_cb: broken room_closure\n");
		return;
	}

	purple_conv_chat_write(chat_conv,
	                       (const char *)user_closure,
	                       plain_msg,
	                       PURPLE_MESSAGE_RECV,
	                       time(NULL));
}

static gboolean
receiving_im(PurpleAccount *account,
             char **sender,
             char **message,
             PurpleConversation *conv,
             int *flags)
{
	struct gotrp_room *pr;
	struct gotr_user *user;
	char *src = g_strdup(*sender);
	char *usr, *div;
	PurpleConversation *cconv;

	purple_debug_info(PLUGIN_ID, "receiving_im: (flags %d) %s->%s: %s\n",
	                  *flags, conv ? conv->title : "(NULL)", *sender, *message);

	/* BEWARE: we rely on pidgin to use "chatroom/nickname" to refer to private
	 * conversations with users in a chat room */
	if (!(div = strchr(src, '/'))) {
		purple_debug_misc(PLUGIN_ID, "im not from a chat\n");
		g_free(src);
		return FALSE;
	}

	*div = '\0';

	if (!(cconv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
	                                                    src,
	                                                    account))) {
		purple_debug_misc(PLUGIN_ID, "could not find matching chat conv\n");
		g_free(src);
		return FALSE;
	}

	if (!(pr = g_hash_table_lookup(gotrp_rooms, cconv))) {
		purple_debug_misc(PLUGIN_ID, "gotr not enabled in this chat\n");
		/**TODO: automatically enable gotr if wanted by settings */
		g_free(src);
		return FALSE;
	}

	usr = g_strdup(++div);
	if (!purple_conv_chat_find_user(PURPLE_CONV_CHAT(cconv), usr)) {
		purple_debug_warning(PLUGIN_ID, "user not in chat: %s\n", usr);
		g_free(src);
		g_free(usr);
		return FALSE;
	}

	/* check if we already know the user */
	if ((user = g_hash_table_lookup(pr->users, usr))) {
		if (!gotr_receive_user(pr->room, user, usr, *message)) {
			purple_debug_misc(PLUGIN_ID, "got malformed gotr message\n");
			g_free(src);
			g_free(usr);
			return FALSE;
		}
		if (!g_hash_table_add(pr->userstrs, usr))
			/* unreachable */
			purple_debug_warning(PLUGIN_ID, "unreachable: userstr replaced\n");
		g_free(src);
		return TRUE;
	}

	/* we don't know the user yet, activate! */
	purple_debug_misc(PLUGIN_ID, "enabling gotr for user %s\n", usr);
	if (!(user = gotr_receive_user(pr->room, NULL, usr, *message))) {
		purple_debug_misc(PLUGIN_ID, "no valid gotr message\n");
		g_free(src);
		g_free(usr);
		return FALSE;
	}

	if (!g_hash_table_replace(pr->users, g_strdup(usr), user))
		/* unreachable */
		purple_debug_warning(PLUGIN_ID, "unreachable: user replaced\n");
	if (!g_hash_table_add(pr->userstrs, usr))
		/* unreachable */
		purple_debug_warning(PLUGIN_ID, "unreachable: userstr replaced\n");
	g_free(src);
	return TRUE;

	/* unreachable, just in case… */
	return FALSE;
}

static void
sending_chat(PurpleAccount *account,
             char **message,
             int id)
{
	struct gotrp_room *pr;
	PurpleConversation *conv;
	PurpleConnection *gc;

	/* ignore gotr messages to prevent loop */
	if (!strncmp("?GOTR?", *message, strlen("?GOTR?")))
		return;

	purple_debug_info(PLUGIN_ID, "sending_chat: %s\n", *message);

	if (!(gc = purple_account_get_connection(account))) {
		purple_debug_error(PLUGIN_ID, "failed to derive connection\n");
		return;
	}

	if (!(conv = purple_find_chat(gc, id))) {
		purple_debug_error(PLUGIN_ID, "failed to find conversation by id\n");
		return;
	}

	if (!(pr = g_hash_table_lookup(gotrp_rooms, conv))) {
		purple_debug_misc(PLUGIN_ID, "gotr not enabled in this chat\n");
		return;
	}

	if (gotr_send(pr->room, *message)) {
		free(*message);
		*message = NULL;
	}
}

static gboolean
receiving_chat(PurpleAccount *account,
               char **sender,
               char **message,
               PurpleConversation *conv,
               int *flags)
{
	struct gotrp_room *pr;

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

	if (!(pr = g_hash_table_lookup(gotrp_rooms, conv))) {
		purple_debug_misc(PLUGIN_ID, "gotr not enabled in this chat\n");
		return FALSE;
	}

	return gotr_receive(pr->room, *message);
}

static void
chat_user_joined(PurpleConversation *conv,
                 const char *name,
                 PurpleConvChatBuddyFlags flags,
                 gboolean new_arrival)
{
	struct gotrp_room *pr;
	struct gotr_user *user;
	char *usr;

	/* we don't need to handle ourselves joining the chat -> ignore signal */
	/**TODO: check if we can improve key exchange if the joining user also
	 * starts sending messages instead of waiting for other users. Use the
	 * strcmp version of the line to accept this signal for already existing
	 * other users in a chat we just joined ourselves. */
//	if (0 == strcmp(PURPLE_CONV_CHAT(conv)->nick, name))
	if (!new_arrival)
		return;

	purple_debug_info(PLUGIN_ID, "chat_user_joined: %s->%s\n",
	                  conv->title, name);

	if (!(pr = g_hash_table_lookup(gotrp_rooms, conv))) {
		purple_debug_misc(PLUGIN_ID, "gotr not enabled in this chat\n");
		return;
	}

	if (!(usr = g_strdup(name))) {
		purple_debug_error(PLUGIN_ID, "unable to malloc username\n");
		return;
	}

	if (!(user = gotr_user_joined(pr->room, usr))) {
		purple_debug_error(PLUGIN_ID, "libgotr could not let user join\n");
		g_free(usr);
		return;
	}

	if (!g_hash_table_add(pr->userstrs, usr)) {
		/* unreachable */
		purple_debug_warning(PLUGIN_ID, "unreachable: userstr replaced\n");
		return;
	}

	if (!g_hash_table_replace(pr->users, g_strdup(name), user)) {
		/* unreachable */
		purple_debug_warning(PLUGIN_ID, "unreachable: user replaced\n");
		return;
	}
}

static void
chat_user_left(PurpleConversation *conv,
               const char *name,
               const char *reason)
{
	struct gotrp_room *pr;
	struct gotr_user *user;

	purple_debug_info(PLUGIN_ID, "chat_user_left: %s->%s\n", conv->title, name);

	if (!(pr = g_hash_table_lookup(gotrp_rooms, conv))) {
		purple_debug_misc(PLUGIN_ID, "gotr not enabled in this chat\n");
		return;
	}

	if (!(user = g_hash_table_lookup(pr->users, name))) {
		purple_debug_misc(PLUGIN_ID, "gotr not enabled for leaving user\n");
		return;
	}

	gotr_user_left(pr->room, user);
}

static void
chat_joined(PurpleConversation *conv)
{
	struct gotrp_room *pr;

	purple_debug_info(PLUGIN_ID, "chat_joined: %s\n", conv->title);

	if (!(pr = calloc(1, sizeof(struct gotrp_room)))) {
		purple_debug_error(PLUGIN_ID, "failed to alloc memory for room\n");
		return;
	}

	if (!(pr->users = g_hash_table_new_full(&g_str_hash,
	                                        &g_str_equal,
	                                        &g_free,
	                                        NULL))) {
		purple_debug_error(PLUGIN_ID, "unable to create users hash table\n");
		free(pr);
		return;
	}

	if (!(pr->userstrs = g_hash_table_new_full(&g_direct_hash,
	                                           &g_direct_equal,
	                                           &g_free,
	                                           NULL))) {
		purple_debug_error(PLUGIN_ID, "unable to create userstrs hash table\n");
		g_hash_table_destroy(pr->users);
		free(pr);
		return;
	}

	pr->room = gotr_join(&gotrp_send_all_cb,
	                     &gotrp_send_user_cb,
	                     &gotrp_receive_user_cb,
	                     conv,
	                     NULL); /* autogen privkey for now */
	if (!pr->room) {
		purple_debug_error(PLUGIN_ID, "got NULL room when joining\n");
		g_hash_table_destroy(pr->users);
		g_hash_table_destroy(pr->userstrs);
		free(pr);
		return;
	}

	if (!g_hash_table_insert(gotrp_rooms, conv, pr)) {
		/* unreachable */
		purple_debug_warning(PLUGIN_ID, "unreachable: chat replaced\n");
	}
}

static void
chat_left(PurpleConversation *conv)
{
	purple_debug_info(PLUGIN_ID, "chat_left: %s\n", conv->title);

	if (!g_hash_table_remove(gotrp_rooms, conv)) {
		purple_debug_error(PLUGIN_ID, "could not find room to leave\n");
		return;
	}
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

static void
destroy_room(gpointer data)
{
	struct gotrp_room *pr = data;

	g_hash_table_destroy(pr->users);
	g_hash_table_destroy(pr->userstrs);
	gotr_leave(pr->room);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
	void *conv = purple_conversations_get_handle();

	gotr_set_log_fn(&gotrp_log_cb);
	if (!gotr_init()) {
		purple_debug_error(PLUGIN_ID, "failed to initialize libgotr\n");
		return FALSE;
	}

	if (!(gotrp_rooms = g_hash_table_new_full(&g_direct_hash,
	                                          &g_direct_equal,
	                                          NULL,
	                                          &destroy_room))) {
		purple_debug_error(PLUGIN_ID, "failed to create room hash table\n");
		return FALSE;
	}

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

	g_hash_table_destroy(gotrp_rooms);

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
