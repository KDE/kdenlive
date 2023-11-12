/*
SPDX-FileCopyrightText: 2014 Till Theato <root@ttill.de>
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include "jobs/taskmanager.h"
#include "kdenlivecore_export.h"
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
#include <knewstuff_version.h>
#include <memory>
#include <unordered_set>

#include <mlt++/MltProfile.h>
#include <mlt++/MltPlaylist.h>

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
class SubtitleEdit;
class SubtitleModel;
class TextBasedEdit;
class GuidesList;
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
    Core(const Core &) = delete;
    Core &operator=(const Core &) = delete;
    Core(Core &&) = delete;
    Core &operator=(Core &&) = delete;

    ~Core() override;

    /**
     * @brief Setup the basics of the application, in particular the connection
     * with Mlt
     * @param MltPath (optional) path to MLT environment
     */
    static bool build(const QString &packageType, bool testMode = false);

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
    void initGUI(bool inSandbox, const QString &MltPath, const QUrl &Url, const QString &clipsToLoad = QString());

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
    QSize getCurrentFrameSize() const;
    /** @brief Returns the frame display size (width x height) of current profile */
    QSize getCurrentFrameDisplaySize() const;
    /** @brief Request project monitor refresh, and restore previously active monitor */
    void refreshProjectMonitorOnce();
    /** @brief Request project monitor refresh if current position is inside range*/
    void refreshProjectRange(QPair<int, int> range);
    /** @brief Request project monitor refresh if referenced item is under cursor */
    void refreshProjectItem(const ObjectId &id);
    /** @brief Returns a reference to a monitor (clip or project monitor) */
    Monitor *getMonitor(int id);
    /** @brief Seek a monitor to position */
    void seekMonitor(int id, int position);
    /** @brief Returns timeline's active track info (position and tag) */
    QPair <int,QString> currentTrackInfo() const;
    /** @brief This function must be called whenever the profile used changes */
    void profileChanged();

    /** @brief Create and push and undo object based on the corresponding functions
        Note that if you class permits and requires it, you should use the macro PUSH_UNDO instead*/
    void pushUndo(const Fun &undo, const Fun &redo, const QString &text);
    void pushUndo(QUndoCommand *command);
    /** @brief display a user info/warning message in statusbar */
    void displayMessage(const QString &message, MessageType type, int timeout = -1);
    /** @brief display timeline selection info in statusbar */
    void displaySelectionMessage(const QString &message);
    /** @brief Clear asset view if itemId is displayed. */
    void clearAssetPanel(int itemId);
    /** @brief Returns the effectstack of a given bin clip. */
    std::shared_ptr<EffectStackModel> getItemEffectStack(const QUuid &uuid, int itemType, int itemId);
    int getItemPosition(const ObjectId &id);
    /** @brief Get item in point. */
    int getItemIn(const ObjectId &id);
    /** @brief Get item in point, possibly from another timeline. */
    int getItemIn(const QUuid &uuid, const ObjectId &id);
    int getItemTrack(const ObjectId &id);
    int getItemDuration(const ObjectId &id);
    QSize getItemFrameSize(const ObjectId &id);
    /** @brief Returns the capabilities of a clip: AudioOnly, VideoOnly or Disabled if both are allowed */
    PlaylistState::ClipState getItemState(const ObjectId &id);
    /** @brief Get a list of video track names with indexes */
    QMap<int, QString> getTrackNames(bool videoOnly);
    /** @brief Returns the composition A track (MLT index / Track id) */
    QPair<int, int> getCompositionATrack(int cid) const;
    void setCompositionATrack(int cid, int aTrack);
    /** @brief Return true if composition's a_track is automatic (no forced track)
     */
    bool compositionAutoTrack(int cid) const;
    std::shared_ptr<DocUndoStack> undoStack();
    double getClipSpeed(int id) const;
    /** @brief Mark an item as invalid for timeline preview */
    void invalidateItem(ObjectId itemId);
    void invalidateRange(QPair<int, int>range);
    void prepareShutdown();
    void finishShutdown();
    /** the keyframe model changed (effect added, deleted, active effect changed), inform timeline */
    void updateItemKeyframes(ObjectId id);
    /** A fade for clip id changed, update timeline */
    void updateItemModel(ObjectId id, const QString &service);
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
    void startMediaCapture(int tid, bool, bool);
    void stopMediaCapture(int tid, bool, bool);
    QStringList getAudioCaptureDevices();
    int getMediaCaptureState();
    bool isMediaMonitoring() const;
    bool isMediaCapturing() const;
    MediaCapture *getAudioDevice();
    /** @brief Returns Project Folder name for capture output location */
    QString getProjectFolderName(bool folderForAudio = false);
    /** @brief Returns a timeline clip's bin id */
    QString getTimelineClipBinId(int cid);
    /** @brief Returns all track ids in timeline */
    std::unordered_set<QString> getAllTimelineTracksId();
    /** @brief Returns a frame duration from a timecode */
    int getDurationFromString(const QString &time);
    /** @brief An error occurred within a filter, inform user */
    void processInvalidFilter(const QString &service, const QString &id, const QString &message);
    /** @brief Update current project's tags */
    void updateProjectTags(int previousCount, const QMap <int, QStringList> &tags);
    /** @brief Returns the project profile */
    Mlt::Profile &getProjectProfile();
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
    void addGuides(const QList <int> &guides);
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
    void resizeMix(int cid, int duration, MixAlignment align, int leftFrames = -1);
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
#if KNEWSTUFF_VERSION < QT_VERSION_CHECK(5, 98, 0)
    /** @brief Instantiates a "Get Hot New Stuff" dialog.
     * @param configFile configuration file for KNewStuff
     * @return number of installed items */
    int getNewStuff(const QString &configFile);
#endif
    /** @brief Get the frame size of the clip above a composition */
    const QSize getCompositionSizeOnTrack(const ObjectId &id);
    void loadTimelinePreview(const QUuid uuid, const QString &chunks, const QString &dirty, bool enablePreview, Mlt::Playlist &playlist);
    /** @brief Returns true if the audio mixer widget is visible */
    bool audioMixerVisible{false};
    QString packageType() { return m_packageType; };
    /** @brief Start / stop audio capture */
    void switchCapture();
    /** @brief Get the uuid of currently active timeline */
    const QUuid currentTimelineId() const;
    /** @brief Update a sequence AV info (has audio/video) */
    void updateSequenceAVType(const QUuid &uuid, int tracksCount);
    /** @brief A list of markers type categories {marker type, {color, category name}} */
    struct MarkerCategory
    {
        QColor color;
        QString displayName;
    };
    QMap<int, MarkerCategory> markerTypes;

private:
    explicit Core(const QString &packageType);
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
    QString m_packageType;
    Timecode m_timecode;
    Mlt::Profile m_thumbProfile;
    /** @brief Mlt profile used in the consumer 's monitors */
    Mlt::Profile m_monitorProfile;
    /** @brief Mlt profile used to build the project's clips */
    Mlt::Profile m_projectProfile;
    bool m_guiConstructed = false;
    /** @brief Check that the profile is valid (width is a multiple of 8 and height a multiple of 2 */
    void checkProfileValidity();
    std::unique_ptr<MediaCapture> m_capture;
    QUrl m_mediaCaptureFile;
    void resetThumbProfile();

public Q_SLOTS:
    /** @brief Trigger (launch) an action by its actionCollection name */
    void triggerAction(const QString &name);
    /** @brief Get an action's descriptive text by its actionCollection name */
    const QString actionText(const QString &name);
    /** @brief Add an action to the app's actionCollection */
    void addActionToCollection(const QString &name, QAction *action);
    /** @brief display a user info/warning message in the project bin */
    void displayBinMessage(const QString &text, int type, const QList<QAction *> &actions = QList<QAction *>(), bool showClose = false, BinMessage::BinCategory messageCategory = BinMessage::BinCategory::NoMessage);
    void displayBinLogMessage(const QString &text, int type, const QString logInfo);
    /** @brief Create small thumbnails for luma used in compositions */
    void buildLumaThumbs(const QStringList &values);
    /** @brief Try to find a display name for the given filename.
     *  This is espacally helpfull for mlt's dynamically created luma files without thumb (luma01.pgm, luma02.pgm,...),
     *  but also for others as it makes the visible name translatable.
     *  @return The name that fits to the filename or if none is found the filename it self
     */
    const QString nameForLumaFile(const QString &filename);
    /** @brief Set current project modified. */
    void setDocumentModified();
    /** @brief Show currently selected effect zone in timeline ruler. */
    void showEffectZone(ObjectId id, QPair <int, int>inOut, bool checked);
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
    /** @brief Start audio recording (after countdown). */
    void startRecording();
    /** @brief Show or hide track head audio rec controls. */
    void monitorAudio(int tid, bool monitor);

Q_SIGNALS:
    void coreIsReady();
    void updateLibraryPath();
    //void updateMonitorProfile();
    /** @brief Call config dialog on a selected page / tab */
    void showConfigDialog(Kdenlive::ConfigPage, int);
    void finalizeRecording(const QString &captureFile);
    void autoScrollChanged();
    /** @brief Send a message to splash screen if still displayed */
    void loadingMessageUpdated(const QString &, int progress = 0, int max = -1);
    /** @brief Opening finished, close splash screen */
    void closeSplash();
    /** @brief Trigger an update of the speech models list */
    void voskModelUpdate(const QStringList models);
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
    void audioLevelsAvailable(const QVector<double>& levels);
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
};
