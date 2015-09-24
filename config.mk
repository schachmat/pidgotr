MAJOR = 0
MINOR = 1

# Customize below to fit your system

# includes and libs
INCS = -I. -I./include -I/usr/include `pkg-config --cflags pidgin glib-2.0`
LIBS = -L/usr/lib -lc `pkg-config --libs pidgin glib-2.0`

# flags
CPPFLAGS = -DGOTR_VERSION_MAJOR=\"${MAJOR}\" -DGOTR_VERSION_MINOR=\"${MINOR}\"
CFLAGS = -g -std=c99 -fPIC -pedantic -Wall -O0 ${INCS} ${CPPFLAGS}
#CFLAGS = -std=c99 -fPIC -pedantic -Wall -O3 ${INCS} ${CPPFLAGS}
LDFLAGS = -g -shared -Wl,-soname,pidgin-gotr.so.${MAJOR} ${LIBS}

# compiler and linker
CC = cc
CSCOPE = cscope
DOXYGEN = doxygen
