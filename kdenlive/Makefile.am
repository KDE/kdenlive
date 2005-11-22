####### kdevelop will overwrite this part!!! (begin)##########
bin_PROGRAMS = kdenlive

mimedir = $(kde_mimedir)/application
mime_DATA = vnd.kde.kdenlive.desktop vnd.kde.kdenlive.scenelist.desktop

## INCLUDES were found outside kdevelop specific part

kdenlive_SOURCES = avfileformatdesc.cpp avfileformatwidget.cpp avfilelist.cpp \
				avformatdescbase.cpp avformatdescbool.cpp avformatdesccodec.cpp avformatdesccodeclist.cpp \
				avformatdesccontainer.cpp avformatdesclist.cpp avformatwidgetbase.cpp avformatwidgetbool.cpp \
				avformatwidgetcodec.cpp avformatwidgetcodeclist.cpp avformatwidgetcontainer.cpp \
				avformatwidgetlist.cpp avlistviewitem.cpp clipdrag.cpp clippropertiesdialog.cpp \
				displayaudiodesc.cpp displayvideodesc.cpp docclipavfile.cpp docclipbase.cpp \
				docclipbaselist.cpp docclipproject.cpp doctrackbase.cpp doctrackbaselist.cpp \
				doctrackclipiterator.cpp doctracksound.cpp doctrackvideo.cpp effect.cpp effectdesc.cpp \
				effectlistdialog.cpp effectparamdialog.cpp exportdialog.cpp gentime.cpp kaddclipcommand.cpp \
				kdenlive.cpp kdenlivedoc.cpp kdenlivesetupdlg.cpp kdenlivesplash.cpp kfixedruler.cpp \
				kmmeditpanel.cpp kmmmonitor.cpp kmmrulerpanel.cpp kmmscreen.cpp \
				kmmtrackkeyframepanel.cpp kmmtrackpanel.cpp kmmtracksoundpanel.cpp kmmtrackvideopanel.cpp \
				kmoveclipscommand.cpp krender.cpp krendermanager.cpp kresizecommand.cpp kruler.cpp \
				krulermodel.cpp krulersliderbase.cpp krulertimemodel.cpp kscalableruler.cpp \
				kselectclipcommand.cpp main.cpp monitormanager.cpp projectlist.cpp projectlistview.cpp \
				rangelist.cpp renderdebugpanel.cpp rendersetupdlg.cpp snaptogrid.cpp \
				trackpanelclipmovefunction.cpp trackpanelclipresizefunction.cpp trackpanelfunction.cpp \
				trackpanelrazorfunction.cpp trackpanelspacerfunction.cpp addtrackdialog_ui.ui kmmeditpanel_ui.ui \
				kmmrulerpanel_ui.ui projectlist_ui.ui rendersetupdlg_ui.ui docclipref.cpp \
				docclipreflist.cpp clipmanager.cpp documentclipnode.cpp documentgroupnode.cpp \
				documentbasenode.cpp docsubclip.cpp kaddrefclipcommand.cpp configureproject_ui.ui \
				configureproject.cpp configureprojectdialog.cpp exportconfig.cpp \
				trackpanelmarkerfunction.cpp trackviewdecorator.cpp trackviewbackgrounddecorator.cpp \
				trackviewvideobackgrounddecorator.cpp trackviewaudiobackgrounddecorator.cpp 			trackviewnamedecorator.cpp \
	trackviewmarkerdecorator.cpp kaddmarkercommand.cpp 			saveprojectfilter.cpp \
	saveprojectnativefilter.cpp loadprojectfilter.cpp 			loadprojectnativefilter.cpp \
	projectformatmanager.cpp loadprojectkinofilter.cpp timecode.cpp 			ktimeline.cpp kmmtimeline.cpp \
	ktrackview.cpp documentmacrocommands.cpp 			saveprojectscenelistfilter.cpp \
	trackpanelfunctionfactory.cpp trackpanelselectnonefunction.cpp 			effectkeyframe.cpp \
	effectdoublekeyframe.cpp effectparamdesc.cpp effectparamdoubledesc.cpp 			effectparameter.cpp \
	effectparamdescfactory.cpp doctrackdecorator.cpp ktrackpanel.cpp 			ktrackclippanel.cpp \
	trackviewdoublekeyframedecorator.cpp effectstack.cpp 			effectstackdialog_ui.ui effectstackdialog.cpp \
	effectdrag.cpp kaddeffectcommand.cpp 			effectdescriptionlist.cpp \
	effectstacklistview.cpp trackpanelclipslipfunction.cpp ktrackplacer.cpp 			kplacer.cpp \
	kclipplacer.cpp kmmclipkeyframepanel.cpp trackpanelcliprollfunction.cpp \
				krollcommand.cpp dynamicToolTip.cpp kmonitor.cpp capturemonitor.cpp \
	configinterface_ui.ui configtimeline_ui.ui kdenlivesettings.kcfgc

kdenlive_LDADD   =  $(LIB_KFILE) $(LIB_KDEUI) $(LIB_KDECORE) $(LIB_QT) $(LIBSOCKET)


EXTRA_DIST = $(mime_DATA) main.cpp kdenlive.cpp kdenlive.h kdenlivedoc.cpp kdenlivedoc.h kdenliveui.rc kdenlive.desktop avlistviewitem.cpp doctrackbase.cpp doctracksound.cpp doctrackvideo.cpp kmmmonitor.cpp kmmscreen.cpp kmmtimeline.cpp projectlist.cpp avlistviewitem.h doctrackbase.h doctracksound.h doctrackvideo.h kmmmonitor.h kmmscreen.h kmmtimeline.h projectlist.h projectlist_ui.ui clipdrag.cpp clipdrag.h kmmtrackvideopanel.cpp kmmtrackvideopanel.h kmmtracksoundpanel.cpp kmmtracksoundpanel.h docclipavfile.cpp docclipavfile.h docclipbase.cpp docclipbase.h docclipproject.cpp docclipproject.h projectlistview.cpp projectlistview.h kruler.cpp kruler.h kfixedruler.cpp kfixedruler.h krulermodel.cpp krulermodel.h kscalableruler.cpp kscalableruler.h krulersliderbase.cpp krulersliderbase.h krulertimemodel.cpp krulertimemodel.h kmmtrackpanel.cpp kmmtrackpanel.h kmmtimelinetrackview.cpp kmmtimelinetrackview.h docclipbaselist.cpp docclipbaselist.h kmmeditpanel_ui.ui kmmeditpanel.cpp kmmeditpanel.h kmmrulerpanel.cpp kmmrulerpanel.h kmmrulerpanel_ui.ui gentime.cpp gentime.h avfilelist.cpp avfilelist.h doctrackbaselist.cpp doctrackbaselist.h krender.cpp krender.h doctrackclipiterator.cpp doctrackclipiterator.h kmmtrackkeyframepanel.cpp kmmtrackkeyframepanel.h rangelist.cpp rangelist.h kmoveclipscommand.cpp kmoveclipscommand.h kselectclipcommand.cpp kselectclipcommand.h kaddclipcommand.cpp kaddclipcommand.h kresizecommand.cpp kresizecommand.h kdenlivesetupdlg.cpp kdenlivesetupdlg.h rendersetupdlg_ui.ui rendersetupdlg.cpp rendersetupdlg.h renderdebugpanel.cpp renderdebugpanel.h krendermanager.cpp krendermanager.h exportdialog.cpp exportdialog.h avfileformatdesc.cpp avfileformatdesc.h avformatdescbase.cpp avformatdescbase.h avformatdescbool.cpp avformatdescbool.h avformatdesclist.cpp avformatdesclist.h avformatwidgetbase.cpp avformatwidgetbase.h avformatwidgetbool.cpp avformatwidgetbool.h avformatwidgetlist.cpp avformatwidgetlist.h avformatdesccontainer.cpp avformatdesccontainer.h avformatwidgetcontainer.cpp avformatwidgetcontainer.h avfileformatwidget.cpp avfileformatwidget.h avformatdesccodeclist.cpp avformatdesccodeclist.h avformatwidgetcodeclist.cpp avformatwidgetcodeclist.h avformatdesccodec.cpp avformatdesccodec.h avformatwidgetcodec.cpp avformatwidgetcodec.h effectlistdialog.cpp effectlistdialog.h effectparamdialog.cpp effectparamdialog.h effectdesc.cpp effectdesc.h displayvideodesc.cpp displayvideodesc.h displayaudiodesc.cpp displayaudiodesc.h effect.cpp effect.h clippropertiesdialog.cpp clippropertiesdialog.h kdenlivesplash.cpp kdenlivesplash.h monitormanager.cpp monitormanager.h docclipref.cpp docclipref.h docclipreflist.cpp docclipreflist.h clipmanager.cpp clipmanager.h documentbasenode.cpp documentbasenode.h documentclipnode.cpp documentclipnode.h documentgroupnode.cpp documentgroupnode.h docsubclip.h docsubclip.cpp trackpanelclipslipfunction.cpp trackpanelclipslipfunction.h

install-data-local:
	$(mkinstalldirs) $(kde_appsdir)/Multimedia/
	$(INSTALL_DATA) $(srcdir)/kdenlive.desktop $(kde_appsdir)/Multimedia/kdenlive.desktop

uninstall-local:
	-rm -f $(kde_appsdir)/Multimedia/kdenlive.desktop

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
INCLUDES= $(all_includes) $(LIBMLT_CFLAGS)

kde_kcfg_DATA = kdenlive.kcfg
METASOURCES = AUTO

# the library search path. 
kdenlive_LDFLAGS = $(all_libraries) $(KDE_RPATH) $(LIBMLT_LIBS) -lmlt++

rcdir = $(kde_datadir)/kdenlive
rc_DATA = kdenliveui.rc


noinst_HEADERS = configureproject.h configureprojectdialog.h exportconfig.h \
			trackpanelmarkerfunction.h trackviewdecorator.h trackviewbackgrounddecorator.h \
			trackviewnamedecorator.h trackviewmarkerdecorator.h kaddmarkercommand.h saveprojectfilter.h \
			saveprojectnativefilter.h loadprojectfilter.h loadprojectnativefilter.h projectformatmanager.h \
			loadprojectkinofilter.h timecode.h ktimeline.h kmmtimeline.h ktrackview.h \
			documentmacrocommands.h trackpanelfunctionfactory.h trackpanelselectnonefunction.h \
			effectkeyframe.h effectdoublekeyframe.h effectparamdesc.h effectparamdoubledesc.h \
			effectparameter.h effectparamdescfactory.h doctrackdecorator.h ktrackpanel.h \
			ktrackclippanel.h trackviewdoublekeyframedecorator.h trackviewaudiobackgrounddecorator.h \
			effectstack.h effectstackdialog.h effectdrag.h kaddeffectcommand.h effectdescriptionlist.h \
			effectstacklistview.h saveprojectscenelistfilter.h ktrackplacer.h kplacer.h kclipplacer.h \
			kmmclipkeyframepanel.h trackpanelcliprollfunction.h krollcommand.h dynamicToolTip.h kmonitor.h capturemonitor.h








