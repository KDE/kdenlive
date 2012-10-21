/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDockWidget>
#include <QUndoView>
#include <QLabel>
#include <QProgressBar>
#include <QEvent>
#include <QTimer>
#include <QShortcut>
#include <QMap>
#include <QString>
#include <QImage>

#include <KXmlGuiWindow>
#include <KTextEdit>
#include <KListWidget>
#include <KTabWidget>
#include <KUndoStack>
#include <KRecentFilesAction>
#include <KComboBox>
#include <kautosavefile.h>
#include <KActionCategory>

#include "effectslist.h"
#include "gentime.h"
#include "definitions.h"
#include "statusbarmessagelabel.h"
#include "dvdwizard.h"
#include "stopmotion/stopmotion.h"
#include "noteswidget.h"
#include "ui_backupdialog_ui.h"

class KdenliveDoc;
class TrackView;
class MonitorManager;
class ProjectList;
class EffectsListView;
class EffectStackView;
class EffectStackView2;
class TransitionSettings;
class Monitor;
class RecMonitor;
class CustomTrackView;
class RenderWidget;
#ifdef USE_JOGSHUTTLE
class JogShuttle;
class JogShuttleAction;
#endif
class DocClipBase;
class Render;
class Transition;
class ScopeManager;
class Histogram;
class Vectorscope;
class Waveform;
class RGBParade;
class KActionCollection;
class AudioSignal;
class AbstractAudioScopeWidget;
class AudioSpectrum;
class Spectrogram;

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kdenlive.MainWindow")

public:

    /** @brief Initialises the main window.
     * @param MltPath (optional) path to MLT environment
     * @param Url (optional) file to open
     * @param clipsToLoad (optional) a comma separated list of clips to import in project
     *
     * If Url is present, it will be opened, otherwhise, if openlastproject is
     * set, latest project will be opened. If no file is open after trying this,
     * a default new file will be created. */
    explicit MainWindow(const QString &MltPath = QString(),
                        const KUrl &Url = KUrl(), const QString & clipsToLoad = QString(), QWidget *parent = 0);
    virtual ~MainWindow();

    /** @brief Locates the MLT environment.
     * @param mltPath (optional) path to MLT environment
     *
     * It tries to set the paths of the MLT profiles and renderer, using
     * mltPath, MLT_PREFIX, searching for the binary `melt`, or asking to the
     * user. It doesn't fill any list of profiles, while its name suggests so. */
    void parseProfiles(const QString &mltPath = QString());

    static EffectsList videoEffects;
    static EffectsList audioEffects;
    static EffectsList customEffects;
    static EffectsList transitions;
    
    /** @brief Cache for luma files thumbnails. */
    static QMap <QString,QImage> m_lumacache;

protected:

    /** @brief Closes the window.
     * @return false if the user presses "Cancel" on a confirmation dialog or
     *     the operation requested (starting waiting jobs or saving file) fails,
     *     true otherwise */
    virtual bool queryClose();

    /** @brief Reports a message in the status bar when an error occurs. */
    virtual void customEvent(QEvent *e);

    /** @brief Enables live search in the timeline. */
    virtual void keyPressEvent(QKeyEvent *ke);

    /** @brief Stops the active monitor when the window gets hidden. */
    virtual void hideEvent(QHideEvent *e);

    /** @brief Filters key events to the live search. */
    bool eventFilter(QObject *obj, QEvent *ev);

    /** @brief Saves the file and the window properties when saving the session. */
    virtual void saveProperties(KConfigGroup &config);

    /** @brief Restores the window and the file when a session is loaded. */
    virtual void readProperties(const KConfigGroup &config);

private:
    KTabWidget* m_timelineArea;
    QProgressBar *m_statusProgressBar;

    ScopeManager *m_scopeManager;

    /** @brief Sets up all the actions and attaches them to the collection. */
    void setupActions();
    KdenliveDoc *m_activeDocument;
    TrackView *m_activeTimeline;
    MonitorManager *m_monitorManager;

    QDockWidget *m_projectListDock;
    ProjectList *m_projectList;

    QDockWidget *m_effectListDock;
    EffectsListView *m_effectList;
    //KListWidget *m_effectList;

    QDockWidget *m_notesDock;
    NotesWidget *m_notesWidget;

    QDockWidget *m_effectStackDock;
    EffectStackView2 *m_effectStack;

    QDockWidget *m_transitionConfigDock;
    TransitionSettings *m_transitionConfig;

    QDockWidget *m_clipMonitorDock;
    Monitor *m_clipMonitor;

    QDockWidget *m_projectMonitorDock;
    Monitor *m_projectMonitor;

    QDockWidget *m_recMonitorDock;
    RecMonitor *m_recMonitor;

    QDockWidget *m_vectorscopeDock;
    Vectorscope *m_vectorscope;

    QDockWidget *m_waveformDock;
    Waveform *m_waveform;

    QDockWidget *m_RGBParadeDock;
    RGBParade *m_RGBParade;

    QDockWidget *m_histogramDock;
    Histogram *m_histogram;

    QDockWidget *m_audiosignalDock;
    AudioSignal *m_audiosignal;

    QDockWidget *m_audioSpectrumDock;
    AudioSpectrum *m_audioSpectrum;

    QDockWidget *m_spectrogramDock;
    Spectrogram *m_spectrogram;

    QDockWidget *m_undoViewDock;
    QUndoView *m_undoView;
    QUndoGroup *m_commandStack;

    KComboBox *m_timecodeFormat;

    /** This list holds all the scopes used in Kdenlive, allowing to manage some global settings */
    QList <QDockWidget *> m_gfxScopesList;

    KActionCategory *m_effectActions;
    QMenu *m_effectsMenu;
    QMenu *m_transitionsMenu;
    QMenu *m_timelineContextMenu;
    QMenu *m_timelineContextClipMenu;
    QMenu *m_timelineContextTransitionMenu;
    KUrl m_startUrl;

    /** Actions used in the stopmotion widget */
    KActionCategory *m_stopmotion_actions;

    /** Action names that can be used in the slotDoAction() slot, with their i18n() names */
    QStringList m_action_names;


    /** @brief Shortcut to remove the focus from any element.
     *
     * It allows to get out of e.g. text input fields and to press another
     * shortcut. */
    QShortcut* m_shortcutRemoveFocus;

    RenderWidget *m_renderWidget;

#ifdef USE_JOGSHUTTLE
    JogShuttle *m_jogProcess;
    JogShuttleAction* m_jogShuttle;
#endif

    KRecentFilesAction *m_fileOpenRecent;
    KAction *m_fileRevert;
    KAction *m_projectSearch;
    KAction *m_projectSearchNext;

    KAction **m_videoEffects;
    KAction **m_audioEffects;
    KAction **m_customEffects;
    KAction **m_transitions;
    KAction *m_buttonAudioThumbs;
    KAction *m_buttonVideoThumbs;
    KAction *m_buttonShowMarkers;
    KAction *m_buttonFitZoom;
    KAction *m_buttonAutomaticSplitAudio;
    KAction *m_normalEditTool;
    KAction *m_overwriteEditTool;
    KAction *m_insertEditTool;
    KAction *m_buttonSelectTool;
    KAction *m_buttonRazorTool;
    KAction *m_buttonSpacerTool;
    KAction *m_buttonSnap;
    KAction *m_saveAction;
    KAction *m_closeAction;
    QSlider *m_zoomSlider;
    KAction *m_zoomIn;
    KAction *m_zoomOut;
    KAction *m_loopZone;
    KAction *m_playZone;
    KAction *m_loopClip;
    KSelectAction *m_loadLayout;
    StatusBarMessageLabel *m_messageLabel;
    QActionGroup *m_clipTypeGroup;
    KActionCollection *m_effectsActionCollection;
    KActionCollection *m_tracksActionCollection;

    bool m_findActivated;
    QString m_findString;
    QTimer m_findTimer;

    void readOptions();
    void saveOptions();
#ifdef USE_JOGSHUTTLE
    void activateShuttleDevice();
#endif
    void connectDocumentInfo(KdenliveDoc *doc);
    void findAhead();
    void doOpenFile(const KUrl &url, KAutoSaveFile *stale);
    void recoverFiles(QList<KAutoSaveFile *> staleFiles, const KUrl &originUrl);

    /** @brief Loads static and dynamic plugins.
     *
     * It scans static plugins as well as the filesystem: it doesn't load more
     * than one plugin per file name, to avoid duplication due to conflicting
     * installations. */
    void loadPlugins();
    void populateMenus(QObject *plugin);
    void addToMenu(QObject *plugin, const QStringList &texts,
                   QMenu *menu, const char *member,
                   QActionGroup *actionGroup);
    void aboutPlugins();

    /** @brief Instantiates a "Get Hot New Stuff" dialog.
     * @param configFile configuration file for KNewStuff
     * @return number of installed items */
    int getNewStuff(const QString &configFile = QString());
    QStringList m_pluginFileNames;
    QByteArray m_timelineState;
    void loadTranscoders();
    void loadClipActions();
    QPixmap createSchemePreviewIcon(const KSharedConfigPtr &config);

    /** @brief Checks that the Kdenlive mime type is correctly installed.
    * @param open If set to true, this will return the mimetype allowed for file opening (adds .tar.gz format)
    * @return The mimetype */
    QString getMimeType(bool open = true);

    /** @brief Populates the "load layout" menu. */
    void loadLayouts();

    StopmotionWidget *m_stopmotion;
    QTime m_timer;
    /** @brief The last selected clip in timeline. */
    ClipItem *m_mainClip;
    /** @brief Update statusbar stylesheet (in case of color theme change). */
    void setStatusBarStyleSheet(const QPalette &p);

public slots:
    /** @brief Prepares opening @param url.
    *
    * Checks if already open and whether backup exists */
    void openFile(const KUrl &url);
    void slotGotProgressInfo(const QString &message, int progress);
    void slotReloadEffects();
    Q_SCRIPTABLE void setRenderingProgress(const QString &url, int progress);
    Q_SCRIPTABLE void setRenderingFinished(const QString &url, int status, const QString &error);


private slots:
    void newFile(bool showProjectSettings = true, bool force = false);
    void activateDocument();
    void connectDocument(TrackView*, KdenliveDoc*);

    /** @brief Shows file open dialog. */
    void openFile();
    void openLastFile();

    /** @brief Checks whether a URL is available to save to.
    * @return Whether the file was saved. */
    bool saveFile();

    /** @brief Shows a save file dialog for saving the project.
    * @return Whether the file was saved. */
    bool saveFileAs();

    /** @brief Set properties to match outputFileName and save the document.
    * @param outputFileName The URL to save to / The document's URL.
    * @return Whether we had success. */
    bool saveFileAs(const QString &outputFileName);

    /** @brief Shows the shortcut dialog. */
    void slotEditKeys();
    void slotPreferences(int page = -1, int option = -1);

    /** @brief Reflects setting changes to the GUI. */
    void updateConfiguration();
    void slotConnectMonitors();
    void slotUpdateClip(const QString &id);
    void slotUpdateMousePosition(int pos);
    void slotAddEffect(const QDomElement effect);
    void slotEditProfiles();
    void slotDetectAudioDriver();
    void slotEditProjectSettings();
    /** @brief Change current document MLT profile. */
    void slotUpdateProjectProfile(const QString &profile);
    void slotDisplayActionMessage(QAction *a);

    /** @brief Turns automatic splitting of audio and video on/off. */
    void slotSwitchSplitAudio();
    void slotSwitchVideoThumbs();
    void slotSwitchAudioThumbs();
    void slotSwitchMarkersComments();
    void slotSwitchSnap();
    void slotRenderProject();
    void slotFullScreen();
    void slotUpdateDocumentState(bool modified);

    /** @brief Sets the timeline zoom slider to @param value.
    *
    * Also disables zoomIn and zoomOut actions if they cannot be used at the moment. */
    void slotSetZoom(int value);
    /** @brief Decreases the timeline zoom level by 1. */
    void slotZoomIn();
    /** @brief Increases the timeline zoom level by 1. */
    void slotZoomOut();
    /** @brief Makes the timeline zoom level fit the timeline content. */
    void slotFitZoom();
    /** @brief Updates the zoom slider tooltip to fit @param zoomlevel. */
    void slotUpdateZoomSliderToolTip(int zoomlevel);

    /** @brief Displays the zoom slider tooltip.
    * @param zoomlevel (optional) The zoom level to show in the tooltip.
    *
    * Adopted from Dolphin (src/statusbar/dolphinstatusbar.cpp) */
    void slotShowZoomSliderToolTip(int zoomlevel = -1);
    /** @brief Close currently opened document. Returns false if something went wrong (cannot save modifications, ...). */
    bool closeCurrentDocument(bool saveChanges = true);
    /** @brief Deletes item in timeline, project tree or effect stack depending on focus. */
    void slotDeleteItem();
    void slotAddClipMarker();
    void slotDeleteClipMarker();
    void slotDeleteAllClipMarkers();
    void slotEditClipMarker();

    /** @brief Adds marker or auide at the current position without showing the marker dialog.
     *
     * Adds a marker if clip monitor is active, otherwise a guide.
     * The comment is set to the current position (therefore not dialog).
     * This can be useful to mark something during playback. */
    void slotAddMarkerGuideQuickly();
    void slotCutTimelineClip();
    void slotInsertClipOverwrite();
    void slotSelectTimelineClip();
    void slotSelectTimelineTransition();
    void slotDeselectTimelineClip();
    void slotDeselectTimelineTransition();
    void slotSelectAddTimelineClip();
    void slotSelectAddTimelineTransition();
    void slotAddVideoEffect(QAction *result);
    void slotAddTransition(QAction *result);
    void slotAddProjectClip(KUrl url, stringMap data = stringMap());
    void slotAddProjectClipList(KUrl::List urls);
    void slotShowClipProperties(DocClipBase *clip);
    void slotShowClipProperties(QList <DocClipBase *>cliplist, QMap<QString, QString> commonproperties);
    void slotTimelineClipSelected(ClipItem* item, bool raise = true);
    void slotTrackSelected(int index, TrackInfo info, bool raise = true);
    void slotActivateTransitionView(Transition *);
    void slotChangeTool(QAction * action);
    void slotChangeEdit(QAction * action);
    void slotSetTool(PROJECTTOOL tool);
    void slotSnapForward();
    void slotSnapRewind();
    void slotClipStart();
    void slotClipEnd();
    void slotZoneStart();
    void slotZoneEnd();
    void slotFind();
    void findTimeout();
    void slotFindNext();
    void slotSelectClipInTimeline();
    void slotClipInTimeline(const QString &clipId);

    void slotInsertSpace();
    void slotRemoveSpace();
    void slotAddGuide();
    void slotEditGuide();
    void slotDeleteGuide();
    void slotDeleteAllGuides();
    void slotGuidesUpdated();

    void slotCopy();
    void slotPaste();
    void slotPasteEffects();

    void slotAdjustClipMonitor();
    void slotAdjustProjectMonitor();
    void slotSaveZone(Render *render, QPoint zone, DocClipBase *baseClip = NULL, KUrl path = KUrl());

    void slotSetInPoint();
    void slotSetOutPoint();
    void slotResizeItemStart();
    void slotResizeItemEnd();
    void configureNotifications();
    void slotInsertTrack(int ix = -1);
    void slotDeleteTrack(int ix = -1);
    /** @brief Shows the configure tracks dialog and updates transitions afterwards. */
    void slotConfigTrack(int ix = -1);
    /** @brief Select all clips in active track. */
    void slotSelectTrack();
    /** @brief Select all clips in timeline. */
    void slotSelectAllTracks();
    void slotGetNewLumaStuff();
    void slotGetNewTitleStuff();
    void slotGetNewRenderStuff();
    void slotGetNewMltProfileStuff();
    void slotAutoTransition();
    void slotRunWizard();
    /** @brief Lets the sampleplugin create a generator.  */
    void generateClip();
    void slotZoneMoved(int start, int end);
    void slotDvdWizard(const QString &url = QString(), const QString &profile = "dv_pal");
    void slotGroupClips();
    void slotUnGroupClips();
    void slotEditItemDuration();
    void slotClipInProjectTree();
    //void slotClipToProjectTree();
    void slotSplitAudio();
    void slotSetAudioAlignReference();
    void slotAlignAudio();
    void slotUpdateClipType(QAction *action);
    void slotShowTimeline(bool show);
    void slotMaximizeCurrent(bool show);
    void slotTranscode(KUrl::List urls = KUrl::List());
    void slotStartClipAction();
    void slotTranscodeClip();
    /** @brief Archive project: creates a copy of the project file with all clips in a new folder. */
    void slotArchiveProject();
    void slotSetDocumentRenderProfile(QMap <QString, QString> props);
    void slotPrepareRendering(bool scriptExport, bool zoneOnly, const QString &chapterFile);

    /** @brief Switches between displaying frames or timecode.
    * @param ix 0 = display timecode, 1 = display frames. */
    void slotUpdateTimecodeFormat(int ix);

    /** @brief Removes the focus of anything. */
    void slotRemoveFocus();
    void slotCleanProject();
    void slotUpdateClipMarkers(DocClipBase *clip);
    void slotRevert();
    void slotShutdown();
    void slotUpdateTrackInfo();

    /** @brief Changes the color scheme. */
    void slotChangePalette(QAction *action, const QString &themename = QString());
    void slotSwitchMonitors();
    void slotCheckRenderStatus();
    void slotInsertZoneToTree();
    void slotInsertZoneToTimeline();

    /** @brief Deletes items from timeline and document.
    * @param ids The ids of the clips to delete.
    * @param folderids The names and ids of the folders to delete. */
    void slotDeleteProjectClips(QStringList ids, QMap<QString, QString> folderids);
    void slotShowTitleBars(bool show);
    void slotSwitchTitles();
    /** @brief Update the capture folder if user asked a change. */
    void slotUpdateCaptureFolder();

    /** @brief Apply new properties to a clip */
    void slotApplyNewClipProperties(const QString id, QMap <QString, QString> props, QMap <QString, QString> newprops, bool refresh, bool reload);

    /** @brief Delete a clip from current project */
    void slotDeleteClip(const QString &id);

    /** @brief Saves the widgets layout */
    void slotSaveLayout(QAction*);
    void slotLoadLayout(QAction*);

    /** @brief The monitor informs that it needs (or not) to have frames sent by the renderer. */
    void slotMonitorRequestRenderFrame(bool request);
    /** @brief Open the stopmotion dialog. */
    void slotOpenStopmotion();
    /** @brief Implements all the actions that are int he ActionsCollection. */
    void slotDoAction(const QString& action_name);
    /** @brief Update project because the use of proxy clips was enabled / disabled. */
    void slotUpdateProxySettings();
    /** @brief Insert current project's timecode into the notes widget. */
    void slotInsertNotesTimecode();
    /** @brief Open the project's backupdialog. */
    void slotOpenBackupDialog(const KUrl url = KUrl());
    /** @brief Disable proxies for this project. */
    void slotDisableProxies();

    void slotElapsedTime();
    /** @brief Open the online services search dialog. */
    void slotDownloadResources();
    
    void slotChangePalette();
    /** @brief Save current timeline clip as mlt playlist. */
    void slotSaveTimelineClip();
    /** @brief Process keyframe data sent from a clip to effect / transition stack. */
    void slotProcessImportKeyframes(GRAPHICSRECTITEM type, const QString& data);
    /** @brief Move playhead to mouse curser position if p key is pressed */
    void slotOnPlayheadKeyPressed();

signals:
    Q_SCRIPTABLE void abortRenderJob(const QString &url);
};


#endif
