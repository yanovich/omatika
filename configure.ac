dnl Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)

AC_INIT([pcs], [0.0], [ynvich@gmail.com])
AC_CONFIG_HEADERS([config.h:config-h.in])

AM_INIT_AUTOMAKE([1.11 foreign silent-rules])

AC_PROG_CC

AC_CHECK_HEADERS(\
		 unistd.h \
		 ,[],[AC_MSG_ERROR([a required header is missing])])

PKG_CHECK_MODULES(YAML, yaml-0.1 >= 0.1)

AC_CHECK_FUNCS([\
		getopt\
])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT