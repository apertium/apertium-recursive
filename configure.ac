AC_PREREQ(2.61)

m4_define([required_libxml_version], [2.6.17])
m4_define([required_apertium_version], [3.8.2])
m4_define([required_lttoolbox_version], [3.7.0])

AC_INIT([apertium-recursive], [1.2.2], [awesomeevildudes@gmail.com])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADER([src/auto_config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CXX
AM_SANITY_CHECK
AC_LANG_CPLUSPLUS

CFLAGS="-Wall -Wextra $CFLAGS"
CXXFLAGS="-Wall -Wextra $CXXFLAGS"

AC_ARG_ENABLE(debug,
              [  --enable-debug  Enable debug compiler options],
              [case "${enableval}" in
              yes ) CXXFLAGS="-g -Wall";CFLAGS="-g -ggdb -Og -DDEBUG" ;;
              no  ) ;;
              *   ) AC_MSG_ERROR("Invalid value ${enableval} for --enable-debug") ;; esac])

PKG_CHECK_MODULES([LTTOOLBOX], [lttoolbox >= required_lttoolbox_version])

AC_SUBST(LTTOOLBOX_CFLAGS)
AC_SUBST(LTTOOLBOX_LIBS)

PKG_CHECK_MODULES([APERTIUM], [apertium >= required_apertium_version])

AC_SUBST(APERTIUM_CFLAGS)
AC_SUBST(APERTIUM_LIBS)

PKG_CHECK_MODULES([LIBXML], [libxml-2.0 >= required_libxml_version])

AC_SUBST(LIBXML_CFLAGS)
AC_SUBST(LIBXML_LIBS)

PKG_CHECK_MODULES([ICU], [icu-i18n, icu-io, icu-uc])

AC_SUBST(ICU_CFLAGS)
AC_SUBST(ICU_LIBS)

# Checks for libraries.
AC_CHECK_LIB(xml2, xmlReaderForFile)

AC_CHECK_FUNCS([setlocale strdup getopt_long])

AC_CHECK_DECLS([fread_unlocked, fwrite_unlocked, fgetc_unlocked, fputc_unlocked, fputs_unlocked, getopt_long])

CPPFLAGS="$CPPFLAGS $CFLAGS $LTTOOLBOX_CFLAGS $APERTIUM_CFLAGS $LIBXML_CFLAGS $ICU_CFLAGS"
LIBS="$LIBS $APERTIUM_LIBS $LTTOOLBOX_LIBS $LIBXML_LIBS $ICU_LIBS"

# Checks for highest supported C++ standard
AC_LANG(C++)
for version in 26 2c 23 2b 20 2a 17; do
  version_flag="-std=c++${version}"
  AX_CHECK_COMPILE_FLAG([${version_flag}], [break], [version_flag=none])
done
AS_IF([test "$version_flag" == none], [
  AC_MSG_ERROR([Could not enable at least C++17 - upgrade your compiler])
])
CXXFLAGS="$CXXFLAGS ${version_flag}"

_found_utf8=no
for ipath in /usr /usr/local /opt /opt/local; do
  for upath in utf8cpp utfcpp utf8; do
    if test -f "$ipath/include/$upath/utf8.h"; then
      echo "found utfcpp at $ipath/include/$upath/utf8.h"
      CPPFLAGS="-I$ipath/include/$upath $CPPFLAGS"
      _found_utf8=yes
    fi
  done
done
if test $_found_utf8 = no
then
  AC_CHECK_HEADER([utf8cpp/utf8.h], [CPPFLAGS="-I/usr/include/utf8cpp/ $CPPFLAGS"], [
    AC_CHECK_HEADER([utf8.h], [], [AC_MSG_ERROR([Could not find utf8.h from utf8cpp / utfcpp])])
  ])
fi

AC_CONFIG_FILES([
                 Makefile
                 src/Makefile
                 apertium-recursive.pc
                 ])
AC_OUTPUT
