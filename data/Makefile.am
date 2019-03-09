NULL =

SUBDIRS =	\
	icons	\
	sounds	\
	$(NULL)

@INTLTOOL_DESKTOP_RULE@
autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = mate-volume-control-status-icon.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

appsdir = $(datadir)/applications
apps_in_files = mate-volume-control.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)

EXTRA_DIST =					\
	$(autostart_in_files)			\
	mate-volume-control.desktop.in.in	\
	$(NULL)

CLEANFILES =					\
	mate-volume-control.desktop		\
	$(NULL)

DISTCLEANFILES =				\
	mate-volume-control-status-icon.desktop	\
	$(NULL)

-include $(top_srcdir)/git.mk
