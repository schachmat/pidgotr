# pidgotr

Pidgin Group OTR is a plugin for pidgin enabling deniable encrypted group
conversations.

# Installation

You need to install [libgotr](https://github.com/schachmat/gotr) before
installing the pidgin-gotr plugin. You also need the development package of
pidgin on your system. Then you can run `make` to build the plugin file. The
resulting `pidgin-gotr.so.*` file needs to be copied to or linked to the
directory `~/.purple/plugins` and the copy/link needs to end with `.so` for
pidgin to detect the plugin. For example copy the file with `cp
pidgin-gotr.so.0.2 ~/.purple/plugins/pidgin-gotr.so`. Finally you only need to
restart (not just close to tray, restart!) pidgin and can enable the "Group OTR"
plugin from the pidgin plugin dialog.

# Using

When enabled and you join a chatroom, the GOTR plugin first tries to load your
key. If it does not exist yet, one will be generated automatically. You need one
key per user account. If the key needed to be generated, you have to rejoin the
chat room. The plugin adds a button to the input field toolbar. With this button
you can temporarily disable the encryption in a chat room to send messages which
can be read by users who don't have the plugin installed. To inform about the
GOTR state and the mode under which messages have been sent/received, the plugin
uses some icons, described in the
[wiki](https://github.com/schachmat/pidgotr/wiki/Icons).

# Restrictions

- The GOTR plugin currently only works with the XMPP/Jabber protocol. It will
  silently ignore other protocols.
- The plugin does not yet handle trust in other peoples public keys.
- If a GOTR using chat participant changes his nick, you need to rejoin the
  chat. This is because the plugin has no way to detect nick renames, but
  correct nick names are required for the GOTR functionality.
