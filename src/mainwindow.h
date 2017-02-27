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
#include <QEvent>
#include <QShortcut>
#include <QMap>
#include <QString>
#include <QImage>
#include <QDBusAbstractAdaptor>

#include <KXmlGuiWindow>
#include <QTabWidget>
#include <kautosavefile.h>
#include <KActionCategory>
#include <KSelectAction>
#include <KColorSchemeManager>

#include "kdenlivecore_export.h"
#include "effectslist/effectslist.h"
#include "gentime.h"
#include "bin/bin.h"
#include "definitions.h"
#include "statusbarmessagelabel.h"
#include "dvdwizard/dvdwizard.h"
#include "stopmotion/stopmotion.h"

class KdenliveDoc;
class EffectsListView;
class EffectStackView;
class EffectStackView2;
class AudioGraphSpectrum;
class Monitor;
class RecMonitor;
class RenderWidget;
class Render;
class Transition;
class KIconLoader;


class /*KDENLIVECORE_EXPORT*/ MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

public:

    explicit MainWindow(QWidget *parent = nullptr);
    /** @brief Initialises the main window.
     * @param MltPath (optional) path to MLT environment
     * @param Url (optional) file to open
     * @param clipsToLoad (optional) a comma separated list of clips to import in project
     *
     * If Url is present, it will be opened, otherwhise, if openlastproject is
     * set, latest project will be opened. If no file is open after trying this,
     * a default new file will be created. */
    void init(const QString &MltPath, const QUrl &Url, const QString &clipsToLoad);
    virtual ~MainWindow();

    static EffectsList videoEffects;
    static EffectsList audioEffects;
    static EffectsList customEffects;
    static EffectsList transitions;

    /** @brief Cache for luma files thumbnails. */
    static QMap<QString, QImage> m_lumacache;
    static QMap<QString, QStringList> m_lumaFiles;

    /** @brief Adds an action to the action collection and stores the name. */
    void addAction(const QString &name, QAction *action);
    /** @brief Adds an action to the action collection and stores the name. */
    QAction *addAction(const QString &name, const QString &text, const QObject *receiver,
                       const char *member, const QIcon &icon = QIcon(), const QKeySequence &shortcut = QKeySequence());

    /**
     * @brief Adds a new dock widget to this window.
     * @param title title of the dock widget
     * @param objectName objectName of the dock widget (required for storing layouts)
     * @param widget widget to use in the dock
     * @param area area to which the dock should be added to
     * @returns the created dock widget
     */
    QDockWidget *addDock(const QString &title, const QString &objectName, QWidget *widget, Qt::DockWidgetArea area = Qt::TopDockWidgetArea);

    // TODO make private again
    QTabWidget *m_timelineArea;
    StopmotionWidget *m_stopmotion;
    QUndoGroup *m_commandStack;
    EffectStackView2 *m_effectStack;
    QUndoView *m_undoView;
    /** @brief holds info about whether movit is available on this system */
    bool m_gpuAllowed;
    int m_exitCode;
    QMap<QString, KActionCategory *> kdenliveCategoryMap;
    QList<QAction *> getExtraActions(const QString &name);
    /** @brief Returns true if docked widget is tabbed with another widget from its object name */
    bool isTabbedWith(QDockWidget *widget, const QString &otherWidget);

protected:
    /** @brief Closes the window.
     * @return false if the user presses "Cancel" on a confirmation dialog or
     *     the operation requested (starting waiting jobs or saving file) fails,
     *     true otherwise */
    bool queryClose() Q_DECL_OVERRIDE;
    void closeEvent(QCloseEvent *) Q_DECL_OVERRIDE;

    /** @brief Reports a message in the status bar when an error occurs. */
    void customEvent(QEvent *e) Q_DECL_OVERRIDE;

    /** @brief Stops the active monitor when the window gets hidden. */
    void hideEvent(QHideEvent *e) Q_DECL_OVERRIDE;

    /** @brief Saves the file and the window properties when saving the session. */
    void saveProperties(KConfigGroup &config) Q_DECL_OVERRIDE;

    /** @brief Restores the window and the file when a session is loaded. */
    void readProperties(const KConfigGroup &config) Q_DECL_OVERRIDE;
    void saveNewToolbarConfig() Q_DECL_OVERRIDE;

private:

    /** @brief Sets up all the actions and attaches them to the collection. */
    void setupActions();

    KColorSchemeManager *m_colorschemes;

    QDockWidget *m_projectBinDock;
    QDockWidget *m_effectListDock;
    EffectsListView *m_effectList;
    QDockWidget *m_transitionListDock;
    EffectsListView *m_transitionList;

    QDockWidget *m_effectStackDock;

    QDockWidget *m_clipMonitorDock;
    Monitor *m_clipMonitor;

    QDockWidget *m_projectMonitorDock;
    Monitor *m_projectMonitor;

    QDockWidget *m_recMonitorDock;
    RecMonitor *m_recMonitor;

    AudioGraphSpectrum *m_audioSpectrum;

    QDockWidget *m_undoViewDock;

    KSelectAction *m_timeFormatButton;
    KSelectAction *m_compositeAction;

    /** This list holds all the scopes used in Kdenlive, allowing to manage some global settings */
    QList<QDockWidget *> m_gfxScopesList;

    KActionCategory *m_effectActions;
    KActionCategory *m_transitionActions;
    QMenu *m_effectsMenu;
    QMenu *m_transitionsMenu;
    QMenu *m_timelineContextMenu;
    QMenu *m_timelineContextClipMenu;
    QMenu *m_timelineContextTransitionMenu;

    /** Actions used in the stopmotion widget */
    KActionCategory *m_stopmotion_actions;

    /** Action names that can be used in the slotDoAction() slot, with their i18n() names */
    QStringList m_actionNames;

    /** @brief Shortcut to remove the focus from any element.
     *
     * It allows to get out of e.g. text input fields and to press another
     * shortcut. */
    QShortcut *m_shortcutRemoveFocus;

    RenderWidget *m_renderWidget;
    StatusBarMessageLabel *m_messageLabel;
    QList<QAction *>m_transitions;
    QAction *m_buttonAudioThumbs;
    QAction *m_buttonVideoThumbs;
    QAction *m_buttonShowMarkers;
    QAction *m_buttonFitZoom;
    QAction *m_buttonAutomaticSplitAudio;
    QAction *m_buttonAutomaticTransition;
    QAction *m_normalEditTool;
    QAction *m_overwriteEditTool;
    QAction *m_insertEditTool;
    QAction *m_buttonSelectTool;
    QAction *m_buttonRazorTool;
    QAction *m_buttonSpacerTool;
    QAction *m_buttonSnap;
    QAction *m_saveAction;
    QSlider *m_zoomSlider;
    QAction *m_zoomIn;
    QAction *m_zoomOut;
    QAction *m_loopZone;
    QAction *m_playZone;
    QAction *m_loopClip;
    QAction *m_proxyClip;
    QActionGroup *m_clipTypeGroup;
    QString m_theme;
    KIconLoader *m_iconLoader;
    KToolBar *m_timelineToolBar;
    QWidget *m_timelineToolBarContainer;
    QLabel *m_trimLabel;

    /** @brief initialize startup values, return true if first run. */
    bool readOptions();
    void saveOptions();
    bool event(QEvent *e) Q_DECL_OVERRIDE;

    void loadGenerators();
    /** @brief Instantiates a "Get Hot New Stuff" dialog.
     * @param configFile configuration file for KNewStuff
     * @return number of installed items */
    int getNewStuff(const QString &configFile = QString());
    QStringList m_pluginFileNames;
    QByteArray m_timelineState;
    void buildDynamicActions();
    void loadClipActions();

    QTime m_timer;
    KXMLGUIClient *m_extraFactory;
    bool m_themeInitialized;
    bool m_isDarkTheme;
    QListWidget *m_effectBasket;
    /** @brief Update widget style. */
    void doChangeStyle();

public slots:
    void slotGotProgressInfo(const QString &message, int progress, MessageType type = DefaultMessage);
    void slotReloadEffects();
    Q_SCRIPTABLE void setRenderingProgress(const QString &url, int progress);
    Q_SCRIPTABLE void setRenderingFinished(const QString &url, int status, const QString &error);
    Q_SCRIPTABLE void addProjectClip(const QString &url);
    Q_SCRIPTABLE void addTimelineClip(const QString &url);
    Q_SCRIPTABLE void addEffect(const QString &effectName);
    Q_SCRIPTABLE void scriptRender(const QString &url);
    Q_NOREPLY void exitApp();

    void slotSwitchVideoThumbs();
    void slotSwitchAudioThumbs();

    void slotPreferences(int page = -1, int option = -1);
    void connectDocument();
    void slotTimelineClipSelected(ClipItem *item, bool reloadStack = true);
    /** @brief Reload project profile in config dialog if changed. */
    void slotRefreshProfiles();
    void updateDockTitleBars(bool isTopLevel = true);
    void configureToolbars() Q_DECL_OVERRIDE;

private slots:
    /** @brief Shows the shortcut dialog. */
    void slotEditKeys();
    void loadDockActions();
    /** @brief Reflects setting changes to the GUI. */
    void updateConfiguration();
    void slotConnectMonitors();
    void slotUpdateClip(const QString &id, bool reload);
    void slotUpdateMousePosition(int pos);
    void slotUpdateProjectDuration(int pos);
    void slotAddEffect(const QDomElement &effect);
    void slotEditProjectSettings();

    /** @brief Turns automatic splitting of audio and video on/off. */
    void slotSwitchSplitAudio(bool enable);
    void slotSwitchMarkersComments();
    void slotSwitchSnap();
    void slotSwitchAutomaticTransition();
    void slotRenderProject();
    void slotStopRenderProject();
    void slotFullScreen();
    /** @brief if modified is true adds "modified" to the caption and enables the save button.
    * (triggered by KdenliveDoc::setModified()) */
    void slotUpdateDocumentState(bool modified);

    /** @brief Sets the timeline zoom slider to @param value.
    *
    * Also disables zoomIn and zoomOut actions if they cannot be used at the moment. */
    void slotSetZoom(int value, bool zoomOnMouse = false);
    /** @brief Decreases the timeline zoom level by 1. */
    void slotZoomIn(bool zoomOnMouse = false);
    /** @brief Increases the timeline zoom level by 1. */
    void slotZoomOut(bool zoomOnMouse = false);
    /** @brief Makes the timeline zoom level fit the timeline content. */
    void slotFitZoom();
    /** @brief Updates the zoom slider tooltip to fit @param zoomlevel. */
    void slotUpdateZoomSliderToolTip(int zoomlevel);

    /** @brief Displays the zoom slider tooltip.
    * @param zoomlevel (optional) The zoom level to show in the tooltip.
    *
    * Adopted from Dolphin (src/statusbar/dolphinstatusbar.cpp) */
    void slotShowZoomSliderToolTip(int zoomlevel = -1);
    /** @brief Deletes item in timeline, project tree or effect stack depending on focus. */
    void slotDeleteItem();
    void slotAddClipMarker();
    void slotDeleteClipMarker(bool allowGuideDeletion = false);
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
    void slotInsertClipInsert();
    void slotExtractZone();
    void slotLiftZone();
    void slotPreviewRender();
    void slotStopPreviewRender();
    void slotDefinePreviewRender();
    void slotRemovePreviewRender();
    void slotClearPreviewRender();
    void slotSelectTimelineClip();
    void slotSelectTimelineTransition();
    void slotDeselectTimelineClip();
    void slotDeselectTimelineTransition();
    void slotSelectAddTimelineClip();
    void slotSelectAddTimelineTransition();
    void slotAddVideoEffect(QAction *result);
    void slotAddTransition(QAction *result);
    void slotAddProjectClip(const QUrl &url, const QStringList &folderInfo);
    void slotAddProjectClipList(const QList<QUrl> &urls);
    void slotTrackSelected(int index, const TrackInfo &info, bool raise = true);
    void slotActivateTransitionView(Transition *transition);
    void slotChangeTool(QAction *action);
    void slotChangeEdit(QAction *action);
    void slotSetTool(ProjectTool tool);
    void slotSnapForward();
    void slotSnapRewind();
    void slotClipStart();
    void slotClipEnd();
    void slotSelectClipInTimeline();
    void slotClipInTimeline(const QString &clipId);

    void slotInsertSpace();
    void slotRemoveSpace();
    void slotRemoveAllSpace();
    void slotAddGuide();
    void slotEditGuide(int pos = -1, const QString &text = QString());
    void slotDeleteGuide();
    void slotDeleteAllGuides();
    void slotGuidesUpdated();

    void slotCopy();
    void slotPaste();
    void slotPasteEffects();
    void slotResizeItemStart();
    void slotResizeItemEnd();
    void configureNotifications();
    void slotInsertTrack();
    void slotDeleteTrack();
    /** @brief Shows the configure tracks dialog and updates transitions afterwards. */
    void slotConfigTrack();
    /** @brief Select all clips in active track. */
    void slotSelectTrack();
    /** @brief Select all clips in timeline. */
    void slotSelectAllTracks();
    void slotUnselectAllTracks();
    void slotGetNewLumaStuff();
    void slotGetNewTitleStuff();
    void slotGetNewRenderStuff();
    void slotGetNewMltProfileStuff();
    void slotAutoTransition();
    void slotRunWizard();
    void slotZoneMoved(int start, int end);
    void slotDvdWizard(const QString &url = QString());
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
    void slotTranscode(const QStringList &urls = QStringList());
    void slotTranscodeClip();
    /** @brief Archive project: creates a copy of the project file with all clips in a new folder. */
    void slotArchiveProject();
    void slotSetDocumentRenderProfile(const QMap<QString, QString> &props);
    void slotPrepareRendering(bool scriptExport, bool zoneOnly, const QString &chapterFile, QString scriptPath = QString());

    /** @brief Switches between displaying frames or timecode.
    * @param ix 0 = display timecode, 1 = display frames. */
    void slotUpdateTimecodeFormat(int ix);

    /** @brief Removes the focus of anything. */
    void slotRemoveFocus();
    void slotCleanProject();
    void slotShutdown();
    void slotUpdateTrackInfo();

    void slotSwitchMonitors();
    void slotSwitchMonitorOverlay(QAction *);
    void slotSwitchDropFrames(bool drop);
    void slotSetMonitorGamma(int gamma);
    void slotCheckRenderStatus();
    void slotInsertZoneToTree();
    void slotInsertZoneToTimeline();

    /** @brief Update the capture folder if user asked a change. */
    void slotUpdateCaptureFolder();

    /** @brief The monitor informs that it needs (or not) to have frames sent by the renderer. */
    void slotMonitorRequestRenderFrame(bool request);
    /** @brief Open the stopmotion dialog. */
    void slotOpenStopmotion();
    /** @brief Update project because the use of proxy clips was enabled / disabled. */
    void slotUpdateProxySettings();
    /** @brief Disable proxies for this project. */
    void slotDisableProxies();
    /** @brief Open the online services search dialog. */
    void slotDownloadResources();

    /** @brief Process keyframe data sent from a clip to effect / transition stack. */
    void slotProcessImportKeyframes(GraphicsRectItem type, const QString &tag, const QString &data);
    /** @brief Move playhead to mouse curser position if defined key is pressed */
    void slotAlignPlayheadToMousePos();

    void slotThemeChanged(const QString &);
    void slotReloadTheme();
    /** @brief Close Kdenlive and try to restart it */
    void slotRestart();
    void triggerKey(QKeyEvent *ev);
    /** @brief Update monitor overlay actions on monitor switch */
    void slotUpdateMonitorOverlays(int id, int code);
    /** @brief Update widget style */
    void slotChangeStyle(QAction *a);
    /** @brief Create temporary top track to preview an effect */
    void createSplitOverlay(Mlt::Filter *filter);
    void removeSplitOverlay();
    /** @brief Create a generator's setup dialog */
    void buildGenerator(QAction *action);
    void slotCheckTabPosition();
    /** @brief Toggle automatic timeline preview on/off */
    void slotToggleAutoPreview(bool enable);
    /** @brief Rebuild/reload timeline toolbar. */
    void rebuildTimlineToolBar();
    void showTimelineToolbarMenu(const QPoint &pos);
    /** @brief Open Cached Data management dialog. */
    void slotManageCache();
    void showMenuBar(bool show);
    /** @brief Change forced icon theme setting (asks for app restart). */
    void forceIconSet(bool force);
    /** @brief Toggle current project's compositing mode. */
    void slotUpdateCompositing(QAction *compose);
    /** @brief Update compositing action to display current project setting. */
    void slotUpdateCompositeAction(int mode);
    /** @brief Cycle through the different timeline trim modes. */
    void slotSwitchTrimMode();
    void setTrimMode(const QString &mode);
    /** @brief Set timeline toolbar icon size. */
    void setTimelineToolbarIconSize(QAction *a);

signals:
    Q_SCRIPTABLE void abortRenderJob(const QString &url);
    void configurationChanged();
    void GUISetupDone();
    void reloadTheme();
    void setPreviewProgress(int);
    void setRenderProgress(int);
    void displayMessage(const QString &, MessageType, int);
};

#endif
