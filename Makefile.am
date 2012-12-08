NULL =

SUBDIRS = po

if HAVE_SOUND_THEME
SUBDIRS += sound-theme
endif

if HAVE_PULSEAUDIO
SUBDIRS += mate-volume-control
endif

if HAVE_GSTMIXER
SUBDIRS += gst-mixer
endif

if HAVE_GSTMIXER_APPLET
SUBDIRS += gst-mixer-applet
endif

DISTCHECK_CONFIGURE_FLAGS = \
	--disable-scrollkeeper	\
	--enable-more-warnings  \
	--enable-compile-warnings=maximum

DIST_SUBDIRS = \
	po 			\
	mate-volume-control	\
	gst-mixer		\
	gst-mixer-applet	\
	sound-theme		\
	$(NULL)

EXTRA_DIST = \
	.version		\
	autogen.sh		\
	configure		\
	COPYING			\
	COPYING-DOCS		\
	COPYING.gst-mixer	\
	build-aux/git-version-gen \
	build-aux/mate-doc-utils.make	\
	m4/as-compiler-flag.m4	\
	m4/as-version.m4	\
	$(INTLTOOL_BUILT:=.in)	\
	$(NULL)

DISTCLEANFILES = \
	build-aux/mate-doc-utils.make	\
	po/.intltool-merge-cache	\
	$(NULL)

ACLOCAL_AMFLAGS = -I m4

# see build-aux/git-version-get for details
BUILT_SOURCES = $(top_srcdir)/.version
$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

-include $(top_srcdir)/git.mk
