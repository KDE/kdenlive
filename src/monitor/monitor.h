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

#ifndef MONITOR_H
#define MONITOR_H

#include "abstractmonitor.h"
#include "bin/model/markerlistmodel.hpp"
#include "definitions.h"
#include "gentime.h"
#include "scopes/sharedframe.h"
#include "timecodedisplay.h"

#include <QTimer>
#include <QToolBar>

#include <memory>
#include <unordered_set>

class SnapModel;
class ProjectClip;
class MonitorManager;
class QSlider;
class QToolButton;
class KDualAction;
class KSelectAction;
class KMessageWidget;
class QScrollBar;
class RecManager;
class QmlManager;
class QLabel;
class GLWidget;
class MonitorAudioLevel;
class MonitorProxy;

namespace Mlt {
class Profile;
class Filter;
} // namespace Mlt

class QuickEventEater : public QObject
{
    Q_OBJECT
public:
    explicit QuickEventEater(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    void addEffect(const QStringList &);
    void doKeyPressEvent(QKeyEvent *);
};


class Monitor : public AbstractMonitor
{
    Q_OBJECT

public:
    friend class MonitorManager;

    Monitor(Kdenlive::MonitorId id, MonitorManager *manager, QWidget *parent = nullptr);
    ~Monitor() override;
    void resetProfile();
    /** @brief Rebuild consumers after a property change */
    void resetConsumer(bool fullReset);
    void setCustomProfile(const QString &profile, const Timecode &tc);
    void setupMenu(QMenu *goMenu, QMenu *overlayMenu, QAction *playZone, QAction *loopZone, QMenu *markerMenu = nullptr, QAction *loopClip = nullptr);
    const QString sceneList(const QString &root, const QString &fullPath = QString(), const QString overlayData = QString());
    const QString activeClipId();
    int position();
    void updateTimecodeFormat();
    void updateMarkers();
    /** @brief Controller for the clip currently displayed (only valid for clip monitor). */
    std::shared_ptr<ProjectClip> currentController() const;
    /** @brief Add timeline guides to the ruler and context menu */
    void setGuides(const QMap<double, QString> &guides);
    void reloadProducer(const QString &id);
    /** @brief Reimplemented from QWidget, updates the palette colors. */
    void setPalette(const QPalette &p);
    /** @brief Returns current project's fps. */
    double fps() const;
    /** @brief Get url for the clip's thumbnail */
    QString getMarkerThumb(GenTime pos);
    int getZoneStart();
    int getZoneEnd();
    QPoint getZoneInfo() const;
    void setUpEffectGeometry(const QRect &r, const QVariantList &list = QVariantList(), const QVariantList &types = QVariantList());
    /** @brief Set a property on the effect scene */
    void setEffectSceneProperty(const QString &name, const QVariant &value);
    /** @brief Returns effective display size */
    QSize profileSize() const;
    QRect effectRect() const;
    QVariantList effectPolygon() const;
    QVariantList effectRoto() const;
    void setEffectKeyframe(bool enable);
    void sendFrameForAnalysis(bool analyse);
    void updateAudioForAnalysis();
    void switchMonitorInfo(int code);
    void restart();
    void mute(bool, bool updateIconOnly = false) override;
    /** @brief Returns the action displaying record toolbar */
    QAction *recAction();
    void refreshIcons();
    /** @brief Send audio thumb data to qml for on monitor display */
    void prepareAudioThumb();
    void connectAudioSpectrum(bool activate);
    /** @brief Set a property on the Qml scene **/
    void setQmlProperty(const QString &name, const QVariant &value);
    void displayAudioMonitor(bool isActive);
    /** @brief Prepare split effect from timeline clip producer **/
    void activateSplit();
    /** @brief Clear monitor display **/
    void clearDisplay();
    void setProducer(std::shared_ptr<Mlt::Producer> producer, int pos = -1);
    void reconfigure();
    /** @brief Saves current monitor frame to an image file, and add it to project if addToProject is set to true **/
    void slotExtractCurrentFrame(QString frameName = QString(), bool addToProject = false);
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
    /** @brief Trigger a refresh of audio thumbs colors */
    void refreshAudioThumbs();
    /** @brief Trigger a refresh of audio thumbs on notrmalization change */
    void normalizeAudioThumbs();
    /** @brief Returns true if monitor is playing */
    bool isPlaying() const;
    /** @brief Enables / disables effect scene*/
    void enableEffectScene(bool enable);
    

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
    void mouseMoveEvent(QMouseEvent *event) override;
    virtual QStringList mimeTypes() const;
    void updateBgColor();

private:
    std::shared_ptr<ProjectClip> m_controller;
    /** @brief The QQuickView that handles our monitor display (video and qml overlay) **/
    GLWidget *m_glMonitor;
    /** @brief Container for our QQuickView monitor display (QQuickView needs to be embedded) **/
    QWidget *m_glWidget;
    /** @brief Scrollbar for our monitor view, used when zooming the monitor **/
    QScrollBar *m_verticalScroll;
    /** @brief Scrollbar for our monitor view, used when zooming the monitor **/
    QScrollBar *m_horizontalScroll;
    /** @brief Widget holding the window for the QQuickView **/
    QWidget *m_videoWidget;
    /** @brief Manager for qml overlay for the QQuickView **/
    QmlManager *m_qmlManager;
    std::shared_ptr<SnapModel> m_snaps;

    std::shared_ptr<Mlt::Filter> m_splitEffect;
    std::shared_ptr<Mlt::Producer> m_splitProducer;
    int m_length;
    bool m_dragStarted;
    RecManager *m_recManager;
    /** @brief The widget showing current time position **/
    TimecodeDisplay *m_timePos;
    KDualAction *m_playAction;
    KSelectAction *m_forceSize;
    /** Has to be available so we can enable and disable it. */
    QAction *m_loopClipAction;
    QAction *m_zoomVisibilityAction;
    QMenu *m_contextMenu;
    QMenu *m_configMenu;
    QMenu *m_playMenu;
    QMenu *m_markerMenu;
    QMenu *m_audioChannels;
    QToolButton *m_streamsButton;
    QAction *m_streamAction;
    QPoint m_DragStartPosition;
    /** true if selected clip is transition, false = selected clip is clip.
     *  Necessary because sometimes we get two signals, e.g. we get a clip and we get selected transition = nullptr. */
    bool m_loopClipTransition;
    GenTime getSnapForPos(bool previous);
    QToolBar *m_toolbar;
    QSlider *m_audioSlider;
    QAction *m_editMarker;
    KMessageWidget *m_infoMessage;
    int m_forceSizeFactor;
    int m_offset;
    MonitorSceneType m_lastMonitorSceneType;
    MonitorAudioLevel *m_audioMeterWidget;
    QTimer m_droppedTimer;
    double m_displayedFps;
    int m_speedIndex;
    QMetaObject::Connection m_switchConnection;

    void adjustScrollBars(float horizontal, float vertical);
    void loadQmlScene(MonitorSceneType type, QVariant sceneData = QVariant());
    void updateQmlDisplay(int currentOverlay);
    /** @brief Create temporary Mlt::Tractor holding a clip and it's effectless clone */
    void buildSplitEffect(Mlt::Producer *original);

private slots:
    void slotSetThumbFrame();
    void slotSeek();
    void updateClipZone(const QPoint zone);
    void slotGoToMarker(QAction *action);
    void slotSetVolume(int volume);
    void slotEditMarker();
    void slotExtractCurrentZone();
    void onFrameDisplayed(const SharedFrame &frame);
    void slotStartDrag();
    void setZoom();
    void slotAdjustEffectCompare();
    void slotShowMenu(const QPoint pos);
    void slotForceSize(QAction *a);
    void slotSeekToKeyFrame();
    /** @brief Display a non blocking error message to user **/
    void warningMessage(const QString &text, int timeout = 5000, const QList<QAction *> &actions = QList<QAction *>());
    void slotLockMonitor(bool lock);
    void slotAddEffect(const QStringList &effect);
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
    void processSeek(int pos);
    /** @brief Check and display dropped frames */
    void checkDrops();
    /** @brief En/Disable the show record timecode feature in clip monitor */
    void slotSwitchRecTimecode(bool enable);

public slots:
    void slotSetScreen(int screenIndex);
    void slotPreviewResource(const QString &path, const QString &title);
    // void slotSetClipProducer(DocClipBase *clip, QPoint zone = QPoint(), bool forceUpdate = false, int position = -1);
    void updateClipProducer(const std::shared_ptr<Mlt::Producer> &prod);
    void updateClipProducer(const QString &playlist);
    void slotOpenClip(const std::shared_ptr<ProjectClip> &controller, int in = -1, int out = -1);
    void slotRefreshMonitor(bool visible);
    void slotSeek(int pos);
    void stop() override;
    void start() override;
    void switchPlay(bool play);
    void updatePlayAction(bool play);
    void slotPlay() override;
    void pause();
    void slotPlayZone();
    void slotLoopZone();
    /** @brief Loops the selected item (clip or transition). */
    void slotLoopClip(QPoint inOut);
    void slotForward(double speed = 0, bool allowNormalPlay = false) override;
    void slotRewind(double speed = 0) override;
    void slotRewindOneFrame(int diff = 1);
    void slotForwardOneFrame(int diff = 1);
    void slotStart();
    void slotEnd();
    void slotSetZoneStart();
    void slotSetZoneEnd();
    void slotZoneStart();
    void slotZoneEnd();
    void slotLoadClipZone(const QPoint &zone);
    void slotSeekToNextSnap();
    void slotSeekToPreviousSnap();
    void adjustRulerSize(int length, const std::shared_ptr<MarkerListModel> &markerModel = nullptr);
    void setTimePos(const QString &pos);
    /** @brief Display the on monitor effect scene (to adjust geometry over monitor). */
    void slotShowEffectScene(MonitorSceneType sceneType, bool temporary = false, QVariant sceneData = QVariant());
    bool effectSceneDisplayed(MonitorSceneType effectType);
    /** @brief split screen to compare clip with and without effect */
    void slotSwitchCompare(bool enable);
    void slotMouseSeek(int eventDelta, uint modifiers) override;
    void slotSwitchFullScreen(bool minimizeOnly = false) override;
    /** @brief Display or hide the record toolbar */
    void slotSwitchRec(bool enable);
    /** @brief Request QImage of current frame */
    void slotGetCurrentImage(bool request);
    /** @brief Enable/disable display of monitor's audio levels widget */
    void slotSwitchAudioMonitor();
    /** @brief Request seeking */
    void requestSeek(int pos);
    /** @brief Check current position to show relevant infos in qml view (markers, zone in/out, etc). */
    void checkOverlay(int pos = -1);
    void refreshMonitorIfActive(bool directUpdate = false) override;
    void forceMonitorRefresh();
    /** @brief Clear read ahead cache, to ensure up to date audio */
    void purgeCache();

signals:
    void screenChanged(int screenIndex);
    void seekPosition(int pos);
    void updateScene();
    void durationChanged(int);
    void refreshClipThumbnail(const QString &);
    void zoneUpdated(const QPoint &);
    void zoneUpdatedWithUndo(const QPoint &, const QPoint &);
    /** @brief  Editing transitions / effects over the monitor requires the renderer to send frames as QImage.
     *      This causes a major slowdown, so we only enable it if required */
    void requestFrameForAnalysis(bool);
    void effectChanged(const QRect &);
    void effectPointsChanged(const QVariantList &);
    void addRemoveKeyframe();
    void seekToNextKeyframe();
    void seekToPreviousKeyframe();
    void seekToKeyframe(int);
    void addClipToProject(const QUrl &);
    /** @brief Request display of current bin clip. */
    void refreshCurrentClip();
    void addEffect(const QStringList &);
    void addMasterEffect(QString, const QStringList &);
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
    void acceptRipple(bool);
    void switchTrimMode(int);
    void activateTrack(int);
    void autoKeyframeChanged();
};

#endif
