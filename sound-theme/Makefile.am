NULL =

SUBDIRS = sounds

noinst_LTLIBRARIES = libsoundtheme.la

AM_CPPFLAGS =						\
        $(WARN_CFLAGS)					\
        $(DISABLE_DEPRECATED)				\
        $(VOLUME_CONTROL_CFLAGS)				\
        -DSOUND_DATA_DIR="\"$(datadir)/sounds\""	\
        -DSOUND_SET_DIR="\"$(pkgdatadir)/sounds\""	\
        $(NULL)

libsoundtheme_la_SOURCES = \
	gvc-sound-theme-chooser.h		\
	gvc-sound-theme-chooser.c		\
	sound-theme-file-utils.h		\
	sound-theme-file-utils.c		\
	$(NULL)

libsoundtheme_la_LIBADD = $(VOLUME_CONTROL_LIBS)
libsoundtheme_la_LDFLAGS = -no-undefined

BUILT_SOURCES =				\
	$(NULL)

CLEANFILES =				\
	$(BUILT_SOURCES)		\
	$(NULL)

MAINTAINERCLEANFILES =                  \
        *~                              \
        Makefile.in

-include $(top_srcdir)/git.mk
