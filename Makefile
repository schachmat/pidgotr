include config.mk

SRC = pidgin-gotr.c
LIB = pidgin-gotr.so.${MAJOR}.${MINOR}
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

${OBJ}: config.mk icons.h

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
