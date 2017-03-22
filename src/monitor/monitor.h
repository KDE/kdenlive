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
#include "gentime.h"
#include "renderer.h"
#include "definitions.h"
#include "timecodedisplay.h"
#include "scopes/sharedframe.h"
#include "effectslist/effectslist.h"

#include <QDomElement>
#include <QToolBar>
#include <QElapsedTimer>

class SmallRuler;
class ClipController;
class AbstractClipItem;
class Transition;
class ClipItem;
class Monitor;
class MonitorManager;
class QSlider;
class KDualAction;
class KSelectAction;
class KMessageWidget;
class QQuickItem;
class QScrollBar;
class RecManager;
class QToolButton;
class QmlManager;
class MonitorAudioLevel;

class QuickEventEater : public QObject
{
    Q_OBJECT
public:
    explicit QuickEventEater(QObject *parent = nullptr);

protected:
    bool eventFilter(QObject *obj, QEvent *event) Q_DECL_OVERRIDE;

signals:
    void addEffect(const QDomElement &);
};

class QuickMonitorEventEater : public QObject
{
    Q_OBJECT
public:
    explicit QuickMonitorEventEater(QWidget *parent);

protected:
    bool eventFilter(QObject *obj, QEvent *event) Q_DECL_OVERRIDE;

signals:
    void doKeyPressEvent(QKeyEvent *);
};

class Monitor : public AbstractMonitor
{
    Q_OBJECT

public:
    Monitor(Kdenlive::MonitorId id, MonitorManager *manager, QWidget *parent = nullptr);
    ~Monitor();
    Render *render;
    AbstractRender *abstractRender() Q_DECL_OVERRIDE;
    void resetProfile(const MltVideoProfile &profile);
    void setCustomProfile(const QString &profile, const Timecode &tc);
    void setupMenu(QMenu *goMenu, QMenu *overlayMenu, QAction *playZone, QAction *loopZone, QMenu *markerMenu = nullptr, QAction *loopClip = nullptr);
    const QString sceneList(const QString &root);
    const QString activeClipId();
    GenTime position();
    /** @brief Check current position to show relevant infos in qml view (markers, zone in/out, etc). */
    void checkOverlay(int pos = -1);
    void updateTimecodeFormat();
    void updateMarkers();
    /** @brief Controller for the clip currently displayed (only valid for clip monitor). */
    ClipController *currentController() const;
    /** @brief Add clip markers to the ruler and context menu */
    void setMarkers(const QList<CommentedTime> &markers);
    /** @brief Add timeline guides to the ruler and context menu */
    void setGuides(const QMap<double, QString> &guides);
    void reloadProducer(const QString &id);
    /** @brief Reimplemented from QWidget, updates the palette colors. */
    void setPalette(const QPalette &p);
    /** @brief Returns a hh:mm:ss timecode from a frame number. */
    QString getTimecodeFromFrames(int pos);
    /** @brief Returns current project's fps. */
    double fps() const;
    /** @brief Returns current project's timecode. */
    Timecode timecode() const;
    /** @brief Get url for the clip's thumbnail */
    QString getMarkerThumb(GenTime pos);
    /** @brief Get current project's folder */
    const QString projectFolder() const;
    /** @brief Get the project's profile info*/
    ProfileInfo profileInfo() const;
    /** @brief Get the project's Mlt profile */
    Mlt::Profile *profile();
    int getZoneStart();
    int getZoneEnd();
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
    void switchDropFrames(bool drop);
    void updateMonitorGamma();
    void mute(bool, bool updateIconOnly = false) Q_DECL_OVERRIDE;
    bool startCapture(const QString &params, const QString &path, Mlt::Producer *p);
    bool stopCapture();
    void reparent();
    /** @brief Returns the action displaying record toolbar */
    QAction *recAction();
    void refreshIcons();
    /** @brief Send audio thumb data to qml for on monitor display */
    void prepareAudioThumb(int channels, QVariantList &audioCache);
    void refreshMonitorIfActive();
    void connectAudioSpectrum(bool activate);
    /** @brief Set a property on the Qml scene **/
    void setQmlProperty(const QString &name, const QVariant &value);
    void displayAudioMonitor(bool isActive);
    /** @brief Prepare split effect from timeline clip producer **/
    void activateSplit();
    /** @brief Clear monitor display **/
    void clearDisplay();
    /** @brief Seeks timeline without refreshing if monitor is not active **/
    void silentSeek(int pos);

protected:
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void resizeEvent(QResizeEvent *event) Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent *event) Q_DECL_OVERRIDE;

    /** @brief Move to another position on mouse wheel event.
     *
     * Moves towards the end of the clip/timeline on mouse wheel down/back, the
     * opposite on mouse wheel up/forward.
     * Ctrl + wheel moves by a second, without Ctrl it moves by a single frame. */
    void wheelEvent(QWheelEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void enterEvent(QEvent *event) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent *event) Q_DECL_OVERRIDE;
    virtual QStringList mimeTypes() const;

private:
    ClipController *m_controller;
    /** @brief The QQuickView that handles our monitor display (video and qml overlay) **/
    GLWidget *m_glMonitor;
    /** @brief Container for our QQuickView monitor display (QQuickView needs to be embedded) **/
    QWidget *m_glWidget;
    /** @brief Scrollbar for our monitor view, used when zooming the monitor **/
    QScrollBar *m_verticalScroll;
    /** @brief Scrollbar for our monitor view, used when zooming the monitor **/
    QScrollBar *m_horizontalScroll;
    /** @brief The ruler widget displaying cursor position **/
    SmallRuler *m_ruler;
    /** @brief Widget holding the window for the QQuickView **/
    QWidget *m_videoWidget;
    /** @brief Manager for qml overlay for the QQuickView **/
    QmlManager *m_qmlManager;

    Mlt::Filter *m_splitEffect;
    Mlt::Producer *m_splitProducer;
    int m_length;
    bool m_dragStarted;
    //TODO: Move capture stuff in own class
    RecManager *m_recManager;
    /** @brief The widget showing current time position **/
    TimecodeDisplay *m_timePos;
    KDualAction *m_playAction;
    KSelectAction *m_forceSize;
    /** Has to be available so we can enable and disable it. */
    QAction *m_loopClipAction;
    QAction *m_sceneVisibilityAction;
    QAction *m_zoomVisibilityAction;
    QAction *m_multitrackView;
    QMenu *m_contextMenu;
    QMenu *m_configMenu;
    QMenu *m_playMenu;
    QMenu *m_markerMenu;
    QPoint m_DragStartPosition;
    /** Selected clip/transition in timeline. Used for looping it. */
    AbstractClipItem *m_selectedClip;
    /** true if selected clip is transition, false = selected clip is clip.
     *  Necessary because sometimes we get two signals, e.g. we get a clip and we get selected transition = nullptr. */
    bool m_loopClipTransition;
    GenTime getSnapForPos(bool previous);
    QToolBar *m_toolbar;
    QToolButton *m_audioButton;
    QSlider *m_audioSlider;
    QAction *m_editMarker;
    KMessageWidget *m_infoMessage;
    int m_forceSizeFactor;
    MonitorSceneType m_lastMonitorSceneType;
    MonitorAudioLevel *m_audioMeterWidget;
    QElapsedTimer m_droppedTimer;
    double m_displayedFps;
    void adjustScrollBars(float horizontal, float vertical);
    void loadQmlScene(MonitorSceneType type);
    void updateQmlDisplay(int currentOverlay);
    /** @brief Connect qml on monitor toolbar buttons */
    void connectQmlToolbar(QQuickItem *root);
    /** @brief Check and display dropped frames */
    void checkDrops(int dropped);
    /** @brief Create temporary Mlt::Tractor holding a clip and it's effectless clone */
    void buildSplitEffect(Mlt::Producer *original, int pos);

private slots:
    void seekCursor(int pos);
    void rendererStopped(int pos);
    void slotExtractCurrentFrame(QString frameName = QString(), bool addToProject = false);
    void slotExtractCurrentFrameToProject();
    void slotSetThumbFrame();
    void slotSaveZone();
    void slotSeek();
    void setClipZone(const QPoint &pos);
    void slotGoToMarker(QAction *action);
    void slotSetVolume(int volume);
    void slotEditMarker();
    void slotExtractCurrentZone();
    void onFrameDisplayed(const SharedFrame &frame);
    void slotStartDrag();
    void setZoom();
    void slotEnableEffectScene(bool enable);
    void slotAdjustEffectCompare();
    void slotShowMenu(const QPoint pos);
    void slotForceSize(QAction *a);
    void slotSeekToKeyFrame();
    /** @brief Display a non blocking error message to user **/
    void warningMessage(const QString &text, int timeout = 5000, const QList<QAction *> &actions = QList<QAction *>());
    void slotLockMonitor(bool lock);
    void slotAddEffect(const QDomElement &effect);
    void slotSwitchPlay();
    void slotEditInlineMarker();
    /** @brief Pass keypress event to mainwindow */
    void doKeyPressEvent(QKeyEvent *);
    /** @brief The timecode was updated, refresh qml display */
    void slotUpdateQmlTimecode(const QString &tc);
    /** @brief There was an error initializing Movit */
    void gpuError();
    void setOffsetX(int x);
    void setOffsetY(int y);
    /** @brief Show/hide monitor zoom */
    void slotEnableSceneZoom(bool enable);
    /** @brief Pan monitor view */
    void panView(QPoint diff);

public slots:
    void slotOpenDvdFile(const QString &);
    //void slotSetClipProducer(DocClipBase *clip, QPoint zone = QPoint(), bool forceUpdate = false, int position = -1);
    void updateClipProducer(Mlt::Producer *prod);
    void updateClipProducer(const QString &playlist);
    void slotOpenClip(ClipController *controller, int in = -1, int out = -1);
    void slotRefreshMonitor(bool visible);
    void slotSeek(int pos);
    void stop() Q_DECL_OVERRIDE;
    void start() Q_DECL_OVERRIDE;
    void switchPlay(bool play);
    void slotPlay() Q_DECL_OVERRIDE;
    void pause();
    void slotPlayZone();
    void slotLoopZone();
    /** @brief Loops the selected item (clip or transition). */
    void slotLoopClip();
    void slotForward(double speed = 0);
    void slotRewind(double speed = 0);
    void slotRewindOneFrame(int diff = 1);
    void slotForwardOneFrame(int diff = 1);
    //void saveSceneList(const QString &path, const QDomElement &info = QDomElement());
    void slotStart();
    void slotEnd();
    void slotSetZoneStart();
    void slotSetZoneEnd(bool discardLastFrame = false);
    void slotZoneStart();
    void slotZoneEnd();
    void slotZoneMoved(int start, int end);
    void slotSeekToNextSnap();
    void slotSeekToPreviousSnap();
    void adjustRulerSize(int length, int offset = 0);
    void setTimePos(const QString &pos);
    QPoint getZoneInfo() const;
    /** @brief Display the on monitor effect scene (to adjust geometry over monitor). */
    void slotShowEffectScene(MonitorSceneType sceneType, bool temporary = false);
    bool effectSceneDisplayed(MonitorSceneType effectType);
    /** @brief split screen to compare clip with and without effect */
    void slotSwitchCompare(bool enable, int pos);
    /** @brief Sets m_selectedClip to @param item. Used for looping it. */
    void slotSetSelectedClip(AbstractClipItem *item);
    void slotSetSelectedClip(ClipItem *item);
    void slotSetSelectedClip(Transition *item);
    void slotMouseSeek(int eventDelta, int modifiers) Q_DECL_OVERRIDE;
    void slotSwitchFullScreen(bool minimizeOnly = false) Q_DECL_OVERRIDE;
    /** @brief Display or hide the record toolbar */
    void slotSwitchRec(bool enable);
    /** @brief Request QImage of current frame */
    void slotGetCurrentImage(bool request);
    /** @brief Enable/disable display of monitor's audio levels widget */
    void slotSwitchAudioMonitor();

signals:
    void renderPosition(int);
    void durationChanged(int);
    void refreshClipThumbnail(const QString &);
    void zoneUpdated(const QPoint &);
    /** @brief  Editing transitions / effects over the monitor requires the renderer to send frames as QImage.
     *      This causes a major slowdown, so we only enable it if required */
    void requestFrameForAnalysis(bool);
    /** @brief Request a zone extraction (ffmpeg transcoding). */
    void extractZone(const QString &id);
    void effectChanged(const QRect &);
    void effectPointsChanged(const QVariantList &);
    void addKeyframe();
    void deleteKeyframe();
    void seekToNextKeyframe();
    void seekToPreviousKeyframe();
    void seekToKeyframe(int);
    void addClipToProject(const QUrl &);
    void showConfigDialog(int, int);
    /** @brief Request display of current bin clip. */
    void refreshCurrentClip();
    void addEffect(const QDomElement &);
    void addMasterEffect(const QString &, const QDomElement &);
    void passKeyPress(QKeyEvent *);
    /** @brief Update the text of a clip marker. */
    void updateClipMarker(const QString &, const QList<CommentedTime> &);
    /** @brief Update the text of a timeline guide. */
    void updateGuide(int, const QString &);
    /** @brief Enable / disable project monitor multitrack view (split view with one track in each quarter). */
    void multitrackView(bool);
    void requestAudioThumb(const QString &);
    void timeCodeUpdated(const QString &);
    void addMarker();
    void deleteMarker(bool deleteGuide = true);
    void seekToPreviousSnap();
    void seekToNextSnap();
    void createSplitOverlay(Mlt::Filter *);
    void removeSplitOverlay();
    void acceptRipple(bool);
    void switchTrimMode(int);
};

#endif
