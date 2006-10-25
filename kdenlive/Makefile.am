####### kdevelop will overwrite this part!!! (begin)##########
bin_PROGRAMS = kdenlive

mimedir = $(kde_mimedir)/application
mime_DATA = vnd.kde.kdenlive.desktop vnd.kde.kdenlive.scenelist.desktop

## INCLUDES were found outside kdevelop specific part

if ENABLE_FIREWIRE
LIB_FIREWIRE = -liec61883
endif

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
					rangelist.cpp snaptogrid.cpp trackpanelclipmovefunction.cpp \
					trackpanelclipresizefunction.cpp trackpanelfunction.cpp trackpanelrazorfunction.cpp \
					trackpanelspacerfunction.cpp addtrackdialog_ui.ui kmmeditpanel_ui.ui kmmrulerpanel_ui.ui \
					docclipref.cpp docclipreflist.cpp clipmanager.cpp documentclipnode.cpp \
					documentgroupnode.cpp documentbasenode.cpp docsubclip.cpp kaddrefclipcommand.cpp \
					configureproject_ui.ui configureproject.cpp configureprojectdialog.cpp exportconfig.cpp \
					trackpanelmarkerfunction.cpp trackviewdecorator.cpp trackviewbackgrounddecorator.cpp \
					trackviewvideobackgrounddecorator.cpp trackviewaudiobackgrounddecorator.cpp trackviewnamedecorator.cpp \
					trackviewmarkerdecorator.cpp kaddmarkercommand.cpp saveprojectfilter.cpp saveprojectnativefilter.cpp \
					loadprojectfilter.cpp loadprojectnativefilter.cpp projectformatmanager.cpp \
					loadprojectkinofilter.cpp timecode.cpp ktimeline.cpp kmmtimeline.cpp ktrackview.cpp \
					documentmacrocommands.cpp saveprojectscenelistfilter.cpp trackpanelfunctionfactory.cpp \
					trackpanelselectnonefunction.cpp effectkeyframe.cpp effectdoublekeyframe.cpp effectparamdesc.cpp \
					effectparamdoubledesc.cpp effectparameter.cpp effectparamdescfactory.cpp doctrackdecorator.cpp \
					ktrackpanel.cpp ktrackclippanel.cpp trackviewdoublekeyframedecorator.cpp \
					effectstack.cpp effectstackdialog_ui.ui effectstackdialog.cpp effectdrag.cpp \
					kaddeffectcommand.cpp effectdescriptionlist.cpp effectstacklistview.cpp \
					trackpanelclipslipfunction.cpp ktrackplacer.cpp kplacer.cpp kclipplacer.cpp kmmclipkeyframepanel.cpp \
					trackpanelcliprollfunction.cpp krollcommand.cpp dynamicToolTip.cpp kmonitor.cpp capturemonitor.cpp \
					configinterface_ui.ui configtimeline_ui.ui kdenlivesettings.kcfgc trackheader_ui.ui \
					projectview_ui.ui createcolorclip_ui.ui loadprogress_ui.ui createimageclip_ui.ui \
					keditclipcommand.cpp keditclipcommand.h trackpanelkeyframefunction.cpp \
					trackpanelkeyframefunction.h effectparamcomplexdesc.cpp effectparamcomplexdesc.h \
					effectcomplexkeyframe.cpp effectcomplexkeyframe.h transition.cpp transition.h transitionstack.cpp \
					transitionstack.h trackviewtransitiondecorator.cpp trackviewtransitiondecorator.h \
					doccliptextfile.cpp doccliptextfile.h titlebasewidget_ui.ui titlewidget.cpp titlewidget.h \
					kthumb.cpp kthumb.h trackpaneltransitionmovefunction.cpp \
					trackpaneltransitionmovefunction.h trackpaneltransitionresizefunction.cpp \
					trackpaneltransitionresizefunction.h transitiondialog.cpp transitiondialog.h transitioncrossfade_ui.ui \
					transitionwipe_ui.ui configproject_ui.ui flatbutton.cpp flatbutton.h clipproperties_ui.ui \
					clipproperties.cpp clipproperties.h transitionpip_ui.ui transitionpipwidget.cpp \
					exportbasewidget_ui.ui exportwidget.cpp exportwidget.h configmisc_ui.ui newproject_ui.ui \
					newproject.cpp createslideshowclip_ui.ui createslideshowclip.cpp createslideshowclip.h \
					changeclipspeed.cpp changeclipspeed.h changeclipspeed_ui.ui initeffects.h initeffects.cpp \
					keditmarkercommand.cpp keditmarkercommand.h addmarker_ui.ui kaddtransitioncommand.cpp \
					kaddtransitioncommand.h configtitler_ui.ui transitionaudiofade_ui.ui exportdvd_ui.ui \
				exportdvddialog.cpp exportdvddialog.h transitionwipewidget.cpp transitionwipewidget.h \
				transitionlumafile_ui.ui definitions.h editencoder_ui.ui

kdenlive_LDADD   =  $(LIB_KFILE) $(LIB_KDEUI) $(LIB_KDECORE) $(LIB_QT) \
		    $(LIBSOCKET) $(LIB_FIREWIRE)


EXTRA_DIST = $(mime_DATA) main.cpp kdenlive.cpp kdenlive.h kdenlivedoc.cpp kdenlivedoc.h kdenliveui.rc eventsrc kdenlive.desktop avlistviewitem.cpp doctrackbase.cpp doctracksound.cpp doctrackvideo.cpp kmmmonitor.cpp kmmscreen.cpp kmmtimeline.cpp projectlist.cpp avlistviewitem.h doctrackbase.h doctracksound.h doctrackvideo.h kmmmonitor.h kmmscreen.h kmmtimeline.h projectlist.h clipdrag.cpp clipdrag.h kmmtrackvideopanel.cpp kmmtrackvideopanel.h kmmtracksoundpanel.cpp kmmtracksoundpanel.h docclipavfile.cpp docclipavfile.h docclipbase.cpp docclipbase.h docclipproject.cpp docclipproject.h projectlistview.cpp projectlistview.h kruler.cpp kruler.h kfixedruler.cpp kfixedruler.h krulermodel.cpp krulermodel.h kscalableruler.cpp kscalableruler.h krulersliderbase.cpp krulersliderbase.h krulertimemodel.cpp krulertimemodel.h kmmtrackpanel.cpp kmmtrackpanel.h kmmtimelinetrackview.cpp kmmtimelinetrackview.h docclipbaselist.cpp docclipbaselist.h kmmeditpanel_ui.ui loadprogress_ui.ui kmmeditpanel.cpp kmmeditpanel.h kmmrulerpanel.cpp kmmrulerpanel.h kmmrulerpanel_ui.ui gentime.cpp gentime.h avfilelist.cpp avfilelist.h doctrackbaselist.cpp doctrackbaselist.h krender.cpp krender.h doctrackclipiterator.cpp doctrackclipiterator.h kmmtrackkeyframepanel.cpp kmmtrackkeyframepanel.h rangelist.cpp rangelist.h kmoveclipscommand.cpp kmoveclipscommand.h kselectclipcommand.cpp kselectclipcommand.h kaddclipcommand.cpp kaddclipcommand.h kresizecommand.cpp kresizecommand.h kdenlivesetupdlg.cpp kdenlivesetupdlg.h krendermanager.cpp krendermanager.h exportdialog.cpp exportdialog.h avfileformatdesc.cpp avfileformatdesc.h avformatdescbase.cpp avformatdescbase.h avformatdescbool.cpp avformatdescbool.h avformatdesclist.cpp avformatdesclist.h avformatwidgetbase.cpp avformatwidgetbase.h avformatwidgetbool.cpp avformatwidgetbool.h avformatwidgetlist.cpp avformatwidgetlist.h avformatdesccontainer.cpp avformatdesccontainer.h avformatwidgetcontainer.cpp avformatwidgetcontainer.h avfileformatwidget.cpp avfileformatwidget.h avformatdesccodeclist.cpp avformatdesccodeclist.h avformatwidgetcodeclist.cpp avformatwidgetcodeclist.h avformatdesccodec.cpp avformatdesccodec.h avformatwidgetcodec.cpp avformatwidgetcodec.h effectlistdialog.cpp effectlistdialog.h effectparamdialog.cpp effectparamdialog.h effectdesc.cpp effectdesc.h displayvideodesc.cpp displayvideodesc.h displayaudiodesc.cpp displayaudiodesc.h effect.cpp effect.h clippropertiesdialog.cpp clippropertiesdialog.h kdenlivesplash.cpp kdenlivesplash.h monitormanager.cpp monitormanager.h docclipref.cpp docclipref.h docclipreflist.cpp docclipreflist.h clipmanager.cpp clipmanager.h documentbasenode.cpp documentbasenode.h documentclipnode.cpp documentclipnode.h documentgroupnode.cpp documentgroupnode.h docsubclip.h docsubclip.cpp trackpanelclipslipfunction.cpp trackpanelclipslipfunction.h


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
INCLUDES= $(all_includes) $(LIBMLT_CFLAGS) @LIBIEC61883_CFLAGS@

kde_kcfg_DATA = kdenlive.kcfg
METASOURCES = AUTO

# the library search path. 
kdenlive_LDFLAGS = $(all_libraries) $(KDE_RPATH) $(LIBMLT_LIBS) -lmlt++

rcdir = $(kde_datadir)/kdenlive
rc_DATA = kdenliveui.rc eventsrc

noinst_HEADERS = configureproject.h configureprojectdialog.h exportconfig.h \
			trackpanelmarkerfunction.h trackviewdecorator.h trackviewbackgrounddecorator.h \
			trackviewnamedecorator.h trackviewmarkerdecorator.h kaddmarkercommand.h saveprojectfilter.h \
			saveprojectnativefilter.h loadprojectfilter.h loadprojectnativefilter.h projectformatmanager.h \
			loadprojectkinofilter.h timecode.h ktimeline.h kmmtimeline.h ktrackview.h \
			documentmacrocommands.h trackpanelfunctionfactory.h trackpanelselectnonefunction.h \
			effectkeyframe.h effectdoublekeyframe.h effectparamdesc.h effectparamdoubledesc.h \
			effectparameter.h effectparamdescfactory.h doctrackdecorator.h ktrackpanel.h \
			ktrackclippanel.h trackviewdoublekeyframedecorator.h trackviewaudiobackgrounddecorator.h \
			effectstack.h effectstackdialog.h effectdrag.h kaddeffectcommand.h \
			effectdescriptionlist.h effectstacklistview.h saveprojectscenelistfilter.h ktrackplacer.h \
			kplacer.h kclipplacer.h kmmclipkeyframepanel.h trackpanelcliprollfunction.h \
			krollcommand.h dynamicToolTip.h kmonitor.h capturemonitor.h newproject.h definitions.h








