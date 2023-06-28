AC_PREREQ(2.60)

AC_INIT([mate-media],
        [1.27.0],
        [https://mate-desktop.org/])

AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz tar-pax foreign check-news])
AM_MAINTAINER_MODE

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

MATE_MAINTAINER_MODE_DEFINES
MATE_COMPILE_WARNINGS

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([string.h math.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T

GLIB_REQUIRED_VERSION=2.50.0
GIO_REQUIRED_VERSION=2.50.0
GTK_REQUIRED_VERSION=3.22.0
CANBERRA_REQUIRED_VERSION=0.13
MATE_MIXER_REQUIRED_VERSION=1.10.0
MATE_DESKTOP_REQUIRED_VERSION=1.17.0
LIBPANEL4_REQUIRED=1.17.0

dnl=======================================================================
dnl Check for the volume control modules
dnl=======================================================================

PKG_CHECK_MODULES(VOLUME_CONTROL,
                  gobject-2.0 >= $GLIB_REQUIRED_VERSION
                  gtk+-3.0 >= $GTK_REQUIRED_VERSION
                  gio-2.0 >= $GIO_REQUIRED_VERSION
                  libcanberra-gtk3 >= $CANBERRA_REQUIRED_VERSION
                  libxml-2.0
                  mate-desktop-2.0 >= $MATE_DESKTOP_REQUIRED_VERSION
                  libmatemixer >= $MATE_MIXER_REQUIRED_VERSION
                  libmatepanelapplet-4.0 >= $LIBPANEL4_REQUIRED
)

AC_SUBST(VOLUME_CONTROL_CFLAGS)
AC_SUBST(VOLUME_CONTROL_LIBS)

dnl=======================================================================
dnl GLib
dnl=======================================================================

GLIB_GSETTINGS

dnl=======================================================================
dnl i18n
dnl=======================================================================
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=mate-media
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define to the Gettext package name])

dnl ---------------------------------------------------------------------------
dnl - Finish
dnl ---------------------------------------------------------------------------

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_ARG_ENABLE(deprecated,
        [AS_HELP_STRING([--enable-deprecated],
                [warn about deprecated usages [default=yes]])],,
         [enable_deprecated=yes])

if test "x$enable_deprecated" = "xyes"; then
    DISABLE_DEPRECATED=""
    AC_SUBST(DISABLE_DEPRECATED)
fi

AC_DEFINE(HAVE_WAYLAND, 1, [Have the Wayland development library])


AC_SUBST(WAYLAND_CFLAGS)
AC_SUBST(WAYLAND_LIBS)


#============================================================================
# status icon
#============================================================================

dnl Whether to  build the volume control statusicon
dnl --

AC_ARG_ENABLE(statusicon, [AS_HELP_STRING([--enable-statusicon], [Enable volume control status icon (default: yes)])],,[enable_statusicon=yes])
if test "x$enable_statusicon" = "xno"; then
    AC_DEFINE(ENABLE_STATUSICON, 0, [Enable if you want to build the tray applet])
else

    AC_DEFINE(ENABLE_STATUSICON, 1, [Enable if you want to build the tray applet])
fi
AM_CONDITIONAL(ENABLE_STATUSICON, test "x$enable_statusicon" != "xno")

#============================================================================
# panel applet
#============================================================================

dnl Whether to build the standalone volume control applet
dnl --


AC_ARG_ENABLE(panelapplet, [AS_HELP_STRING([--enable-panelapplet], [Enable volume control panel applet (gives better icon rendering than a status icon on hidpi displays) (default: yes)])],,[enable_panelapplet=yes])
if test "x$enable_panelapplet" = "xno"; then
    AC_DEFINE(ENABLE_PANELAPPLET, 0, [Enable if you want to build the panel applet])
else
    AC_DEFINE(ENABLE_PANELAPPLET, 1, [Enable if you want to build the panel applet])
fi
AM_CONDITIONAL(ENABLE_PANELAPPLET, test "x$enable_panelapplet" != "xno")

dnl Whether to build the volume control applet in-process
dnl --

AC_ARG_ENABLE(in-process, [AS_HELP_STRING([--enable-in-process], [Enable in-process build of panel applet (default: yes, required for wayland)])],,[enable_in-process=yes])

AM_CONDITIONAL(IN_PROCESS, test "x$enable_in_process" != "xno")
if test "x$enable_in_process" != "xno"; then
    AC_DEFINE(IN_PROCESS, 1, [Enable if you want to build the panel applet in-process])
fi

AC_STRUCT_DIRENT_D_TYPE

# $$enable_wayland will be set to "yes", "no" or "auto"

AC_ARG_ENABLE(wayland,
              [AS_HELP_STRING([--enable-wayland],
                              [Explicitly enable or disable Wayland support
                              (default is to enable only if Wayland client development library is detected)])],
              [enable_wayland=$enableval],
              [enable_wayland=auto])

# Check if we have gtk-layer-shell installed, and thus should build with Wayland support
have_wayland=no
if test "x$enable_wayland" != "xno"; then
  PKG_CHECK_MODULES(WAYLAND, gtk-layer-shell-0 wayland-client, have_wayland=yes, [
        if test "x$enable_wayland" = "xyes"; then
          AC_MSG_ERROR([Wayland enabled but GTK Layer Shell library not found])
        fi
    ])
fi

AM_CONDITIONAL(HAVE_WAYLAND, [test "x$have_wayland" = "xyes"])

if test "x$have_wayland" = "xyes"; then
  AC_DEFINE(HAVE_WAYLAND, 1, [Have the Wayland development library])
else
  AC_DEFINE(HAVE_WAYLAND, 0, [Have the Wayland development library])
fi

AC_SUBST(WAYLAND_CFLAGS)
AC_SUBST(WAYLAND_LIBS)

#=============================================================================================
# Sanity check: must build either status icon or panel applet and wayland requires in-process
#=============================================================================================

if test "x$enable_statusicon" = "xno" && test "x$enable_panelapplet" = "xno"; then
	AC_MSG_ERROR([must build either volume control status icon or volume control applet])
fi

if test "x$enable_in_process" = "xno" && test "x$have_wayland" = "xyes"; then
	AC_MSG_ERROR([must build in-process to enable wayland])
fi

if test "x$enable_in_process" = "xyes" && test "x$have_wayland" = "xno"; then
	AC_MSG_ERROR([in-process is only for wayland builds])
fi

AC_CONFIG_FILES([
Makefile
data/Makefile
data/mate-volume-control.desktop.in
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/16x16/status/Makefile
data/icons/22x22/Makefile
data/icons/22x22/status/Makefile
data/icons/24x24/Makefile
data/icons/24x24/status/Makefile
data/icons/32x32/Makefile
data/icons/32x32/status/Makefile
data/icons/scalable/Makefile
data/icons/scalable/devices/Makefile
data/icons/scalable/status/Makefile
data/sounds/Makefile
man/Makefile
po/Makefile.in
mate-volume-control/Makefile
])

AC_OUTPUT

echo "
Configure summary:

    ${PACKAGE_STRING}
    `echo $PACKAGE_STRING | sed "s/./=/g"`

    Prefix .....................: ${prefix}
    Source code location........: ${srcdir}
    Compiler ...................: ${CC}
    Compiler flags .............: ${CFLAGS}
    Warning flags ..............: ${WARN_CFLAGS}
"
#this is needed to get any output when the default is used
#rather than explicitly specifying whether to build the applet or the tray icon

if test "x$enable_statusicon" = "xno"; then
	echo "    Building status icon .......: no"
else
	echo "    Building status icon .......: yes"
fi
if test "x$enable_panelapplet" = "xno"; then
	echo "    Building panel applet ......: no"
else 
	echo "    Building panel applet ......: yes"
fi
if test "x$enable_in_process" = "xno"; then
	echo "    Building in-process .......: no"
else
	echo "    Building in-process .......: yes"
fi
if test "x$enable_wayland" = "xno"; then
	echo "    Building with wayland support .......: no"
else
	echo "    Building with wayland support .......: yes"
fi
#get a newline in the terminal
echo ""
