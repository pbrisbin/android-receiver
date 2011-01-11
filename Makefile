
include config.mk

SRC = android-receiver.c
OBJ = ${SRC:.c=.o}

all: android-receiver

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.mk
mail-query: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}


clean:
	rm -f android-receiver ${OBJ}

.PHONY: all clean
