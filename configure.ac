# Initialization
AC_PREREQ([2.69])
AC_INIT([openpower-debug-collector], [1.0],[https://github.com/openbmc/openpower-debug-collector/issues])
AC_LANG([C++])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign dist-xz])
AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_ARG_ENABLE([dump_collection],
        [
AS_HELP_STRING([--enable-dump_collection],
                [To specify whether dump offload is enabled])])
AM_CONDITIONAL([DUMP_COLLECTION], [test "x$enable_dump_collection" == "xyes"])
AS_IF([test "x$enable_dump_collection" == "xyes"], [
    AC_CHECK_LIB([pdbg], [sbe_dump])

    AC_ARG_VAR(OP_DUMP_OBJPATH, [The Dbus root])
    AS_IF([test "x$OP_DUMP_OBJPATH" == "x"],
           [OP_DUMP_OBJPATH="/org/openpower/dump"])
    AC_DEFINE_UNQUOTED([OP_DUMP_OBJPATH], ["$OP_DUMP_OBJPATH"], [The Dbus root])

    AC_ARG_VAR(OP_DUMP_BUSNAME, [The OpenPOWER dump collector bus name])
    AS_IF([test "x$OP_DUMP_BUSNAME" == "x"],
           [OP_DUMP_BUSNAME="org.open_power.Dump.Manager"])
    AC_DEFINE_UNQUOTED([OP_DUMP_BUSNAME],
                        ["$OP_DUMP_BUSNAME"],
                         [The OpenPOWER dump collector bus name])

    AC_ARG_VAR(HB_DUMP_FILE_PREFIX, [The prefix to identify hostboot dump])
    AS_IF([test "x$HB_DUMP_FILE_PREFIX" == "x"],
           [HB_DUMP_FILE_PREFIX="hbdump_"])
    AC_DEFINE_UNQUOTED([HB_DUMP_FILE_PREFIX],
                        ["$HB_DUMP_FILE_PREFIX"],
                          [The prefix to identify hostboot dump])

    AC_ARG_VAR(HB_DUMP_DBUS_OBJPATH, [The hostboot dump manager path])
    AS_IF([test "x$HB_DUMP_DBUS_OBJPATH" == "x"],
           [HB_DUMP_DBUS_OBJPATH="/xyz/openbmc_project/dump/hostboot"])
    AC_DEFINE_UNQUOTED([HB_DUMP_DBUS_OBJPATH],
                        ["$HB_DUMP_DBUS_OBJPATH"],
                         [The hostboot dump manager path])

    AC_ARG_VAR(HB_DUMP_COLLECTION_PATH,
               [The path to store collected hostboot dump files])
    AS_IF([test "x$HB_DUMP_COLLECTION_PATH" == "x"],
           [HB_DUMP_COLLECTION_PATH="/tmp/openpower-dumps/hostboot"])
    AC_DEFINE_UNQUOTED([HB_DUMP_COLLECTION_PATH],
                        ["$HB_DUMP_COLLECTION_PATH"],
                          [The path to store collected hostboot dump files])

    PKG_CHECK_MODULES([LIBDT_API], [libdt-api])
    LIBS="$LIBS $LIBDT_API_LIBS"
    CFLAGS="$CFLAGS $LIBDT_API_CFLAGS"
    AC_CONFIG_FILES([dump/org.open_power.Dump.Manager.service])
])

PKG_CHECK_MODULES([SDEVENTPLUS], [sdeventplus])
# Generate elog-errors.hpp if gen_errors optional feature is enabled
AC_ARG_ENABLE([gen_errors],
    AS_HELP_STRING([--enable-gen_errors], [Enable elog-errors.hpp generation ]),
    [],[gen_errors=yes])
AM_CONDITIONAL([GEN_ERRORS], [test "x$enable_gen_errors" != "xno"])

# Check for compiler, packages only when install_error_yaml optional feature
# is disabled
AC_ARG_ENABLE([install_error_yaml],
    AS_HELP_STRING([--enable-install_error_yaml], [Enable installing error yaml file]),
    [], [install_error_yaml=no])
AM_CONDITIONAL([INSTALL_ERROR_YAML], [test "x$enable_install_error_yaml" = "xyes"])
AS_IF([test "x$enable_install_error_yaml" != "xyes"], [
    # Checks for typedefs, structures, and compiler characteristics.
    AX_CXX_COMPILE_STDCXX_17([noext])
    AX_APPEND_COMPILE_FLAGS([-Wall -Werror], [CXXFLAGS])

    # Check for sdbus++
    AC_PATH_PROG([SDBUSPLUSPLUS], [sdbus++])
    AS_IF([test "x$SDBUSPLUSPLUS" == "x"],
        AC_MSG_ERROR(["Requires sdbus++"]))

    LT_INIT

    # Checks for libraries.
    AX_PKG_CHECK_MODULES([SYSTEMD], [], [libsystemd >= 221], [],
        [AC_MSG_ERROR(["systemd required and not found."])])

    # Checks for modules
    PKG_CHECK_MODULES([PHOSPHOR_LOGGING], [phosphor-logging],,
        [AC_MSG_ERROR([Could not find phosphor-logging...openbmc/phosphor-logging package required])])
])

PKG_PROG_PKG_CONFIG
AC_ARG_WITH([systemdsystemunitdir],
     [AS_HELP_STRING([--with-systemdsystemunitdir=DIR], [Directory for systemd service files])],,
     [with_systemdsystemunitdir=auto])
AS_IF([test "x$with_systemdsystemunitdir" = "xyes" -o "x$with_systemdsystemunitdir" = "xauto"], [
     def_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)

     AS_IF([test "x$def_systemdsystemunitdir" = "x"],
   [AS_IF([test "x$with_systemdsystemunitdir" = "xyes"],
    [AC_MSG_ERROR([systemd support requested but pkg-config unable to query systemd package])])
    with_systemdsystemunitdir=no],
   [with_systemdsystemunitdir="$def_systemdsystemunitdir"])])
AS_IF([test "x$with_systemdsystemunitdir" != "xno"],
      [AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])])

# Create configured output
AC_CONFIG_FILES([Makefile ])
AC_OUTPUT
