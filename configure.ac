# Autoconf for axel
# Copyright 2016 Joao Eriberto Mota Filho
# Copyright 2016 Stephen Thirlwall
# Copyright 2017 Antonio Quartulli
# Copyright 2017 Ismael Luceno
# Copyright 2017 Vlad Glagolev
# This file is under same license of Axel

AC_PREREQ([2.69])
PKG_PREREQ([0.29])

AXEL_PKG([axel], [https://github.com/axel-download-accelerator/axel/issues])
AXEL_VER_READ([VERSION])
AC_INIT

AC_CONFIG_SRCDIR([src/conf.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE
PKG_PROG_PKG_CONFIG

AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AS_IF([test "x$ac_cv_prog_cc_c99" = "xno"], [
    AC_MSG_ERROR([C99 compiler required])
])

AC_PROG_INSTALL

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([ \
	arpa/inet.h \
	fcntl.h \
	libintl.h \
	limits.h \
	locale.h \
	netdb.h \
	netinet/in.h \
	stdlib.h \
	string.h \
	sys/ioctl.h \
	sys/socket.h \
	sys/time.h \
	unistd.h \
])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T

# Checks for library functions.
AC_CHECK_FUNCS([ \
	getaddrinfo \
	gettimeofday \
	inet_ntoa \
	malloc \
	memset \
	nanosleep \
	realloc \
	select \
	setlocale \
	socket \
	strcasecmp \
	strchr \
	strerror \
	strncasecmp \
	strpbrk \
	strrchr \
	strstr \
	strtoul \
])

# Optional (but included-by-default) ssl support
AC_ARG_WITH([ssl],
    AC_HELP_STRING([--without-ssl],[disable TLS support]), [], [with_ssl=yes])

AS_IF([test "x$with_ssl" != xno], [
    PKG_CHECK_MODULES([SSL], [openssl])
    AC_DEFINE([HAVE_SSL], [1], [SSL])
], AC_MSG_NOTICE([SSL support disabled]))

# Add Gettext
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.18])
LIBS="$LIBS $LIBINTL"

# Large file support
AC_SYS_LARGEFILE

# POSIX threads
AX_PTHREAD()
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

#### Definitions ####

# OS
AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED([ARCH], ["$host_os"], ["Define architecture"])

# Flags
CFLAGS="$CFLAGS -Wall"

case "$host_os" in
darwin*)
    AC_DEFINE(_DARWIN_C_SOURCE, 1, [Define on Darwin to expose all libc features])
;;
solaris*)
    LIBS="$LIBS -lsocket -lnsl"
;;
hpux*)
    if [ -d /usr/local/lib/hpux32 ]; then
       LDFLAGS="$LDFLAGS -L/usr/local/lib/hpux32"
    fi
;;
esac

#   ---> WARNING: NetBSD not tested! Using OpenBSD settings.

# etc path
AC_DEFINE_UNQUOTED([ETCDIR], ["/etc"], ["Define /etc directory path"])

# Other
AH_TEMPLATE([_REENTRANT], [Local definition])
AC_DEFINE_UNQUOTED([_REENTRANT])
AH_TEMPLATE([_THREAD_SAFE], [Local definition])
AC_DEFINE_UNQUOTED([_THREAD_SAFE])

#####################

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	po/Makefile.in
])
AC_OUTPUT
