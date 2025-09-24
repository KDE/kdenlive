/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractmonitor.h"
#include "bin/model/markerlistmodel.hpp"
#include "definitions.h"
#include "utils/gentime.h"
#include "scopes/sharedframe.h"
#include "widgets/timecodedisplay.h"

#include <QMutex>
#include <QTimer>
#include <QToolBar>
#include <QWidgetAction>

#include <memory>
#include <unordered_set>

class SnapModel;
class ProjectClip;
class MonitorManager;
class MonitorProxy;
class QSlider;
class QToolButton;
class KActionMenu;
class KDualAction;
class KSelectAction;
class KMessageWidget;
class QScrollBar;
class RecManager;
class QmlManager;
class QLabel;
class VideoWidget;
class MonitorAudioLevel;
class MarkerSortModel;

namespace Mlt {
class Profile;
class Filter;
} // namespace Mlt

class VolumeAction : public QWidgetAction
{
    Q_OBJECT
public:
    explicit VolumeAction(QObject *parent);
    QWidget *createWidget(QWidget *parent) override;

Q_SIGNALS:
    void volumeChanged(int volume);
};


class Monitor : public AbstractMonitor
{
    Q_OBJECT

public:
    friend class MonitorManager;
    friend class MaskManager;

    Monitor(Kdenlive::MonitorId id, MonitorManager *manager, QWidget *parent = nullptr);
    ~Monitor() override;

    enum MonitorOverlay {
        InfoOverlay = 0x01,
        TimecodeOverlay = 0x02,
        MarkersOverlay = 0x04,
        AudioWaveformOverlay = 0x10,
        PlaybackFpsOverlay = 0x20,
        ClipJobsOverlay = 0x40
    };

    QTimer refreshMonitorTimer;
    bool locked{false};
    void resetProfile();
    /** @brief Rebuild consumers after a property change */
    void resetConsumer(bool fullReset);
    void setupMenu(QMenu *goMenu, QMenu *overlayMenu, QAction *playZone, QAction *playZoneFromCursor, QAction *loopZone, QMenu *markerMenu = nullptr,
                   QAction *loopClip = nullptr);
    const QString activeClipId(bool withSequence = false);
    int position();
    void updateTimecodeFormat();
    void updateMarkers();
    /** @brief Controller for the clip currently displayed (only valid for clip monitor). */
    std::shared_ptr<ProjectClip> currentController() const;
    void reloadProducer(const QString &id);
    /** @brief Reimplemented from QWidget, updates the palette colors. */
    void setPalette(const QPalette &p);
    /** @brief Returns current project's fps. */
    double fps() const;
    /** @brief Get url for the clip's thumbnail */
    QString getMarkerThumb(GenTime pos);
    int getZoneStart();
    int getZoneEnd();
    /** @brief Get the in and out points selected in the clip monitor.
     * @return a QPoint where x() is the in point, and y() is the out point + 1.
     * E.g. if the user sets in=100, out=101, then this returns <100, 102>.
     */
    QPoint getZoneInfo() const;
    /** @brief Initialize the qml monitor overlay with points and rect related to the current frame
     */
    void setUpEffectGeometry(const QVariantList &list, const QVariantList &types = QVariantList(), const QVariantList &keyframes = QVariantList(),
                             const QRect &box = QRect());
    void setUpEffectGeometry(const QRect &r, const QVariantList &list = QVariantList(), const QVariantList &types = QVariantList(),
                             const QVariantList &keyframes = QVariantList(), const QRect &box = QRect());
    /** @brief Set a property on the effect scene */
    void setEffectSceneProperty(const QString &name, const QVariant &value);
    /** @brief Returns effective display size */
    QSize profileSize() const;
    void setEffectKeyframe(bool enable, bool outside);
    void sendFrameForAnalysis(bool analyse);
    void updateAudioForAnalysis();
    void switchMonitorInfo(int code);
    void restart();
    void mute(bool) override;
    /** @brief Returns the action displaying record toolbar */
    QAction *recAction();
    void refreshIcons();
    /** @brief Send audio thumb data to qml for on monitor display */
    void prepareAudioThumb();
    void connectAudioSpectrum(bool activate);
    /** @brief Set a property on the Qml scene **/
    void setQmlProperty(const QString &name, const QVariant &value);
    void displayAudioMonitor(bool isActive);
    /** @brief Set the guides list model to currently active item (bin clip or timeline) **/
    void updateGuidesList();
    /** @brief Prepare split effect from timeline clip producer **/
    void activateSplit();
    /** @brief Clear monitor display **/
    void clearDisplay();
    void reconfigure();
    /** @brief Saves current monitor frame to an image file, and add it to project if addToProject is set to true **/
    void slotExtractCurrentFrame(QString frameName = QString(), bool addToProject = false, bool toClipboard = false);
    /** @brief Zoom in active monitor */
    void slotZoomIn();
    /** @brief Zoom out active monitor */
    void slotZoomOut();
    /** @brief Set a property on the MLT consumer */
    void setConsumerProperty(const QString &name, const QString &value);
    /** @brief Play or Loop zone sets a fake "out" on the producer. It is necessary to reset this before reloading the producer */
    void resetPlayOrLoopZone(const QString &binId);
    /** @brief Returns a pointer to monitor proxy, allowing to manage seek and consumer position */
    MonitorProxy *getControllerProxy();
    /** @brief Update active track in multitrack view */
    void updateMultiTrackView(int tid);
    /** @brief Returns true if monitor is currently fullscreen */
    bool monitorIsFullScreen() const;
    void reloadActiveStream();
    /** @brief Returns true if monitor is playing */
    bool isPlaying() const;
    /** @brief Enables / disables effect scene*/
    void enableEffectScene(bool enable);
    /** @brief Update the document's uuid - used for qml thumb cache*/
    void updateDocumentUuid();
    /** @brief Focus the timecode to allow editing*/
    void focusTimecode();
    /** @brief Set a timecode offset for current timeline*/
    void setTimecodeOffset(int offset);
    /** @brief Ensure the video widget has focus to make keyboard shortcuts work */
    void fixFocus();
    /** @brief Show a rec countdown over the monitor **/
    void startCountDown();
    void stopCountDown();
    /** @brief Reset monitor scene to default **/
    void resetScene();
    /** @brief Set monitor zone **/
    void loadZone(int in, int out);
    /** @brief Extract current frame to image file with path **/
    void extractFrame(const QString &path);
    /** @brief Returns some infos about the GPU */
    const QStringList getGPUInfo();
    /** @brief Set a flag to tell if the monitor will need a refresh on next activation */
    void markDirty(const QUuid uuid);
    /** @brief If true, the monitor will need a refresh on next activation */
    bool isDirty() const;
    /** @brief Display a  mask as overlay to the clip
     *  @param maskFile the video mask file
     *  @param in the zone in for the mask
     *  @param out the zone out for the mask
     *  @param maskMode set to 1 for edit, 2 for preview only */
    void previewMask(const QString &binId, const QString &maskFile, int in, int out, MaskModeType::MaskCreationMode maskMode);
    /** @brief Get current mask mode */
    MaskModeType::MaskCreationMode maskMode();
    /** @brief Update the preview mask properties */
    void updatePreviewMask();
    /** @brief Apply timecode display styling based on current active state */
    void applyTimecodeDisplayStyling();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

    /** @brief Move to another position on mouse wheel event.
     *
     * Moves towards the end of the clip/timeline on mouse wheel down/back, the
     * opposite on mouse wheel up/forward.
     * Ctrl + wheel moves by a second, without Ctrl it moves by a single frame. */
    void wheelEvent(QWheelEvent *event) override;
    void updateBgColor();

private:
    std::shared_ptr<ProjectClip> m_controller;
    /** @brief The QQuickView that handles our monitor display (video and qml overlay) **/
    VideoWidget *m_glMonitor;
    /** @brief Container for our QQuickView monitor display (QQuickView needs to be embedded) **/
    QWidget *m_glWidget;
    /** @brief Scrollbar for our monitor view, used when zooming the monitor **/
    QScrollBar *m_verticalScroll;
    /** @brief Scrollbar for our monitor view, used when zooming the monitor **/
    QScrollBar *m_horizontalScroll;
    /** @brief Manager for qml overlay for the QQuickView **/
    QmlManager *m_qmlManager;
    std::shared_ptr<SnapModel> m_snaps;
    /** @brief If we are displaying a timeline sequence clip from a playlist Bin clip, otherwise null **/
    QUuid m_activeSequence;

    std::shared_ptr<Mlt::Filter> m_splitEffect;
    std::unique_ptr<Mlt::Transition> m_maskOpacity;
    std::shared_ptr<Mlt::Filter> m_maskInvert;
    std::shared_ptr<Mlt::Producer> m_splitProducer;
    std::shared_ptr<MarkerListModel> m_markerModel{nullptr};
    int m_length;
    bool m_dragStarted;
    RecManager *m_recManager;
    /** @brief The widget showing current time position **/
    TimecodeDisplay *m_timePos;
    KDualAction *m_playAction;
    KSelectAction *m_forceSize;
    KSelectAction *m_background;
    /** Has to be available so we can enable and disable it. */
    QAction *m_loopClipAction;
    QAction *m_zoomVisibilityAction;
    KActionMenu *m_configMenuAction;
    QMenu *m_contextMenu;
    QMenu *m_playMenu;
    KActionMenu *m_markerMenu;
    QMenu *m_audioChannels;
    QToolButton *m_streamsButton;
    QAction *m_streamAction;
    /** true if selected clip is transition, false = selected clip is clip.
     *  Necessary because sometimes we get two signals, e.g. we get a clip and we get selected transition = nullptr. */
    bool m_loopClipTransition;
    GenTime getSnapForPos(bool previous);
    QToolBar *m_toolbar;
    QToolBar *m_trimmingbar;
    QAction *m_oneLess;
    QAction *m_oneMore;
    QAction *m_fiveLess;
    QAction *m_fiveMore;
    QLabel *m_trimmingOffset;
    QAction *m_editMarker;
    KMessageWidget *m_infoMessage;
    int m_forceSizeFactor;
    MonitorSceneType m_lastMonitorSceneType;
    MonitorSceneType m_nextSceneType{MonitorSceneType::MonitorSceneNone};
    MonitorAudioLevel *m_audioMeterWidget;
    QTimer m_droppedTimer;
    double m_displayedFps;
    int m_speedIndex;
    QMetaObject::Connection m_switchConnection;
    QMetaObject::Connection m_captureConnection;

    void adjustScrollBars(float horizontal, float vertical);
    void updateQmlDisplay(int currentOverlay);
    /** @brief Create temporary Mlt::Tractor holding a clip and it's effectless clone */
    void buildSplitEffect(Mlt::Producer *original);
    /** @brief Returns true if monitor is currently visible (not in a tab or hidden)*/
    bool monitorVisible() const;
    /** To easily get them when creating the right click menu */
    QAction *m_markIn;
    QAction *m_markOut;
    QUuid m_displayedUuid;
    bool m_dirty{false};
    QMutex m_openMutex;
    QString m_activeControllerId;
    QTimer m_preventSleepTimer;

protected:
    void loadQmlScene(MonitorSceneType type, const QVariant &sceneData = QVariant(), bool resetProperties = false);

private Q_SLOTS:
    void slotSetThumbFrame();
    void slotSeek();
    void updateClipZone(const QPoint zone);
    void slotGoToMarker(QAction *action);
    void slotSetVolume(int volume);
    void slotEditMarker();
    void slotExtractCurrentZone();
    void onFrameDisplayed(const SharedFrame &frame);
    void slotStartDrag();
    void setZoom(float zoomRatio);
    void slotAdjustEffectCompare();
    void slotShowMenu(const QPoint pos);
    void slotForceSize(QAction *a);
    void buildBackgroundedProducer(int pos);
    void slotLockMonitor(bool lock);
    void slotSwitchPlay();
    void slotEditInlineMarker();
    /** @brief Pass keypress event to mainwindow */
    void doKeyPressEvent(QKeyEvent *);
    /** @brief There was an error initializing Movit */
    void gpuError();
    void setOffsetX(int x);
    void setOffsetY(int y);
    /** @brief Pan monitor view */
    void panView(QPoint diff);
    void slotSeekPosition(int);
    void addSnapPoint(int pos);
    void removeSnapPoint(int pos);
    /** @brief Process seek and optionally pause monitor */
    void processSeek(int pos, bool noAudioScrub = false);
    /** @brief Check and display dropped frames */
    void checkDrops();
    void addControlPoint(double x, double y, bool extend, bool exclude);
    void moveControlPoint(int ix, double x, double y);
    void addControlRect(double x, double y, double width, double height, bool extend);
    /** @brief Check if powermanagement sleep should be inhibited*/
    void updatePowerManagement();

public Q_SLOTS:
    void slotCreateRangeMarkerFromZone();
    void slotCreateRangeMarkerFromZoneQuick();
    void updateTimelineProducer();
    void setProducer(const QUuid, std::shared_ptr<Mlt::Producer> producer, int pos = -1);
    void slotSetScreen(int screenIndex);
    void slotPreviewResource(const QString &path, const QString &title);
    // void slotSetClipProducer(DocClipBase *clip, QPoint zone = QPoint(), bool forceUpdate = false, int position = -1);
    void updateClipProducer(const std::shared_ptr<Mlt::Producer> &prod);
    void updateClipProducer(const QString &playlist);
    bool slotOpenClip(const std::shared_ptr<ProjectClip> &controller, int in = -1, int out = -1, const QUuid &sequenceUuid = QUuid());
    void slotRefreshMonitor(bool visible);
    void slotSeek(int pos);
    void stop() override;
    void start() override;
    void switchPlay(bool play);
    void updatePlayAction(bool play);
    void slotPlay() override;
    void pause();
    void slotPlayZone(bool startFromIn = true);
    void slotLoopZone();
    /** @brief Loops the selected item (clip or transition). */
    void slotLoopClip(std::pair<int, int> inOut);
    void slotForward(double speed = 0, bool allowNormalPlay = false) override;
    void slotRewind(double speed = 0) override;
    void slotRewindOneFrame(int diff = 1);
    void slotForwardOneFrame(int diff = 1);
    /** @brief Display a non blocking error message to user **/
    void warningMessage(const QString &text, int timeout = 5000, const QList<QAction *> &actions = QList<QAction *>());
    void slotStart();
    /** @brief Set position and information for the trimming preview
     * @param pos Absolute position in frames
     * @param offset Difference in frames between @p pos and the current position (to be displayed in the monitor toolbar)
     * @param frames1 Position in frames to be displayed in the monitor overlay for preview tile one
     * @param frames2 Position in frames to be displayed in the monitor overlay for preview tile two
     */
    void slotTrimmingPos(int pos, int offset, int frames1, int frames2);
    /** @brief Move the position for the trimming preview by the given offset
    * @param offset How many frames the position should be moved
    * @see slotTrimmingPos
    */
    void slotTrimmingPos(int offset);
    void slotEnd();
    void slotSetZoneStart();
    void slotSetZoneEnd();
    void slotZoneStart();
    void slotZoneEnd();
    void slotLoadClipZone(const QPoint &zone);
    void slotSeekToNextSnap();
    void slotSeekToPreviousSnap();
    void adjustRulerSize(int length, const std::shared_ptr<MarkerSortModel> &markerModel = nullptr);
    void setTimePos(const QString &pos);
    /** @brief Display the on monitor effect scene (to adjust geometry over monitor). */
    void slotShowEffectScene(MonitorSceneType sceneType, bool temporary = false, const QVariant &sceneData = QVariant());
    bool effectSceneDisplayed(MonitorSceneType effectType);
    /** @brief split screen to compare clip with and without effect */
    void slotSwitchCompare(bool enable);
    void slotMouseSeek(int eventDelta, uint modifiers) override;
    void slotSwitchFullScreen(bool minimizeOnly = false) override;
    /** @brief Display or hide the record toolbar */
    void slotSwitchRec(bool enable);
    /** @brief Display or hide the trimming toolbar and monitor scene*/
    void slotSwitchTrimming(bool enable);
    /** @brief Request QImage of current frame */
    void slotGetCurrentImage(bool request);
    /** @brief Enable/disable display of monitor's audio levels widget */
    void slotSwitchAudioMonitor();
    /** @brief En/Disable the show record timecode feature in clip monitor */
    void slotSwitchRecTimecode(bool enable);
    /** @brief Request seeking */
    void requestSeek(int pos);
    /** @brief Request seeking only if monitor is visible*/
    void requestSeekIfVisible(int pos);
    /** @brief Check current position to show relevant infos in qml view (markers, zone in/out, etc). */
    void checkOverlay(int pos = -1);
    void refreshMonitorIfActive(bool directUpdate = false) override;
    void refreshMonitor(bool directUpdate = false, bool slowRefresh = false);
    void forceMonitorRefresh();
    /** @brief Clear read ahead cache, to ensure up to date audio */
    void purgeCache();
    /** @brief Stop displaying a  mask as overlay to the clip */
    void abortPreviewMask(bool rebuildProducer = true);

Q_SIGNALS:
    void screenChanged(int screenIndex);
    void seekPosition(int pos);
    void seekRemap(int pos);
    void updateScene();
    void durationChanged(int);
    void zoneUpdated(const QPoint &);
    void zoneUpdatedWithUndo(const QPoint &, const QPoint &);
    void effectChanged(const QRectF &);
    void effectPointsChanged(const QVariantList &);
    void addRemoveKeyframe(bool addOnly);
    /** @brief Seek to an effect keyframe
     *  @param ix the index of the keyframe we want to reach
     *  @param offset if offset != 0, the ix param is ignored and we seek to previous (-1) or next(+1) keyframe
     */
    void seekToKeyframe(int ix, int offset);
    void addClipToProject(const QUrl &);
    /** @brief Request display of current bin clip. */
    void refreshCurrentClip();
    void addTimelineEffect(const QStringList &);
    void passKeyPress(QKeyEvent *);
    /** @brief Enable / disable project monitor multitrack view (split view with one track in each quarter). */
    void multitrackView(bool, bool);
    void timeCodeUpdated(const QString &);
    void addMarker();
    void deleteMarker(bool deleteGuide = true);
    void seekToPreviousSnap();
    void seekToNextSnap();
    void createSplitOverlay(std::shared_ptr<Mlt::Filter>);
    void removeSplitOverlay();
    void activateTrack(int, bool notesMode = false);
    void autoKeyframeChanged();
    void zoneDurationChanged();
    void blockSceneChange(bool);
    void addMonitorControlPoint(int position, const QSize frameSize, int xPos, int yPos, bool extend, bool exclude);
    void moveMonitorControlPoint(int ix, int position, const QSize frameSize, int xPos, int yPos);
    void addMonitorControlRect(int position, const QSize frameSize, const QRect rect, bool extend);
    void generateMask();
    void disablePreviewMask();
    void sceneChanged(MonitorSceneType sceneType);
    void effectRotationChanged(double rotation);
};
