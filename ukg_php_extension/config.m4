dnl $Id$
dnl config.m4 for extension ukg

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(ukg, for ukg support,
dnl Make sure that the comment is aligned:
dnl [  --with-ukg             Include ukg support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(ukg, whether to enable ukg support,
dnl Make sure that the comment is aligned:
[  --enable-ukg           Enable ukg support])

if test "$PHP_UKG" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-ukg -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/ukg.h"  # you most likely want to change this
  dnl if test -r $PHP_UKG/$SEARCH_FOR; then # path given as parameter
  dnl   UKG_DIR=$PHP_UKG
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for ukg files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       UKG_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$UKG_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the ukg distribution])
  dnl fi

  dnl # --with-ukg -> add include path
  dnl PHP_ADD_INCLUDE($UKG_DIR/include)

  dnl # --with-ukg -> check for lib and symbol presence
  dnl LIBNAME=ukg # you may want to change this
  dnl LIBSYMBOL=ukg # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $UKG_DIR/lib, UKG_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_UKGLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong ukg lib version or lib not found])
  dnl ],[
  dnl   -L$UKG_DIR/lib -lm -ldl
  dnl ])
  dnl
  dnl PHP_SUBST(UKG_SHARED_LIBADD)

  PHP_NEW_EXTENSION(ukg, ukg.c, $ext_shared)
fi
