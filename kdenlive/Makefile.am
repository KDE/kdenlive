####### kdevelop will overwrite this part!!! (begin)##########
bin_PROGRAMS = kdenlive

mimedir = $(kde_mimedir)/application
mime_DATA = vnd.kde.kdenlive.desktop vnd.kde.kdenlive.scenelist.desktop

## INCLUDES were found outside kdevelop specific part

if ENABLE_FIREWIRE
LIB_FIREWIRE = -liec61883
endif

kdenlive_SOURCES = addmarker_ui.ui addtrackdialog_ui.ui avfilelist.cpp \
	aviconviewitem.cpp aviconviewitem.h avlistviewitem.cpp capturemonitor.cpp clipdrag.cpp \
	clipmanager.cpp clipproperties.cpp clipproperties.h clipproperties_ui.ui \
	clippropertiesdialog.cpp configcapture_ui.ui confighard_ui.ui configinterface_ui.ui \
	configmisc_ui.ui configproject_ui.ui configtimeline_ui.ui configtitler_ui.ui \
	configureproject_ui.ui configureprojectdialog.cpp createcolorclip_ui.ui createimageclip_ui.ui \
	createslideshowclip.cpp createslideshowclip.h createslideshowclip_ui.ui definitions.h \
	displayaudiodesc.cpp displayvideodesc.cpp docclipavfile.cpp docclipbase.cpp \
	docclipbaselist.cpp docclipproject.cpp docclipref.cpp docclipreflist.cpp \
	doccliptextfile.cpp doccliptextfile.h docclipvirtual.cpp docclipvirtual.h docsubclip.cpp \
	doctrackbase.cpp doctrackbaselist.cpp doctrackclipiterator.cpp doctrackdecorator.cpp \
	doctracksound.cpp doctrackvideo.cpp documentbasenode.cpp documentclipnode.cpp \
	documentgroupnode.cpp documentmacrocommands.cpp dynamicToolTip.cpp editencoder_ui.ui \
	editmetadata_ui.ui effect.cpp effectcomplexkeyframe.cpp effectcomplexkeyframe.h \
	effectdesc.cpp effectdescriptionlist.cpp effectdoublekeyframe.cpp effectdrag.cpp \
	effectkeyframe.cpp effectlistdialog.cpp effectparamcomplexdesc.cpp \
	effectparamcomplexdesc.h effectparamdesc.cpp effectparamdescfactory.cpp effectparamdialog.cpp \
	effectparamdoubledesc.cpp effectparameter.cpp effectstack.cpp effectstackdialog.cpp \
	effectstackdialog_ui.ui effectstacklistview.cpp exportbasewidget_ui.ui exportdvd_ui.ui \
	exportdvddialog.cpp exportdvddialog.h exportwidget.cpp exportwidget.h firstrun_ui.ui \
	flatbutton.cpp flatbutton.h gentime.cpp initeffects.cpp initeffects.h \
	kaddclipcommand.cpp kaddeffectcommand.cpp kaddmarkercommand.cpp kaddrefclipcommand.cpp \
	kaddtransitioncommand.cpp kaddtransitioncommand.h kclipplacer.cpp kdenlive.cpp kdenlivedoc.cpp \
	kdenlivesettings.kcfgc kdenlivesetupdlg.cpp kdenlivesplash.cpp keditclipcommand.cpp \
	keditclipcommand.h keditmarkercommand.cpp keditmarkercommand.h kfixedruler.cpp \
	kmmclipkeyframepanel.cpp kmmeditpanel.cpp kmmeditpanel_ui.ui kmmmonitor.cpp kmmrecpanel.cpp \
	kmmrecpanel.h kmmrecpanel_ui.ui kmmrulerpanel.cpp kmmrulerpanel_ui.ui kmmscreen.cpp \
	kmmtimeline.cpp kmmtrackkeyframepanel.cpp kmmtrackpanel.cpp kmmtracksoundpanel.cpp \
	kmmtrackvideopanel.cpp kmonitor.cpp kmoveclipscommand.cpp kplacer.cpp krender.cpp \
	krendermanager.cpp kresizecommand.cpp krollcommand.cpp kruler.cpp krulermodel.cpp \
	krulersliderbase.cpp krulertimemodel.cpp kscalableruler.cpp kselectclipcommand.cpp \
	kthumb.cpp kthumb.h ktimeline.cpp ktrackpanel.cpp ktrackplacer.cpp ktrackview.cpp \
	listviewtagsearch.cpp listviewtagsearch.h \
	loadprogress_ui.ui loadprojectfilter.cpp loadprojectkinofilter.cpp \
	loadprojectnativefilter.cpp main.cpp monitormanager.cpp newproject.cpp newproject_ui.ui \
	newstuff.cpp newstuff.h projectformatmanager.cpp projecticonview.cpp \
	projecticonview.h projectlist.cpp projectlistview.cpp projectview_ui.ui rangelist.cpp \
	saveprojectfilter.cpp saveprojectnativefilter.cpp saveprojectscenelistfilter.cpp \
	snaptogrid.cpp timecode.cpp titlebasewidget_ui.ui titlewidget.cpp titlewidget.h \
	trackheader_ui.ui trackpanelclipmovefunction.cpp trackpanelclipresizefunction.cpp \
	trackpanelcliprollfunction.cpp trackpanelclipslipfunction.cpp trackpanelfunction.cpp \
	trackpanelfunctionfactory.cpp trackpanelkeyframefunction.cpp trackpanelkeyframefunction.h \
	trackpanelmarkerfunction.cpp trackpanelrazorfunction.cpp trackpanelselectnonefunction.cpp \
	trackpanelspacerfunction.cpp trackpaneltransitionmovefunction.cpp trackpaneltransitionmovefunction.h \
	trackpaneltransitionresizefunction.cpp trackpaneltransitionresizefunction.h \
	trackviewaudiobackgrounddecorator.cpp trackviewbackgrounddecorator.cpp trackviewdecorator.cpp \
	trackviewdoublekeyframedecorator.cpp trackviewmarkerdecorator.cpp trackviewnamedecorator.cpp \
	trackviewtransitiondecorator.cpp trackviewtransitiondecorator.h trackviewvideobackgrounddecorator.cpp \
	transition.cpp transition.h transitionaudiofade_ui.ui transitioncrossfade_ui.ui \
	transitiondialog.cpp transitiondialog.h transitionlumafile_ui.ui transitionpip_ui.ui \
	transitionpipwidget.cpp transitionstack.cpp transitionstack.h transitionwipe_ui.ui \
	transitionwipewidget.cpp transitionwipewidget.h

kdenlive_LDADD   =  $(LIB_KFILE) $(LIB_KDEUI) $(LIB_KDECORE) $(LIB_QT) \
		    $(LIBSOCKET) $(LIB_FIREWIRE) $(LIB_KNEWSTUFF)


EXTRA_DIST = $(mime_DATA) main.cpp kdenlive.cpp kdenlive.h kdenlivedoc.cpp kdenlivedoc.h kdenliveui.rc eventsrc kdenlive.desktop avlistviewitem.cpp doctrackbase.cpp doctracksound.cpp doctrackvideo.cpp kmmmonitor.cpp kmmscreen.cpp kmmtimeline.cpp projectlist.cpp avlistviewitem.h doctrackbase.h doctracksound.h doctrackvideo.h kmmmonitor.h kmmscreen.h kmmtimeline.h projectlist.h clipdrag.cpp clipdrag.h kmmtrackvideopanel.cpp kmmtrackvideopanel.h kmmtracksoundpanel.cpp kmmtracksoundpanel.h docclipavfile.cpp docclipavfile.h docclipbase.cpp docclipbase.h docclipproject.cpp docclipproject.h projectlistview.cpp projectlistview.h kruler.cpp kruler.h kfixedruler.cpp kfixedruler.h krulermodel.cpp krulermodel.h kscalableruler.cpp kscalableruler.h krulersliderbase.cpp krulersliderbase.h krulertimemodel.cpp krulertimemodel.h kmmtrackpanel.cpp kmmtrackpanel.h kmmtimelinetrackview.cpp kmmtimelinetrackview.h docclipbaselist.cpp docclipbaselist.h kmmeditpanel_ui.ui loadprogress_ui.ui kmmeditpanel.cpp kmmeditpanel.h kmmrulerpanel.cpp kmmrulerpanel.h kmmrulerpanel_ui.ui gentime.cpp gentime.h avfilelist.cpp avfilelist.h doctrackbaselist.cpp doctrackbaselist.h krender.cpp krender.h doctrackclipiterator.cpp doctrackclipiterator.h kmmtrackkeyframepanel.cpp kmmtrackkeyframepanel.h rangelist.cpp rangelist.h kmoveclipscommand.cpp kmoveclipscommand.h kselectclipcommand.cpp kselectclipcommand.h kaddclipcommand.cpp kaddclipcommand.h kresizecommand.cpp kresizecommand.h kdenlivesetupdlg.cpp kdenlivesetupdlg.h krendermanager.cpp krendermanager.h avfileformatdesc.cpp avfileformatdesc.h avformatdescbase.cpp avformatdescbase.h avformatdescbool.cpp avformatdescbool.h avformatdesclist.cpp avformatdesclist.h avformatwidgetbase.cpp avformatwidgetbase.h avformatwidgetbool.cpp avformatwidgetbool.h avformatwidgetlist.cpp avformatwidgetlist.h avformatdesccontainer.cpp avformatdesccontainer.h avformatwidgetcontainer.cpp avformatwidgetcontainer.h avfileformatwidget.cpp avfileformatwidget.h avformatdesccodeclist.cpp avformatdesccodeclist.h avformatwidgetcodeclist.cpp avformatwidgetcodeclist.h avformatdesccodec.cpp avformatdesccodec.h avformatwidgetcodec.cpp avformatwidgetcodec.h effectlistdialog.cpp effectlistdialog.h effectparamdialog.cpp effectparamdialog.h effectdesc.cpp effectdesc.h displayvideodesc.cpp displayvideodesc.h displayaudiodesc.cpp displayaudiodesc.h effect.cpp effect.h clippropertiesdialog.cpp clippropertiesdialog.h kdenlivesplash.cpp kdenlivesplash.h monitormanager.cpp monitormanager.h docclipref.cpp docclipref.h docclipreflist.cpp docclipreflist.h clipmanager.cpp clipmanager.h documentbasenode.cpp documentbasenode.h documentclipnode.cpp documentclipnode.h documentgroupnode.cpp documentgroupnode.h docsubclip.h docsubclip.cpp trackpanelclipslipfunction.cpp trackpanelclipslipfunction.h


xdg_apps_DATA = kdenlive.desktop


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
INCLUDES= $(all_includes) $(LIBMLT_CFLAGS) @LIBIEC61883_CFLAGS@ -D__STDC_CONSTANT_MACROS

kde_kcfg_DATA = kdenlive.kcfg
METASOURCES = AUTO

# the library search path. 
kdenlive_LDFLAGS = $(all_libraries) $(KDE_RPATH) $(LIBMLT_LIBS) -lmlt++

rcdir = $(kde_datadir)/kdenlive
rc_DATA = kdenliveui.rc eventsrc

noinst_HEADERS = configureprojectdialog.h trackpanelmarkerfunction.h \
	trackviewdecorator.h trackviewbackgrounddecorator.h trackviewnamedecorator.h \
	trackviewmarkerdecorator.h kaddmarkercommand.h saveprojectfilter.h saveprojectnativefilter.h \
	loadprojectfilter.h loadprojectnativefilter.h projectformatmanager.h \
	loadprojectkinofilter.h timecode.h ktimeline.h kmmtimeline.h ktrackview.h \
	documentmacrocommands.h trackpanelfunctionfactory.h trackpanelselectnonefunction.h \
	effectkeyframe.h effectdoublekeyframe.h effectparamdesc.h effectparamdoubledesc.h \
	effectparameter.h effectparamdescfactory.h doctrackdecorator.h ktrackpanel.h \
	trackviewdoublekeyframedecorator.h trackviewaudiobackgrounddecorator.h effectstack.h effectstackdialog.h \
	effectdrag.h kaddeffectcommand.h effectdescriptionlist.h effectstacklistview.h \
	saveprojectscenelistfilter.h ktrackplacer.h kplacer.h kclipplacer.h kmmclipkeyframepanel.h \
	trackpanelcliprollfunction.h krollcommand.h dynamicToolTip.h kmonitor.h capturemonitor.h \
	newproject.h definitions.h








