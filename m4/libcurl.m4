dnl Based on libpng.m4

AC_DEFUN([AC_FIND_FILE],
[
$3=NO
for i in $2;
do
  for j in $1;
  do
    echo "configure: __oline__: $i/$j" >&AC_FD_CC
    if test -r "$i/$j"; then
      echo "taking that" >&AC_FD_CC
      $3=$i
      break 2
    fi
  done
done
])

AC_DEFUN([FIND_CURL_HELPER],
[
AC_MSG_CHECKING([for libcurl])
AC_CACHE_VAL(ac_cv_lib_curl,
[
ac_save_LIBS="$LIBS"
LIBS="$all_libraries $USER_LDFLAGS -lcurl -lm"
ac_save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS $all_includes $USER_INCLUDES"
AC_TRY_LINK(
[
#ifdef __cplusplus
extern "C" {
#endif
void curl_version();
#ifdef __cplusplus
}
#endif
],
[curl_version();],
            eval "ac_cv_lib_curl=-lcurl",
            eval "ac_cv_lib_curl=no")
LIBS="$ac_save_LIBS"
CFLAGS="$ac_save_CFLAGS"
])

if eval "test ! \"`echo $ac_cv_lib_curl`\" = no"; then
  enable_libcurl=yes
  LIBCURL_LIBS="$ac_cv_lib_curl"
  AC_MSG_RESULT($ac_cv_lib_curl)
else
  AC_MSG_RESULT(no)
  $1
fi
])


AC_DEFUN([POPPLER_FIND_CURL],
[
dnl first look for libraries
FIND_CURL_HELPER(
   FIND_CURL_HELPER(normal, [],
    [
       LIBCURL_LIBS=
    ]
   )
)

dnl then search the headers
curl_incdirs="`eval echo $includedir` /usr/include /usr/local/include "
AC_FIND_FILE(curl/curl.h, $curl_incdirs, curl_incdir)
test "x$curl_incdir" = xNO && curl_incdir=

dnl if headers _and_ libraries are missing, this is no error, and we
dnl continue with a warning (the user will get no http support)
dnl if only one is missing, it means a configuration error, but we still
dnl only warn
if test -n "$curl_incdir" && test -n "$LIBCURL_LIBS" ; then
  AC_DEFINE_UNQUOTED(ENABLE_LIBCURL, 1, [Define if you have libcurl])
else
  if test -n "$curl_incdir" ; then 
    AC_MSG_WARN([
There is an installation error in libcurl support. You seem to have only the
headers installed. You may need to either provide correct --with-extra-...
options, or the development package of libcurl. You can get a source package of
libcurl from http://curl.haxx.se/download.html
Disabling HTTP support.
])
  elif test -n "$LIBCURL_LIBS" ; then
    AC_MSG_WARN([
There is an installation error in libcurl support. You seem to have only the
libraries installed. You may need to either provide correct --with-extra-...
options, or the development package of libcurl. You can get a source package of
libcurl from http://curl.haxx.se/download.html
Disabling HTTP support.
])
  else
    AC_MSG_WARN([libcurl not found. disable HTTP support.])
  fi
  curl_incdir=
  enable_libcurl=no
  LIBCURL_LIBS=
fi

AC_SUBST(LIBCURL_LIBS)
])
