NULL =

SUBDIRS =	\
	icons	\
	sounds	\
	$(NULL)

if ENABLE_STATUSICON
autostartdir = $(sysconfdir)/xdg/autostart
endif
autostart_in_files = mate-volume-control-status-icon.desktop.in
if ENABLE_STATUSICON
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
$(autostart_DATA): $(autostart_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@
endif

appsdir = $(datadir)/applications
apps_in_files = mate-volume-control.desktop.in
apps_DATA = $(apps_in_files:.desktop.in=.desktop)
$(apps_DATA): $(apps_in_files)
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

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
