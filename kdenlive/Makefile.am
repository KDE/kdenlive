####### kdevelop will overwrite this part!!! (begin)##########
bin_PROGRAMS = kdenlive

## INCLUDES were found outside kdevelop specific part

kdenlive_SOURCES = kmmtrackpanel.cpp avfile.cpp krulertimemodel.cpp krulersliderbase.cpp kscalableruler.cpp krulermodel.cpp kfixedruler.cpp kruler.cpp projectlistview.cpp docclipproject.cpp docclipbase.cpp docclipavfile.cpp kmmeditpanelimplementation.cpp kmmtracksoundpanel.cpp kmmtrackvideopanel.cpp clipdrag.cpp projectlist_ui.ui kmmeditpanel.ui addtrackdialog_ui.ui projectlist.cpp kmmtimeline.cpp kmmscreen.cpp kmmmonitor.cpp doctrackvideo.cpp doctracksound.cpp doctrackbase.cpp avlistviewitem.cpp kdenliveview.cpp kdenlivedoc.cpp kdenlive.cpp main.cpp 
kdenlive_LDADD   = -lartskde -lmcop $(LIB_KFILE) $(LIB_KDEUI) $(LIB_KDECORE) $(LIB_QT) $(LIBSOCKET)


EXTRA_DIST = main.cpp kdenlive.cpp kdenlive.h kdenlivedoc.cpp kdenlivedoc.h kdenliveview.cpp kdenliveview.h kdenliveui.rc kdenlive.desktop lo32-app-kdenlive.png lo16-app-kdenlive.png avlistviewitem.cpp doctrackbase.cpp doctracksound.cpp doctrackvideo.cpp kmmmonitor.cpp kmmscreen.cpp kmmtimeline.cpp projectlist.cpp addtrackdialog_ui.ui avlistviewitem.h doctrackbase.h doctracksound.h doctrackvideo.h kmmeditpanel.ui kmmmonitor.h kmmscreen.h kmmtimeline.h projectlist.h projectlist_ui.ui clipdrag.cpp clipdrag.h kmmtrackvideopanel.cpp kmmtrackvideopanel.h kmmtracksoundpanel.cpp kmmtracksoundpanel.h kmmeditpanelimplementation.cpp kmmeditpanelimplementation.h docclipavfile.cpp docclipavfile.h docclipbase.cpp docclipbase.h docclipproject.cpp docclipproject.h projectlistview.cpp projectlistview.h kruler.cpp kruler.h kfixedruler.cpp kfixedruler.h krulermodel.cpp krulermodel.h kscalableruler.cpp kscalableruler.h krulersliderbase.cpp krulersliderbase.h krulertimemodel.cpp krulertimemodel.h avfile.cpp avfile.h kmmtrackpanel.cpp kmmtrackpanel.h 

install-data-local:
	$(mkinstalldirs) $(kde_appsdir)/Applications/
	$(INSTALL_DATA) $(srcdir)/kdenlive.desktop $(kde_appsdir)/Applications/kdenlive.desktop
	$(mkinstalldirs) $(kde_icondir)/locolor/32x32/apps/
	$(INSTALL_DATA) $(srcdir)/lo32-app-kdenlive.png $(kde_icondir)/locolor/32x32/apps/kdenlive.png
	$(mkinstalldirs) $(kde_icondir)/locolor/16x16/apps/
	$(INSTALL_DATA) $(srcdir)/lo16-app-kdenlive.png $(kde_icondir)/locolor/16x16/apps/kdenlive.png

uninstall-local:
	-rm -f $(kde_appsdir)/Applications/kdenlive.desktop
	-rm -f $(kde_icondir)/locolor/32x32/apps/kdenlive.png
	-rm -f $(kde_icondir)/locolor/16x16/apps/kdenlive.png

####### kdevelop will overwrite this part!!! (end)############
# These paths are KDE specific. Use them:
# kde_appsdir         Where your application's menu entry (.desktop) should go to.
# kde_icondir         Where your icon should go to - better use KDE_ICON.
# kde_sounddir        Where your sounds should go to.
# kde_htmldir         Where your docs should go to. (contains lang subdirs)
# kde_datadir         Where you install application data. (Use a subdir)
# kde_locale          Where translation files should go to. (contains lang subdirs)
# kde_cgidir          Where cgi-bin executables should go to.
# kde_confdir         Where config files should go to (system-wide ones with default values).
# kde_mimedir         Where mimetypes .desktop files should go to.
# kde_servicesdir     Where services .desktop files should go to.
# kde_servicetypesdir Where servicetypes .desktop files should go to.
# kde_toolbardir      Where general toolbar icons should go to (deprecated, use KDE_ICON).
# kde_wallpaperdir    Where general wallpapers should go to.
# kde_templatesdir    Where templates for the "New" menu (Konqueror/KDesktop) should go to.
# kde_bindir          Where executables should go to. Use bin_PROGRAMS or bin_SCRIPTS.
# kde_libdir          Where shared libraries should go to. Use lib_LTLIBRARIES.
# kde_moduledir       Where modules (e.g. parts) should go to. Use kde_module_LTLIBRARIES.
# kde_styledir        Where Qt/KDE widget styles should go to (new in KDE 3).
# kde_designerdir     Where Qt Designer plugins should go to (new in KDE 3).

# set the include path for X, qt and KDE
INCLUDES= $(all_includes)

METASOURCES = AUTO

# the library search path. 
kdenlive_LDFLAGS = $(all_libraries) $(KDE_RPATH)

rcdir = $(kde_datadir)/kdenlive
rc_DATA = kdenliveui.rc

messages: rc.cpp
	LIST=`find . -name \*.h -o -name \*.hh -o -name \*.H -o -name \*.hxx -o -name \*.hpp -o -name \*.cpp -o -name \*.cc -o -name \*.cxx -o -name \*.ecpp -o -name \*.C`; \
	if test -n "$$LIST"; then \
	 $(XGETTEXT) $$LIST -o $(podir)/kdenlive.pot; \
	fi

