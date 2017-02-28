MAJOR = 0
MINOR = 1

# Customize below to fit your system

# includes and libs
INCS = -I. -I/usr/local/include -I/usr/include `pkg-config --cflags pidgin glib-2.0 gtk+-2.0`
LIBS = -L/usr/local/lib -L/usr/lib -lc -lgotr -lgcrypt `pkg-config --libs pidgin glib-2.0 gtk+-2.0`

# flags
CPPFLAGS = -DGOTRP_VERSION_MAJOR=\"${MAJOR}\" -DGOTRP_VERSION_MINOR=\"${MINOR}\"
CFLAGS = -g -std=c99 -fPIC -pedantic -Wall -O0 ${INCS} ${CPPFLAGS}
#CFLAGS = -std=c99 -fPIC -pedantic -Wall -O3 ${INCS} ${CPPFLAGS}
LDFLAGS = -g -shared -Wl,-soname,pidgin-gotr.so.${MAJOR} ${LIBS}

# compiler and linker
CC = cc
CSCOPE = cscope
DOXYGEN = doxygen
