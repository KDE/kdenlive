/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QComboBox>
#ifndef NODBUS
#include <QDBusAbstractAdaptor>
#endif
#include <QDockWidget>
#include <QEvent>
#include <QImage>
#include <QMap>
#include <QProgressDialog>
#include <QShortcut>
#include <QString>
#include <QUndoView>
#include <QUuid>

#include <KActionCategory>
#include <KAutoSaveFile>
#include <KColorSchemeManager>
#include <KSelectAction>
#include <KXmlGuiWindow>
#include <knewstuff_version.h>
#include <kxmlgui_version.h>
#include <mlt++/Mlt.h>
#include <utility>

#include "bin/bin.h"
#include "definitions.h"
#include "jobs/abstracttask.h"
#include "kdenlive_debug.h"
#include "kdenlivecore_export.h"
#include "pythoninterfaces/otioconvertions.h"
#include "statusbarmessagelabel.h"
#include "utils/gentime.h"

class AssetPanel;
class AudioGraphSpectrum;
class EffectBasket;
class EffectListWidget;
class TransitionListWidget;
class KIconLoader;
class KdenliveDoc;
class Monitor;
class Render;
class RenderWidget;
class TimelineTabs;
class TimelineWidget;
class TimelineContainer;
class Transition;
class TimelineItemModel;
class MonitorProxy;
class KDualAction;

class MltErrorEvent : public QEvent
{
public:
    explicit MltErrorEvent(QString message)
        : QEvent(QEvent::User)
        , m_message(std::move(message))
    {
    }

    QString message() const { return m_message; }

private:
    QString m_message;
};

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
     * If Url is present, it will be opened, otherwise, if openlastproject is
     * set, latest project will be opened. If no file is open after trying this,
     * a default new file will be created. */
    void init(const QString &mltPath);
    ~MainWindow() override;

    /** @brief Cache for luma files thumbnails. */
    static QMap<QString, QImage> m_lumacache;
    static QMap<QString, QStringList> m_lumaFiles;

    /** @brief Adds an action to the action collection and stores the name. */
    void addAction(const QString &name, QAction *action, const QKeySequence &shortcut = QKeySequence(), KActionCategory *category = nullptr);
    /** @brief Same as above, but takes a string for category to populate it with kdenliveCategoryMap */
    void addAction(const QString &name, QAction *action, const QKeySequence &shortcut, const QString &category);
    /** @brief Adds an action to the action collection and stores the name. */
    QAction *addAction(const QString &name, const QString &text, const QObject *receiver, const char *member, const QIcon &icon = QIcon(),
                       const QKeySequence &shortcut = QKeySequence(), KActionCategory *category = nullptr);
    /** @brief Same as above, but takes a string for category to populate it with kdenliveCategoryMap */
    QAction *addAction(const QString &name, const QString &text, const QObject *receiver, const char *member, const QIcon &icon,
                       const QKeySequence &shortcut, const QString &category);

    void processRestoreState(const QByteArray &state);

    /**
     * @brief Adds a new dock widget to this window.
     * @param title title of the dock widget
     * @param objectName objectName of the dock widget (required for storing layouts)
     * @param widget widget to use in the dock
     * @param area area to which the dock should be added to
     * @param shortcut default shortcut to raise the dock
     * @returns the created dock widget
     */
    QDockWidget *addDock(const QString &title, const QString &objectName, QWidget *widget, Qt::DockWidgetArea area = Qt::TopDockWidgetArea);

    QUndoGroup *m_commandStack;
    QUndoView *m_undoView;
    /** @brief holds info about whether movit is available on this system */
    bool m_gpuAllowed;
    int m_exitCode{EXIT_SUCCESS};
    QMap<QString, KActionCategory *> kdenliveCategoryMap;
    QList<QAction *> getExtraActions(const QString &name);
    /** @brief Returns true if docked widget is tabbed with another widget from its object name */
    bool isTabbedWith(QDockWidget *widget, const QString &otherWidget);
    
    /** @brief Returns true if mixer widget is tabbed */
    bool isMixedTabbed() const;

    /** @brief Returns a pointer to the current timeline */
    TimelineWidget *getCurrentTimeline() const;
    /** @brief Returns a pointer to the timeline with @uuid */
    TimelineWidget *getTimeline(const QUuid uuid) const;
    void getSequenceProperties(const QUuid &uuid, QMap<QString, QString> &props);
    void closeTimelineTab(const QUuid uuid);
    /** @brief Returns a list of opened tabs uuids */
    const QStringList openedSequences() const;

    /** @brief Returns true if a timeline widget is available */
    bool hasTimeline() const;
    
    /** @brief Returns true if the timeline widget is visible */
    bool timelineVisible() const;
    
    /** @brief Raise (show) the clip or project monitor */
    void raiseMonitor(bool clipMonitor);

    /** @brief Raise (show) the project bin*/
    void raiseBin();
    /** @brief Add a bin widget*/
    void addBin(Bin *bin, const QString &binName = QString());
    /** @brief Get the main (first) bin*/
    Bin *getBin();
    /** @brief Block/Unblock all bin selection signals*/
    void blockBins(bool block);
    /** @brief Get the active (focused) bin or first one if none is active*/
    Bin *activeBin();
    /** @brief Ensure all bin widgets are tabbed together*/
    void tabifyBins();
    int binCount() const;

    /** @brief Hide subtitle track and delete its temporary file*/
    void resetSubtitles(const QUuid &uuid);

    /** @brief Restart the application and delete config files if clean is true */
    void cleanRestart(bool clean);

    /** @brief Show current tool key combination in status bar */
    void showToolMessage();
    /** @brief Show the widget's default key binding message */
    void setWidgetKeyBinding(const QString &text = QString());
    /** @brief Show a key binding in status bar */
    void showKeyBinding(const QString &text = QString());
    /** @brief Disable multicam mode if it was active */
    void disableMulticam();

#if KNEWSTUFF_VERSION < QT_VERSION_CHECK(5, 98, 0)
    /** @brief Instantiates a "Get Hot New Stuff" dialog.
     * @param configFile configuration file for KNewStuff
     * @return number of installed items */
    int getNewStuff(const QString &configFile);
#endif

    /** @brief Check if the maximum cached data size is not exceeded. */
    void checkMaxCacheSize();
    TimelineWidget *openTimeline(const QUuid &uuid, const QString &tabName, std::shared_ptr<TimelineItemModel> timelineModel);
    /** @brief Bring a timeline tab in front. Returns false if no tab exists for this timeline. */
    bool raiseTimeline(const QUuid &uuid);
    void connectTimeline();
    void disconnectTimeline(TimelineWidget *timeline);
    static QProcessEnvironment getCleanEnvironement();

protected:
    /** @brief Closes the window.
     * @return false if the user presses "Cancel" on a confirmation dialog or
     *     the operation requested (starting waiting jobs or saving file) fails,
     *     true otherwise */
    bool queryClose() override;
    void closeEvent(QCloseEvent *) override;
    bool eventFilter(QObject *object, QEvent *event) override;

    /** @brief Reports a message in the status bar when an error occurs. */
    void customEvent(QEvent *e) override;

    /** @brief Stops the active monitor when the window gets hidden. */
    void hideEvent(QHideEvent *e) override;

    /** @brief Saves the file and the window properties when saving the session. */
    void saveProperties(KConfigGroup &config) override;

    void saveNewToolbarConfig() override;

private:
    /** @brief Sets up all the actions and attaches them to the collection. */
    void setupActions();
    /** @brief Rebuild the dock menu according to existing dock widgets. */
    void updateDockMenu();

    OtioConvertions m_otioConvertions;
    KColorSchemeManager *m_colorschemes;

    QDockWidget *m_projectBinDock;
    QDockWidget *m_effectListDock;
    QDockWidget *m_compositionListDock;
    TransitionListWidget *m_compositionList;
    EffectListWidget *m_effectList2;

    AssetPanel *m_assetPanel{nullptr};
    QDockWidget *m_effectStackDock;

    QDockWidget *m_clipMonitorDock;
    Monitor *m_clipMonitor{nullptr};

    QDockWidget *m_projectMonitorDock;
    Monitor *m_projectMonitor{nullptr};

    AudioGraphSpectrum *m_audioSpectrum;

    QDockWidget *m_undoViewDock;
    QDockWidget *m_mixerDock;
    QDockWidget *m_onlineResourcesDock;

    KSelectAction *m_timeFormatButton;
    QAction *m_compositeAction;

    TimelineTabs *m_timelineTabs{nullptr};
    QVector <Bin*>m_binWidgets;

    /** This list holds all the scopes used in Kdenlive, allowing to manage some global settings */
    QList<QDockWidget *> m_gfxScopesList;

    KActionCategory *m_effectActions;
    KActionCategory *m_transitionActions;
    QMenu *m_effectsMenu;
    QMenu *m_transitionsMenu;
    QMenu *m_timelineContextMenu;
    QList<QAction *> m_timelineClipActions;
    KDualAction *m_useTimelineZone;

    /** Action names that can be used in the slotDoAction() slot, with their i18n() names */
    QStringList m_actionNames;

    /** @brief Shortcut to remove the focus from any element.
     *
     * It allows one to get out of e.g. text input fields and to press another
     * shortcut. */
    QShortcut *m_shortcutRemoveFocus;

    RenderWidget *m_renderWidget{nullptr};
    StatusBarMessageLabel *m_messageLabel{nullptr};
    QList<QAction *> m_transitions;
    QAction *m_buttonAudioThumbs;
    QAction *m_buttonVideoThumbs;
    QAction *m_buttonShowMarkers;
    QAction *m_buttonFitZoom;
    QAction *m_buttonTimelineTags;
    QAction *m_normalEditTool;
    QAction *m_overwriteEditTool;
    QAction *m_insertEditTool;
    QAction *m_buttonSelectTool;
    QAction *m_buttonRazorTool;
    QAction *m_buttonSpacerTool;
    QAction *m_buttonRippleTool;
    QAction *m_buttonRollTool;
    QAction *m_buttonSlipTool;
    QAction *m_buttonSlideTool;
    QAction *m_buttonMulticamTool;
    QAction *m_buttonSnap;
    QAction *m_saveAction;
    QSlider *m_zoomSlider;
    QAction *m_zoomIn;
    QAction *m_zoomOut;
    QAction *m_loopZone;
    QAction *m_playZone;
    QAction *m_loopClip;
    QAction *m_proxyClip;
    QAction *m_buttonSubtitleEditTool;
    QString m_theme;
    KIconLoader *m_iconLoader;
    KToolBar *m_timelineToolBar;
    TimelineContainer *m_timelineToolBarContainer;
    QLabel *m_trimLabel;
    QActionGroup *m_scaleGroup;
    ToolType::ProjectTool m_activeTool;
    /** @brief Store latest mouse position in timeline. */
    int m_mousePosition;

    KHamburgerMenu *m_hamburgerMenu;

    /** @brief initialize startup values, return true if first run. */
    bool readOptions();
    void saveOptions();

    QStringList m_pluginFileNames;
    QByteArray m_timelineState;
    void buildDynamicActions();
    void loadClipActions();
    void loadContainerActions();

    QTime m_timer;
    KXMLGUIClient *m_extraFactory;
    bool m_themeInitialized{false};
    bool m_isDarkTheme{false};
    EffectBasket *m_effectBasket;
    /** @brief Update widget style. */
    void doChangeStyle();

    QProgressDialog *m_loadingDialog;

public Q_SLOTS:
    void slotReloadEffects(const QStringList &paths);
    Q_SCRIPTABLE void setRenderingProgress(const QString &url, int progress, int frame);
    Q_SCRIPTABLE void setRenderingFinished(const QString &url, int status, const QString &error);
    Q_SCRIPTABLE void addProjectClip(const QString &url, const QString & folder = QStringLiteral("-1"));
    Q_SCRIPTABLE void addTimelineClip(const QString &url);
    Q_SCRIPTABLE void addEffect(const QString &effectId);
    Q_SCRIPTABLE void scriptRender(const QString &url);
#ifndef NODBUS
    Q_NOREPLY void exitApp();
#endif

    void slotSwitchVideoThumbs();
    void slotSwitchAudioThumbs();
    void appHelpActivated();

    void slotPreferences();
    void slotShowPreferencePage(Kdenlive::ConfigPage page, int option = -1);
    void connectDocument();
    /** @brief Reload project profile in config dialog if changed. */
    void slotRefreshProfiles();
    /** @brief Decreases the timeline zoom level by 1. */
    void slotZoomIn(bool zoomOnMouse = false);
    /** @brief Increases the timeline zoom level by 1. */
    void slotZoomOut(bool zoomOnMouse = false);
    /** @brief Enable or disable the use of timeline zone for edits. */
    void slotSwitchTimelineZone(bool toggled);
    /** @brief Open the online services search dialog. */
    void slotDownloadResources();
    /** @brief Initialize the subtitle model on project load. */
    void slotInitSubtitle(const QMap<QString, QString> &subProperties, const QUuid &uuid);
    /** @brief Display the subtitle track and initialize subtitleModel if necessary. */
    void slotEditSubtitle(const QMap<QString, QString> &subProperties = {});
    /** @brief Show/hide subtitle track. */
    void slotShowSubtitles(bool show);
    void slotTranscode(const QStringList &urls = QStringList());
    /** @brief Open the transcode to edit friendly format dialog. */
    void slotFriendlyTranscode(const QString &binId, bool checkProfile);
    /** @brief Add subtitle clip to timeline */
    void slotAddSubtitle(const QString &text = QString());
    /** @brief Ensure subtitle track is displayed */
    void showSubtitleTrack();
    /** @brief The path of the current document changed (save as), update render settings */
    void updateProjectPath(const QString &path);
    /** @brief Update compositing action to display current project setting. */
    void slotUpdateCompositeAction(bool enable);
    /** @brief Update duration of project in timeline toolbar. */
    void slotUpdateProjectDuration(int pos);
    /** @brief The current timeline selection zone changed... */
    void slotUpdateZoneDuration(int duration);
    /** @brief Remove all unused clips from the project. */
    void slotCleanProject();
    void slotEditProjectSettings(int ix = 0);
    /** @brief Sets the timeline zoom slider to @param value.
     *
     * Also disables zoomIn and zoomOut actions if they cannot be used at the moment. */
    void slotSetZoom(int value, bool zoomOnMouse = false);
    /** @brief if modified is true adds "modified" to the caption and enables the save button.
     * (triggered by KdenliveDoc::setModified()) */
    void slotUpdateDocumentState(bool modified);
    /** @brief Open the clip job management dialog */
    void manageClipJobs(AbstractTask::JOBTYPE type = AbstractTask::NOJOBTYPE, QWidget *parentWidget = nullptr);
    /** @brief Deletes item in timeline, project tree or effect stack depending on focus. */
    void slotDeleteItem();
    /** @brief Export a subtitle file */
    void slotExportSubtitle();
    /** @brief Display current mouse pos */
    void slotUpdateMousePosition(int pos, int duration = -1);

private Q_SLOTS:
    /** @brief Shows the shortcut dialog. */
    void slotEditKeys();
    void loadDockActions();
    /** @brief Reflects setting changes to the GUI. */
    void updateConfiguration();
    void slotConnectMonitors();
    void slotSwitchMarkersComments();
    void slotSwitchSnap();
    void slotShowTimelineTags();
    void slotRenderProject();
    void slotStopRenderProject();
    void slotFullScreen();

    /** @brief Makes the timeline zoom level fit the timeline content. */
    void slotFitZoom();
    /** @brief Updates the zoom slider tooltip to fit @param zoomlevel. */
    void slotUpdateZoomSliderToolTip(int zoomlevel);
    /** @brief Timeline was zoom, update slider to reflect that */
    void updateZoomSlider(int value);

    /** @brief Displays the zoom slider tooltip.
     * @param zoomlevel (optional) The zoom level to show in the tooltip.
     *
     * Adopted from Dolphin (src/statusbar/dolphinstatusbar.cpp) */
    void slotShowZoomSliderToolTip(int zoomlevel = -1);
    void slotAddClipMarker();
    void slotDeleteClipMarker(bool allowGuideDeletion = false);
    void slotDeleteAllClipMarkers();
    void slotEditClipMarker();

    /** @brief Adds marker or guide at the current position without showing the marker dialog.
     *
     * Adds a marker if clip monitor is active, otherwise a guide.
     * The comment is set to the current position (therefore not dialog).
     * This can be useful to mark something during playback. */
    void slotAddMarkerGuideQuickly();
    void slotCutTimelineClip();
    void slotCutTimelineAllClips();
    void slotInsertClipOverwrite();
    void slotInsertClipInsert();
    void slotExtractZone();
    void slotLiftZone();
    void slotPreviewRender();
    void slotStopPreviewRender();
    void slotDefinePreviewRender();
    void slotRemovePreviewRender();
    void slotClearPreviewRender(bool resetZones = true);
    void slotSelectTimelineClip();
    void slotSelectTimelineZone();
    void slotSelectTimelineTransition();
    void slotDeselectTimelineClip();
    void slotDeselectTimelineTransition();
    void slotSelectAddTimelineClip();
    void slotSelectAddTimelineTransition();
    void slotAddEffect(QAction *result);
    void slotAddTransition(QAction *result);
    void slotAddProjectClip(const QUrl &url, const QString &folderInfo);
    void slotAddTextNote(const QString &text);
    void slotAddProjectClipList(const QList<QUrl> &urls);
    void slotChangeTool(QAction *action);
    void slotChangeEdit(QAction *action);
    void slotSetTool(ToolType::ProjectTool tool);
    void slotSnapForward();
    void slotSnapRewind();
    void slotGuideForward();
    void slotGuideRewind();
    void slotClipStart();
    void slotClipEnd();
    void slotSelectClipInTimeline();
    void slotClipInTimeline(const QString &clipId, const QList<int> &ids);

    void slotInsertSpace();
    void slotRemoveSpace();
    void slotRemoveSpaceInAllTracks();
    void slotRemoveAllSpacesInTrack();
    void slotRemoveAllClipsInTrack();
    void slotAddGuide();
    void slotEditGuide();
    void slotExportGuides();
    void slotLockGuides(bool lock);
    void slotDeleteGuide();
    void slotDeleteAllGuides();

    void slotCopy();
    void slotPaste();
    void slotPasteEffects();
    void slotResizeItemStart();
    void slotResizeItemEnd();
    void configureNotifications();
    void slotSeparateAudioChannel();
    /** @brief Normalize audio channels before displaying them */
    void slotNormalizeAudioChannel();
    /** @brief Toggle automatic fit track height */
    void slotAutoTrackHeight(bool enable);
    void slotInsertTrack();
    void slotDeleteTrack();
    /** @brief Show context menu to switch current track target audio stream. */
    void slotSwitchTrackAudioStream();
    void slotShowTrackRec(bool checked);
    /** @brief Select all clips in active track. */
    void slotSelectTrack();
    /** @brief Select all clips in timeline. */
    void slotSelectAllTracks();
    void slotUnselectAllTracks();
#if KXMLGUI_VERSION < QT_VERSION_CHECK(5, 98, 0)
    void slotGetNewKeyboardStuff(QComboBox *schemesList);
#endif
    void slotAutoTransition();
    void slotRunWizard();
    void slotGroupClips();
    void slotUnGroupClips();
    void slotEditItemDuration();
    void slotClipInProjectTree();
    // void slotClipToProjectTree();
    void slotSplitAV();
    void slotSwitchClip();
    void slotSetAudioAlignReference();
    void slotAlignAudio();
    void slotUpdateTimelineView(QAction *action);
    void slotShowTimeline(bool show);
    void slotTranscodeClip();
    /** @brief Archive project: creates a copy of the project file with all clips in a new folder. */
    void slotArchiveProject();
    void slotSetDocumentRenderProfile(const QMap<QString, QString> &props);

    /** @brief Switches between displaying frames or timecode.
     * @param ix 0 = display timecode, 1 = display frames. */
    void slotUpdateTimecodeFormat(int ix);

    /** @brief Removes the focus of anything. */
    void slotRemoveFocus();
    void slotShutdown();

    void slotSwitchMonitors();
    void slotSwitchMonitorOverlay(QAction *);
    void slotSwitchDropFrames(bool drop);
    void slotSetMonitorGamma(int gamma);
    void slotCheckRenderStatus();
    void slotInsertZoneToTree();
    /** @brief Focus the timecode widget of current monitor. */
    void slotFocusTimecode();

    /** @brief The monitor informs that it needs (or not) to have frames sent by the renderer. */
    void slotMonitorRequestRenderFrame(bool request);
    /** @brief Update project because the use of proxy clips was enabled / disabled. */
    void slotUpdateProxySettings();
    /** @brief Disable proxies for this project. */
    void slotDisableProxies();

    /** @brief Process keyframe data sent from a clip to effect / transition stack. */
    void slotProcessImportKeyframes(GraphicsRectItem type, const QString &tag, const QString &keyframes);
    /** @brief Move playhead to mouse cursor position if defined key is pressed */
    void slotAlignPlayheadToMousePos();

    void slotThemeChanged(const QString &name);
    /** @brief Close Kdenlive and try to restart it */
    void slotRestart(bool clean = false);
    void triggerKey(QKeyEvent *ev);
    /** @brief Update monitor overlay actions on monitor switch */
    void slotUpdateMonitorOverlays(int id, int code);
    /** @brief Update widget style */
    void slotChangeStyle(QAction *a);
    /** @brief Create temporary top track to preview an effect */
    void createSplitOverlay(std::shared_ptr<Mlt::Filter> filter);
    void removeSplitOverlay();
    /** @brief Create a generator's setup dialog */
    void buildGenerator(QAction *action);
    void slotCheckTabPosition();
    /** @brief Toggle automatic timeline preview on/off */
    void slotToggleAutoPreview(bool enable);
    void showTimelineToolbarMenu(const QPoint &pos);
    /** @brief Open Cached Data management dialog. */
    void slotManageCache();
    void showMenuBar(bool show);
    /** @brief Change forced icon theme setting (asks for app restart). */
    void forceIconSet(bool force);
    /** @brief Toggle current project's compositing mode. */
    void slotUpdateCompositing(bool checked);
    /** @brief Set timeline toolbar icon size. */
    void setTimelineToolbarIconSize(QAction *a);
    void slotEditItemSpeed();
    void slotRemapItemTime();
    /** @brief Request adjust of timeline track height */
    void resetTimelineTracks();
    /** @brief Set keyboard grabbing on current timeline item */
    void slotGrabItem();
    /** @brief Collapse or expand current item (depending on focused widget: effet, track)*/
    void slotCollapse();
    /** @brief Save currently selected timeline clip as bin subclip*/
    void slotExtractClip();
    /** @brief Save currently selected timeline clip as bin subclip*/
    void slotSaveZoneToBin();
    /** @brief Expand current timeline clip (recover clips and tracks from an MLT playlist) */
    void slotExpandClip();
    /** @brief Focus and activate an audio track from a shortcut sequence */
    void slotActivateAudioTrackSequence();
    /** @brief Focus and activate a video track from a shortcut sequence */
    void slotActivateVideoTrackSequence();
    /** @brief Select target for current track */
    void slotActivateTarget();
    /** @brief Enable/disable subtitle track */
    void slotDisableSubtitle();
    /** @brief Lock / unlock subtitle track */
    void slotLockSubtitle();
    /** @brief Import a subtitle file */
    void slotImportSubtitle();
    /** @brief Display the subtitle manager widget */
    void slotManageSubtitle();
    /** @brief Start a speech recognition on timeline zone */
    void slotSpeechRecognition();
    /** @brief Copy debug information like lib versions, gpu mode state,... to clipboard */
    void slotCopyDebugInfo();
    void slotRemoveBinDock(const QString &name);
    /** @brief Focus the guides list search line */
    void slotSearchGuide();
    /** @brief Copy current timeline selection to a new sequence clip / Timeline tab */
    void slotCreateSequenceFromSelection();

Q_SIGNALS:
    Q_SCRIPTABLE void abortRenderJob(const QString &url);
    void configurationChanged();
    void GUISetupDone();
    void setPreviewProgress(int);
    void setRenderProgress(int);
    void displayMessage(const QString &, MessageType, int);
    void displaySelectionMessage(const QString &);
    void displayProgressMessage(const QString &, MessageType, int, bool canBeStopped = false);
    /** @brief Project profile changed, update render widget accordingly. */
    void updateRenderWidgetProfile();
    /** @brief Clear asset view if itemId is displayed. */
    void clearAssetPanel(int itemId = -1);
    void assetPanelWarning(const QString service, const QString id, const QString message);
    void adjustAssetPanelRange(int itemId, int in, int out);
    /** @brief Enable or disable the undo stack. For example undo/redo should not be enabled when dragging a clip in timeline or we risk corruption. */
    void enableUndo(bool enable);
    bool showTimelineFocus(bool focus, bool highlight);
    void removeBinDock(const QString &name);
};
