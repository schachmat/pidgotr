include config.mk

SRC = pidgin-gotr.c
LIB = pidgin-gotr.so.${MAJOR}.${MINOR}
INC = 
OBJ = ${SRC:.c=.o}

all: options cscope ${LIB}

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
	@${CC} ${LDFLAGS} -o ${@} ${OBJ}

cscope: ${SRC} ${INC}
	@echo cScope
	@${CSCOPE} -R -b

doc: Doxyfile
	@echo DOXYGEN
	@${DOXYGEN} >/dev/null

clean:
	@echo cleaning
	@rm -f ${LIB} ${OBJ}

.PHONY: all pidgin-gotr.so options cscope doc clean structs
