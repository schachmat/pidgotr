include config.mk

SRC = pidgin-gotr.c gtk-ui.c
LIB = pidgin-gotr.so.${MAJOR}.${MINOR}
INC = gtk-ui.h
OBJ = ${SRC:.c=.o}

all: options ${LIB}

options:
	@echo build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

.c.o:
	@echo CC $<
	@${CC} -c ${CFLAGS} $<

${OBJ}: ${INC} config.mk

${LIB}: ${OBJ}
	@echo CC $<
	@${CC} -o ${@} ${OBJ} ${LDFLAGS}

doc: Doxyfile
	@echo DOXYGEN
	@${DOXYGEN} >/dev/null

clean:
	@echo cleaning
	@rm -f ${LIB} ${OBJ}

.PHONY: all pidgin-gotr.so options doc clean structs
