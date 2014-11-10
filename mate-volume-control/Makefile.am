NULL =

bin_PROGRAMS = 						\
	mate-volume-control-applet			\
	mate-volume-control				\
	$(NULL)

AM_CPPFLAGS =						\
	$(WARN_CFLAGS)					\
	$(VOLUME_CONTROL_CFLAGS)			\
	$(DISABLE_DEPRECATED)				\
	-DLOCALE_DIR=\""$(datadir)/locale"\"		\
	-DICON_DATA_DIR="\"$(pkgdatadir)/icons\"" 	\
	-DSOUND_SET_DIR="\"$(pkgdatadir)/sounds\""	\
	$(NULL)

noinst_LTLIBRARIES = libmatevolumecontrol.la
libmatevolumecontrol_la_SOURCES =			\
	gvc-channel-bar.h				\
	gvc-channel-bar.c				\
	$(NULL)

mate_volume_control_applet_LDADD =			\
	-lm						\
	libmatevolumecontrol.la				\
	$(VOLUME_CONTROL_LIBS)				\
	$(NULL)

mate_volume_control_applet_SOURCES =			\
	gvc-stream-status-icon.h			\
	gvc-stream-status-icon.c			\
	gvc-applet.h					\
	gvc-applet.c					\
	applet-main.c					\
	$(NULL)

mate_volume_control_LDADD =				\
	-lm						\
	libmatevolumecontrol.la				\
	$(VOLUME_CONTROL_LIBS)				\
	$(NULL)

mate_volume_control_SOURCES =				\
	gvc-balance-bar.h				\
	gvc-balance-bar.c				\
	gvc-level-bar.h					\
	gvc-level-bar.c					\
	gvc-combo-box.h					\
	gvc-combo-box.c					\
	gvc-sound-theme-chooser.c 			\
	gvc-sound-theme-chooser.h 			\
	gvc-speaker-test.h				\
	gvc-speaker-test.c				\
	gvc-utils.c 					\
	gvc-utils.h					\
	sound-theme-file-utils.c			\
	sound-theme-file-utils.h			\
	gvc-mixer-dialog.c				\
	gvc-mixer-dialog.h				\
	dialog-main.c					\
	$(NULL)

BUILT_SOURCES =						\
	$(NULL)

CLEANFILES =						\
	$(BUILT_SOURCES)				\
	$(NULL)

MAINTAINERCLEANFILES =					\
	*~						\
	Makefile.in

-include $(top_srcdir)/git.mk
