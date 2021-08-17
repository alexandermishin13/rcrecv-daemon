# $FreeBSD$

PREFIX?= /usr/local
MK_DEBUG_FILES=	no

PROG=	rcrecv-daemon
BINDIR=	${PREFIX}/sbin

SCRIPTS= ${PROG}.sh

SCRIPTSNAME_${PROG}.sh=		${PROG}
SCRIPTSDIR_${PROG}.sh=		${PREFIX}/etc/rc.d

MAN=	${PROG}.8
MANDIR=	${PREFIX}/man/man

CFLAGS+= -Wall -I/usr/local/include
LDADD=	-L/usr/local/lib -lc -lutil -lgpio

uninstall:
	rm ${BINDIR}/${PROG}
	rm ${MANDIR}8/${MAN}.gz
	rm ${PREFIX}/etc/rc.d/${PROG}

check:
	cppcheck \
	    --enable=all \
	    --force \
	    -I/usr/local/include \
	    ./

.include <bsd.prog.mk>
