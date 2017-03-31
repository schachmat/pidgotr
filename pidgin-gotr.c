/* The GOTR pidgin plugin
 * Copyright (C) 2017 Markus Teich
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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <libpurple/account.h>
#include <libpurple/conversation.h>
#include <libpurple/core.h>
#include <libpurple/debug.h>
#include <libpurple/version.h>
#include <pidgin/gtkconv.h>
#include <pidgin/gtkconvwin.h>
#include <pidgin/gtkimhtml.h>
#include <pidgin/gtkplugin.h>

#include <gotr.h>

#include "icons.h"

struct gotrp_room {
	struct gotr_chatroom *room;
	GHashTable           *users;

	/** caches messages encrypted with GOTR */
	GList *msgsGreen;

	/** caches messages not encrypted with GOTR */
	GList *msgsRed;
};


/** track if current message is from user or generated by the gotr plugin */
static gboolean gotrpOriginatedMsg = FALSE;

/** internal room mapping from (PurpleConversation*) to (struct gotrp_room*) */
static GHashTable *gotrpRooms = NULL;

/** cache for libgotr-generated group chat messages to prevent displaying them
 * in the senders chat window. */
static GList *gotrpMsgCache = NULL;

/** id of the image to display with the next message */
static int imgNextID = 0;

/** id of the green lock icon image id */
static int imgGreenID = 0;

/** id of the yellow lock icon image id */
static int imgYellowID = 0;

/** id of the red lock icon image id */
static int imgRedID = 0;

/** child PID of keygen process */
static pid_t genpid = -1;

/** where to put our "keygen done" message */
static PurpleConversation *gotrKeygenDoneConv = NULL;


static void
onGotrLog(const char *format,
          ...)
{
	va_list      ap;
	const size_t mlen = 2048;
	char         msg[mlen];

	va_start(ap, format);
	vsnprintf(msg, mlen, format, ap);
	va_end(ap);
	msg[mlen - 1] = '\0';

	purple_debug_warning(PLUGIN_ID, "%s", msg);
}


static int
onGotrSendAll(void       *roomClosure,
              const char *b64Msg)
{
	PurpleConvChat *chatConv;

	if (!(chatConv = PURPLE_CONV_CHAT(roomClosure))) {
		purple_debug_error(PLUGIN_ID, "send_all_cb: broken roomClosure\n");
		return 0;
	}

	gotrpMsgCache = g_list_prepend(gotrpMsgCache, g_strdup(b64Msg));

	purple_conv_chat_send_with_flags(chatConv,
	                                 b64Msg,
	                                 PURPLE_MESSAGE_INVISIBLE |
	                                 PURPLE_MESSAGE_NO_LOG);
	return 1;
}


static int
onGotrSendUser(void       *roomClosure,
               void       *userClosure,
               const char *b64Msg)
{
	const size_t       blen = 2048;
	char               buf[blen];
	PurpleConversation *conv = roomClosure;
	PurpleConvChat     *chatConv;
	PurpleConversation *imConv;

	if (!(chatConv = PURPLE_CONV_CHAT(conv))) {
		purple_debug_error(PLUGIN_ID, "send_user_cb: broken roomClosure\n");
		return 0;
	}

	/* BEWARE: we rely on pidgin to use "chatroom/nickname" to refer to private
	 * conversations with users in a chat room */
	if (blen <= snprintf(buf,
	                     blen,
	                     "%s/%s",
	                     conv->name,
	                     (char *)userClosure)) {
		purple_debug_error(PLUGIN_ID, "send_user_cb: buffer too small\n");
		return 0;
	}

	imConv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
	                                               buf,
	                                               conv->account);
	if (!imConv) {
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
		                   "lol ignore me",  /* unused setup message needed */
		                   NULL,
		                   PURPLE_MESSAGE_INVISIBLE | PURPLE_MESSAGE_NO_LOG);
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/conversations/im/hide_new",
		                        old);
		g_free(old);

		imConv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
		                                               buf,
		                                               conv->account);
		if (!imConv) {
			purple_debug_error(PLUGIN_ID,
			                   "failed to create hidden conversation with %s\n",
			                   buf);
			return 0;
		}
	}

	purple_conv_im_send_with_flags(PURPLE_CONV_IM(imConv),
	                               b64Msg,
	                               PURPLE_MESSAGE_INVISIBLE |
	                               PURPLE_MESSAGE_NO_LOG);

	return 1;
}


static void
onGotrRecvUser(void       *roomClosure,
               void       *userClosure,
               const char *plainMsg)
{
	PurpleConvChat    *chatConv;
	struct gotrp_room *pr;

	if (!(chatConv = PURPLE_CONV_CHAT(roomClosure))) {
		purple_debug_error(PLUGIN_ID, "recv_user_cb: broken roomClosure\n");
		return;
	}

	if (!(pr = g_hash_table_lookup(gotrpRooms, roomClosure))) {
		purple_debug_misc(PLUGIN_ID, "gotr not enabled in this chat\n");
		return;
	}

	pr->msgsGreen = g_list_prepend(pr->msgsGreen, g_strdup(plainMsg));
	purple_conv_chat_write(chatConv,
	                       (const char *)userClosure,
	                       plainMsg,
	                       PURPLE_MESSAGE_RECV,
	                       time(NULL));
}


GtkWidget *
buildConfWidget(PurplePlugin *plugin)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 5);
	GtkWidget *notebook = gtk_notebook_new();
	GtkWidget *configbox = gtk_vbox_new(FALSE, 5);
	GtkWidget *fingerprintbox = gtk_vbox_new(FALSE, 5);

//	gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);
	gtk_container_set_border_width(GTK_CONTAINER(configbox), 5);
	gtk_container_set_border_width(GTK_CONTAINER(fingerprintbox), 5);

	//TODO

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
	                         configbox,
	                         gtk_label_new("Config"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
	                         fingerprintbox,
	                         gtk_label_new("Known fingerprints"));

	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

	gtk_widget_show_all(vbox);
	return vbox;
}


gboolean
checkKeygenDone(gpointer data)
{
	int status = 0;
	pid_t ret;

	errno = 0;
	if (0 == (ret = waitpid(genpid, &status, WNOHANG)))
		return TRUE; /* still running, check back in another second */

	if (0 != errno && ECHILD != errno)
		purple_debug_error(PLUGIN_ID,
		                   "checkKeygenDone: waitpid: %s\n",
		                   strerror(errno));
	else if (genpid == ret && (!WIFEXITED(status) || 0 != WEXITSTATUS(status)))
		/* this will probably never be called since pidgin seems to SIG_IGN the
		 * SIGCHLD and thus we don't get the exit status */
		purple_debug_error(PLUGIN_ID,
		                   "keygen process returned error code: %d\n",
		                   WEXITSTATUS(status));

	/* close dialog even on errors so users can resume using pidgin */
	gtk_widget_destroy(data);
	genpid = -1;

	/* inform user */
	if (gotrKeygenDoneConv) {
		purple_conversation_write(gotrKeygenDoneConv,
		                          NULL,
		                          "GOTR key generation done!\n"
		                          "Please rejoin this chat to enable GOTR.",
		                          PURPLE_MESSAGE_SYSTEM,
		                          time(NULL));
	}
	gotrKeygenDoneConv = NULL;
	return FALSE;
}


void
showGenkeyDialog()
{
	GtkWidget *d;

	d = gtk_message_dialog_new(NULL,
	                           0,
	                           GTK_MESSAGE_INFO,
	                           GTK_BUTTONS_NONE,
	                           "Please wait until your secret key is ready.");
	if (!d)
		purple_debug_warning(PLUGIN_ID, "could not create genkey dialog\n");
	else
		gtk_widget_show(d);

	/* check every second if we are done generating our key */
	g_timeout_add(1000, &checkKeygenDone, d);
}


char *
strToHex(const char *buffer) {
	char *ret;
	size_t buffer_length = strlen(buffer);

	if (!(ret = calloc(2 * buffer_length + 1, 1)))
		return NULL;

	for (size_t i = 0; i < buffer_length; i++)
		snprintf(&ret[2*i], 3, "%02X", buffer[i]);

	return ret;
}


gchar *
buildKeyFname(PurpleAccount *acc)
{
	gchar *aname;
	gchar *fname;
	gchar *ret;

	if (!acc || !acc->username) {
		purple_debug_error(PLUGIN_ID, "unable to derive account name\n");
		return NULL;
	}

	if (!(aname = strToHex(acc->username))) {
		purple_debug_warning(PLUGIN_ID, "buildKeyFname: out of memory\n");
		return NULL;
	}

	if (!(fname = g_strdup_printf("gotr_%s.skey", aname))) {
		purple_debug_warning(PLUGIN_ID, "buildKeyFname: out of memory 2\n");
		free(aname);
		return NULL;
	}
	free(aname);

	if (!(ret = g_build_filename(purple_user_dir(), fname, NULL))) {
		purple_debug_warning(PLUGIN_ID, "buildKeyFname: out of memory 3\n");
		g_free(fname);
		return NULL;
	}
	g_free(fname);

	return ret;
}


void
createPrivkey(PurpleAccount *acc)
{
	gchar *afname;

	if (-1 != genpid) {
		purple_debug_warning(PLUGIN_ID, "can only gen one key at a time\n");
		return;
	}

	if (!(afname = buildKeyFname(acc))) {
		purple_debug_error(PLUGIN_ID, "buildKeyFname failed\n");
		return;
	}

	switch (genpid = fork()) {
	case -1: /* error */
		purple_debug_error(PLUGIN_ID, "unable to fork keygen process\n");
		break;
	case 0: /* child */
		execlp("gotr_genkey", "gotr_genkey", afname, (char *)NULL);
		/* can not log from new proc. die and log after waitpid() in parent */
		_exit(1);
	default: /* parent */
		showGenkeyDialog(genpid);
	}

	g_free(afname);
}


static gboolean
onReceivingIM(PurpleAccount      *account,
              char               **sender,
              char               **message,
              PurpleConversation *conv,
              int                *flags)
{
	struct gotrp_room  *pr;
	struct gotr_user   *user;
	char               *src = g_strdup(*sender);
	char               *usr;
	char               *div;
	PurpleConversation *cconv;

	purple_debug_info(PLUGIN_ID, "onReceivingIM: (flags %d) %s->%s: %s\n",
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

	if (!(pr = g_hash_table_lookup(gotrpRooms, cconv))) {
		purple_debug_misc(PLUGIN_ID, "gotr not enabled in this chat\n");
		/**TODO: automatically enable gotr if wanted by settings */
		g_free(src);
		return FALSE;
	}

	usr = ++div;
	if (!purple_conv_chat_find_user(PURPLE_CONV_CHAT(cconv), usr)) {
		purple_debug_warning(PLUGIN_ID, "user not in chat: %s\n", usr);
		g_free(src);
		return FALSE;
	}

	/* make copy which can be safely passed to gotr_receive_user */
	usr = g_strdup(usr);

	/* check if we already know the user */
	if ((user = g_hash_table_lookup(pr->users, usr))) {
		if (!gotr_receive_user(pr->room, user, usr, *message)) {
			purple_debug_misc(PLUGIN_ID, "got malformed gotr message\n");
			g_free(src);
			g_free(usr);
			return FALSE;
		}
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
	g_free(src);
	return TRUE;

	/* unreachable, just in case… */
	return FALSE;
}


static void
onSendingChat(PurpleAccount *account,
              char          **message,
              int           id)
{
	struct gotrp_room  *pr;
	PurpleConversation *conv;
	PurpleConnection   *gc;

	/* ignore gotr messages to prevent loop */
	if (gotrpOriginatedMsg)
		return;

	purple_debug_info(PLUGIN_ID, "onSendingChat: %s\n", *message);

	if (!(gc = purple_account_get_connection(account))) {
		purple_debug_error(PLUGIN_ID, "failed to derive connection\n");
		return;
	}

	if (!(conv = purple_find_chat(gc, id))) {
		purple_debug_error(PLUGIN_ID, "failed to find conversation by id\n");
		return;
	}

	if (!(pr = g_hash_table_lookup(gotrpRooms, conv))) {
		purple_debug_misc(PLUGIN_ID, "gotr not enabled in this chat\n");
		return;
	}

	gotrpOriginatedMsg = TRUE;
	if (gotr_send(pr->room, *message)) {
		pr->msgsGreen = g_list_prepend(pr->msgsGreen, g_strdup(*message));
		purple_conv_chat_write(PURPLE_CONV_CHAT(conv),
		                       PURPLE_CONV_CHAT(conv)->nick,
		                       *message,
		                       PURPLE_MESSAGE_SEND,
		                       time(NULL));
		free(*message);
		*message = NULL;
	} else {
		pr->msgsRed = g_list_prepend(pr->msgsRed, g_strdup(*message));
	}
	gotrpOriginatedMsg = FALSE;
}


static gboolean
onWritingChat(PurpleAccount      *account,
              const char         *who,
              char               **message,
              PurpleConversation *conv,
              PurpleMessageFlags flags)
{
	GList *elem;

	if ((elem = g_list_find_custom(gotrpMsgCache,
	                               *message,
	                               (GCompareFunc)strcmp))) {
		purple_debug_misc(PLUGIN_ID, "not writing to %s: %s\n",
		                  conv->title, *message);
		g_free(elem->data);
		gotrpMsgCache = g_list_delete_link(gotrpMsgCache, elem);
		return TRUE;
	}

	purple_debug_misc(PLUGIN_ID, "writing to %s: %s\n", conv->title, *message);
	return FALSE;
}


static gboolean
onReceivingChat(PurpleAccount      *account,
                char               **sender,
                char               **message,
                PurpleConversation *conv,
                int                *flags)
{
	struct gotrp_room *pr;

	if (*flags & PURPLE_MESSAGE_DELAYED) {
		purple_debug_misc(PLUGIN_ID,
		                  "ignoring received delayed message from %s in %s "
		                  "(probably replay from pidgin history): %s\n",
		                  *sender,
		                  conv->title,
		                  *message);
		/* ignore messages starting with "?GOTR?". We can't say for sure if they
		 * are actual GOTR messages, but taking the chance of some
		 * false-positive matches hidden from the user is better than confusing
		 * them with the ?GOTR? messages each time they join a chat where the
		 * past messages are resent. */
		return 0 == strncmp("?GOTR?", *message, strlen("?GOTR?"));
	}

	/* ignore own messages */
	if (*flags & PURPLE_MESSAGE_SEND) {
		purple_debug_misc(PLUGIN_ID, "ignoring own msg: %s\n", *message);
		return FALSE;
	}

	if (!(pr = g_hash_table_lookup(gotrpRooms, conv))) {
		purple_debug_misc(PLUGIN_ID, "gotr not enabled in this chat\n");
		return FALSE;
	}

	purple_debug_info(PLUGIN_ID, "onReceivingChat: (flags %d) %s->%s: %s\n",
	                  *flags, conv->title, *sender, *message);

	if (gotr_receive(pr->room, *message)) {
		return TRUE;
	} else {
		pr->msgsRed = g_list_prepend(pr->msgsRed, g_strdup(*message));
		return FALSE;
	}
}


static void
onChatUserJoined(PurpleConversation       *conv,
                 const char               *name,
                 PurpleConvChatBuddyFlags flags,
                 gboolean                 newArrival)
{
	struct gotrp_room *pr;
	struct gotr_user  *user;
	char              *usr;

	/* we don't need to handle ourselves joining the chat -> ignore signal */
	if (!newArrival)
		return;

	purple_debug_info(PLUGIN_ID, "onChatUserJoined: %s->%s\n",
	                  conv->title, name);

	if (!(pr = g_hash_table_lookup(gotrpRooms, conv))) {
		purple_debug_misc(PLUGIN_ID, "gotr not enabled in this chat\n");
		return;
	}

	if (g_hash_table_contains(pr->users, name)) {
		purple_debug_misc(PLUGIN_ID, "user already in chat\n");
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

	if (!g_hash_table_replace(pr->users, g_strdup(name), user)) {
		/* unreachable */
		purple_debug_warning(PLUGIN_ID, "unreachable: user replaced\n");
		return;
	}
}


static void
onChatUserLeft(PurpleConversation *conv,
               const char         *name,
               const char         *reason)
{
	struct gotrp_room *pr;
	struct gotr_user  *user;

	purple_debug_info(PLUGIN_ID, "onChatUserLeft: %s->%s\n", conv->title, name);

	if (!(pr = g_hash_table_lookup(gotrpRooms, conv))) {
		purple_debug_misc(PLUGIN_ID, "gotr not enabled in this chat\n");
		return;
	}

	if (!(user = g_hash_table_lookup(pr->users, name))) {
		purple_debug_misc(PLUGIN_ID, "gotr not enabled for leaving user\n");
		return;
	}

	gotr_user_left(pr->room, user);

	if (!g_hash_table_remove(pr->users, name))
		/* unreachable */
		purple_debug_warning(PLUGIN_ID, "unreachable: unable to remove user\n");
}


static void
onChatJoined(PurpleConversation *conv)
{
	struct gotrp_room *pr;
	gchar *fname;

	purple_debug_info(PLUGIN_ID, "onChatJoined: %s\n", conv->title);

	if (!(pr = calloc(1, sizeof(struct gotrp_room)))) {
		purple_debug_error(PLUGIN_ID, "failed to alloc memory for room\n");
		return;
	}

	/* create hash table before joining to make sure we have memory available */
	if (!(pr->users = g_hash_table_new_full(&g_str_hash,
	                                        &g_str_equal,
	                                        &g_free,
	                                        NULL))) {
		purple_debug_error(PLUGIN_ID, "unable to create users hash table\n");
		free(pr);
		return;
	}

	/* derive private key file name for account joining the chat */
	if (!conv->account) {
		purple_debug_error(PLUGIN_ID, "unable to derive account\n");
		return;
	}
	if (!(fname = buildKeyFname(conv->account))) {
		purple_debug_error(PLUGIN_ID, "buildKeyFname failed\n");
		return;
	}

	pr->room = gotr_join(&onGotrSendAll,
	                     &onGotrSendUser,
	                     &onGotrRecvUser,
	                     conv,
	                     fname);
	if (!pr->room) {
		g_free(fname);
		g_hash_table_destroy(pr->users);
		free(pr);

		if (-1 == genpid) {
			/* no other keygen in process, can generate new one. This additional
			 * check is needed to prevent gotrKeygenDoneConv from being
			 * overwritten in case another process is already on. */
			char *buf;

			buf = g_strdup_printf("No GOTR private key was found for %s.\n"
			                      "A new key is being generated currently...",
			                      conv->account->username);
			purple_conversation_write(conv,
			                          NULL,
			                          buf,
			                          PURPLE_MESSAGE_SYSTEM,
			                          time(NULL));
			g_free(buf);
			purple_debug_info(PLUGIN_ID, "starting private key generation\n");
			gotrKeygenDoneConv = conv;
			createPrivkey(conv->account);
		} else {
			purple_conversation_write(conv,
			                          NULL,
			                          "Another GOTR key is being generated.\n"
			                          "Please try to rejoin this chat later!",
			                          PURPLE_MESSAGE_SYSTEM,
			                          time(NULL));
		}
		return;
	}
	g_free(fname);

	if (!g_hash_table_insert(gotrpRooms, conv, pr)) {
		/* unreachable */
		purple_debug_warning(PLUGIN_ID, "unreachable: chat replaced\n");
	}
}


static void
onChatLeft(PurpleConversation *conv)
{
	purple_debug_info(PLUGIN_ID, "onChatLeft: %s\n", conv->title);

	if (!g_hash_table_remove(gotrpRooms, conv)) {
		purple_debug_error(PLUGIN_ID, "could not find room to leave\n");
		return;
	}
}


static gboolean
onDisplayingChat(PurpleAccount      *account,
                 const char         *who,
                 char               **message,
                 PurpleConversation *conv,
                 PurpleMessageFlags flags)
{
	struct gotrp_room *pr;
	GList             *elemGreen;
	GList             *elemRed;

	if (!(pr = g_hash_table_lookup(gotrpRooms, conv))) {
		purple_debug_misc(PLUGIN_ID, "gotr not enabled in this chat\n");
		return FALSE;
	}

	if (!(elemGreen = g_list_find_custom(pr->msgsGreen,
	                                     *message,
	                                     (GCompareFunc)strcmp))) {
		char *m = purple_markup_linkify(*message);
		elemGreen = g_list_find_custom(pr->msgsGreen, m,
		                               (GCompareFunc)strcmp);
		g_free(m);
	}

	if (!(elemRed = g_list_find_custom(pr->msgsRed,
	                                   *message,
	                                   (GCompareFunc)strcmp))) {
		char *m = purple_markup_linkify(*message);
		elemRed = g_list_find_custom(pr->msgsRed, m,
		                             (GCompareFunc)strcmp);
		g_free(m);
	}

	if (elemGreen && elemRed) {
		purple_debug_warning(PLUGIN_ID, "msg could be green or red: %s\n",
		                     *message);
		imgNextID = imgYellowID;
		g_free(elemGreen->data);
		pr->msgsGreen = g_list_delete_link(pr->msgsGreen, elemGreen);
		g_free(elemRed->data);
		pr->msgsRed = g_list_delete_link(pr->msgsRed, elemRed);
	} else if (elemGreen) {
		imgNextID = imgGreenID;
		g_free(elemGreen->data);
		pr->msgsGreen = g_list_delete_link(pr->msgsGreen, elemGreen);
	} else if (elemRed) {
		imgNextID = imgRedID;
		g_free(elemRed->data);
		pr->msgsRed = g_list_delete_link(pr->msgsRed, elemRed);
	} else {
		imgNextID = imgYellowID;
	}

	return FALSE;
}


static char *
onTimestamp(PurpleConversation *conv,
            time_t             when,
            gboolean           showDate)
{
	PidginConversation *gtkconv;
	char               *markup;

	if (0 == imgNextID)
		return NULL;

	if (!(gtkconv = PIDGIN_CONVERSATION(conv))) {
		purple_debug_warning(PLUGIN_ID, "unable to get gtk conv handle\n");
		return NULL;
	}

	if (!(markup = g_strdup_printf("<img id=\"%d\"> ", imgNextID))) {
		purple_debug_warning(PLUGIN_ID, "unable to format markup\n");
		return NULL;
	}

	gtk_imhtml_append_text_with_images((GtkIMHtml *)gtkconv->imhtml,
	                                   markup, 0, NULL);
	g_free(markup);
	imgNextID = 0;

	return NULL;
}


static void
quitting()
{
	purple_imgstore_unref_by_id(imgGreenID);
	imgGreenID = 0;
	purple_imgstore_unref_by_id(imgYellowID);
	imgYellowID = 0;
	purple_imgstore_unref_by_id(imgRedID);
	imgRedID = 0;
}


static void
destroyRoom(gpointer data)
{
	struct gotrp_room *pr = data;

	g_hash_table_destroy(pr->users);
	g_list_free_full(pr->msgsGreen, &g_free);
	pr->msgsGreen = NULL;
	g_list_free_full(pr->msgsRed, &g_free);
	pr->msgsRed = NULL;
	gotr_leave(pr->room);
	free(pr);
}


static gboolean
pluginLoad(PurplePlugin *plugin)
{
	void *conv = purple_conversations_get_handle();

	gotr_set_log_fn(&onGotrLog);
	if (!gotr_init()) {
		purple_debug_error(PLUGIN_ID, "failed to initialize libgotr\n");
		return FALSE;
	}

	if (!(gotrpRooms = g_hash_table_new_full(&g_direct_hash,
	                                         &g_direct_equal,
	                                         NULL,
	                                         &destroyRoom))) {
		purple_debug_error(PLUGIN_ID, "failed to create room hash table\n");
		return FALSE;
	}

	purple_signal_connect(conv, "writing-chat-msg", plugin,
	                      PURPLE_CALLBACK(onWritingChat), NULL);
	purple_signal_connect(conv, "receiving-im-msg", plugin,
	                      PURPLE_CALLBACK(onReceivingIM), NULL);
	purple_signal_connect(conv, "sending-chat-msg", plugin,
	                      PURPLE_CALLBACK(onSendingChat), NULL);
	purple_signal_connect(conv, "receiving-chat-msg", plugin,
	                      PURPLE_CALLBACK(onReceivingChat), NULL);
	purple_signal_connect(conv, "chat-buddy-joined", plugin,
	                      PURPLE_CALLBACK(onChatUserJoined), NULL);
	purple_signal_connect(conv, "chat-buddy-left", plugin,
	                      PURPLE_CALLBACK(onChatUserLeft), NULL);
	purple_signal_connect(conv, "chat-joined", plugin,
	                      PURPLE_CALLBACK(onChatJoined), NULL);
	purple_signal_connect(conv, "chat-left", plugin,
	                      PURPLE_CALLBACK(onChatLeft), NULL);
	purple_signal_connect_priority(pidgin_conversations_get_handle(),
	                               "conversation-timestamp", plugin,
	                               PURPLE_CALLBACK(onTimestamp), NULL,
	                               PURPLE_SIGNAL_PRIORITY_LOWEST);
	purple_signal_connect_priority(pidgin_conversations_get_handle(),
	                               "displaying-chat-msg", plugin,
	                               PURPLE_CALLBACK(onDisplayingChat), NULL,
	                               PURPLE_SIGNAL_PRIORITY_LOWEST);
	purple_signal_connect(purple_get_core(),
	                      "quitting", plugin,
	                      PURPLE_CALLBACK(quitting), NULL);

	/* load icons to purple imgstore */
	imgGreenID = purple_imgstore_add_with_id(g_memdup(green_png,
	                                                  sizeof(green_png)),
	                                         sizeof(green_png),
	                                         NULL);
	imgYellowID = purple_imgstore_add_with_id(g_memdup(yellow_png,
	                                                   sizeof(yellow_png)),
	                                          sizeof(yellow_png),
	                                          NULL);
	imgRedID = purple_imgstore_add_with_id(g_memdup(red_png,
	                                                sizeof(red_png)),
	                                       sizeof(red_png),
	                                       NULL);

	purple_debug_info(PLUGIN_ID, "plugin loaded\n");
	return TRUE;
}


static gboolean
pluginUnload(PurplePlugin *plugin)
{
	g_hash_table_destroy(gotrpRooms);
	g_list_free_full(gotrpMsgCache, &g_free);
	gotrpMsgCache = NULL;

	purple_signals_disconnect_by_handle(plugin);
	quitting();

	purple_debug_info(PLUGIN_ID, "plugin unloaded\n");
	return TRUE;
}


static PidginPluginUiInfo uiConfig = { buildConfWidget };


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

	"Off-the-Record for group chats",
	"GOTR Plugin",
	"Markus Teich <teichm@in.tum.de>",
	"https://github.com/schachmat/gotr",

	pluginLoad,
	pluginUnload,
	NULL,

	&uiConfig,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static void
initPlugin(PurplePlugin *plugin)
{
}


PURPLE_INIT_PLUGIN(hello_world, initPlugin, info)
