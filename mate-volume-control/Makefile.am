if ENABLE_STATUSICON
bin_PROGRAMS = \
	 mate-volume-control-status-icon \
	 mate-volume-control \
	$(NULL)
else !ENABLE_STATUSICON
bin_PROGRAMS = \
	 mate-volume-control \
	$(NULL)
endif !ENABLE_STATUSICON

AM_CPPFLAGS = \
	$(WARN_CFLAGS) \
	$(VOLUME_CONTROL_CFLAGS) \
	$(DISABLE_DEPRECATED) \
	-DMATELOCALEDIR=\""$(datadir)/locale"\" \
	-DLOCALE_DIR=\""$(datadir)/locale"\" \
	-DICON_DATA_DIR="\"$(pkgdatadir)/icons\"" \
	-DSOUND_SET_DIR="\"$(pkgdatadir)/sounds\"" \
	-I$(srcdir) \
	$(NULL)

applets_in_files = org.mate.applets.GvcApplet.mate-panel-applet.desktop.in
service_in_files = org.mate.panel.applet.GvcAppletFactory.service.in

if ENABLE_PANELAPPLET
appletsdir       = $(datadir)/mate-panel/applets
applets_DATA     = $(applets_in_files:.mate-panel-applet.desktop.in=.mate-panel-applet)

if IN_PROCESS
APPLET_LOCATION = $(libdir)/libmate-volume-control-applet.so
mate_volume_control_applet_libdir= $(libdir)
mate_volume_control_applet_lib_LTLIBRARIES=libmate-volume-control-applet.la
IN_PROCESS = true
else !IN_PROCESS
APPLET_LOCATION = $(libexecdir)/mate-volume-control-applet
servicedir       = $(datadir)/dbus-1/services
service_DATA     = $(service_in_files:.service.in=.service)
IN_PROCESS = false

$(service_in_files:.service.in=.service): $(service_in_files)
	$(AM_V_GEN)sed \
		-e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
		$< > $@
endif !IN_PROCESS

$(applets_in_files): $(applets_in_files).in Makefile
	$(AM_V_GEN)sed \
		-e "s|\@APPLET_LOCATION\@|$(APPLET_LOCATION)|" \
		-e "s|\@IN_PROCESS\@|$(IN_PROCESS)|" \
		$< > $@

$(applets_DATA): $(applets_in_files) Makefile
	$(AM_V_GEN) $(MSGFMT) --desktop --keyword=Name --keyword=Description --template $< -d $(top_srcdir)/po -o $@

endif ENABLE_PANELAPPLET

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
endif ENABLE_STATUSICON

if ENABLE_PANELAPPLET
if IN_PROCESS
libmate_volume_control_applet_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libmate_volume_control_applet_la_SOURCES = \
	gvc-stream-applet-icon.h \
	gvc-stream-applet-icon.c \
	gvc-applet.h \
	gvc-applet.c \
	applet-main.c \
	$(NULL)

libmate_volume_control_applet_la_CFLAGS = $(WARN_CFLAGS)

libmate_volume_control_applet_la_LIBADD = \
	-lm \
	libmatevolumecontrol.la \
    $(MATE_APPLETS4_CFLAGS)		\
	$(VOLUME_CONTROL_LIBS) \
	$(NULL)

if ENABLE_WAYLAND
libmate_volume_control_applet_la_LIBADD += \
	$(WAYLAND_LIBS)

AM_CPPFLAGS += \
	$(WAYLAND_CFLAGS)
endif ENABLE_WAYLAND

else !IN_PROCESS
libexec_PROGRAMS = mate-volume-control-applet
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

mate_volume_control_applet_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

endif !IN_PROCESS
endif ENABLE_PANELAPPLET

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
	$(applets_in_files) \
	$(NULL)

MAINTAINERCLEANFILES = \
	*~ \
	Makefile.in

EXTRA_DIST = \
	$(applets_in_files).in \
	$(service_in_files)

-include $(top_srcdir)/git.mk
