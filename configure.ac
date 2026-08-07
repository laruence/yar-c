# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)
AC_INIT(yar-c, 1.1.0, laruence@gmail.com)

AC_CONFIG_SRCDIR([yar_server.c])
AC_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC([gcc cc])
AC_PROG_LIBTOOL

# Checks for libraries.
AC_MSG_CHECKING(for event)
AC_ARG_WITH(event,
            [  --with-event=[DIR]  Path to event installation dir],
            [EVENT_DIR=$withval],
            [EVENT_DIR="no"])

if test "$EVENT_DIR" = "no"; then
    for i in /usr /usr/local /opt/local /opt/homebrew; do
        if test -r $i"/include/event.h"; then
            EVENT_DIR=$i
            EVENT_HEADER=$i"/include/"
            EVENT_LIBRARY=$i"/lib/"
            CFLAGS=$CFLAGS" -I${EVENT_HEADER}"
            LDFLAGS=$LDFLAGS" -Wl,-rpath,${EVENT_LIBRARY} -L${EVENT_LIBRARY}"
            break
        fi
    done
else
    if test -r $EVENT_DIR"/include/event.h"; then
        EVENT_HEADER=$EVENT_DIR"/include/"
        EVENT_LIBRARY=$EVENT_DIR"/lib/"
        CFLAGS=$CFLAGS" -I${EVENT_HEADER}"
        LDFLAGS=$LDFLAGS" -Wl,-rpath,${EVENT_LIBRARY} -L${EVENT_LIBRARY}"
    else
        AC_MSG_ERROR([could not find event in '$EVENT_DIR'])
    fi
fi

if test -z "$EVENT_HEADER"; then
   AC_MSG_ERROR([could not find event])
else
   AC_MSG_RESULT([found in $EVENT_DIR])
fi

AC_CHECK_LIB([event], [event_set], [], [AC_MSG_ERROR([Could not find event library])])

AC_MSG_CHECKING(for msgpack)
AC_ARG_WITH(msgpack,
            [  --with-msgpack=[DIR]  Path to msgpack installation dir],
            [MSGPACK_DIR=$withval],
            [MSGPACK_DIR="no"])

if test "$MSGPACK_DIR" = "no"; then
    for i in /usr /usr/local /opt/local /opt/homebrew; do
        if test -r $i"/include/msgpack.h"; then
            MSGPACK_DIR=$i
            MSGPACK_HEADER=$i"/include/"
            MSGPACK_LIBRARY=$i"/lib/"
			CFLAGS=$CFLAGS" -I${MSGPACK_HEADER}"
			LDFLAGS=$LDFLAGS" -Wl,-rpath,${MSGPACK_LIBRARY} -L${MSGPACK_LIBRARY}"
			break
        fi
    done
else
    if test -r $MSGPACK_DIR"/include/msgpack.h"; then
        MSGPACK_HEADER=$MSGPACK_DIR"/include/"
        MSGPACK_LIBRARY=$MSGPACK_DIR"/lib/"
        CFLAGS=$CFLAGS" -I${MSGPACK_HEADER}"
        LDFLAGS=$LDFLAGS" -Wl,-rpath,${MSGPACK_LIBRARY} -L${MSGPACK_LIBRARY}"
    else 
        AC_MSG_ERROR([could not find msgpack in '$MSGPACK_DIR'])
    fi
fi

if test -z "$MSGPACK_HEADER"; then
   AC_MSG_ERROR([could not find msgpack])
else
   AC_MSG_RESULT([found in $MSGPACK_DIR])
fi

AC_CHECK_LIB([msgpackc], [msgpack_unpacker_init], [], [ AC_MSG_ERROR([check for msgpack_uppacker_init in msgpack failed]) ])

# cJSON is optional, it enables the JSON packager; when it is missing the
# library still builds and only speaks msgpack
AC_MSG_CHECKING(for cjson)
AC_ARG_WITH(cjson,
            [  --with-cjson=[DIR]  Path to cjson installation dir],
            [CJSON_DIR=$withval],
            [CJSON_DIR="no"])

if test "$CJSON_DIR" = "no"; then
    for i in /usr /usr/local /opt/local /opt/homebrew; do
        if test -r $i"/include/cjson/cJSON.h"; then
            CJSON_DIR=$i
            CJSON_HEADER=$i"/include/"
            CJSON_LIBRARY=$i"/lib/"
            CFLAGS=$CFLAGS" -I${CJSON_HEADER}"
            LDFLAGS=$LDFLAGS" -Wl,-rpath,${CJSON_LIBRARY} -L${CJSON_LIBRARY}"
            break
        fi
    done
else
    if test -r $CJSON_DIR"/include/cjson/cJSON.h"; then
        CJSON_HEADER=$CJSON_DIR"/include/"
        CJSON_LIBRARY=$CJSON_DIR"/lib/"
        CFLAGS=$CFLAGS" -I${CJSON_HEADER}"
        LDFLAGS=$LDFLAGS" -Wl,-rpath,${CJSON_LIBRARY} -L${CJSON_LIBRARY}"
    else
        AC_MSG_ERROR([could not find cjson in '$CJSON_DIR'])
    fi
fi

JSON_LIBS=""
if test -n "$CJSON_HEADER"; then
    AC_CHECK_LIB([cjson], [cJSON_ParseWithLengthOpts],
        [AC_DEFINE([HAVE_JSON], [1], [Define to 1 if cJSON is available for the JSON packager])
         JSON_LIBS="-lcjson"
         AC_MSG_RESULT([found in $CJSON_DIR])],
        [AC_MSG_RESULT([header found but library check failed, JSON packager disabled])])
else
    AC_MSG_RESULT([not found, JSON packager disabled])
fi
AC_SUBST(JSON_LIBS)

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h stdlib.h string.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([bzero gethostbyname inet_ntoa select socket strchr strdup strerror strncasecmp strrchr])

AC_SUBST(CFLAGS)

AC_CONFIG_FILES([Makefile example/Makefile])
AC_OUTPUT
