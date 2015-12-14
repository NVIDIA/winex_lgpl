dnl Process this file with autoconf to produce a configure script.
dnl Original author: Michael Patra
dnl See ChangeLog file for detailed change history.

m4_define(REWIND_VERSION,regexp(m4_include(VERSION),[version \([-.0-9A-Za-z]+\)],[\1]))


AC_PREREQ(2.50)
AC_INIT([Cedega],REWIND_VERSION)
AC_CONFIG_SRCDIR(dlls/ntdll/version.c)		
AC_CONFIG_HEADERS(include/config.h)
AC_CONFIG_AUX_DIR(tools)
AC_CANONICAL_TARGET

dnl **** Command-line arguments ****

dnl Default values
case $host in
    *darwin*) LIBEXT="dylib" ;;
    *)        LIBEXT="so" ;;
esac

dnl If building on amd64, treat it as i686 for our purposes so that we don't
dnl have to specify --target all the time
dnl NOTE - this will likely need to be changed or removed if we start trying
dnl to do a real 64-bit compilation at some point
case $target_cpu in
    x86_64) target_cpu=i686 ;;
esac

AC_ARG_ENABLE(package, AC_HELP_STRING([--enable-package],[compile wine with mac packaging options]))
AC_ARG_ENABLE(release, AC_HELP_STRING([--enable-release],[compile wine with release options]))


AC_ARG_ENABLE( debug, AC_HELP_STRING([--disable-debug], [compile out all debugging messages]),
                enable_debug="$enableval", enable_debug="yes" )
                
AC_ARG_ENABLE( ptrace-deny, AC_HELP_STRING([--enable-ptrace-deny], [prevents GDB from attaching to this app]),
                enable_ptrace_deny="$enableval", enable_ptrace_deny="no")

AC_ARG_ENABLE( trace, AC_HELP_STRING([--disable-trace],[compile out TRACE messages]),
                enable_trace="$enableval", enable_trace="yes" )

AC_ARG_ENABLE( opengl, AC_HELP_STRING([--enable-opengl],[force usage of OpenGL even if the latter is thread-safe via pthread]),
                enable_opengl="$enableval",enable_opengl="yes")

AC_ARG_ENABLE( remote-gdb, AC_HELP_STRING([--enable-remote-gdb],[allow debugging with gdb in remote mode]),
                enable_remote_gdb="$enableval", enable_remote_gdb="no" )

AC_ARG_ENABLE( osmesa, AC_HELP_STRING([--enable-osmesa],[compile with OSMesa, currently incompatible with nVidia drivers]),
                enable_osmesa="$enableval", enable_osmesa="no" )

AC_ARG_ENABLE( pthreads, AC_HELP_STRING([--enable-pthreads],[compile with pthreads]),
                enable_pthreads="$enableval", enable_pthreads="no" )

AC_ARG_ENABLE( sdldrv, AC_HELP_STRING([--enable-sdldrv], [build the sdl driver]),
                enable_sdldrv="$enableval", enable_sdldrv="no" )

AC_ARG_ENABLE( macos-sdk, AC_HELP_STRING([--enable-macos-sdk], [set the mac sdk]),
                mac_os_sdk="$enableval", mac_os_sdk="" )

AC_ARG_WITH( curses, AC_HELP_STRING([--without-curses],[do not use curses]) ) 

AC_ARG_ENABLE( xshm, AC_HELP_STRING([--disable-xshm],[do not use X shared memory even if available]),
                enable_xshm="$enableval", enable_xshm="yes" )

AC_ARG_ENABLE( warnings-as-errors, AC_HELP_STRING([--enable-warnings-as-errors],[treat all compile warnings as errors]),
                enable_warnings_as_errors="$enableval", enable_warnings_as_errors="no" )

AC_ARG_ENABLE( freetype2-lib-check, AC_HELP_STRING([--disable-freetype2-lib-check],[Assume libfreetype is of the correct version and do not check]),
                enable_freetype2_lib_check="$enableval", enable_freetype2_lib_check="yes" )

AC_ARG_ENABLE( fontconfig-lib-check, AC_HELP_STRING([--disable-fontconfig-lib-check],[Assume libfontconfig is of the correct version and do not check]),
                enable_fontconfig_lib_check="$enableval", enable_fontconfig_lib_check="yes" )

AC_ARG_ENABLE( xrender-lib-check, AC_HELP_STRING([--disable-xrender-lib-check],[Assume libXrender is of the correct version and do not check]),
                enable_xrender_lib_check="$enableval", enable_xrender_lib_check="yes" )

AC_ARG_ENABLE( xrandr-lib-check, AC_HELP_STRING([--disable-xrandr-lib-check],[Assume libXrandr is of the correct version and do not check]),
                enable_xrandr_lib_check="$enableval", enable_xrandr_lib_check="yes" )

AC_ARG_ENABLE( xcursor-lib-check, AC_HELP_STRING([--disable-xcursor-lib-check],[Assume libXcursor is of the correct version and do not check]),
                enable_xcursor_lib_check="$enableval", enable_xcursor_lib_check="yes" )

AC_ARG_ENABLE( aic, AC_HELP_STRING([--disable-aic],[enable Accelerated Interprocess Communication]),
                enable_aic="$enableval", enable_aic="yes" )

AC_ARG_WITH(ssl-include,
        AC_HELP_STRING([--with-ssl-include=path], [specify path to search for OpenSSL include files]),
        [], [with_ssl_include=""])

AC_SUBST(OPTIONS)

if test "x$enable_debug" = "xno"
then
    AC_DEFINE(NO_DEBUG_MSGS,1,[Define to disable all debug messages.])
fi
if test "x$enable_trace" = "xno" -o "x$enable_debug" = "xno"
then
    AC_DEFINE(NO_TRACE_MSGS,1,[Define to disable trace messages.])
fi


if test "x$enable_ptrace_deny" = "xyes"
then
    AC_DEFINE(ENABLE_PT_DENY_ATTACH, 1, [Define to prevent GDB from being able to attach to this app])
fi


if test "x$enable_remote_gdb" = "xyes"
then
    AC_DEFINE(ENABLE_GDB_REMOTE, 1, [Define if the user wants remote gdb support])
fi

dnl **** Check for some programs ****

AC_PROG_MAKE_SET
AC_PROG_CC

dnl Check for -m32
BUILD_32BIT_OPTION=""
AC_CACHE_CHECK([for gcc -m32 support], ac_cv_c_gcc_m32,
  [saved_cflags=$CFLAGS
  CFLAGS="$CFLAGS -m32"
  AC_TRY_COMPILE(,[return 0],ac_cv_c_gcc_m32="yes",ac_cv_c_gcc_m32="no")
  CFLAGS=$saved_cflags]
)
if test "$ac_cv_c_gcc_m32" = "yes"
then
  BUILD_32BIT_OPTION="-m32"
  CFLAGS="$CFLAGS -m32"
fi
AC_SUBST(BUILD_32BIT_OPTION)

AC_PROG_CPP
AC_PROG_CXX
AC_PATH_XTRA
AC_PROG_YACC
AC_PROG_LEX

dnl **** store the current CFLAGS in CEDEGACFLAGS ****
CEDEGACFLAGS="$CEDEGACFLAGS $CFLAGS"

# set MacOSX defaults
# under Objective C - warnings actually should be errors
# Also force relocation of the main executable and set up the reserved memory segment
case $host in
   *darwin*)
        dnl Look in usual spots for common case (fast)
        AC_MSG_CHECKING([for the Mac OS X SDK location])
        MACSDKPREFIX="";
        if test -d "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs"
        then
            AC_MSG_RESULT([/Applications/Xcode.app])
            MACSDKPREFIX="/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs"
        elif test -d "/Developer/SDKs"
        then
            AC_MSG_RESULT([/Developer])
            MACSDKPREFIX="/Developer/SDKs"
        else
            dnl Last effort to locate SDK folder by taking the first matching MacOSX SDK in the Xcode path (slow)
            XCODEPATH=`xcode-select --print-path`
            MACSDKPREFIX=`find "$XCODEPATH" -name "MacOSX*.sdk" -type d | head -1 | xargs -I{} dirname {}`
            if test -n "$MACSDKPREFIX"
            then
                AC_MSG_RESULT(["$MACSDKPREFIX"])
            else
                AC_MSG_RESULT([not found])
                AC_MSG_ERROR([No Mac OS X SDK folder found. Please install Xcode and/or run 'sudo xcode-select <Xcode location>'.])
            fi
        fi

        AC_MSG_CHECKING([for the Mac OS X SDK version])
        if test "X$mac_os_sdk" = "X"
        then
            if test -d "$MACSDKPREFIX/MacOSX10.5.sdk"
            then
                AC_MSG_RESULT([10.5])
                mac_os_sdk=10.5
            elif test -d "$MACSDKPREFIX/MacOSX10.6.sdk"
            then
                AC_MSG_RESULT([10.6])
                mac_os_sdk=10.6
            elif test -d "$MACSDKPREFIX/MacOSX10.7.sdk"
            then
                AC_MSG_RESULT([10.7])
                mac_os_sdk=10.7
            else
                AC_MSG_RESULT([not found])
                AC_MSG_ERROR([No supported Mac OS X SDK found.  Please specify one with --enable-macos-sdk=...])
            fi
        fi

        MINVERSION="";
        MAXVERSION="";
        MACSDK="";
        AC_DEFINE( TG_MAC_OS_X_VERSION_TIGER, 1040, [internal tiger SDK version number])
        AC_DEFINE( TG_MAC_OS_X_VERSION_LEOPARD, 1050, [internal leopard SDK version number])
        AC_DEFINE( TG_MAC_OS_X_VERSION_SNOWLEOPARD, 1060, [internal snow leopard SDK version number])
        AC_DEFINE( TG_MAC_OS_X_VERSION_LION, 1070, [internal lion SDK version number])
        if test "$mac_os_sdk" = "10.4"
        then
            AC_DEFINE( MACOSX_DEPLOYMENT_TARGET, 10.4, [Define the target Mac OS X target version])
            AC_DEFINE( MAC_OS_X_VERSION_MIN_REQUIRED, 1040, [Define min supported Mac OS X version])
            AC_DEFINE( MAC_OS_X_VERSION_MAX_ALLOWED, 1040, [Define max Mac OS X version features can be used from])
            AC_DEFINE( TG_MAC_OS_X_SDK_VERSION, 1040, [internal symbol defining the SDK version we are currently building with])
            MINVERSION=1040;
            MAXVERSION=1040;
            MACSDK="$MACSDKPREFIX/MacOSX10.4u.sdk";
        elif test "$mac_os_sdk" = "10.5"
        then
            AC_DEFINE( MACOSX_DEPLOYMENT_TARGET, 10.5, [Define the target Mac OS X target version])
            AC_DEFINE( MAC_OS_X_VERSION_MIN_REQUIRED, 1050, [Define min supported Mac OS X version])
            AC_DEFINE( MAC_OS_X_VERSION_MAX_ALLOWED, 1050, [Define max Mac OS X version features can be used from])
            AC_DEFINE( TG_MAC_OS_X_SDK_VERSION, 1050, [internal symbol defining the SDK version we are currently building with])
            MINVERSION=1050;
            MAXVERSION=1050;
            MACSDK="$MACSDKPREFIX/MacOSX10.5.sdk";
        elif test "$mac_os_sdk" = "10.6"
        then
            AC_DEFINE( MACOSX_DEPLOYMENT_TARGET, 10.6, [Define the target Mac OS X target version])
            AC_DEFINE( MAC_OS_X_VERSION_MIN_REQUIRED, 1060, [Define min supported Mac OS X version])
            AC_DEFINE( MAC_OS_X_VERSION_MAX_ALLOWED, 1060, [Define max Mac OS X version features can be used from])
            AC_DEFINE( TG_MAC_OS_X_SDK_VERSION, 1060, [internal symbol defining the SDK version we are currently building with])
            MINVERSION=1060;
            MAXVERSION=1060;
            MACSDK="$MACSDKPREFIX/MacOSX10.6.sdk";
        elif test "$mac_os_sdk" = "10.7"
        then
            AC_DEFINE( MACOSX_DEPLOYMENT_TARGET, 10.7, [Define the target Mac OS X target version])
            AC_DEFINE( MAC_OS_X_VERSION_MIN_REQUIRED, 1070, [Define min supported Mac OS X version])
            AC_DEFINE( MAC_OS_X_VERSION_MAX_ALLOWED, 1070, [Define max Mac OS X version features can be used from])
            AC_DEFINE( TG_MAC_OS_X_SDK_VERSION, 1070, [internal symbol defining the SDK version we are currently building with])
            MINVERSION=1070;
            MAXVERSION=1070;
            MACSDK="$MACSDKPREFIX/MacOSX10.7.sdk";
        else
            AC_MSG_ERROR([an unsupported MacOS SDK version was specified.  Please choose one of "10.4", "10.5", "10.6" or "10.7".])
        fi
        AC_CHECK_FILE("$MACSDK",,AC_MSG_ERROR([MacOS SDK "$MACSDK" not found. Please specify with '--enable-macos-sdk=...']))
        OBJC="cc -Werror -fobjc-exceptions -MMD -MF \$(*D)/\$(*F).d";
        OBJCXX="CC -MMD -MF \$(*D)/\$(*F).d";
        _OBJC_SRCS="\$(OBJC_SRCS)";
        _OBJCXX_SRCS="\$(OBJCXX_SRCS)";
        MACEXTRALIBS="\$(MACEXTRALIBS)";
        MACVERFLAGS="-mmacosx-version-min=$mac_os_sdk -isysroot $MACSDK"
        # Store the following in EXTRA_CFLAGS so that it doesn't get included
        # when cross-compiling
        EXTRA_CFLAGS="$EXTRA_CFLAGS $MACVERFLAGS -F$MACSDK/System/Library/Frameworks -I/usr/local/include";
        # And a copy in CFLAGS for the compile tests; this will get
        # overwritten later
        CFLAGS="$CFLAGS $EXTRA_CFLAGS"
        AC_SUBST(LDEXECFLAGS,["$BUILD_32BIT_OPTION $MACVERFLAGS -seg1addr 0x80000000 -Wl,-segaddr,WINE,0x00000000 -Wl,-segaddr,WINE2,0x7ffe0000 -syslibroot,$MACSDK -F$MACSDK/System/Library/Frameworks"])
        enable_pthreads="yes" # Always on on the Mac
        enable_xshm="no" # xshm is broken on the Mac
        ;;
   *)
        OBJC="";
        OBJCXX="";
        _OBJC_SRCS="";
        _OBJCXX_SRCS="";
        MACEXTRALIBS="";
        ;;
esac

AC_SUBST(OBJC)
AC_SUBST(OBJCXX)
AC_SUBST(_OBJC_SRCS)
AC_SUBST(_OBJCXX_SRCS)
AC_SUBST(MACEXTRALIBS)

# Some of the libraries we link against on the commandline are different
# between Mac OS X and other platforms. In order to support this, we create
# some variables to be used on the import and extralibs lines of Makefile.in
# for DLLs.  This is the default (non-MacOS X) setup
IMPORTS_KERNEL32=kernel32
IMPORTS_NTDLL=""
IMPORTS_GDI32=""
IMPORTS_USER32=""
EXTRALIBS_DL="-ldl"

# Also, on platforms where we are supporting SDL, we need to link against
# the SDL libraries.
# This is the default, not linking against SDL
LIBSDL=""

# MacOS X has its GL libraries in a very odd place. If we aren't careful
# we will link against the bogus ones that come with XonX (X11 for MacOS X)
# On a non-mac system, this isn't needed, so the default is blank.
LIBMACGL=""

# MacOS X CoreAudio library - only used on the Mac..
LIBMACCA=""

# MacOS X joystick frameworks. Not used elsewhere.
LIBJOYSTICK=""

STRIP_OPTION="--strip-unneeded"

dnl **** Just additional warning checks, since AC_PROG just sets 'yacc' even
dnl **** without one present.
AC_CHECK_PROGS(XYACC,$YACC bison yacc,none)
if test "$XYACC" = "none"
then
  AC_MSG_ERROR([no suitable bison/yacc found. Please install the 'bison' package.])
fi
AC_CHECK_PROGS(XLEX,$LEX flex lex,none)
if test "$XLEX" = "none"
then
  AC_MSG_ERROR([no suitable lex found. Please install the 'flex' package.])
fi
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_LN_S
AC_CHECK_PROG(C2MAN,c2man,c2man,\$(TOPSRCDIR)/tools/c2man.pl)
AC_PATH_PROG(LDCONFIG, ldconfig, true, [/sbin /usr/sbin $PATH])

AC_PATH_PROG(STRIP,strip,none)
if test "$STRIP" = "none"
then
  AC_MSG_ERROR([no suitable strip found.])
fi

AC_PATH_PROG(AS,as,none)
if test "$AS" = "none"
then
  AC_MSG_ERROR([no suitable as found.])
else
  dnl Check for AS --32
  AC_CACHE_CHECK([for AS --32 support], ac_cv_c_as_32,
    [saved_cflags=$CFLAGS
    CFLAGS="$CFLAGS -Wa,--32"
    AC_TRY_COMPILE(,[return 0],ac_cv_c_as_32="yes",ac_cv_c_as_32="no")
    CFLAGS=$saved_cflags]
  )
  if test "$ac_cv_c_as_32" = "yes"
  then
    AS="$AS --32"
    AC_SUBST(AS)
  fi
fi

case $host in
   *darwin*)
        AC_SUBST(AS,["$AS -arch i386"])
        ;;
esac

AC_PATH_PROG(PNGTOPNM,pngtopnm,none)
if test "$PNGTOPNM" = "none"
then
  AC_MSG_WARN([unable to locate pngtopnm. This is usually part of 'netpbm'])
  PNGTOPNM = ""
fi
AC_SUBST(PNGTOPNM)

AC_PATH_PROG(PPMTOBMP,ppmtobmp,none)
if test "$PPMTOBMP" = "none"
then
  AC_MSG_WARN([unable to locate ppmtobmp. This is usually part of 'netpbm'])
  PPMTOBMP = ""
fi
AC_SUBST(PPMTOBMP)

dnl **** Test for Accelerated Interproces Communication capabilities ****

## Should now test for SYSV shm stuff
AC_CHECK_FUNCS( \
	semop \
	shmget \
	ftok \
        ) 
 
## Check for POSIX IPC
AC_CHECK_HEADERS(\
        sys/types.h \
        sys/mman.h \
        semaphore.h,
        )
 
POSIX_SHM_LIB_NAME=""
AC_MSG_CHECKING([if the server should support AIC])
if test "z$enable_aic" == "zyes"
then
  AC_MSG_RESULT([yes])
  AC_DEFINE(ENABLE_AIC,1,[Define to enable accelerated interprocess communication between client and server.])

  AC_CHECK_LIB( rt, shm_open, [ POSIX_SHM_LIB_NAME="-lrt" ] )
  if test "z$POSIX_SHM_LIB_NAME" != "z"
  then
    SAVED_LIBS="$LIBS"
    LIBS="$LIBS $POSIX_SHM_LIB_NAME"
    AC_CHECK_FUNCS( \
      shm_open \
      shm_unlink,
      [ AC_DEFINE( HAVE_POSIX_SHM_FUNCS, [1], [Are the POSIX shared memory functions available] ) ]
    )
    LIBS="$SAVED_LIBS"
  else
    AC_CHECK_FUNCS( \
      shm_open \
      shm_unlink,
      [ AC_DEFINE( HAVE_POSIX_SHM_FUNCS, [1], [Are the POSIX shared memory functions available] ) ]
    )
  fi
else
  AC_MSG_RESULT([no])
fi
AC_SUBST(POSIX_SHM_LIB_NAME)

dnl Check for lint
AC_CHECK_PROGS(LINT, lclint lint)
if test "$LINT" = "lint"
then
  LINTFLAGS="$LINTFLAGS -errchk=%all,no%longptr64 -errhdr=%user -Ncheck=macro -Nlevel=4"
  dnl LINTFLAGS='-D_SIZE_T "-Dsize_t=unsigned long" -errchk=longptr64'
fi
AC_SUBST(LINT)
AC_SUBST(LINTFLAGS)

case $host_os in
  *cygwin*) LDCOMBINE="ld -r --enable-stdcall-fixup" ;;
  *) 
    case $host in
      x86_64*linux*) LDCOMBINE="ld -r -m elf_i386" ;;
      *) LDCOMBINE="ld -r" ;;
    esac
esac
AC_SUBST(LDCOMBINE)

dnl Check if we should build the preloader
AC_MSG_CHECKING([if the preloader should be built])
case $target_os in
  linux*)
    case $target_cpu in
      *i*86*) AC_SUBST(PRELOADER_BINARY, "wine-preloader")
              AC_MSG_RESULT([yes]) ;;
      *) AC_SUBST(PRELOADER_BINARY, "")
         AC_MSG_RESULT([no]) ;;
  esac ;;
  *) AC_MSG_RESULT([no]) ;;
esac

dnl **** Check for some libraries ****

PTHREAD_DEFS=""
PTHREAD_LINK=""
AC_SUBST(PTHREAD_DEFS)
AC_SUBST(PTHREAD_LINK)


if test "x$enable_pthreads" = "xyes"
then
  AC_CHECK_LIB(pthread,pthread_create,
    [ AC_DEFINE(PTHREAD_NAME,[pthread],[Name of the pthread library to link against])
      AC_DEFINE(HAVE_LIBPTHREAD, 1, [Define if libpthread is available])
      [PTHREAD_LINK="-lpthread"]
      [PTHREAD_DEFS="-DUSE_PTHREADS"]
    ],
    [AC_MSG_ERROR([--enable-pthreads specified, but pthreads not found.])])

  AC_CHECK_FUNCS(pthread_attr_setstack)
fi

dnl Check for -lm
AC_CHECK_LIB(m,sqrt)
dnl Check for -li386 for NetBSD and OpenBSD
AC_CHECK_LIB(i386,i386_set_ldt)
dnl Check for -lossaudio for NetBSD
AC_CHECK_LIB(ossaudio,_oss_ioctl)
dnl Check for -lw for Solaris
AC_CHECK_FUNCS(iswalnum,,AC_CHECK_LIB(w,iswalnum))
dnl Check for -lnsl for Solaris
AC_CHECK_FUNCS(gethostbyname,,AC_CHECK_LIB(nsl,gethostbyname))
dnl Check for -lsocket for Solaris
AC_CHECK_FUNCS(connect,,AC_CHECK_LIB(socket,connect))
dnl Check for -lxpg4 for FreeBSD
AC_CHECK_LIB(xpg4,_xpg4_setrunelocale)
dnl Check for -lmmap for OS/2
AC_CHECK_LIB(mmap,mmap)

dnl Check for openpty
UTILLIBS=""
AC_SUBST(UTILLIBS)
AC_CHECK_FUNCS(openpty,,
	[AC_CHECK_LIB(util,openpty,
		[AC_DEFINE(HAVE_OPENPTY)
		UTILLIBS="-lutil"]
	)])

DLLIBS=""
AC_SUBST(DLLIBS)
AC_CHECK_HEADERS(dlfcn.h,
    [AC_CHECK_FUNCS(dlopen,,
        [AC_CHECK_LIB(dl,dlopen,
                     [AC_DEFINE(HAVE_DLOPEN,1,[Define if you have dlopen])
                     DLLIBS="-ldl"],
                     LIBEXT="a")]
 	)],
 	LIBEXT="a"
    )

JPEGLIB=""
AC_SUBST(JPEGLIB)
AC_CHECK_HEADERS(jpeglib.h,
    AC_CHECK_LIB(jpeg,jpeg_start_decompress,
    	AC_DEFINE(HAVE_LIBJPEG,1,[Define if you have libjpeg including devel headers])
	JPEGLIB="-ljpeg"
    )
)

case $host in
    *darwin*)
        AC_CHECK_HEADERS(libjpeg/jpeglib.h,,
                        [AC_MSG_ERROR([**** You must have libjpeg framework version 6b or greater installed ****])])
        AC_DEFINE(HAVE_LIBJPEG,1,[Define if you have libjpeg including devel headers])
        CEDEGACFLAGS="$CEDEGACFLAGS -I /Library/Frameworks/libjpeg.framework/Headers";
        JPEGLIB="-framework libjpeg"
       ;;
esac

LIBPNG=""
AC_CHECK_HEADERS(png.h,
    AC_HAVE_LIBRARY(z,
       AC_CHECK_LIB(png, png_create_write_struct,
          AC_DEFINE(HAVE_LIBPNG,1,[Define if you have libpng including devel headers])
	  LIBPNG="-lpng -lz"
          ,,[-lz]
       )
    )
)
AC_SUBST(LIBPNG)

case $host in
    *darwin*)
        AC_CHECK_HEADERS(libpng/png.h,,
                        [AC_MSG_ERROR([**** You must have libpng framework version 1.2.13 or greater installed ****])])
        AC_DEFINE(HAVE_LIBPNG,1,[Define if you have libpng including devel headers])
        CEDEGACFLAGS="$CEDEGACFLAGS -I /Library/Frameworks/libpng.framework/Headers";
        LIBPNG="-framework libpng -lz"
        ;;
esac

LIBRESOLV=""
AC_CHECK_LIB(resolv,res_9_init,
   AC_DEFINE(HAVE_LIBRESOLV,1,[Define if you have libresolv including devel headers])
   LIBRESOLV="-lresolv"
)
AC_SUBST(LIBRESOLV)

if test x"$with_ssl_include" != x""
then
        SSLINCL="-I$with_ssl_include"
else
        SSLINCL=""
fi
ac_save_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$SSLINCL $CPPFLAGS"
AC_CHECK_HEADERS(openssl/ssl.h,
        AC_DEFINE(HAVE_OPENSSL,1,[Define if you have OpenSSL headers])
)
AC_SUBST(SSLINCL)
CPPFLAGS="$ac_save_CPPFLAGS"

AC_CHECK_HEADERS(fpu_control.h,
        AC_DEFINE(HAVE_FPU_CONTROL,1,[Define if you have fpu_control.h])
)

AC_CHECK_HEADERS(values.h,
        AC_DEFINE(HAVE_VALUES_H,1,[Define if you have values.h])
)

AC_SUBST(XLIB)
AC_SUBST(XFILES)
XFILES=""
AC_SUBST(D3DGLFILES)
D3DGLFILES=""
AC_SUBST(OPENGLFILES)
OPENGLFILES=""
AC_SUBST(GLU32FILES)
GLU32FILES=""
if test "$have_x" = "yes"
then
    XLIB="-lXext -lX11"
    dnl *** For some reason, the X include path isn't getting detected
    dnl properly on MacOS X; thus for now we'll hardcode it
    case $host in
        *darwin*) X_CFLAGS="$X_CFLAGS -I$MACSDK/usr/X11R6/include" ;;
    esac
    ac_save_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $X_CFLAGS"
    LIBTSX11='-L$(TOPOBJDIR)/libs/tsx11 -lwine_tsx11'

    dnl *** All of the following tests require X11/Xlib.h
    AC_CHECK_HEADERS(X11/Xlib.h,
      [
        dnl *** Check for X keyboard extension
        AC_CHECK_HEADERS(X11/XKBlib.h,
            [ dnl *** If X11/XKBlib.h exists...
              AC_CHECK_LIB(X11, XkbQueryExtension,
              AC_DEFINE(HAVE_XKB, 1, [Define if you have the XKB extension]),,
              $X_LIBS -lXext -lX11 $X_EXTRA_LIBS)
            ],
            AC_MSG_WARN([[Xkb extension not found, Wine will be built without it]]),
            [#include <X11/Xlib.h>]
        )

        dnl *** Check for X Shm extension
        if test "x$enable_xshm" = "xno"
        then
           AC_MSG_WARN([[XShm extension not checked for, Wine will be built without it]])
        else
           AC_CHECK_HEADERS(X11/extensions/XShm.h,
               [ dnl *** If X11/extensions/XShm.h exists...
                 AC_CHECK_LIB(Xext, XShmQueryExtension,
                 AC_DEFINE(HAVE_LIBXXSHM, 1, [Define if you have the X Shm extension]),,
                 $X_LIBS -lXext -lX11 $X_EXTRA_LIBS)
               ],
               AC_MSG_WARN([[XShm extension not found, Wine will be built without it]]),
               [#include <X11/Xlib.h>]
           )
        fi

        dnl *** Check for Xutil
        AC_CHECK_HEADERS(X11/Xutil.h,,,
            [#include <X11/Xlib.h>]
        )

        dnl *** Check for Xt
        AC_CHECK_HEADERS(X11/Intrinsic.h,,
          [AC_MSG_ERROR([Missing required package libXt.])],
            [#include <X11/Xlib.h>]
        )

        dnl *** Check for X shape extension
        AC_CHECK_HEADERS(X11/extensions/shape.h,
            [ dnl *** If X11/extensions/shape.h exists...
              AC_CHECK_LIB(Xext,XShapeQueryExtension,
	      AC_DEFINE(HAVE_LIBXSHAPE, 1, [Define if you have the X Shape extension]),,
	      $X_LIBS -lXext -lX11 $X_EXTRA_LIBS)
	    ],
	    AC_MSG_WARN([[XShape extension not found, Wine will be built without it]]),
            [#include <X11/Xlib.h>
             #ifdef HAVE_X11_XUTIL_H
             # include <X11/Xutil.h>
             #endif]
        )
        
        dnl *** Check for XFree86 DGA / DGA 2.0 extension
        AC_CHECK_HEADERS(X11/extensions/xf86dga.h,
            [ dnl *** If X11/extensions/xf86dga.h exists, check 
              dnl *** for XDGAQueryExtension()...
              AC_CHECK_LIB(Xxf86dga, XDGAQueryExtension,
                [ dnl *** If found...
                  AC_DEFINE(HAVE_LIBXXF86DGA2, 1,
                            [Define if you have the Xxf86dga library version 2])
                  X_PRE_LIBS="$X_PRE_LIBS -lXxf86dga"
               ],
                [ dnl *** If not found, look for XF86DGAQueryExtension()
                  dnl *** instead (DGA 2.0 not found)...
                  AC_CHECK_LIB(Xxf86dga, XF86DGAQueryExtension,
                    [ AC_DEFINE(HAVE_LIBXXF86DGA, 1,
                                [Define if you have the Xxf86dga library version 1])
                      X_PRE_LIBS="$X_PRE_LIBS -lXxf86dga"
                    ],,
                    $X_LIBS -lXext -lX11 $X_EXTRA_LIBS
                  )
                ],
                $X_LIBS -lXext -lX11 $X_EXTRA_LIBS
              )
            ],
            AC_MSG_WARN([[DGA extension not found, Wine will be built without it]]),
            [#include <X11/Xlib.h>]
        )

        dnl *** Check for XFree86 VMODE extension
        AC_CHECK_HEADERS(X11/extensions/xf86vmode.h,
            [ dnl *** If X11/extensions/xf86vmode.h exists...
                AC_CHECK_LIB(Xxf86vm, XF86VidModeQueryExtension,
                  [ AC_DEFINE(HAVE_LIBXXF86VM, 1, [Define if you have the Xxf86vm library])
                     X_PRE_LIBS="$X_PRE_LIBS -lXxf86vm"
                  ],,
                  $X_LIBS -lXext -lX11 $X_EXTRA_LIBS
                )
            ],
            AC_MSG_WARN([[XFree86 VMODE extension not found, Wine will be built without it]]),
            [#include <X11/Xlib.h>]
        )

        dnl *** Check for XVideo extension supporting XvImages
        AC_CHECK_HEADERS(X11/extensions/Xvlib.h,
            [ dnl *** If X11/extensions/Xvlib.h exists...
                AC_CHECK_LIB(Xv, XvShmCreateImage,
                  [ AC_DEFINE(HAVE_XVIDEO, 1, [Define if the X libraries support XVideo])
                     X_PRE_LIBS="$X_PRE_LIBS -lXv"
                  ],,
                  $X_LIBS -lXext -lX11 $X_EXTRA_LIBS
                )
            ],
            AC_MSG_WARN([[XVideo extension not found, Wine will be built without it]]),
            [#include <X11/Xlib.h>]
        )

        dnl *** Check for XCursor extension
        AC_CHECK_HEADERS(X11/Xcursor/Xcursor.h,xcursor_header=yes,xcursor_header=no, [#include <X11/Xlib.h>])
        if test "$xcursor_header" = "yes"
        then
             AC_CHECK_LIB(Xcursor, XcursorSupportsARGB, xcursor_lib=yes, xcursor_lib=no, $X_LIBS -lXext -lX11 $X_EXTRA_LIBS )
             if test "$xcursor_lib" = "yes" -o "$enable_xcursor_lib_check" = "no"
             then
                AC_DEFINE(HAVE_XCURSOR, 1, [Define if the X libraries support Xcursor])
             else
                AC_MSG_WARN([[XCursor extension library not found, Cedega will be built without it]])
             fi
        else
            AC_MSG_WARN([[XCursor extension headers not found, Cedega will be built without it]])
        fi

        dnl *** Check for XRender and XRandR extensions. XRandR depends on XRender.
        dnl *** These two libraries have bypasses since they're very optional and dynamically loaded in Cedega
        dnl *** to ensure a greater backwards compatibility.
        AC_CHECK_LIB( Xrender, XRenderQueryExtension,
                       xrender_lib=yes,
                       xrender_lib=no,
                       $X_LIBS -lXext -lX11 $X_EXTRA_LIBS ) 
        if test "$xrender_lib" = "yes" -o "$enable_xrender_lib_check" = "no"
        then
            dnl *** If X11/extensions/Xrender.h exists...
            AC_CHECK_HEADERS( X11/extensions/Xrender.h,
                               xrender_header=yes,
                               xrender_header=no,
                               [#include <X11/Xlib.h>] )
            if test "$xrender_header" = "yes"
            then
                AC_DEFINE(HAVE_LIBXRENDER, 1, [Define if you have the XRender extension library])

                # dnl *** Check for XRandR extension check for XRRQueryExtension & XRRConfigSizes (XRANDR 1.1)
                AC_CHECK_LIB( Xrandr, XRRQueryExtension,
                               AC_CHECK_LIB( Xrandr, XRRConfigSizes,
                                              xrandr_lib=yes,
                                              xrandr_lib=no,
                                              $X_LIBS -lXext -lX11 $X_EXTRA_LIBS -lXrender ),
                               xrandr_lib="no",
                               $X_LIBS -lXext -lX11 $X_EXTRA_LIBS -lXrender )
               if test "$xrandr_lib" = "yes" -o "$enable_xrandr_lib_check" = "no"
               then
                   dnl *** If X11/extensions/Xrandr.h exists... 
                   AC_CHECK_HEADERS( X11/extensions/Xrandr.h,
                                      xrandr_header=yes,
                                      xrandr_header=no,
                                      [#include <X11/Xlib.h>] )
                   if test "$xrandr_header" = "yes"
                   then
                       AC_DEFINE(HAVE_LIBXRANDR, 1, [Define if you have the Xrandr extension library])
                   else
                       AC_MSG_WARN([[X RandR extension headers not found. Cedega will be built without it.]])
                   fi
               else
                   AC_MSG_WARN([[X RandR extension library not found. Cedega will be built without it.]])
               fi
            else
                AC_MSG_WARN([[X Render extension headers not found. Cedega will be built without it. Will not check for X RandR extension.]])
            fi
        else
            AC_MSG_WARN([[X Render extension library not found. Cedega will be built without it. Will not check for X RandR extension.]])
        fi

     ]
    ) dnl *** End of X11/Xlib.h check

    dnl Check for the presence of OpenGL
    if test "x$enable_opengl" != "xno"
    then
        if test -f /usr/X11R6/lib/libGL.a -a ! -f /usr/X11R6/lib/libGL.so
        then
            case $host in
                 *darwin*)
               ;;
		*) AC_MSG_ERROR([/usr/X11R6/lib/libGL.a is present on your system.
This prevents linking to OpenGL. Delete the file and restart configure.]) ;;
            esac
        fi

	AC_CHECK_HEADERS(GL/gl.h GL/glu.h GL/glx.h)

        if test "$ac_cv_header_GL_glu_h" = "no"
        then
            case $host in
                 *darwin*)
               ;;
		*) AC_MSG_ERROR([GL/glu.h is not found.]) ;;
            esac
        fi
        

	if test "$ac_cv_header_GL_gl_h" = "yes" -a "$ac_cv_header_GL_glx_h" = "yes"
	then
	    AC_CHECK_HEADERS(GL/glext.h,,,[#include <GL/gl.h>])
	    dnl Check for some problems due to old Mesa versions
	    AC_CACHE_CHECK([for up-to-date OpenGL version], wine_cv_opengl_version_OK,
	      AC_TRY_COMPILE(
		[#include <GL/gl.h>],
		[GLenum test = GL_UNSIGNED_SHORT_5_6_5;],
		[wine_cv_opengl_version_OK="yes"],
		[wine_cv_opengl_version_OK="no"]
	      )
            )

	    dnl Check for the thread-safety of the OpenGL library
	    AC_CACHE_CHECK([for thread-safe OpenGL version], 
			   wine_cv_opengl_version_threadsafe,
              [saved_libs=$LIBS
	       LIBS="$X_LIBS -lGL"
	       AC_TRY_LINK([],[pthread_getspecific();],
			      [wine_cv_opengl_version_threadsafe="yes"],
			      [wine_cv_opengl_version_threadsafe="no"])
	       LIBS=$saved_libs]
	    )

	    if test "$wine_cv_opengl_version_OK" = "yes"
            then
		dnl Check for the presence of the library
		AC_CHECK_LIB(GL,glXCreateContext,
			     X_PRE_LIBS="$X_PRE_LIBS -lGL"
			     ,,
			     $X_LIBS -lXext -lX11 -lm $X_EXTRA_LIBS)

		if test "$ac_cv_lib_GL_glXCreateContext" = "yes"
		then
			D3DGLFILES='$(D3DGLFILES)'
			OPENGLFILES='$(OPENGLFILES)'
			AC_DEFINE(HAVE_OPENGL, 1, [Define if OpenGL is present on the system])

			AC_CHECK_LIB(GL,glXGetProcAddressARB,
                                     AC_DEFINE(HAVE_GLX_GETPROCADDRESS, 1,
                                               [Define if the OpenGL library supports the glXGetProcAddressARB call]),,
                                     $X_LIBS -lXext -lX11 -lm $X_EXTRA_LIBS)

			if test "$ac_cv_lib_GL_glXGetProcAddressARB" = "yes"
			then
			       AC_CACHE_CHECK([for OpenGL extension functions prototypes], wine_cv_extension_prototypes,
			          [AC_TRY_COMPILE([#include <GL/gl.h>
				  		  #ifdef HAVE_GL_GLEXT_H
						  # include <GL/glext.h>
						  #endif
						  ],
						 [PFNGLCOLORTABLEEXTPROC test_proc;],
						 [wine_cv_extension_prototypes="yes"],
						 [wine_cv_extension_prototypes="no"]
				  )]
				)
				if test "$wine_cv_extension_prototypes" = "yes"
				then
				    AC_DEFINE(HAVE_GLEXT_PROTOTYPES, 1,
                                              [Define if the OpenGL headers define extension typedefs])
				fi
			fi

		fi

                dnl Check for use of OSMESA
                OSMESA=no
                AC_SUBST(OSMESA)
                if test "x$enable_osmesa" = "xyes"
                then
                  AC_CHECK_LIB(OSMesa, OSMesaCreateContextExt,
                     [X_PRE_LIBS="-L/usr/lib/the_sims/winex_sims/lib -lOSMesa $X_PRE_LIBS"
                      OSMESA=yes
                      AC_DEFINE(HAVE_OSMESA, 1, [Define if OSMesa is to be used for D3D])],
                     AC_CHECK_LIB(MesaOS, OSMesaCreateContextExt,
                        [X_PRE_LIBS="-L/usr/lib/the_sims/winex_sims/lib -lMesaOS $X_PRE_LIBS"
                         OSMESA=yes
                         AC_DEFINE(HAVE_OSMESA, 1, [Define if OSMesa is to be used for D3D])],
                        OSMESA=no,
                        -L/usr/local/lib),
                     -L/usr/local/lib -lGL $X_LIBS -lXext -lX11 -lm $X_EXTRA_LIBS)
                fi

	        dnl Check for GLU32 library.
		AC_CHECK_LIB(GLU,gluLookAt,
			     [X_PRE_LIBS="$X_PRE_LIBS -lGLU"
			     GLU32FILES='$(GLU32FILES)']
			     ,,
			     $X_LIBS $X_PRE_LIBS -lXext -lX11 -lm $X_EXTRA_LIBS
		)

                SAVE_LIBS="$LIBS"
                LIBS="$X_PRE_LIBS -lGLU -lGL $X_LIBS -lXext -lX11 -lm $X_EXTRA_LIBS"
                AC_CHECK_FUNCS(gluBuild2DMipmapLevels)
                LIBS="$SAVE_LIBS"
	     fi
	 fi
    fi

    CPPFLAGS="$ac_save_CPPFLAGS"
    XFILES='$(XFILES)'
else
    XLIB=""
    X_CFLAGS=""
    X_LIBS=""
    LIBTSX11=""

# do OpenGL for non-X here... 
    if test "x$enable_opengl" = "xyes"
    then
        AC_DEFINE(HAVE_OPENGL, 1, [Define if OpenGL is present on the system])
            case $host in
                *darwin*)
                          D3DGLFILES='$(D3DGLFILES)'
			  OPENGLFILES='$(OPENGLFILES)'
			  GLU32FILES='$(GLU32FILES)'
                          AC_DEFINE(HAVE_MACOSX_GL, 1, [Define if we need to avoid use of GL dir prefix for GL headers])
                          XLIB="$XLIB";
                          X_CFLAGS="$X_CFLAGS -I $MACSDK/System/Library/Frameworks/OpenGL.framework/Headers";
                          X_LIBS="$X_LIBS -framework OpenGL";
                          ;;
            esac
    fi
fi

dnl **** Check which curses lib to use ***
CURSESLIBS=""
if test "x$with_curses" != "xno"
then
    AC_CHECK_HEADERS(ncurses.h,
        [AC_CHECK_LIB(ncurses,waddch,
            [AC_DEFINE(HAVE_LIBNCURSES, 1, [Define if you have the ncurses library (-lncurses)])
             CURSESLIBS="-lncurses"],
             [AC_CHECK_HEADERS(curses.h,
                 [AC_CHECK_LIB(curses,waddch,
                     [AC_DEFINE(HAVE_LIBCURSES, 1, [Define if you have the curses library (-lcurses)])
                      CURSESLIBS="-lcurses"])])])])
    saved_libs="$LIBS"
    LIBS="$CURSESLIBS $LIBS"
    AC_CHECK_FUNCS(getbkgd resizeterm)
    LIBS="$saved_libs"
fi
AC_SUBST(CURSESLIBS)

CUPSLIBS=""
dnl **** Check for CUPS ****
wine_cv_warn_cups_h=no
AC_CHECK_LIB(cups,cupsGetPPD,
	[AC_CHECK_HEADER(cups/cups.h,
	    [AC_DEFINE(HAVE_CUPS, 1, [Define if we have CUPS])
	    CUPSLIBS="-lcups"],
            wine_cv_warn_cups_h=yes)]
)
AC_SUBST(CUPSLIBS)

dnl **** Check for SANE ****
AC_CHECK_PROG(sane_devel,sane-config,sane-config,no)
if test "$sane_devel" = "no"
then
    SANELIB=""
    SANEDEF=""
else
    SANELIB="`$sane_devel --libs`"
    SANEDEF="`$sane_devel --cflags`"
    ac_save_CPPFLAGS="$CPPFLAGS"
    ac_save_LIBS="$LIBS"
    CPPFLAGS="$CPPFLAGS $SANEDEF"
    LIBS="$LIBS $SANELIB"
    AC_CHECK_HEADER(sane/sane.h,
	[AC_CHECK_LIB(sane,sane_open,
	    [AC_DEFINE(HAVE_SANE, 1, [Define if we have SANE development environment])],
	    [SANELIB=""
             SANEDEF=""])
        ],
	[SANELIB=""
         SANEDEF=""]
    )
    LIBS="$ac_save_LIBS"
    CPPFLAGS="$ac_save_CPPFLAGS"
fi
AC_SUBST(SANELIB)
AC_SUBST(SANEDEF)

dnl **** Check for FreeType 2 ****
AC_CHECK_LIB(freetype,FT_Init_FreeType,ft_lib=yes,ft_lib=no,$X_LIBS)
if test "$ft_lib" = "no" -a "x$enable_freetype2_lib_check" = "xyes" -a "x$enable_sdldrv" = "no"
then
    FREETYPEINCL=""
    wine_cv_msg_freetype=no
else
    AC_CHECK_PROG(ft_devel,freetype-config,freetype-config,no)
    if test "$ft_devel" = "no"
    then
        AC_CHECK_PROG(ft_devel2,freetype2-config,freetype2-config,no)
        if test "$ft_devel2" = "freetype2-config"
	then
		ft_devel=$ft_devel2
	fi
    fi

    case $host in
        *darwin*)
           FREETYPEINCL="-I$MACSDK/usr/X11R6/include -I$MACSDK/usr/X11R6/include/freetype2"
           ft_devel="yes" ;;
        *) if test "$ft_devel" != "no"
           then
              FREETYPEINCL=`$ft_devel --cflags`
           fi ;;
    esac

    if test "$ft_devel" = "no"
    then
	FREETYPEINCL=""
	wine_cv_msg_freetype=yes
    else
	ac_save_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$FREETYPEINCL $CPPFLAGS"
	AC_CHECK_HEADERS(ft2build.h)
	AC_CHECK_HEADERS(freetype/freetype.h \
                         freetype/ftglyph.h \
                         freetype/tttables.h \
                         freetype/ftnames.h \
                         freetype/ftsnames.h \
                         freetype/ttnameid.h \
                         freetype/ftoutln.h \
                         freetype/internal/sfnt.h,,,
                         [#if HAVE_FT2BUILD_H
                          # include <ft2build.h>
                          #endif]
			)

	AC_CHECK_HEADER(freetype/tttables.h,
                  [AC_CHECK_DECL( FT_Load_Sfnt_Table, 
		                  [AC_DEFINE( HAVE_FT_LOAD_SFNT_TABLE, 1, [Define if you have the FT_Load_Sfnt_Table.] )],,
		                  [#if HAVE_FT2BUILD_H
                                      # include <ft2build.h>
				      # include <freetype/tttables.h>
                                   #endif]
				  )]
		)
			
	AC_TRY_CPP([#include <ft2build.h>
                    #include <freetype/fttrigon.h>],
                    [AC_DEFINE(HAVE_FREETYPE_FTTRIGON_H, 1,
          [Define if you have the <freetype/fttrigon.h> header file.])
                    wine_cv_fttrigon=yes],
                    wine_cv_fttrigon=no)
	CPPFLAGS="$ac_save_CPPFLAGS"
	dnl Check that we have at least freetype/freetype.h
	if test "$ac_cv_header_freetype_freetype_h" = "yes" -a "$wine_cv_fttrigon" = "yes"
	then
	    AC_DEFINE(HAVE_FREETYPE, 1, [Define if FreeType 2 is installed])
	    wine_cv_msg_freetype=no
	else
	    FREETYPEINCL=""
	    wine_cv_msg_freetype=yes
	fi
    fi
fi
AC_SUBST(FREETYPEINCL)

dnl **** Check for FontConfig ****
ac_save_CPPFLAGS="$CPPFLAGS"
case $host in
    *darwin*) FONTCONFINCL="-I$MACSDK/usr/X11R6/include"
              CPPFLAGS="$CPPFLAGS -L$MACSDK/usr/X11R6/lib"
              ;;
    *) FONTCONFINCL="" ;;
esac
CPPFLAGS="$CPPFLAGS $FONTCONFINCL"
AC_CHECK_LIB(fontconfig,FcInit,fc_lib=yes,fc_lib=no,)
if test "$fc_lib" = "yes" -o "x$enable_fontconfig_lib_check" = "xno"
then
    AC_CHECK_HEADERS(fontconfig/fontconfig.h)
    if test "$ac_cv_header_fontconfig_fontconfig_h" = "yes"
    then
        AC_DEFINE(HAVE_FONTCONFIG, 1, [Define if Fontconfig is installed])
    fi
fi
CPPFLAGS="$ac_save_CPPFLAGS"
AC_SUBST(FONTCONFINCL)

dnl **** Check for parport (currently Linux only) ****
AC_CACHE_CHECK([for parport header/ppdev.h], ac_cv_c_ppdev,
 AC_TRY_COMPILE(
   [#include <linux/ppdev.h>],
   [ioctl (1,PPCLAIM,0)],
   [ac_cv_c_ppdev="yes"],
   [ac_cv_c_ppdev="no"])
 )
if test "$ac_cv_c_ppdev" = "yes"
then
    AC_DEFINE(HAVE_PPDEV, 1, [Define if we can use ppdev.h for parallel port access])
fi

dnl **** Check for IPX (currently Linux only) ****
AC_CACHE_CHECK([for GNU style IPX support], ac_cv_c_ipx_gnu,
 AC_TRY_COMPILE(
   [#include <sys/types.h>
    #ifdef HAVE_SYS_SOCKET_H
    # include <sys/socket.h>
    #endif
    #include <netipx/ipx.h>],
   [((struct sockaddr_ipx *)0)->sipx_family == AF_IPX],
   [ac_cv_c_ipx_gnu="yes"],
   [ac_cv_c_ipx_gnu="no"])
 )
if test "$ac_cv_c_ipx_gnu" = "yes"
then
    AC_DEFINE(HAVE_IPX_GNU, 1, [Define if IPX should use netipx/ipx.h from libc])
fi

if test "$ac_cv_c_ipx_gnu" = "no"
then
 AC_CACHE_CHECK([for linux style IPX support], ac_cv_c_ipx_linux,
  AC_TRY_COMPILE(
    [#include <sys/types.h>
     #ifdef HAVE_SYS_SOCKET_H
     # include <sys/socket.h>
     #endif
     #include <asm/types.h>
     #include <linux/ipx.h>],
    [((struct sockaddr_ipx *)0)->sipx_family == AF_IPX],
    [ac_cv_c_ipx_linux="yes"],
    [ac_cv_c_ipx_linux="no"])
  )
  if test "$ac_cv_c_ipx_linux" = "yes"
  then
      AC_DEFINE(HAVE_IPX_LINUX, 1, [Define if IPX includes are taken from Linux kernel])
  fi
fi


dnl **** Check for ENOMEDIUM inside of errno.h ****
AC_CACHE_CHECK([for ENOMEDIUM in errno.h],
	ac_cv_c_enomedium_in_errno_h,
        AC_EGREP_CPP(yes,
           [#include <errno.h>
            #ifdef ENOMEDIUM
              yes
            #endif
           ], 
           ac_cv_c_enomedium_in_errno_h=yes,
           ac_cv_c_enomedium_in_errno_h=no)
   )
if test "$ac_cv_c_enomedium_in_errno_h" = "yes"
then
   AC_DEFINE(HAVE_ERRNODEF_ENOMEDIUM, 1, [Define if ENOMEDIUM is defined inside of errno.h])
fi


dnl **** Check for Open Sound System ****
AC_CHECK_HEADERS(sys/soundcard.h machine/soundcard.h soundcard.h, break)

AC_CACHE_CHECK([for Open Sound System],
	ac_cv_c_opensoundsystem,
	AC_TRY_COMPILE([
	#if defined(HAVE_SYS_SOUNDCARD_H)
		#include <sys/soundcard.h>
	#elif defined(HAVE_MACHINE_SOUNDCARD_H)
		#include <machine/soundcard.h>
	#elif defined(HAVE_SOUNDCARD_H)
		#include <soundcard.h>
	#endif
	],[

/* check for one of the Open Sound System specific SNDCTL_ defines */
#if !defined(SNDCTL_DSP_STEREO)
#error No open sound system
#endif
],ac_cv_c_opensoundsystem="yes",ac_cv_c_opensoundsystem="no"))

if test "$ac_cv_c_opensoundsystem" = "yes"
then
    AC_DEFINE(HAVE_OSS, 1, [Define if you have the Open Sound system])
fi

AC_CACHE_CHECK([for Open Sound System/MIDI interface],
	ac_cv_c_opensoundsystem_midi,
	AC_TRY_COMPILE([
	#if defined(HAVE_SYS_SOUNDCARD_H)
		#include <sys/soundcard.h>
	#elif defined(HAVE_MACHINE_SOUNDCARD_H)
		#include <machine/soundcard.h>
	#elif defined(HAVE_SOUNDCARD_H)
		#include <soundcard.h>
	#endif
	],[

/* check for one of the Open Sound System specific SNDCTL_SEQ defines */
#if !defined(SNDCTL_SEQ_SYNC)
#error No open sound system MIDI interface
#endif
],ac_cv_c_opensoundsystem_midi="yes",ac_cv_c_opensoundsystem_midi="no"))

if test "$ac_cv_c_opensoundsystem_midi" = "yes"
then
    AC_DEFINE(HAVE_OSS_MIDI, 1, [Define if you have the Open Sound system (MIDI interface)])
fi

dnl **** Check for ALSA ****
ALSALIBS=""
AC_CHECK_HEADERS(alsa/asoundlib.h sys/asoundlib.h)
if test "$ac_cv_header_alsa_asoundlib_h" = "yes" -o "$ac_cv_header_sys_asoundlib_h" = "yes"
then
    AC_CHECK_LIB(asound,snd_pcm_open,
        AC_DEFINE(HAVE_ALSA,1,[Define if you have ALSA including devel headers])
        ALSALIBS="-lasound"
    )
fi
AC_SUBST(ALSALIBS)

dnl **** Check for OpenLDAP ***
AC_SUBST(LDAPLIBS,"")
if test "$ac_cv_header_ldap_h" = "yes" -a "$ac_cv_header_lber_h" = "yes"
then
    AC_CHECK_TYPE(LDAPSortKey,
        [AC_CHECK_LIB(ldap_r, ldap_initialize,
            [AC_CHECK_LIB(lber, ber_init,
                [AC_DEFINE(HAVE_LDAP, 1, [Define if you have the OpenLDAP development environment])
                 LDAPLIBS="-lldap_r -llber"],,
                 [$LIBPTHREAD])],,
                 [$LIBPTHREAD])],,
        [#include <ldap.h>])
    AC_CHECK_FUNCS(\
        ldap_count_references \
        ldap_first_reference \
        ldap_next_reference \
        ldap_parse_reference \
        ldap_parse_sort_control \
        ldap_parse_sortresponse_control \
        ldap_parse_vlv_control \
        ldap_parse_vlvresponse_control,
        [$LDAPLIBS $LIBPTHREAD])
fi

dnl **** Check for valgrind ****
AC_CHECK_HEADERS(\
	valgrind/valgrind.h \
	valgrind/helgrind.h \
	valgrind/memcheck.h)

dnl **** Check for broken glibc mmap64 ****

AC_CACHE_CHECK( [whether mmap64 works defined as mmap], ac_cv_mmap64_works,
	AC_TRY_RUN([
		#define _FILE_OFFSET_BITS 64
		#include <stdio.h>
		#include <unistd.h>
		#include <fcntl.h>
		#include <sys/mman.h>
		#include <errno.h>

		int main(int argc,char **argv) {
			int fd = open("conftest.map",O_CREAT|O_RDWR,0600);
			if (fd == -1) exit(1);

			unlink("conftest.map");

			write(fd,"test",4);

			if ((-1 == mmap(0,4,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0)) &&
			    (errno == EINVAL)
			) {
				exit(1);
			}
			close(fd);
			fprintf(stderr,"success!\n");
			exit(0);
		}

	],
    ac_cv_mmap64_works="yes",
    ac_cv_mmap64_works="no",
    ac_cv_mmap64_works="no") )

if test "$ac_cv_mmap64_works" = "yes"
then
    AC_DEFINE(_FILE_OFFSET_BITS, 64, [Set this to 64 to enable 64-bit file support on Linux])
fi

dnl **** store the current EXTRA_CFLAGS in CEDEGAEXTRA_CFLAGS ****
CEDEGAEXTRA_CFLAGS="$CEDEGAEXTRA_CFLAGS $EXTRA_CFLAGS"

dnl **** Check for gcc strength-reduce bug ****

if test "x${GCC}" = "xyes"
then
  CEDEGACFLAGS="$CEDEGACFLAGS -Wall -pipe"
  case $host in
     *darwin*)
        case $host_cpu in i[[3456789]]86*) 
           CEDEGACFLAGS="$CEDEGACFLAGS -msse2" ;;
        esac
  esac

  AC_CACHE_CHECK( [for gcc strength-reduce bug], ac_cv_c_gcc_strength_bug,
                  AC_TRY_RUN([
int	L[[4]] = {0,1,2,3};
int main(void) {
  static int Array[[3]];
  unsigned int B = 3;
  int i;
  for(i=0; i<B; i++) Array[[i]] = i - 3;
  for(i=0; i<4 - 1; i++) L[[i]] = L[[i + 1]];
  L[[i]] = 4;
  
  exit( Array[[1]] != -2 || L[[2]] != 3);
}],
    ac_cv_c_gcc_strength_bug="no",
    ac_cv_c_gcc_strength_bug="yes",
    ac_cv_c_gcc_strength_bug="yes") )
  if test "$ac_cv_c_gcc_strength_bug" = "yes"
  then
    CEDEGACFLAGS="$CEDEGACFLAGS -fno-strength-reduce"
  fi

  dnl Check for -mpreferred-stack-boundary
  AC_CACHE_CHECK([for gcc -mpreferred-stack-boundary=2 support],
                 ac_cv_c_gcc_stack_boundary,
  [saved_cflags=$CFLAGS
  CFLAGS="$CFLAGS -mpreferred-stack-boundary=2"
  AC_TRY_COMPILE(,[return 0],ac_cv_c_gcc_stack_boundary="yes",ac_cv_c_gcc_stack_boundary="no")
  CFLAGS=$saved_cflags
  ])
  if test "$ac_cv_c_gcc_stack_boundary" = "yes"
  then
    case $host in
       *darwin*) ;;
       *) CEDEGACFLAGS="$CEDEGACFLAGS -mpreferred-stack-boundary=2" ;;
    esac
  fi

  dnl **** Check if compiler version can handle -fno-keep-static-consts flag ****
  AC_CACHE_CHECK([if gcc compiler supports -fno-keep-static-consts flag],
                 ac_cv_c_gcc_supports_no_keep_static_consts,
  [saved_cflags=$CFLAGS
   CFLAGS="$CFLAGS -fno-keep-static-consts"
   AC_TRY_COMPILE([],
		 [return 0],
		 ac_cv_c_gcc_supports_no_keep_static_consts="yes",
		 ac_cv_c_gcc_supports_no_keep_static_consts="no"
 	         )
  CFLAGS=$saved_cflags
  ])
  if test "$ac_cv_c_gcc_supports_no_keep_static_consts" = "yes"
  then
    CEDEGACFLAGS="$CEDEGACFLAGS -fno-keep-static-consts"
  fi

  dnl **** Check if compiler thinks that __const is a keyword ****
  AC_CACHE_CHECK([if gcc compiler thinks that __const is a keyword],
                 ac_cv_c_gcc_is_dumb_with_consts,
  [
   AC_TRY_COMPILE([],
                  [__const int bob = 0; int fred(void) { return bob; } ],
		  ac_cv_c_gcc_is_dumb_with_consts="yes",
		  ac_cv_c_gcc_is_dumb_with_consts="no"
 	         )
  ])
  if test "$ac_cv_c_gcc_is_dumb_with_consts" = "yes"
  then
    CEDEGACFLAGS="$CEDEGACFLAGS -D__const=const"
  fi

  dnl Check whether compiler understands __builtin_expect.
  AC_CACHE_CHECK([if the compiler has__builtin_expect],
                 ac_cv_c_gcc_supports_builtin_expect,
                 [
                   AC_TRY_COMPILE([],
                                  [int foo (int a) { a = __builtin_expect (a, 10); return a == 10 ? 0 : 1; } ],
                                  ac_cv_c_gcc_supports_builtin_expect="yes",
                                  ac_cv_c_gcc_supports_builtin_expect="no"
                                 )
                 ])
  if test "$ac_cv_c_gcc_supports_builtin_expect" = "yes"
  then
    AC_DEFINE(HAVE_BUILTIN_EXPECT,1,[Define if compiler supports __builtin_expect])
  fi

  dnl Check for -fno-strict-aliasing
  AC_CACHE_CHECK([if gcc supports the -fno-strict-aliasing option],
                 ac_cv_c_gcc_no_strict_aliasing,
                 [ saved_cflags=$CFLAGS
                   CFLAGS="$CFLAGS -fno-strict-aliasing"
                   AC_TRY_COMPILE([],
		                  [return 0],
		                  ac_cv_c_gcc_no_strict_aliasing="yes",
		                  ac_cv_c_gcc_no_strict_aliasing="no"
 	                         )
                   CFLAGS=$saved_cflags
                 ])
  if test "$ac_cv_c_gcc_no_strict_aliasing" = "yes"
  then
    CEDEGACFLAGS="$CEDEGACFLAGS -fno-strict-aliasing"
  fi

  dnl Check for -fno-optimize-sibling-calls - at least in gcc-4 on Mac and 3.4 on Linux, this can
  dnl result in incorrect code
  AC_CACHE_CHECK([if gcc supports the -fno-optimize-sibling-calls option],
                 ac_cv_c_gcc_no_optimize_sibling_calls,
                 [ saved_cflags=$CFLAGS
                   CFLAGS="$CFLAGS -fno-optimize-sibling-calls"
                   AC_TRY_COMPILE([],
		                  [return 0],
		                  ac_cv_c_gcc_no_optimize_sibling_calls="yes",
		                  ac_cv_c_gcc_no_optimize_sibling_calls="no"
 	                         )
                   CFLAGS=$saved_cflags
                 ])
  if test "$ac_cv_c_gcc_no_optimize_sibling_calls" = "yes"
  then
    CEDEGACFLAGS="$CEDEGACFLAGS -fno-optimize-sibling-calls"
  fi

  dnl Check for -Werror-implicit-function-declaration
  AC_CACHE_CHECK([if gcc supports the -Werror-implicit-function-declaration option],
                 ac_cv_c_gcc_error_implicit_function,
                 [ saved_cflags=$CFLAGS
                   CFLAGS="$CFLAGS -Werror-implicit-function-declaration"
                   AC_TRY_COMPILE([],
                                 [return 0],
                                 ac_cv_c_gcc_error_implicit_function="yes",
                                 ac_cv_c_gcc_error_implicit_function="no"
                                )
                   CFLAGS=$saved_cflags
                 ])
  if test "$ac_cv_c_gcc_error_implicit_function" = "yes"
  then
    CEDEGACFLAGS="$CEDEGACFLAGS -Werror-implicit-function-declaration"
  fi

  dnl Check for -Wa,--execstack
  AC_CACHE_CHECK([if gcc supports the -Wa,--execstack option],
                 ac_cv_c_gcc_execstack,
                 [ saved_cflags=$CFLAGS
                   CFLAGS="$CFLAGS -Wa,--execstack"
                   AC_TRY_COMPILE([],
		                  [return 0],
		                  ac_cv_c_gcc_execstack="yes",
		                  ac_cv_c_gcc_execstack="no"
 	                         )
                   CFLAGS=$saved_cflags
                 ])
  if test "$ac_cv_c_gcc_execstack" = "yes"
  then
    CEDEGAEXTRA_CFLAGS="$CEDEGAEXTRA_CFLAGS -Wa,--execstack"
  fi

  dnl Check for -MMD
  AC_CACHE_CHECK([if gcc supports the -MMD option],
                 ac_cv_c_gcc_gen_depend_and_compile,
                 [ saved_cflags=$CFLAGS
                   CFLAGS="$CFLAGS -MMD"
                   AC_TRY_COMPILE([],
                                  [return 0],
                                  ac_cv_c_gcc_gen_depend_and_compile="yes",
                                  ac_cv_c_gcc_gen_depend_and_compile="no"
                                 )
                   CFLAGS=$saved_cflags
                 ])

  AC_MSG_CHECKING([should compile warnings be treated as errors])
  if test "x$enable_warnings_as_errors" = "xyes"
  then
    AC_MSG_RESULT([yes])

    dnl **** Check if compiler version can handle -Werror ****
    AC_CACHE_CHECK([if gcc compiler supports -Werror flag],
                   ac_cv_c_gcc_supports_w_error,
                   [saved_cflags=$CFLAGS
                   CFLAGS="-Wall -Werror"
                   AC_TRY_COMPILE([],
                                  [int fred(void) { return 0; }],
                                  ac_cv_c_gcc_supports_w_error="yes",
                                  ac_cv_c_gcc_supports_w_error="no"
                                 )
                   CFLAGS=$saved_cflags
                   ])

    if test "$ac_cv_c_gcc_supports_w_error" = "yes"
    then
      CEDEGACFLAGS="$CEDEGACFLAGS -Werror"
    fi

  else
      AC_MSG_RESULT([no])
  fi

fi

AC_SUBST(MAKEDEPENDS_AND_COMPILE) 

dnl *** Determine how we're going to create dependencies ***
if test "x${GCC}" = "xyes" -a "$ac_cv_c_gcc_gen_depend_and_compile" = "yes"
then
  # the "-MF \$(*D)/\$(*F).d" makes sure the preprocessor puts the dependency
  # file in the same directory as the source file.  This shouldn't be needed,
  # but the Fedora 7 preprocessor puts it in the current directory, even when
  # the source file is in a subdirectory.
  MAKEDEPENDS_AND_COMPILE="\$(CC) -MMD -MF \$(*D)/\$(*F).d -c \$(ALLCFLAGS) -o \$@ \$<"
else
  # Don't know how to do dependencies so cheap out.
  MAKEDEPENDS_AND_COMPILE="echo this doesn't work"
fi

dnl **** Check if we need to place .type inside a .def directive ****

AC_CACHE_CHECK([whether .type can sit inside a .def directive],
	       ac_cv_c_type_in_def,
[saved_libs=$LIBS
LIBS="conftest_asm.s $LIBS"
cat > conftest_asm.s <<EOF
	.globl _ac_test
	.def _ac_test; .scl 2; .type 32; .endef
_ac_test:
	.long 0
EOF
AC_TRY_LINK(,,ac_cv_c_type_in_def="yes",ac_cv_c_type_in_def="no")
LIBS=$saved_libs])
if test "$ac_cv_c_type_in_def" = "yes"
then
  AC_DEFINE(NEED_TYPE_IN_DEF, 1, [Define if .type asm directive must be inside a .def directive])
fi

AC_CACHE_CHECK([whether .type directive is supported by itself],
	       ac_cv_c_type_supported,
[saved_libs=$LIBS
LIBS="conftest_asm.s $LIBS"
cat > conftest_asm.s <<EOF
	.globl _ac_test
	.type _ac_test,@function
_ac_test:
	.long 0
EOF
AC_TRY_LINK(,,ac_cv_c_type_supported="yes",ac_cv_c_type_supported="no")
LIBS=$saved_libs])
if test "$ac_cv_c_type_supported" = "yes"
then
  AC_DEFINE(TYPE_ASM_SUPPORTED, 1, [Define if .type asm directive is supported by itself])
fi
 
dnl **** Check for underscore on external symbols ****

AC_CACHE_CHECK([whether external symbols need an underscore prefix],
               ac_cv_c_extern_prefix,
[saved_libs=$LIBS
LIBS="conftest_asm.s $LIBS"
cat > conftest_asm.s <<EOF
	.globl _ac_test
_ac_test:
	.long 0
EOF
AC_TRY_LINK([extern int ac_test;],[if (ac_test) return 1],
            ac_cv_c_extern_prefix="yes",ac_cv_c_extern_prefix="no")
LIBS=$saved_libs])
if test "$ac_cv_c_extern_prefix" = "yes"
then
  AC_DEFINE(NEED_UNDERSCORE_PREFIX, 1,
            [Define if symbols declared in assembly code need an underscore prefix])
fi

dnl **** Check whether stdcall symbols need to be decorated ****

AC_CACHE_CHECK([whether stdcall symbols need to be decorated],
               ac_cv_c_stdcall_decoration,
[saved_libs=$LIBS
LIBS="conftest_asm.s $LIBS"
if test "$ac_cv_c_extern_prefix" = "yes"
then
cat > conftest_asm.s <<EOF
	.globl _ac_test@0
_ac_test@0:
EOF
else
cat > conftest_asm.s <<EOF
	.globl ac_test@0
ac_test@0:
EOF
fi
AC_TRY_LINK([extern void __attribute__((__stdcall__)) ac_test(void);],
            [ac_test(); return 0],
            ac_cv_c_stdcall_decoration="yes",ac_cv_c_stdcall_decoration="no")
LIBS=$saved_libs])
if test "$ac_cv_c_stdcall_decoration" = "yes"
then
  AC_DEFINE(NEED_STDCALL_DECORATION, 1,
            [Define if stdcall symbols need to be decorated])
fi

dnl **** Check for .string in assembler ****

AC_CACHE_CHECK([whether assembler accepts .string],
               ac_cv_c_asm_string,
[saved_libs=$LIBS
LIBS="conftest_asm.s $LIBS"
cat > conftest_asm.s <<EOF
	.string "test"
EOF
AC_TRY_LINK(,,ac_cv_c_asm_string="yes",ac_cv_c_asm_string="no")
LIBS=$saved_libs])
if test "$ac_cv_c_asm_string" = "yes"
then
  AC_DEFINE(HAVE_ASM_STRING, 1, [Define to use .string instead of .ascii])
fi

dnl **** Check for working dll ****

LDSHARED=""
LDDLLFLAGS=""
if test "$LIBEXT" = "so"
then
  AC_CACHE_CHECK([whether we can build a GNU style ELF dll],
                 ac_cv_c_dll_gnuelf,
  [saved_cflags=$CFLAGS
  CFLAGS="$CFLAGS -fPIC -shared -Wl,-soname,conftest.so.1.0,-Bsymbolic"
  AC_TRY_LINK(,[return 1],ac_cv_c_dll_gnuelf="yes",ac_cv_c_dll_gnuelf="no")
  CFLAGS=$saved_cflags
  ])
  if test "$ac_cv_c_dll_gnuelf" = "yes"
  then
    LDSHARED="\$(CC) $BUILD_32BIT_OPTION -shared \$(TOPSRCDIR)/linux.map \$(SONAME:%=-Wl,-soname,%)"
    LDDLLFLAGS="-Wl,-Bsymbolic"

    AC_CACHE_CHECK([whether the linker supports --export-dynamic],
                   ac_cv_c_export_dynamic,
                   [saved_cflags=$CFLAGS
                    CFLAGS="$CFLAGS -fPIC $BUILD_32BIT_OPTION -Wl,--export-dynamic"
                    AC_TRY_LINK(,[return 1],ac_cv_c_export_dynamic="yes",ac_cv_c_export_dynamic="no")
                    CFLAGS=$saved_cflags
                    ])
    if test "$ac_cv_c_export_dynamic" = "yes"
    then
        AC_SUBST(LDEXECFLAGS,["$BUILD_32BIT_OPTION -Wl,--export-dynamic"])
    fi

    case $target_cpu in
      *i*86*)
         AC_CACHE_CHECK([whether the linker can relocate to 0x3f000100],
                        ac_cv_c_reloc,
                        [saved_cflags=$CFLAGS
                         CFLAGS="$CFLAGS -fPIC -Wl,--section-start,.interp=0x3f000100"
                         AC_TRY_LINK(,[return 1],ac_cv_c_reloc="yes",ac_cv_c_reloc="no")
                         CFLAGS=$saved_cflags
                         ])
         if test "$ac_cv_c_reloc" = "yes"
         then
             AC_SUBST(LDEXECFLAGS,["$LDEXECFLAGS -Wl,--section-start,.interp=0x3f000100"])
         fi
         ;;
    esac
  else
    AC_CACHE_CHECK(whether we can build a UnixWare (Solaris) dll,
                 ac_cv_c_dll_unixware,
    [saved_cflags=$CFLAGS
    CFLAGS="$CFLAGS -fPIC -Wl,-G,-h,conftest.so.1.0,-B,symbolic"
    AC_TRY_LINK(,[return 1],ac_cv_c_dll_unixware="yes",ac_cv_c_dll_unixware="no")
    CFLAGS=$saved_cflags
    ])
    if test "$ac_cv_c_dll_unixware" = "yes"
    then
      LDSHARED="\$(CC) -Wl,-G \$(SONAME:%=-Wl,-h,%)"
      LDDLLFLAGS="-Wl,-B,symbolic"
    fi
  fi
  if test "$ac_cv_c_dll_gnuelf" = "no" -a "$ac_cv_c_dll_unixware" = "no"
  then
    LIBEXT="a"
    AC_CHECK_PROG(DLLWRAP,dllwrap,dllwrap,false)
    if test "$DLLWRAP" = "dllwrap"; then
      dnl FIXME - check whether dllwrap works correctly...
      case $host_os in
        *cygwin*)
            LIBEXT="dll"
            ;;
      esac
    fi
  fi

fi

dnl Check for cross compiler for test programs
AC_SUBST(CROSSTEST,"")
TG_CHECK_MINGW_PROG(CROSSCC,gcc,false)
TG_CHECK_MINGW_PROG(DLLTOOL,dlltool,false)
TG_CHECK_MINGW_PROG(CROSSWINDRES,windres,false)
TG_CHECK_MINGW_PROG(MINGWAR,ar,false)
if test "$CROSSCC" != "false"; then CROSSTEST="\$(CROSSTEST)"; fi

if test "$LIBEXT" = "a"; then
  AC_MSG_ERROR(
[could not find a way to build shared libraries.
It is currently not possible to build Wine without shared library
(.so) support to allow transparent switch between .so and .dll files.
If you are using Linux, you will need a newer binutils.]
)
fi

DLLFLAGS=""
LDPATH=""

if test "$LIBEXT" = "so"; then
    DLLFLAGS="-fPIC"
    LDPATH="LD_LIBRARY_PATH=\"\$(TOPOBJDIR)/libs/port:\$(TOPOBJDIR)/libs/unicode:\$\$LD_LIBRARY_PATH\""
elif test "$LIBEXT" = "dll"; then
    #DLLFLAGS="-fPIC" # -fPIC doesn't work(at least in cygwin-b20) - FIXME
    LDPATH="PATH=\"\$(TOPOBJDIR)/libs/port:\"\$(TOPOBJDIR)/libs/unicode:\$\$PATH\""
elif test "$LIBEXT" = "dylib"; then
     DLLFLAGS="-fPIC"
    LDPATH="DYLD_LIBRARY_PATH=\"\$(TOPOBJDIR)/libs/port:\$(TOPOBJDIR)/libs/unicode:\$\$DYLD_LIBRARY_PATH\""
    LDSHARED="\$(CC) -dynamiclib -fPIC $BUILD_32BIT_OPTION -undefined dynamic_lookup $MACVERFLAGS"
    LDCXXSHARED="\$(CXX) -dynamiclib -fPIC $BUILD_32BIT_OPTION -undefined dynamic_lookup $MACVERFLAGS"
fi

dnl Check for the presence of libSDL
AC_MSG_CHECKING([Checking if the sdldrv should be built])
if test "x$enable_sdldrv" = "xyes"
then
    AC_MSG_RESULT([yes])

    # FIXME - Should add a test to see if the SDL libraries and all their
    # dependencies are available
    SDLDRVFILES=sdldrv
    SDL_VERSION=1.2.0
    case $host in
        *darwin*)
            AC_CHECK_HEADER(SDL/SDL.h,,
                            [AC_MSG_ERROR([**** You must have SDL framework version 1.2.5 or greater installed ****])])
           ;;
        *)
            AM_PATH_SDL($SDL_VERSION,:,
                        AC_MSG_ERROR([*** SDL version $SDL_VERSION not found!])
                       )
           ;;
    esac
else
    AC_MSG_RESULT([no])
fi

dnl Set up arguments for release mode of wine
if test "x$enable_release" = "xyes"
then
    case $host in
        *darwin*) CFLAGS="$CFLAGS -DNDEBUG -O3" ;;
    esac
else
    #in all other cases, we want to enable -g flags...
    CFLAGS="$CFLAGS -g"
fi

darwin_sdldrv_build="no"
case $host in
    *darwin*)
        LDFLAGS="$LDFLAGS"
        if test "x$enable_sdldrv" = "xyes" 
        then
            darwin_sdldrv_build="yes"
            LIBSDL="-framework SDL"
        fi
        LIBSDL="$LIBSDL -framework Cocoa -framework OpenGL -framework Carbon -framework WebKit"
        LIBMACCA="-framework CoreAudio -framework AudioToolbox"
        LIBJOYSTICK="-framework IOKit -framework ForceFeedback -framework CoreFoundation"
        LIBSDL="$LIBSDL -framework SystemConfiguration"
        dnl Set up arguments for packaged mode of wine
        PACKAGE_LINK="-install_name @executable_path/../Frameworks/\$@"
        PACKAGE_LINK_TEMPLATE="-install_name @executable_path/../Frameworks/\$\$@"
        AC_SUBST(PACKAGE_LINK)
        AC_SUBST(PACKAGE_LINK_TEMPLATE)
        if test "x$enable_package" = "xyes"
        then
          LDSHARED="$LDSHARED -install_name @executable_path/../Frameworks/\$@"
        fi
    ;;
esac

AM_CONDITIONAL(DARWIN_SDLDRV, test "x$darwin_sdldrv_build" = "xyes")

AC_CHECK_HEADER(CoreAudio/CoreAudio.h,
	AC_DEFINE(HAVE_MACOSX_COREAUDIO, 1, [Define if you are on MacOSX and have CoreAudio]) )

AC_CHECK_HEADER(Security/Security.h,
	AC_DEFINE(HAVE_SECURITY_SECURITY_H, 1, [Define if you are on MacOSX and have Security]) )


AC_SUBST(MAC_RELEASE_LDFLAGS)

AC_SUBST(DLLFLAGS)
AC_SUBST(LDSHARED)
AC_SUBST(LDDLLFLAGS)
AC_SUBST(LIBEXT)
AC_SUBST(LDPATH)
AC_SUBST(SDLDRVFILES)
AC_SUBST(PS2DRVFILES)
AC_SUBST(IMPORTS_KERNEL32)
AC_SUBST(IMPORTS_NTDLL)
AC_SUBST(IMPORTS_GDI32)
AC_SUBST(IMPORTS_USER32)
AC_SUBST(EXTRALIBS_DL)
AC_SUBST(LIBSDL)
AC_SUBST(LIBMACGL)
AC_SUBST(LIBMACCA)
AC_SUBST(LIBTSX11)
AC_SUBST(LIBJOYSTICK)
AC_SUBST(STRIP_OPTION)

AC_DEFUN([WINE_CHECK_ERRNO],
[
  AC_CACHE_CHECK(for the errno variable name on this platform: $1, wine_cv_errno_name_$1,
    [AC_TRY_LINK(
      [#include <stdio.h>
       #include <errno.h>],
      [ printf( "%d\n", *$1() ); ],
      wine_cv_errno_name_$1=yes,
      wine_cv_errno_name_$1=no
    )]
  )

if test "$wine_cv_errno_name_$1" = "yes"
then
    wine_cv_errno_name=$1 
fi
])

wine_cv_errno_name="" 
dnl Linux style errno location
WINE_CHECK_ERRNO(__errno_location)
dnl FreeBSD style errno location
WINE_CHECK_ERRNO(__error)
dnl Solaris style errno location
WINE_CHECK_ERRNO(___errno)
dnl UnixWare style errno location
WINE_CHECK_ERRNO(__thr_errno)
dnl NetBSD style errno location
WINE_CHECK_ERRNO(__errno)

if test "z$wine_cv_errno_name" != "z" 
then
  AC_DEFINE_UNQUOTED(ERRNO_LOCATION,$wine_cv_errno_name,
	             [Define to the name of the function returning the location of errno for libc])
fi



dnl **** Check for functions ****

AC_FUNC_ALLOCA()
AC_CHECK_FUNCS(\
	__libc_fork \
	_lwp_create \
	_pclose \
	_popen \
	_snprintf \
	_stricmp \
	_strnicmp \
	chsize \
	clone \
	ecvt \
	finite \
	fpclass \
	ftruncate \
	ftruncate64 \
	getaddrinfo \
	getlogin_r \
	getnetbyaddr \
	getnetbyname \
	getpagesize \
	getprotobyname \
	getprotobynumber \
	getrlimit \
	getservbyport \
	getsockopt \
	inet_network \
	lseek64 \
	lstat \
	memmove \
	mkstemps \
	mmap \
	pclose \
	popen \
	pread \
	pwrite \
	readdir \
	rfork \
	select \
	sendmsg \
	settimeofday \
	sigaltstack \
	snprintf \
	statfs \
	strcasecmp \
	strerror \
	strncasecmp \
	tcgetattr \
	timegm \
	usleep \
	vfscanf \
	wait4 \
	waitpid \
)

dnl **** Check for header files ****

AC_CHECK_HEADERS(\
	arpa/inet.h \
	arpa/nameser.h \
	asm/vm86.h \
	direct.h \
	dirent.h \
	elf.h \
	float.h \
	getopt.h \
	ieeefp.h \
	ifaddrs.h \
	io.h \
	libio.h \
	libutil.h \
	link.h \
	linux/cdrom.h \
	linux/compiler.h \
	linux/hdreg.h \
	linux/input.h \
	linux/joystick.h \
	linux/major.h \
	linux/param.h \
	linux/serial.h \
	linux/ucdrom.h \
	mach/mach.h \
 	mach-o/dyld.h \
	netdb.h \
	netinet/in.h \
	netinet/in_systm.h \
	netinet/tcp.h \
	netinet/tcp_fsm.h \
	poll.h \
	pty.h \
	pwd.h \
	regex.h \
	sched.h \
	scsi/sg.h \
	socket.h \
	stdint.h \
	strings.h \
	sys/cdio.h \
	sys/errno.h \
	sys/file.h \
	sys/filio.h \
	sys/ioctl.h \
	sys/ipc.h \
	sys/link.h \
	sys/lwp.h \
	sys/mman.h \
	sys/modem.h \
	sys/msg.h \
	sys/param.h \
	sys/poll.h \
	sys/ptrace.h \
	sys/reg.h \
	sys/sem.h \
	sys/shm.h \
	sys/signal.h \
	sys/socket.h \
	sys/socketvar.h \
	sys/sockio.h \
	sys/statfs.h \
	sys/strtio.h \
	sys/syscall.h \
	sys/sysctl.h \
	sys/time.h \
	sys/v86.h \
	sys/v86intr.h \
	sys/vfs.h \
	sys/vm86.h \
	sys/wait.h \
	syscall.h \
	unistd.h \
	utmp.h \
)
AC_HEADER_STAT()

dnl **** Checks for headers that depend on other ones ****

AC_CHECK_HEADERS(sys/mount.h sys/user.h,,,
    [#include <sys/types.h>
     #if HAVE_SYS_PARAM_H
     # include <sys/param.h>
     #endif])

AC_CHECK_HEADERS([netinet/in_pcb.h netinet/ip_var.h net/if.h net/if_arp.h net/if_dl.h net/if_types.h net/route.h resolv.h],,,
    [#include <sys/types.h>
     #if HAVE_SYS_SOCKET_H
     # include <sys/socket.h>
     #endif
     #if HAVE_SYS_SOCKETVAR_H
     # include <sys/socketvar.h>
     #endif
     #if HAVE_NETINET_IN_H
     # include <netinet/in.h>
     #endif])

AC_CHECK_HEADERS([netinet/tcp_var.h],,,
    [#include <sys/types.h>
     #if HAVE_SYS_SOCKET_H
     # include <sys/socket.h>
     #endif
     #if HAVE_SYS_SOCKETVAR_H
     # include <sys/socketvar.h>
     #endif
     #if HAVE_NETINET_IN_H
     # include <netinet/in.h>
     #endif
     #ifdef HAVE_NETINET_TCP_H
     # include <netinet/tcp.h>
     #endif])

AC_CHECK_HEADERS(netinet/ip.h,,,
    [#include <sys/types.h>
     #if HAVE_SYS_SOCKET_H
     # include <sys/socket.h>
     #endif
     #if HAVE_NETINET_IN_SYSTM_H
     # include <netinet/in_systm.h>
     #endif])

AC_CHECK_HEADERS(ucontext.h,,,[#include <signal.h>])

dnl **** Check for types ****

AC_C_CONST
AC_C_INLINE
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_SIZEOF(char)
AC_CHECK_SIZEOF(short)
AC_CHECK_SIZEOF(int)
AC_CHECK_SIZEOF(long long)

AC_CACHE_CHECK([whether linux/input.h is for real],
	wine_cv_linux_input_h,
	AC_TRY_COMPILE([
	    #include <linux/input.h>
	] , [
	    int foo = EVIOCGBIT(EV_ABS,42);
	    int bar = BTN_PINKIE;
	    int fortytwo = 42;
	],
	wine_cv_linux_input_h=yes,
	wine_cv_linux_input_h=no,
	no
	)
    )
    if test "$wine_cv_linux_input_h" = "yes"
    then
        AC_DEFINE(HAVE_CORRECT_LINUXINPUT_H, 1,
                  [Define if we have linux/input.h AND it contains the INPUT event API])
    fi

   
AC_CACHE_CHECK([whether we can use re-entrant gethostbyname_r Linux style],
   	wine_cv_linux_gethostbyname_r_6,
	AC_TRY_LINK([
#include <netdb.h>
	], [
    char *name=NULL;
    struct hostent he;
    struct hostent *result;
    char *buf=NULL;
    int bufsize=0;
    int res,errnr;
    char *addr=NULL;
    int addrlen=0;
    int addrtype=0;
    res=gethostbyname_r(name,&he,buf,bufsize,&result,&errnr);
    res=gethostbyaddr_r(addr, addrlen, addrtype,&he,buf,bufsize,&result,&errnr);
    ],
	wine_cv_linux_gethostbyname_r_6=yes,
	wine_cv_linux_gethostbyname_r_6=no
	)
   )
   if test "$wine_cv_linux_gethostbyname_r_6" = "yes"
   then
      AC_DEFINE(HAVE_LINUX_GETHOSTBYNAME_R_6, 1,
                [Define if Linux-style gethostbyname_r and gethostbyaddr_r are available])
   fi

if test "$ac_cv_header_linux_joystick_h" = "yes"
then
   AC_CACHE_CHECK([whether linux/joystick.h uses the Linux 2.2+ API],
   	wine_cv_linux_joystick_22_api,
	AC_TRY_COMPILE([
	#include <sys/types.h>
	#include <sys/ioctl.h>
	#include <linux/joystick.h>

	struct js_event blub;
	#if !defined(JS_EVENT_AXIS) || !defined(JS_EVENT_BUTTON)
	#error "no 2.2 header"
	#endif
	],/*empty*/,
	wine_cv_linux_joystick_22_api=yes,
	wine_cv_linux_joystick_22_api=no,
	wine_cv_linux_joystick_22_api=no
	)
   )
   if test "$wine_cv_linux_joystick_22_api" = "yes"
   then
      AC_DEFINE(HAVE_LINUX_22_JOYSTICK_API, 1,
                [Define if <linux/joystick.h> defines the Linux 2.2 joystick API])
   fi
fi

dnl **** statfs checks ****

if test "$ac_cv_header_sys_vfs_h" = "yes"
then
    AC_CACHE_CHECK( [whether sys/vfs.h defines statfs],
		    wine_cv_sys_vfs_has_statfs,
	AC_TRY_COMPILE([
	#include <sys/types.h>
	#ifdef HAVE_SYS_PARAM_H
	# include <sys/param.h>
	#endif
	#include <sys/vfs.h>
	],[
		struct statfs stfs;

		memset(&stfs,0,sizeof(stfs));
	],wine_cv_sys_vfs_has_statfs=yes,wine_cv_sys_vfs_has_statfs=no
	)
    )
    if test "$wine_cv_sys_vfs_has_statfs" = "yes"
    then
      AC_DEFINE(STATFS_DEFINED_BY_SYS_VFS, 1,
                [Define if the struct statfs is defined by <sys/vfs.h>])
    fi
fi

if test "$ac_cv_header_sys_statfs_h" = "yes"
then
    AC_CACHE_CHECK( [whether sys/statfs.h defines statfs],
		    wine_cv_sys_statfs_has_statfs,
	AC_TRY_COMPILE([
	#include <sys/types.h>
	#ifdef HAVE_SYS_PARAM_H
	# include <sys/param.h>
	#endif
	#include <sys/statfs.h>
	],[
		struct statfs stfs;
	],wine_cv_sys_statfs_has_statfs=yes,wine_cv_sys_statfs_has_statfs=no
	)
    )
    if test "$wine_cv_sys_statfs_has_statfs" = "yes"
    then
      AC_DEFINE(STATFS_DEFINED_BY_SYS_STATFS, 1,
                [Define if the struct statfs is defined by <sys/statfs.h>])
    fi
fi

if test "$ac_cv_header_sys_mount_h" = "yes"
then
    AC_CACHE_CHECK( [whether sys/mount.h defines statfs],
		    wine_cv_sys_mount_has_statfs,
	AC_TRY_COMPILE([
	#include <sys/types.h>
	#ifdef HAVE_SYS_PARAM_H
	# include <sys/param.h>
	#endif
	#include <sys/mount.h>
	],[
		struct statfs stfs;
	],wine_cv_sys_mount_has_statfs=yes,wine_cv_sys_mount_has_statfs=no
	)
    )
    if test "$wine_cv_sys_mount_has_statfs" = "yes"
    then
      AC_DEFINE(STATFS_DEFINED_BY_SYS_MOUNT, 1,
                [Define if the struct statfs is defined by <sys/mount.h>])
    fi
fi

dnl *** Leopard defines a new type for sigaltstack
if test "$ac_cv_func_sigaltstack" = "yes"
then
   AC_CACHE_CHECK( [whether sigaltstack takes a stack_t],
		    wine_cv_type_sigaltstack_takes_stack_t,
	AC_TRY_COMPILE([#include <signal.h>
                #include <stdlib.h>],
                [stack_t teststack;
                sigaltstack (&teststack, NULL);
	],wine_cv_type_sigaltstack_takes_stack_t=yes,
        wine_cv_type_sigaltstack_takes_stack_t=no
	)
    )
    if test "$wine_cv_type_sigaltstack_takes_stack_t" = "yes"
    then
      AC_DEFINE(SIGALTSTACK_TAKES_STACK_T, 1,
                [Define if signal.h defines stack_t, which will then be used by sigaltstack])
    fi
fi

dnl *** Check for some structure members

dnl Macro to check if a structure contains a specified member
dnl Usage: WINE_CHECK_STRUCT_MEMBER(struct,member,[includes,[action-if-found,[action-if-not-found]]])

AC_DEFUN([WINE_CHECK_STRUCT_MEMBER],
[AC_CACHE_CHECK([for $2 in struct $1], ac_cv_c_$1_$2,
 AC_TRY_COMPILE([$3],[struct $1 s; s.$2 = 0],ac_cv_c_$1_$2="yes",ac_cv_c_$1_$2="no"))
AS_IF([ test "x$ac_cv_c_$1_$2" = "xyes"],[$4],[$5])
])

AC_DEFUN([WINE_CHECK_PLAIN_STRUCT_MEMBER],
[AC_CACHE_CHECK([for $2 in struct $1], ac_cv_c_$1_$2,
 AC_TRY_COMPILE([$3],[$1 s; s.$2 = 0],ac_cv_c_$1_$2="yes",ac_cv_c_$1_$2="no"))
AS_IF([ test "x$ac_cv_c_$1_$2" = "xyes"],[$4],[$5])
])

dnl **** FIXME: what about mixed cases, where we need two of them? ***

WINE_CHECK_STRUCT_MEMBER(statfs,f_bfree,
[#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef STATFS_DEFINED_BY_SYS_MOUNT
# include <sys/mount.h>
#else
# ifdef STATFS_DEFINED_BY_SYS_VFS
#  include <sys/vfs.h>
# else
#  ifdef STATFS_DEFINED_BY_SYS_STATFS
#   include <sys/statfs.h>
#  endif
# endif
#endif],
    [AC_DEFINE(STATFS_HAS_BFREE, 1, [Define if the struct statfs has the member bfree])])

WINE_CHECK_STRUCT_MEMBER(statfs,f_bavail,
[#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef STATFS_DEFINED_BY_SYS_MOUNT
# include <sys/mount.h>
#else
# ifdef STATFS_DEFINED_BY_SYS_VFS
#  include <sys/vfs.h>
# else
#  ifdef STATFS_DEFINED_BY_SYS_STATFS
#   include <sys/statfs.h>
#  endif
# endif
#endif],
    [AC_DEFINE(STATFS_HAS_BAVAIL, 1, [Define if the struct statfs has the member bavail])])

dnl Check for file descriptor passing with msg_accrights
WINE_CHECK_STRUCT_MEMBER(msghdr,msg_accrights,
[#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif],
    [AC_DEFINE(HAVE_MSGHDR_ACCRIGHTS, 1, [Define if struct msghdr contains msg_accrights])])

dnl Check for the sa_len member in struct sockaddr
WINE_CHECK_STRUCT_MEMBER(sockaddr,sa_len,
[#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif],
    [AC_DEFINE(HAVE_STRUCT_SOCKADDR_SA_LEN, 1, [Define if struct sockaddr contains sa_len])])

dnl Check for the sun_len member in struct sockaddr_un
WINE_CHECK_STRUCT_MEMBER(sockaddr_un,sun_len,
[#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_SYS_UN_H
# include <sys/un.h>
#endif],
    [AC_DEFINE(HAVE_SOCKADDR_SUN_LEN, 1, [Define if struct sockaddr_un contains sun_len])])

if test "x$enable_pthreads" = "xno"
then
dnl Check for the __m_kind member in struct pthread_mutex_t
  WINE_CHECK_PLAIN_STRUCT_MEMBER(pthread_mutex_t,__m_kind,
    [#define _REENTRANT
     #include <pthread.h>],
    [AC_DEFINE(HAVE_MUTEX_M_KIND, 1, [Define if struct pthread_mutex_t contains __m_kind])],
    [
        dnl Check for the __data.__kind member in struct pthread_mutex_t
        AC_CACHE_CHECK([for __data.__kind in struct pthread_mutex_t],
           ac_cv_c_pthread_mutex_t___data__kind,
           AC_TRY_COMPILE([#define _REENTRANT
                           #include <pthread.h>],
                          [pthread_mutex_t s; s.__data.__kind = 0],
                          ac_cv_c_pthread_mutex_t___data__kind="yes",
                          ac_cv_c_pthread_mutex_t___data__kind="no"))
        AS_IF([ test "x$ac_cv_c_pthread_mutex_t___data__kind" = "xyes"],
              [AC_DEFINE(HAVE_MUTEX_KIND, 1, [Define if struct pthread_mutex_t contains __data.__kind])],
              [AC_MSG_ERROR([Unable to find either __m_kind or __data.__kind in pthread_mutex_t])])
  ])
fi

dnl *** check for x86 Mach thread state and their structure member prefix
if test "$ac_cv_header_mach_mach_h" = "yes"
then
  AC_CACHE_CHECK([for x86_thread_state32_t],
                  ac_cv_mach_x86_state,
                 [
                   AC_TRY_COMPILE([#include <mach/mach.h>],
                                  [x86_thread_state32_t regs;],
                                  ac_cv_mach_x86_state="yes",
                                  ac_cv_mach_x86_state="no"
                                 )
                 ])
  if test "$ac_cv_mach_x86_state" = "yes"
  then
    AC_DEFINE(HAVE_MACH_X86_STATE,1,
        [Define if we have Mach x86_thread_state32_t])

        dnl *** Check whether members of the thread state are prefixed by __
        WINE_CHECK_PLAIN_STRUCT_MEMBER(x86_thread_state32_t,__eax,
                [#include <mach/mach.h>],
                [AC_DEFINE(MACH_X86_STATE_NEEDS_PREFIX, 1,
                        [Define if members of x86_thread_state32_t are prefixed by __])])
  fi
fi

dnl *** check for the need to define __i386__

X86FILES=""
AC_SUBST(X86FILES)

DDRAW_HACKS=""
AC_SUBST(DDRAW_HACKS)


case $target_cpu in
 *i[[3456789]]86*)
    case $host in
      *darwin*) ;;
      *) X86FILES='$(X86FILES)' ;;
    esac
    DDRAW_HACKS=""
    AC_CACHE_CHECK([whether we need to define __i386__],ac_cv_cpp_def_i386,
      AC_EGREP_CPP(yes,[#ifndef __i386__
        yes
        #endif],
      ac_cv_cpp_def_i386="yes", ac_cv_cpp_def_i386="no"))
    ;;
esac
if test "$ac_cv_cpp_def_i386" = "yes"
then
    CEDEGACFLAGS="$CEDEGACFLAGS -D__i386__"
    LINTFLAGS="$LINTFLAGS -D__i386__"
fi

dnl **** Test for winebuild tools future host ****
dnl **** Yes, it's a @$%&! kluge ****
echo "Checking winebuild tool host..."
case "$host_cpu" in
    i[[3456789]]86*)  ;;
    sparc*) CEDEGACFLAGS="$CEDEGACFLAGS -D__target_sparc__" ;;
    mips*) CEDEGACFLAGS="$CEDEGACFLAGS -D__target_mips__" ;;
    alpha*) CEDEGACFLAGS="$CEDEGACFLAGS -D__target_alpha__" ;;
    *powerpc*) CEDEGACFLAGS="$CEDEGACFLAGS -D__PPC__" ;;
esac

case "$host_os" in
    *darwin*) 
        CEDEGACFLAGS="$CEDEGACFLAGS -fno-common"
        IMPORTS_KERNEL32=""
        IMPORTS_GDI32="gdi32"
        IMPORTS_NTDLL="ntdll"
        IMPORTS_USER32="user32"
        EXTRALIBS_DL=""
        LIBMACGL="-L$MACSDK/System/Library/Frameworks/OpenGL.framework/Libraries"
        STRIP_OPTION="-x";;
	*)  ;;
esac


dnl **** Setup MSVC compatibility ****

if test "$ac_cv_sizeof_char" = "1"
then
  CEDEGACFLAGS="$CEDEGACFLAGS -D__int8=char"
else
  AC_MSG_ERROR([Sizeof char is not the expected 1 byte - is <$ac_cv_sizeof_char>. Please handle]);
fi

if test "$ac_cv_sizeof_short" = "2"
then
  CEDEGACFLAGS="$CEDEGACFLAGS -D__int16=short"
else
  AC_MSG_ERROR([Sizeof short is not the expected 2 bytes - is <$ac_cv_sizeof_short>. Please handle]);
fi

if test "$ac_cv_sizeof_int" = "4"
then
  CEDEGACFLAGS="$CEDEGACFLAGS -D__int32=int"
else
  AC_MSG_ERROR([Sizeof int is not the expected 4 bytes - is <$ac_cv_sizeof_int>. Please handle]);
fi  

if test "$ac_cv_sizeof_long_long" = "8"
then
  CEDEGACFLAGS="$CEDEGACFLAGS \"-D__int64=long long\""
else
  AC_MSG_ERROR([Sizeof long long is not the expected 8 bytes - is <$ac_cv_sizeof_long_long>. Please handle]);
fi  

# Set EXTRA_CFLAGS to be the current value of CEDEGAEXTRA_CFLAGS
EXTRA_CFLAGS="$CEDEGAEXTRA_CFLAGS"
AC_SUBST(EXTRA_CFLAGS)

# Set CFLAGS to be the current value of CEDEGACFLAGS so that it's up-to-date.
CFLAGS="$CEDEGACFLAGS"

dnl **** Generate output files ****

dnl Macro to create non-existent directories from config.status
dnl Usage: WINE_CONFIG_EXTRA_DIR(dirname)
AC_DEFUN([WINE_CONFIG_EXTRA_DIR],
[AC_CONFIG_COMMANDS([$1],[test -d "$1" || (AC_MSG_NOTICE([creating $1]) && mkdir "$1")])])

AH_TOP([#ifndef __WINE_CONFIG_H
#define __WINE_CONFIG_H])

AH_BOTTOM([#endif /* __WINE_CONFIG_H */])

#WINE_CONFIG_EXTRA_DIR(dlls/ddraw/d3ddevice)
#WINE_CONFIG_EXTRA_DIR(dlls/ddraw/dclipper)
#WINE_CONFIG_EXTRA_DIR(dlls/ddraw/ddraw)
#WINE_CONFIG_EXTRA_DIR(dlls/ddraw/direct3d)
#WINE_CONFIG_EXTRA_DIR(dlls/ddraw/dpalette)
#WINE_CONFIG_EXTRA_DIR(dlls/ddraw/dsurface)
#WINE_CONFIG_EXTRA_DIR(dlls/dinput/joystick)
#WINE_CONFIG_EXTRA_DIR(dlls/dinput/keyboard)
#WINE_CONFIG_EXTRA_DIR(dlls/dinput/mouse)
WINE_CONFIG_EXTRA_DIR(dlls/kernel/messages)
WINE_CONFIG_EXTRA_DIR(dlls/user/dde)
WINE_CONFIG_EXTRA_DIR(dlls/user/resources)
WINE_CONFIG_EXTRA_DIR(dlls/user/tests)
#WINE_CONFIG_EXTRA_DIR(dlls/wineps/data)
WINE_CONFIG_EXTRA_DIR(include/wine)
WINE_CONFIG_EXTRA_DIR(programs/regapi/tests)
#WINE_CONFIG_EXTRA_DIR(programs/winetest/tests)

if (test -f WINEX_VERSION)
then
   WINEX_VERSION="Cedega `cat WINEX_VERSION`"
else
   WINEX_VERSION="Cedega CVS"
fi


AC_CONFIG_COMMANDS([include/wine/version.h],
[AC_MSG_NOTICE([creating include/wine/version.h])
cat >$tmp/version.h <<CEOF
/* Generated automatically by configure; DO NOT EDIT! */
#define WINE_RELEASE_INFO "$winex_version"
CEOF
if cmp -s $tmp/version.h include/wine/version.h 2>/dev/null; then
  AC_MSG_NOTICE([include/wine/version.h is unchanged])
  rm -f $tmp/version.h
else
  rm -f include/wine/version.h
  mv $tmp/version.h include/wine/version.h
fi],
[winex_version="$WINEX_VERSION"]
)

BUILD_NATIVE=Makenative.rules
AC_SUBST_FILE(BUILD_NATIVE)	

MAKE_RULES=Make.rules
AC_SUBST_FILE(MAKE_RULES)

MAKE_DLL_RULES=dlls/Makedll.rules
AC_SUBST_FILE(MAKE_DLL_RULES)

MAKE_TEST_RULES=dlls/Maketest.rules
AC_SUBST_FILE(MAKE_TEST_RULES)

MAKE_PROG_RULES=programs/Makeprog.rules
AC_SUBST_FILE(MAKE_PROG_RULES)

AC_CONFIG_FILES([
Make.rules
dlls/Makedll.rules
dlls/Maketest.rules
programs/Makeprog.rules
Makefile
dlls/Makefile
dlls/advapi32/Makefile
dlls/advpack/Makefile
dlls/avicap32/Makefile
dlls/bcrypt/Makefile
dlls/cabinet/Makefile
dlls/comctl32/Makefile
dlls/crypt32/Makefile
dlls/dbghelp/Makefile
dlls/ddraw/Makefile
dlls/dnsapi/Makefile
dlls/dsound/Makefile
dlls/dwmapi/Makefile
dlls/dxdiagn/Makefile
dlls/gdi/Makefile
dlls/gdi/enhmetafiledrv/Makefile
dlls/gdi/metafiledrv/Makefile
dlls/gdi/win16drv/Makefile
dlls/gdiplus/Makefile
dlls/glu32/Makefile
dlls/hlink/Makefile
dlls/icmp/Makefile
dlls/imagehlp/Makefile
dlls/iphlpapi/Makefile
dlls/kernel/Makefile
dlls/lzexpand/Makefile
dlls/mlang/Makefile
dlls/mpr/Makefile
dlls/msacm/Makefile
dlls/msacm/applemp3/Makefile
dlls/msacm/imaadp32/Makefile
dlls/msacm/msadp32/Makefile
dlls/msacm/msg711/Makefile
dlls/msacm/winemp3/Makefile
dlls/msdmo/Makefile
dlls/mshtml/Makefile
dlls/msi/Makefile
dlls/msimg32/Makefile
dlls/msvideo/Makefile
dlls/msvideo/msrle32/Makefile
dlls/netapi32/Makefile
dlls/ntdll/Makefile
dlls/ntdll/msdos/Makefile
dlls/odbccp32/Makefile
dlls/ole32/Makefile
dlls/oleaut32/Makefile
dlls/powrprof/Makefile
dlls/psapi/Makefile
dlls/quartz/Makefile
dlls/riched20/Makefile
dlls/richedit/Makefile
dlls/rpcrt4/Makefile
dlls/rsaenh/Makefile
dlls/schannel/Makefile
dlls/secur32/Makefile
dlls/sensapi/Makefile
dlls/setupapi/Makefile
dlls/sfc/Makefile
dlls/shdocvw/Makefile
dlls/shell32/Makefile
dlls/shlwapi/Makefile
dlls/urlmon/Makefile
dlls/user/Makefile
dlls/userenv/Makefile
dlls/usp10/Makefile
dlls/version/Makefile
dlls/windowscodecs/Makefile
dlls/winhttp/Makefile
dlls/wininet/Makefile
dlls/winmm/Makefile
dlls/winmm/mcianim/Makefile
dlls/winmm/mciavi/Makefile
dlls/winmm/mcicda/Makefile
dlls/winmm/mciseq/Makefile
dlls/winmm/mciwave/Makefile
dlls/winmm/midimap/Makefile
dlls/winmm/wavemap/Makefile
dlls/winmm/winealsa/Makefile
dlls/winmm/wineoss/Makefile
dlls/wintrust/Makefile
dlls/wldap32/Makefile
dlls/wow32/Makefile
dlls/wtsapi32/Makefile
documentation/Makefile
include/Makefile
libs/Makefile
libs/openssl/Makefile
libs/port/Makefile
libs/ptmalloc3/Makefile
libs/unicode/Makefile
libs/uuid/Makefile
libs/wine/Makefile
libs/wpp/Makefile
miscemu/Makefile
programs/Makefile
programs/msiexec/Makefile
programs/regsvr32/Makefile
tools/Makefile
tools/winebuild/Makefile
tools/winedump/Makefile
tools/wmc/Makefile
tools/wrc/Makefile
tools/widl/Makefile ])

powerpc_build="no"
case $host_cpu in
    *powerpc*) powerpc_build="yes" ;;
esac

AM_CONDITIONAL(COMPILE_POWERPC, test "x$powerpc_build" = "xyes")

darwin_build="no"
case $host in
    *darwin*)
        darwin_build="yes"
    ;;
esac

AM_CONDITIONAL(DARWIN, test "x$darwin_build" = "xyes")



AC_OUTPUT

if test "$have_x" = "no"
then
  echo
  echo "*** Warning: X development files not found. Wine will be built without"
  echo "*** X support, which currently does not work, and would probably not be"
  echo "*** what you want anyway. You will need to install devel packages of"
  echo "*** Xlib/Xfree86 at the very least."
fi

if test "$ac_cv_lib_ncurses_resizeterm" = "no" -a "$ac_cv_lib_ncurses_waddch" = "yes"
then
  echo
  echo "*** Warning: resizeterm not found in ncurses. Wine will be built without"
  echo "*** terminal resize support. Consider upgrading ncurses."
fi

if test "$wine_cv_opengl_version_OK" = "no"
then
  echo
  echo "*** Warning: old Mesa headers detected. Wine will be built without Direct3D"
  echo "*** support. Consider upgrading your Mesa libraries (http://www.mesa3d.org/)."
fi

if test "$wine_cv_opengl_version_threadsafe" = "yes" -a "x$enable_opengl" = "xno"
then
  echo
  echo "*** Warning: the OpenGL version you have installed relies on libpthread for"
  echo "*** thread-safety. To prevent crashes, OpenGL support has been removed."
  echo "*** A fix for glibc 2.1.3 that seems to work is included in this version of Wine,"
  echo "*** start configure with '--enable-opengl' to force OpenGL support."
fi

if test "$wine_cv_opengl_version_threadsafe" = "yes" -a "x$enable_opengl" != "xno"
then
  echo
  echo "*** Warning: you explicitly linked in a thread-safe OpenGL version. If you"
  echo "*** experience unusual crashes on DirectDraw games, try first to disable OpenGL"
  echo "*** support before reporting bugs."
fi

if test "$wine_cv_warn_cups_h" = "yes"
then
  echo
  echo "*** Note: You have cups runtime libraries, but no development"
  echo "*** libraries. Install the cups-devel package or whichever package"
  echo "*** contains cups.h to enable CUPS support in Wine."
fi

if test "$wine_cv_msg_freetype" = "yes"
then
  echo
  echo "*** Note: Your system appears to have the FreeType 2 runtime libraries"
  echo "*** installed, but 'freetype-config' is not in your PATH. Install the"
  echo "*** freetype-devel package (or its equivalent on your distribution) to"
  echo "*** enable Wine to use TrueType fonts."
fi

echo
echo "Configure finished.  Do 'make' to compile Cedega."
echo

dnl Local Variables:
dnl comment-start: "dnl "
dnl comment-end: ""
dnl comment-start-skip: "\\bdnl\\b\\s *"
dnl compile-command: "autoconf"
dnl End:
