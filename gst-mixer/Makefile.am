Desktopdir = $(datadir)/applications
SUBDIRS = doc icons pixmaps src

@INTLTOOL_DESKTOP_RULE@
Desktop_in_files = mate-volume-control.desktop.in.in
Desktop_DATA = $(Desktop_in_files:.desktop.in.in=.desktop)

@INTLTOOL_XML_NOMERGE_RULE@
gsettings_SCHEMAS = \
	org.mate.volume-control.gschema.xml
@GSETTINGS_RULES@

%.gschema.xml.in: %.gschema.xml.in.in Makefile
	$(AM_V_GEN) $(SED) -e 's^\@GETTEXT_PACKAGE\@^$(GETTEXT_PACKAGE)^g' < $< > $@

EXTRA_DIST = \
	$(Desktop_in_files) \
	$(gsettings_schemas_in_in) 

CLEANFILES = \
	$(Desktop_DATA) \
	$(gsettings_SCHEMAS_in) \
	$(gsettings_SCHEMAS) \
	*.gschema.valid

DISTCLEANFILES = \
	$(Desktop_in_files:.desktop.in.in=.desktop.in)

-include $(top_srcdir)/git.mk
