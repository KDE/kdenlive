####### kdevelop will overwrite this part!!! (begin)##########
bin_PROGRAMS = kdenlive
kdenlive_SOURCES = projectlist_ui.ui kmmtrackvideo.cpp kmmtracksound.cpp docclipvideo.cpp docclipsound.cpp docclipbase.cpp doctrackvideo.cpp doctracksound.cpp doctrackbase.cpp kmmtrackvideopanel.cpp kmmtrackvideopanel_ui.ui kmmtracksoundpanel.cpp kmmtracksoundpanel_ui.ui kmmeditpanelimplementation.cpp kmmeditpanel.ui kmmscreen.cpp kmmmonitor.cpp avlistviewitem.cpp avfile.cpp projectlist.cpp kmmtrackbase.cpp kmmtimeline.cpp kmmruler.cpp kdenliveview.cpp kdenlivedoc.cpp kdenlive.cpp main.cpp 
kdenlive_LDADD   = -lmcop -lartskde $(LIB_KFILE) $(LIB_KDEUI) $(LIB_KDECORE) $(LIB_QT) $(LIBSOCKET)


EXTRA_DIST = main.cpp kdenlive.cpp kdenlive.h kdenlivedoc.cpp kdenlivedoc.h kdenliveview.cpp kdenliveview.h kdenliveui.rc kdenlive.desktop lo32-app-kdenlive.png lo16-app-kdenlive.png kmmruler.cpp kmmruler.h kmmtimeline.cpp kmmtimeline.h kmmtrackbase.cpp kmmtrackbase.h projectlist.cpp projectlist.h avfile.cpp avfile.h avlistviewitem.cpp avlistviewitem.h kmmmonitor.cpp kmmmonitor.h kmmscreen.cpp kmmscreen.h kmmeditpanel.ui kmmeditpanelimplementation.cpp kmmeditpanelimplementation.h kmmtracksoundpanel_ui.ui kmmtracksoundpanel.cpp kmmtracksoundpanel.h kmmtrackvideopanel_ui.ui kmmtrackvideopanel.cpp kmmtrackvideopanel.h doctrackbase.cpp doctrackbase.h doctracksound.cpp doctracksound.h doctrackvideo.cpp doctrackvideo.h docclipbase.cpp docclipbase.h docclipsound.cpp docclipsound.h docclipvideo.cpp docclipvideo.h kmmtracksound.cpp kmmtracksound.h kmmtrackvideo.cpp kmmtrackvideo.h projectlist_ui.ui 

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
# this 10 paths are KDE specific. Use them:
# kde_htmldir       Where your docs should go to. (contains lang subdirs)
# kde_appsdir       Where your application file (.kdelnk) should go to. 
# kde_icondir       Where your icon should go to.
# kde_minidir       Where your mini icon should go to.
# kde_datadir       Where you install application data. (Use a subdir)
# kde_locale        Where translation files should go to.(contains lang subdirs)
# kde_cgidir        Where cgi-bin executables should go to.
# kde_confdir       Where config files should go to.
# kde_mimedir       Where mimetypes should go to.
# kde_toolbardir    Where general toolbar icons should go to.
# kde_wallpaperdir  Where general wallpapers should go to.

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

