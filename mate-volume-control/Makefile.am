if ENABLE_STATUSICON
bin_PROGRAMS = \
	 mate-volume-control-status-icon \
	 mate-volume-control \
	$(NULL)
else
bin_PROGRAMS = \
	 mate-volume-control \
	$(NULL)
endif

if ENABLE_PANELAPPLET
libexec_PROGRAMS = mate-volume-control-applet
endif

AM_CPPFLAGS = \
	$(WARN_CFLAGS) \
	$(VOLUME_CONTROL_CFLAGS) \
	$(DISABLE_DEPRECATED) \
	-DMATELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DLOCALE_DIR=\""$(datadir)/locale"\" \
	-DICON_DATA_DIR="\"$(pkgdatadir)/icons\"" \
	-DSOUND_SET_DIR="\"$(pkgdatadir)/sounds\"" \
	-I$(srcdir) \
	$(NULL)

if ENABLE_PANELAPPLET
APPLET_LOCATION = $(libexecdir)/mate-volume-control-applet

appletsdir       = $(datadir)/mate-panel/applets
applets_in_files = org.mate.applets.GvcApplet.mate-panel-applet.in
applets_DATA     = $(applets_in_files:.mate-panel-applet.in=.mate-panel-applet)

$(applets_in_files): $(applets_in_files).in Makefile
	$(AM_V_GEN)sed \
		-e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
		$< > $@
%.mate-panel-applet: %.mate-panel-applet.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

servicedir       = $(datadir)/dbus-1/services
service_in_files = org.mate.panel.applet.GvcAppletFactory.service.in
service_DATA     = $(service_in_files:.service.in=.service)

org.mate.panel.applet.GvcAppletFactory.service: $(service_in_files)
	$(AM_V_GEN)sed \
		-e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
		$< > $@

EXTRA_DIST = \
	$(applets_in_files).in \
	$(service_in_files)

endif

noinst_LTLIBRARIES = libmatevolumecontrol.la
libmatevolumecontrol_la_SOURCES = \
	gvc-channel-bar.h \
	gvc-channel-bar.c \
	$(NULL)

if ENABLE_STATUSICON
mate_volume_control_status_icon_LDADD = \
	-lm \
	libmatevolumecontrol.la \
	$(VOLUME_CONTROL_LIBS) \
	$(NULL)

mate_volume_control_status_icon_SOURCES = \
	gvc-stream-status-icon.h \
	gvc-stream-status-icon.c \
	gvc-status-icon.h \
	gvc-status-icon.c \
	status-icon-main.c \
	$(NULL)

mate_volume_control_status_icon_CFLAGS = $(WARN_CFLAGS)
endif

if ENABLE_PANELAPPLET
mate_volume_control_applet_LDADD = \
	-lm \
	libmatevolumecontrol.la \
	$(VOLUME_CONTROL_LIBS) \
	$(MATEDESKTOP_LIBS) \
	$(NULL)

mate_volume_control_applet_SOURCES = \
	gvc-stream-applet-icon.h \
	gvc-stream-applet-icon.c \
	gvc-applet.h \
	gvc-applet.c \
	applet-main.c \
	$(NULL)

mate_volume_control_applet_CFLAGS = $(WARN_CFLAGS)
endif

mate_volume_control_LDADD = \
	-lm \
	libmatevolumecontrol.la \
	$(VOLUME_CONTROL_LIBS) \
	$(NULL)

mate_volume_control_SOURCES = \
	gvc-balance-bar.h \
	gvc-balance-bar.c \
	gvc-level-bar.h \
	gvc-level-bar.c \
	gvc-combo-box.h \
	gvc-combo-box.c \
	gvc-sound-theme-chooser.c \
	gvc-sound-theme-chooser.h \
	gvc-speaker-test.h \
	gvc-speaker-test.c \
	gvc-utils.c \
	gvc-utils.h \
	sound-theme-file-utils.c \
	sound-theme-file-utils.h \
	gvc-mixer-dialog.c \
	gvc-mixer-dialog.h \
	dialog-main.c \
	$(NULL)

BUILT_SOURCES = \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

MAINTAINERCLEANFILES = \
	*~ \
	Makefile.in

-include $(top_srcdir)/git.mk
