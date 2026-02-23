/*
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "jobs/taskmanager.h"
#include "kdenlivecore_export.h"
#include "layouts/layoutinfo.h"
#include "undohelper.hpp"
#include "utils/timecode.h"

#include <KSharedDataCache>

#include <QColor>
#include <QMutex>
#include <QObject>
#include <QPoint>
#include <QTextEdit>
#include <QThreadPool>
#include <QUrl>

#include <memory>
#include <unordered_set>

#include <mlt++/MltPlaylist.h>
#include <mlt++/MltProfile.h>

class Bin;
class DocUndoStack;
class EffectStackModel;
class KdenliveDoc;
class LibraryWidget;
class MainWindow;
class MediaCapture;
class MediaBrowser;
class MixerManager;
class Monitor;
class MonitorManager;
class ProfileModel;
class ProjectItemModel;
class ProjectManager;
class Splash;
class SubtitleEdit;
class SubtitleModel;
class TextBasedEdit;
class GuidesList;
class KeyframeModelList;
class TimeRemap;

namespace Mlt {
class Repository;
class Producer;
} // namespace Mlt

#define EXIT_RESTART (42)
#define EXIT_CLEAN_RESTART (43)
#define pCore Core::self()

/**
 * @class Core
 * @brief Singleton that provides access to the different parts of Kdenlive
 *
 * Needs to be initialize before any widgets are created in MainWindow.
 * Plugins should be loaded after the widget setup.
 */
class /*KDENLIVECORE_EXPORT*/ Core : public QObject
{
    Q_OBJECT

public:
    friend class KdenliveDoc;
    friend class ProjectManager;
    Core(const Core &) = delete;
    Core &operator=(const Core &) = delete;
    Core(Core &&) = delete;
    Core &operator=(Core &&) = delete;
    QReadWriteLock xmlMutex;
    bool closing{false};
    QString lastActiveBin;
    bool debugMode{false};

    ~Core() override;

    /**
     * @brief Setup the basics of the application, in particular the connection
     * with Mlt
     * @param MltPath (optional) path to MLT environment
     */
    static bool build(LinuxPackageType packageType, bool testMode = false, bool debugMode = false, bool showWelcome = true);

    void initHeadless(const QUrl &url);

    /**
     * @brief Init the GUI part of the app and show the main window
     * @param inSandbox does the app run in a sanbox? If yes, we use App path to deduce
     * other binaries paths (melt, ffmpeg, etc)
     * @param MltPath
     * @param Url (optional) file to open
     * If Url is present, it will be opened, otherwise, if openlastproject is
     * set, latest project will be opened. If no file is open after trying this,
     * a default new file will be created.
     * @param clipsToLoad
     */
    void initGUI(const QString &MltPath, const QUrl &Url, const QStringList &clipsToLoad = {});

    /** @brief Returns a pointer to the singleton object. */
    static std::unique_ptr<Core> &self();

    /** @brief Delete the global core instance */
    static void clean();

    /** @brief Returns a pointer to the main window. */
    MainWindow *window();

    /** @brief Open a file using an external app. */
    QString openExternalApp(QString appPath, const QStringList args);

    /** @brief Returns a pointer to the project manager. */
    ProjectManager *projectManager();
    /** @brief Returns a pointer to the current project. */
    KdenliveDoc *currentDoc();
    /** @brief Returns project's timecode. */
    Timecode timecode() const;
    /** @brief Returns a pointer to the monitor manager. */
    MonitorManager *monitorManager();
    /** @brief Returns a pointer to the media browser widget. */
    MediaBrowser *mediaBrowser();
    /** @brief Returns a pointer to the view of the project bin. */
    Bin *bin();
    /** @brief Returns a pointer to the view of the active bin (or main bin on no focus). */
    Bin *activeBin();
    /** @brief Select a clip in the Bin from its id. */
    void selectBinClip(const QString &id, bool activateMonitor = true, int frame = -1, const QPoint &zone = QPoint());
    /** @brief Selects an item in the current timeline (clip, composition, subtitle). */
    void selectTimelineItem(int id);
    /** @brief Returns a pointer to the model of the project bin. */
    std::shared_ptr<ProjectItemModel> projectItemModel();
    /** @brief Returns a pointer to the library. */
    LibraryWidget *library();
    /** @brief Returns a pointer to the subtitle edit. */
    SubtitleEdit *subtitleWidget();
    /** @brief Returns a pointer to the text based editing widget. */
    TextBasedEdit *textEditWidget();
    /** @brief Returns a pointer to the guides list widget. */
    GuidesList *guidesList();
    /** @brief Returns a pointer to the time remapping widget. */
    TimeRemap *timeRemapWidget();
    /** @brief Returns true if clip displayed in remap widget is the bin clip with id clipId. */
    bool currentRemap(const QString &clipId);
    /** @brief Returns a pointer to the audio mixer. */
    MixerManager *mixer();
    ToolType::ProjectTool activeTool();

    /** @brief Returns a pointer to MLT's repository */
    std::unique_ptr<Mlt::Repository> &getMltRepository();

    /** @brief Returns a pointer to the current profile */
    std::unique_ptr<ProfileModel> &getCurrentProfile() const;
    const QString &getCurrentProfilePath() const;

    /** @brief Define the active profile
     *  @returns true if profile exists, false if not found
     */
    bool setCurrentProfile(const QString profilePath);
    /** @brief Returns Sample Aspect Ratio of current profile */
    double getCurrentSar() const;
    /** @brief Returns Display Aspect Ratio of current profile */
    double getCurrentDar() const;

    /** @brief Returns frame rate of current profile */
    double getCurrentFps() const;

    /** @brief Returns the frame size (width x height) of current profile */
    const QSize getCurrentFrameSize() const;
    /** @brief Returns the frame display size (width x height) of current profile */
    const QSize getCurrentFrameDisplaySize() const;
    /** @brief Request project monitor refresh, and restore previously active monitor */
    void refreshProjectMonitorOnce(bool slowUpdate = false);
    /** @brief Request project monitor refresh if current position is inside range*/
    void refreshProjectRange(QPair<int, int> range);
    /** @brief Request project monitor refresh if referenced item is under cursor */
    void refreshProjectItem(const ObjectId &id);
    /** @brief Returns true if position pos is inside the item */
    bool itemContainsPos(const ObjectId &id, int pos);
    /** @brief Returns a reference to a monitor (clip or project monitor) */
    Monitor *getMonitor(int id);
    /** @brief Seek a monitor to position */
    void seekMonitor(int id, int position);
    void setMonitorZone(int id, QPoint zone);
    /** @brief Returns timeline's active track info (position and tag) */
    QPair<int, QString> currentTrackInfo() const;
    /** @brief This function must be called whenever the profile used changes */
    void profileChanged();

    /** @brief Create and push and undo object based on the corresponding functions
        Note that if you class permits and requires it, you should use the macro PUSH_UNDO instead*/
    void pushUndo(const Fun &undo, const Fun &redo, const QString &text);
    void pushUndo(QUndoCommand *command);
    /** @brief display timeline selection info in statusbar */
    void displaySelectionMessage(const QString &message);
    /** @brief Clear asset view if itemId is displayed. */
    void clearAssetPanel(int itemId);
    /** @brief Returns the effectstack of a given bin clip. */
    std::shared_ptr<EffectStackModel> getItemEffectStack(const QUuid &uuid, int itemType, int itemId);
    int getItemPosition(const ObjectId &id);
    /** @brief Get item in point. */
    int getItemIn(const ObjectId &id);
    int getItemTrack(const ObjectId &id);
    int getItemDuration(const ObjectId &id);
    QSize getItemFrameSize(const ObjectId &id);
    /** @brief Returns the capabilities of a clip: AudioOnly, VideoOnly or Disabled if both are allowed, and its type */
    std::pair<PlaylistState::ClipState, ClipType::ProducerType> getItemState(const ObjectId &id);
    /** @brief Get a list of video track names with indexes */
    QMap<int, QString> getTrackNames(bool videoOnly);
    /** @brief Returns the composition A track (MLT index / Track id) */
    QPair<int, int> getCompositionATrack(int cid) const;
    void setCompositionATrack(int cid, int aTrack);
    /** @brief Return true if composition's a_track is automatic (no forced track)
     */
    bool compositionAutoTrack(int cid) const;
    std::shared_ptr<DocUndoStack> undoStack();
    double getClipSpeed(ObjectId id) const;
    /** @brief Mark an item as invalid for timeline preview */
    void invalidateItem(ObjectId itemId);
    void invalidateRange(QPair<int, int> range);
    /** @brief Mark an item audio as invalid for timeline preview */
    void invalidateAudio(ObjectId itemId);
    /** @brief Mark an audio range as invalid for timeline preview */
    void invalidateAudioRange(const QUuid &uuid, int in, int out);
    void prepareShutdown();
    void finishShutdown();
    /** the keyframe model changed (effect added, deleted, active effect changed), inform timeline */
    void updateItemKeyframes(ObjectId id);
    /** A fade for clip id changed, update timeline */
    void updateItemModel(ObjectId id, const QString &service, const QString &updatedParam);
    /** Show / hide keyframes for a timeline clip */
    void showClipKeyframes(ObjectId id, bool enable);
    Mlt::Profile &thumbProfile();
    /** @brief Returns the current project duration */
    int projectDuration() const;
    /** @brief Returns true if current project has some rendered timeline preview  */
    bool hasTimelinePreview() const;
    /** @brief Returns monitor position  */
    int getMonitorPosition(Kdenlive::MonitorId id = Kdenlive::ProjectMonitor) const;
    /** @brief Handles audio and video capture **/
    void startMediaCapture(const QUuid &uuid, int tid, bool, bool);
    void stopMediaCapture(int tid, bool, bool);
    QStringList getAudioCaptureDevices();
    int getMediaCaptureState();
    /** @brief True if we are currently monitoring audio */
    bool isMediaMonitoring() const;
    /** @brief True if we are currently capturing */
    bool isMediaCapturing() const;
    /** @brief True if we are currently displaying the record countdown */
    bool captureShowsCountDown() const;
    std::shared_ptr<MediaCapture> getAudioDevice();
    /** @brief Returns configured folder for audio capture storage */
    QString getProjectCaptureFolderName();
    /** @brief Returns a timeline clip's bin id */
    const QString getTimelineClipBinId(ObjectId id);
    /** @brief Returns all track ids in timeline */
    std::unordered_set<QString> getAllTimelineTracksId();
    /** @brief Returns a frame duration from a timecode */
    int getDurationFromString(const QString &time);
    /** @brief An error occurred within a filter, inform user */
    void processInvalidFilter(const QString &service, const QString &message, const QString &log = QString());
    /** @brief Update current project's tags */
    void updateProjectTags(int previousCount, const QMap<int, QStringList> &tags);
    /** @brief Returns the project profile */
    Mlt::Profile &getProjectProfile();
    /** @brief Returns the project profile frame rate num/den */
    std::pair<int, int> getProjectFpsInfo() const;
    /** @brief Returns the consumer profile, that will be scaled
     *  according to preview settings. Should only be used on the consumer */
    Mlt::Profile &getMonitorProfile();
    /** @brief Returns a copy of current timeline's master playlist */
    std::unique_ptr<Mlt::Producer> getMasterProducerInstance();
    /** @brief Returns a copy of a track's playlist */
    std::unique_ptr<Mlt::Producer> getTrackProducerInstance(int tid);
    /** @brief Returns the undo stack index (position). */
    int undoIndex() const;
    /** @brief Enable / disable monitor multitrack view. Returns false if multitrack was not previously enabled */
    bool enableMultiTrack(bool enable);
    /** @brief Returns number of audio channels for this project. */
    int audioChannels();
    /** @brief Add guides in the project. */
    void addGuides(const QMap<QUuid, QMap<int, QString>> &guides);
    /** @brief Temporarily un/plug a list of clips in timeline. */
    void temporaryUnplug(const QList<int> &clipIds, bool hide);
    /** @brief Transcode a video file. */
    void transcodeFile(const QString &url);
    /** @brief Display key binding info in statusbar. */
    void setWidgetKeyBinding(const QString &mess = QString());
    KSharedDataCache audioThumbCache;
    /* @brief The thread job pool for clip jobs, allowing to set a max number of concurrent jobs */
    TaskManager taskManager;
    /** @brief The number of clip load jobs changed */
    void loadingClips(int, bool allowInterrupt = false);
    /** @brief Resize current mix item */
    void resizeMix(const ObjectId id, int duration, MixAlignment align, int leftFrames = -1);
    /** @brief Get Mix cut pos (the duration of the mix on the right clip) */
    int getMixCutPos(const ObjectId &) const;
    /** @brief Get alignment info for a mix item */
    MixAlignment getMixAlign(const ObjectId &) const;
    /** @brief Closing current document, do some cleanup */
    void cleanup();
    /** @brief Clear clip reference in timeremap widget */
    void clearTimeRemap();
    /** @brief Create the dock widgets */
    void buildDocks();
    /** @brief Restore the default app layout */
    void restoreLayout();
    /** @brief Get the frame size of the clip above a composition */
    const QSize getCompositionSizeOnTrack(const ObjectId &id);
    void loadTimelinePreview(const QUuid uuid, const QString &chunks, const QString &dirty, bool enablePreview, Mlt::Playlist &playlist);
    /** @brief Returns true if the audio mixer widget is visible */
    bool audioMixerVisible{false};
    LinuxPackageType packageType() { return m_packageType; };
    /** @brief Start / stop audio capture */
    void switchCapture();
    /** @brief Get the uuid of currently active timeline */
    const QUuid currentTimelineId() const;
    /** @brief Update a sequence AV info (has audio/video) */
    void updateSequenceAVType(const QUuid &uuid, int tracksCount);
    /** @brief If a clip is grouped, return the count of items using the same effect */
    int getAssetGroupedInstance(const ObjectId &id, const QString &assetId);
    /** @brief Apply an effect command on all clips in a group */
    void groupAssetCommand(const ObjectId &id, const QString &assetId, const QModelIndex &index, const QString &previousValue, QString value,
                           QUndoCommand *command);
    void groupAssetKeyframeCommand(const ObjectId &id, const QString &assetId, const QModelIndex &index, GenTime pos, const QVariant &previousValue,
                                   const QVariant &value, int ix, QUndoCommand *command);
    void groupAssetMultiKeyframeCommand(const ObjectId &id, const QString &assetId, const QList<QModelIndex> &indexes, GenTime pos,
                                        const QStringList &sourceValues, const QStringList &values, QUndoCommand *command);
    /** @brief Return all similar keyframe models from the selection */
    QList<std::shared_ptr<KeyframeModelList>> getGroupKeyframeModels(const ObjectId &id, const QString &assetId);
    /** @brief Remove all effect instances in a group */
    void removeGroupEffect(const ObjectId &id, const QString &assetId, int originalId);
    /** @brief Disable/enable all instance of an effect in a group */
    void applyEffectDisableToGroup(const ObjectId &id, const QString &assetId, bool disable, Fun &undo, Fun &redo);
    /** @brief Returns true if all ui elements have been initialized */
    bool guiReady() const;
    /** @brief A list of markers type categories {marker type, {color, category name}} */
    struct MarkerCategory
    {
        QColor color;
        QString displayName;
    };
    QMap<int, MarkerCategory> markerTypes;
    void folderRenamed(const QString &binId, const QString &folderName);
    /** @brief Open a file in a file manager, workaround needed for Flatpak crash https://bugs.kde.org/show_bug.cgi?id=486494 */
    void highlightFileInExplorer(QList<QUrl> urls);
    /** @brief When a timeline operation changes the clip under the mouse cursor, inform the qml view */
    void updateHoverItem(const QUuid &uuid);
    /** @brief Returns true if the asset has {audio, video} */
    std::pair<bool, bool> assetHasAV(ObjectId id);
    /** @brief Show an item's effect stack */
    void showEffectStackFromId(ObjectId owner);
    /** @brief Get the Bin id and position offset for the selected timeline clip */
    std::pair<QString, int> getSelectedClipAndOffset();
    /** @brief Get the current offset for active timeline */
    int currentTimelineOffset();
    /** HW decoder changed */
    void updateHwDecoding();
    /** Close the application */
    void closeApp();

    void startHideBarsTimer();
    void updateHideBarsTimer(bool inhibit);
    /** @brief This is the producer that serves as a placeholder while a clip is being loaded. It is created in Core at startup */
    std::unique_ptr<Mlt::Producer> mediaUnavailable;
    /** @brief Returns true if the project uses a vertical profile */
    bool isVertical() const;
    /** @brief Returns a list of luma files compatible with current project profile */
    const QStringList getLumasForProfile();

private:
    explicit Core(LinuxPackageType packageType, bool debugMode = false);
    static std::unique_ptr<Core> m_self;

    /** @brief Makes sure Qt's locale and system locale settings match. */
    void initLocale();

    MainWindow *m_mainWindow{nullptr};
    ProjectManager *m_projectManager{nullptr};
    MonitorManager *m_monitorManager{nullptr};
    std::shared_ptr<ProjectItemModel> m_projectItemModel;
    LibraryWidget *m_library{nullptr};
    SubtitleEdit *m_subtitleWidget{nullptr};
    TextBasedEdit *m_textEditWidget{nullptr};
    GuidesList *m_guidesList{nullptr};
    TimeRemap *m_timeRemapWidget{nullptr};
    MixerManager *m_mixerWidget{nullptr};
    MediaBrowser *m_mediaBrowser{nullptr};

    /** @brief Current project's profile path */
    QString m_currentProfile;

    QString m_profile;
    LinuxPackageType m_packageType;
    Timecode m_timecode;
    Mlt::Profile m_thumbProfile;
    /** @brief Mlt profile used in the consumer 's monitors */
    Mlt::Profile m_monitorProfile;
    /** @brief Mlt profile used to build the project's clips */
    Mlt::Profile m_projectProfile;
    bool m_guiConstructed{false};
    bool m_abortInitAndRestart{false};
    Splash *m_splash{nullptr};
    QEventLoop m_loop;
    /** @brief Check that the profile is valid (width is a multiple of 8 and height a multiple of 2 */
    void checkProfileValidity();
    std::shared_ptr<MediaCapture> m_capture;
    QUrl m_mediaCaptureFile;
    QTimer m_hideTimer;
    void resetThumbProfile();
    /** @brief Build the Splash Screen.
     *  @param firstRun if true, we display the First start Quick Config dialog
     *  @param showWelcome if true, display the welcome screen. If false, a simple splash screen
     *  @param showCrashRecovery if true, always display the crash recovery option
     *  @param wasUpgraded if true, show a short upgrade message */
    void buildSplash(bool firstRun, bool showWelcome, bool showCrashRecovery, bool wasUpgraded);

protected:
    /** @brief A unique session id for this app instance */
    QString sessionId;

public Q_SLOTS:
    /** @brief Trigger (launch) an action by its actionCollection name */
    void triggerAction(const QString &name);
    /** @brief Get an action's descriptive text by its actionCollection name */
    const QString actionText(const QString &name);
    /** @brief Add an action to the app's actionCollection */
    void addActionToCollection(const QString &name, QAction *action);
    /** @brief display a user info/warning message in statusbar */
    void displayMessage(const QString &message, MessageType type, int timeout = -1);
    /** @brief Create small thumbnails for luma used in compositions */
    void buildLumaThumbs(const QStringList &values);
    /** @brief Try to find a display name for the given filename.
     *  This is especially helpful for mlt's dynamically created luma files without thumb (luma01.pgm, luma02.pgm,...),
     *  but also for others as it makes the visible name translatable.
     *  @return The name that fits to the filename or if none is found the filename it self
     */
    const QString nameForLumaFile(const QString &filename);
    /** @brief Set current project modified. */
    void setDocumentModified();
    /** @brief Show currently selected effect zone in timeline ruler. */
    void showEffectZone(ObjectId id, QPair<int, int> inOut, bool checked);
    void updateMasterZones();
    /** @brief Open the proxies test dialog. */
    void testProxies();
    /** @brief Refresh the monitor profile when project profile changes. */
    void updateMonitorProfile();
    /** @brief Add a new Bin Widget. */
    void addBin(const QString &id = QString());
    /** @brief Transcode a bin clip video. */
    void transcodeFriendlyFile(const QString &binId, bool checkProfile);
    /** @brief Reset audio  monitoring volume and channels. */
    void resetAudioMonitoring();
    /** @brief Stop monitoring audio (without affecting the track header record control. */
    void setAudioMonitoring(bool);
    /** @brief Start audio recording (after countdown). */
    void startRecording(bool showCountdown = false);
    /** @brief Show or hide track head audio rec controls. */
    void monitorAudio(int tid, bool monitor);
    /** @brief Open a documentation link, showing a warning box first */
    void openDocumentationLink(const QUrl &link);
    void openLink(const QUrl &link);

private Q_SLOTS:
    /** @brief display a user info/warning message in the project bin */
    void displayBinMessagePrivate(const QString &text, int type, const QList<QAction *> &actions = QList<QAction *>(), bool showClose = false,
                                  BinMessage::BinCategory messageCategory = BinMessage::BinCategory::NoMessage);
    void displayBinLogMessagePrivate(const QString &text, int type, const QString logInfo);
    void cleanRestart(bool cleanAndRestart);
    void startFromGuessedProfile(QString descriptiveString, QString fps, bool interlaced, int vTracks, int aTracks);

Q_SIGNALS:
    void coreIsReady();
    void updateLibraryPath();
    // void updateMonitorProfile();
    /** @brief Call config dialog on a selected page / tab */
    void showConfigDialog(Kdenlive::ConfigPage, int);
    void finalizeRecording(const QUuid uuid, const QString &captureFile);
    void autoScrollChanged();
    void centeredPlayheadChanged();
    /** @brief Update the message about the current loading progress */
    void loadingMessageNewStage(const QString &message, int max = -1);
    /** @brief Increase the progress of the loading message by 1 */
    void loadingMessageIncrease();
    /** @brief Hide the message about the loading progress */
    void loadingMessageHide();
    /** @brief Inform timeline of current drag end */
    void processDragEnd();
    /** @brief Opening finished, close splash screen */
    void closeSplash();
    /** @brief Trigger an update of the speech models list */
    void speechModelUpdate(SpeechToTextEngine::EngineType engine, const QStringList models);
    /** @brief Update current effect zone */
    void updateEffectZone(const QPoint p, bool withUndo);
    /** @brief The effect stask is about to be deleted, disconnect everything */
    void disconnectEffectStack();
    /** @brief Add a time remap effect to clip and show keyframes dialog */
    void remapClip(int cid);
    /** @brief A monitor property changed, check if we need to reset */
    void monitorProfileUpdated();
    /** @brief Color theme changed, process refresh */
    void updatePalette();
    /** @brief Emitted when a clip is resized (to handle clip monitor inserted zones) */
    void clipInstanceResized(const QString &binId);
    /** @brief Contains the project audio levels */
    void audioLevelsAvailable(const QVector<double> &levels);
    /** @brief Audio levels config changed, update audio level widgets */
    void audioLevelsConfigChanged();
    /** @brief A frame was displayed in monitor, update audio mixer */
    void updateMixerLevels(int pos);
    /** @brief Audio recording was started or stopped*/
    void switchTimelineRecord(bool on);
    /** @brief Launch audio recording on track tid*/
    void recordAudio(int tid, bool record);
    /** @brief Inform widgets that the project profile (and possibly fps) changed */
    void updateProjectTimecode();
    /** @brief Visible guide categories changed, reload snaps in timeline */
    void refreshActiveGuides();
    /** @brief The default marker category was changed, update guides list button */
    void updateDefaultMarkerCategory();
    /** @brief The default speech engine was changed */
    void speechEngineChanged();
    /** @brief Auto fit track height property changed, adjust size */
    void autoTrackHeight(bool);
    /** @brief The number of missing clips in the project changed, inform user when rendering */
    void gotMissingClipsCount(int total, int used);
    /** @brief Tell the current progress task to stop */
    void stopProgressTask();
    /** @brief A project clip was deleted */
    void binClipDeleted(int cid);
    /** @brief An MLT warning was issued */
    void mltWarning(const QString &message);
    /** @brief Request display of effect stack for a Bin clip. */
    void requestShowBinEffectStack(const QString &clipName, std::shared_ptr<EffectStackModel>, QSize frameSize, bool showKeyframes);
    /** @brief Save guide categories in document properties */
    void saveGuideCategories();
    /** @brief When creating a backup file, also save a thumbnail of current timeline */
    void saveTimelinePreview(const QString &path);
    /** Enable the built in transform effect for current item */
    void enableBuildInTransform();
    /** refresh sam config */
    void samConfigUpdated();
    /** Show / hide the automask panel */
    void switchMaskPanel(bool show);
    void transcodeProgress(ObjectId id, int progress);
    /** Inform of an autosave starting operation */
    void startAutoSave();
    /** @brief display a user info/warning message in the project bin */
    void displayBinMessage(const QString &text, int type, const QList<QAction *> &actions = QList<QAction *>(), bool showClose = false,
                           BinMessage::BinCategory messageCategory = BinMessage::BinCategory::NoMessage);
    void displayBinLogMessage(const QString &text, int type, const QString logInfo);
    /** Some properties related to rendering changed, update */
    void updateRenderOffset();
    void hideBars(bool);
    void switchTitleBars();
    void loadLayoutById(QString layoutId, bool onlyIfNoPrevious = false);
    void switchDarkPalette(bool dark);
    void mainWindowReady();
    void loadLayoutFromData(const QString layout, bool onlyIfNoPrevious = false);
    /** The project profile changed, check if we have a more appropriate layout (horizontal/vertical) */
    void adjustLayoutToDar();
    /** Should be called when the mainwindow has been constructed and before any dialog is shown to hide the splash screen */
    void GUISetupDone();
};
