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

#include <KXmlGuiWindow>
#include <KTextEdit>
#include <KListWidget>
#include <KTabWidget>
#include <KUndoStack>
#include <KRecentFilesAction>
#include <KComboBox>
#include <kautosavefile.h>

#include "effectslist.h"
#include "gentime.h"
#include "definitions.h"
#include "statusbarmessagelabel.h"
#include "dvdwizard.h"

class KdenliveDoc;
class TrackView;
class MonitorManager;
class ProjectList;
class EffectsListView;
class EffectStackView;
class TransitionSettings;
class Monitor;
class RecMonitor;
class CustomTrackView;
class RenderWidget;
#ifndef NO_JOGSHUTTLE
class JogShuttle;
#endif /* NO_JOGSHUTTLE */
class DocClipBase;
class Render;
class Transition;
class KActionCollection;

class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kdenlive.MainWindow")

public:

    /** @brief Initialises the main window.
     * @param MltPath (optional) path to MLT environment
     * @param Url (optional) file to open
     *
     * If Url is present, it will be opened, otherwhise, if openlastproject is
     * set, latest project will be opened. If no file is open after trying this,
     * a default new file will be created. */
    explicit MainWindow(const QString &MltPath = QString(),
                        const KUrl &Url = KUrl(), QWidget *parent = 0);

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

    QDockWidget *m_effectStackDock;
    EffectStackView *m_effectStack;

    QDockWidget *m_transitionConfigDock;
    TransitionSettings *m_transitionConfig;

    QDockWidget *m_clipMonitorDock;
    Monitor *m_clipMonitor;

    QDockWidget *m_projectMonitorDock;
    Monitor *m_projectMonitor;

    QDockWidget *m_recMonitorDock;
    RecMonitor *m_recMonitor;

    QDockWidget *m_undoViewDock;
    QUndoView *m_undoView;
    QUndoGroup *m_commandStack;

    KComboBox *m_timecodeFormat;

    QMenu *m_videoEffectsMenu;
    QMenu *m_audioEffectsMenu;
    QMenu *m_customEffectsMenu;
    QMenu *m_transitionsMenu;
    QMenu *m_timelineContextMenu;
    QMenu *m_timelineContextClipMenu;
    QMenu *m_timelineContextTransitionMenu;
    KUrl m_startUrl;

    /** @brief Shortcut to remove the focus from any element.
     *
     * It allows to get out of e.g. text input fields and to press another
     * shortcut. */
    QShortcut* m_shortcutRemoveFocus;

    RenderWidget *m_renderWidget;

#ifndef NO_JOGSHUTTLE
    JogShuttle *m_jogProcess;
#endif /* NO_JOGSHUTTLE */

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
    StatusBarMessageLabel *m_messageLabel;
    QActionGroup *m_clipTypeGroup;
    KActionCollection *m_effectsActionCollection;

    bool m_findActivated;
    QString m_findString;
    QTimer m_findTimer;

    void readOptions();
    void saveOptions();
#ifndef NO_JOGSHUTTLE
    void activateShuttleDevice();
    void slotShuttleAction(int code);
#endif /* NO_JOGSHUTTLE */
    void connectDocumentInfo(KdenliveDoc *doc);
    void findAhead();
    void doOpenFile(const KUrl &url, KAutoSaveFile *stale);
    void recoverFiles(QList<KAutoSaveFile *> staleFiles);
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
    QPixmap createSchemePreviewIcon(const KSharedConfigPtr &config);

    /** @brief Checks that the Kdenlive mime type is correctly installed.
    * @return The mimetype */
    QString getMimeType();

public slots:
    /** @brief Prepares opening @param url.
    *
    * Checks if already open and whether backup exists */
    void openFile(const KUrl &url);
    void slotGotProgressInfo(const QString &message, int progress);
    Q_SCRIPTABLE void setRenderingProgress(const QString &url, int progress);
    Q_SCRIPTABLE void setRenderingFinished(const QString &url, int status, const QString &error);

private slots:
    void newFile(bool showProjectSettings = true, bool force = false);
    void queryQuit();
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
    void slotRaiseMonitor(bool clipMonitor);
    void slotUpdateClip(const QString &id);
    void slotUpdateMousePosition(int pos);
    void slotAddEffect(const QDomElement effect, GenTime pos = GenTime(), int track = -1);
    void slotEditProfiles();
    void slotDetectAudioDriver();
    void slotEditProjectSettings();
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

    void closeCurrentDocument(bool saveChanges = true);
    /** @brief Deletes item in timeline, project tree or effect stack depending on focus. */
    void slotDeleteItem();
    void slotAddClipMarker();
    void slotDeleteClipMarker();
    void slotDeleteAllClipMarkers();
    void slotEditClipMarker();
    void slotCutTimelineClip();
    void slotInsertClipOverwrite();
    void slotSelectTimelineClip();
    void slotSelectTimelineTransition();
    void slotDeselectTimelineClip();
    void slotDeselectTimelineTransition();
    void slotSelectAddTimelineClip();
    void slotSelectAddTimelineTransition();
    void slotAddVideoEffect(QAction *result);
    void slotAddAudioEffect(QAction *result);
    void slotAddCustomEffect(QAction *result);
    void slotAddTransition(QAction *result);
    void slotAddProjectClip(KUrl url);
#ifndef NO_JOGSHUTTLE
    void slotShuttleButton(int code);
#endif /* NO_JOGSHUTTLE */
    void slotShowClipProperties(DocClipBase *clip);
    void slotShowClipProperties(QList <DocClipBase *>cliplist, QMap<QString, QString> commonproperties);
    void slotActivateEffectStackView();
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
    void slotReloadEffects();

    void slotAdjustClipMonitor();
    void slotAdjustProjectMonitor();
    void slotSaveZone(Render *render, QPoint zone);

    void slotSetInPoint();
    void slotSetOutPoint();
    void slotResizeItemStart();
    void slotResizeItemEnd();
    void configureNotifications();
    void slotInsertTrack(int ix = 0);
    void slotDeleteTrack(int ix = 0);
    /** @brief Shows the configure tracks dialog and updates transitions afterwards. */
    void slotConfigTrack(int ix = -1);
    void slotGetNewLumaStuff();
    void slotGetNewTitleStuff();
    void slotGetNewRenderStuff();
    void slotGetNewMltProfileStuff();
    void slotAutoTransition();
    void slotRunWizard();
    /** @brief Lets the sampleplugin create a generator.  */
    void generateClip();
    void slotZoneMoved(int start, int end);
    void slotUpdatePreviewSettings();
    void slotDvdWizard(const QString &url = QString(), const QString &profile = "dv_pal");
    void slotGroupClips();
    void slotUnGroupClips();
    void slotEditItemDuration();
    void slotClipInProjectTree();
    void slotSplitAudio();
    void slotUpdateClipType(QAction *action);
    void slotShowTimeline(bool show);
    void slotMaximizeCurrent(bool show);
    void slotTranscode(KUrl::List urls = KUrl::List());
    void slotTranscodeClip();
    void slotSetDocumentRenderProfile(const QString &dest, const QString &group, const QString &name, const QString &file);
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

signals:
    Q_SCRIPTABLE void abortRenderJob(const QString &url);
};


#endif
