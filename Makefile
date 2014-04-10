CFLAGS =
LDFLAGS =
BASICLIBRARIES = -ltermcap

less :
	${CC} ${CFLAGS} ${LDFLAGS} $@.c ${BASICLIBRARIES}

clean :
	rm *.o *.out
