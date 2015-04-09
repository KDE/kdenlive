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
#include "monitor/sharedframe.h"


#include <QLabel>
#include <QDomElement>
#include <QToolBar>
#include <QWindow>
#include <QIcon>

class SmallRuler;
class ClipController;
class AbstractClipItem;
class Transition;
class ClipItem;
class Monitor;
class MonitorManager;
class QSlider;
class TwostateAction;
class QQuickItem;
class QScrollBar;

class Monitor : public AbstractMonitor
{
    Q_OBJECT

public:
    Monitor(Kdenlive::MonitorId id, MonitorManager *manager, QWidget *parent = 0);
    ~Monitor();
    Render *render;
    AbstractRender *abstractRender();
    void resetProfile(const QString &profile);
    void setCustomProfile(const QString &profile, const Timecode &tc);
    void resetSize();
    void pause();
    void setupMenu(QMenu *goMenu, QAction *playZone, QAction *loopZone, QMenu *markerMenu = NULL, QAction *loopClip = NULL, QWidget* parent = NULL);
    const QString sceneList();
    const QString activeClipId();
    GenTime position();
    /** @brief Check current position to show relevant infos in qml view (markers, zone in/out, etc). */
    void checkOverlay();
    void updateTimecodeFormat();
    void updateMarkers();
    /** @brief Controller for the clip currently displayed (only valid for clip monitor). */
    ClipController *currentController() const;
    void setMarkers(const QList <CommentedTime> &markers);
    void reloadProducer(const QString &id);
    QFrame *m_volumePopup;
    /** @brief Reimplemented from QWidget, updates the palette colors. */
    void setPalette ( const QPalette & p);
    /** @brief Returns a hh:mm:ss timecode from a frame number. */
    QString getTimecodeFromFrames(int pos);
    /** @brief Returns current project's fps. */
    double fps() const;
    /** @brief Get url for the clip's thumbnail */
    QString getMarkerThumb(GenTime pos);

    int getZoneStart();
    int getZoneEnd();
    void setUpEffectGeometry(QRect r);
    void setUpEffectGeometry(int x, int y, int w, int h);
    QRect effectRect() const;
    void setEffectKeyframe(bool enable);
    void sendFrameForAnalysis(bool analyse);

protected:
    void mousePressEvent(QMouseEvent * event);
    void mouseReleaseEvent(QMouseEvent * event);
    void mouseDoubleClickEvent(QMouseEvent * event);
    void resizeEvent(QResizeEvent *event);

    /** @brief Move to another position on mouse wheel event.
     *
     * Moves towards the end of the clip/timeline on mouse wheel down/back, the
     * opposite on mouse wheel up/forward.
     * Ctrl + wheel moves by a second, without Ctrl it moves by a single frame. */
    void wheelEvent(QWheelEvent * event);
    void mouseMoveEvent(QMouseEvent *event);
    virtual QStringList mimeTypes() const;

private:
    ClipController *m_controller;
    /** @brief The QQuickView that handles our monitor display (video and qml overlay) **/
    GLWidget *m_glMonitor;
    /** @brief Container for our QQuickView monitor display (QQuickView needs to be embeded) **/
    QWidget *m_glWidget;
    /** @brief Scrollbar for our monitor view, used when zooming the monitor **/
    QScrollBar *m_verticalScroll;
    /** @brief Scrollbar for our monitor view, used when zooming the monitor **/
    QScrollBar *m_horizontalScroll;
    /** @brief The ruler widget displaying cursor position **/
    SmallRuler *m_ruler;
    
    Mlt::Filter *m_splitEffect;
    Mlt::Producer *m_splitProducer;
    int m_length;
    bool m_dragStarted;
    QIcon m_playIcon;
    QIcon m_pauseIcon;
    /** @brief The widget showing current time position **/
    TimecodeDisplay *m_timePos;
    TwostateAction *m_playAction;
    /** Has to be available so we can enable and disable it. */
    QAction *m_loopClipAction;
    QMenu *m_contextMenu;
    QMenu *m_configMenu;
    QMenu *m_playMenu;
    QMenu *m_markerMenu;
    QPoint m_DragStartPosition;
    /** Selected clip/transition in timeline. Used for looping it. */
    AbstractClipItem *m_selectedClip;
    /** true if selected clip is transition, false = selected clip is clip.
     *  Necessary because sometimes we get two signals, e.g. we get a clip and we get selected transition = NULL. */
    bool m_loopClipTransition;
    /** @brief Is the qml overlay displayed in the monitor **/
    bool m_showEffectScene;
    GenTime getSnapForPos(bool previous);
    QToolBar *m_toolbar;
    QWidget *m_volumeWidget;
    QSlider *m_audioSlider;
    QAction *m_editMarker;
    /** @brief The base item of the qml view in monitor, used to set properties on the view that affect display **/
    QQuickItem *m_rootItem;
    /** @brief Switch the QQuickView monitor between normal and fullscreen mode **/
    void switchFullScreen();
    void adjustScrollBars(float horizontal, float vertical);
    void loadMasterQml();

private slots:
    void seekCursor(int pos);
    void rendererStopped(int pos);
    void slotExtractCurrentFrame();
    void slotSetThumbFrame();
    void slotSetSizeOneToOne();
    void slotSetSizeOneToTwo();
    void slotSaveZone();
    void slotSeek();
    void setClipZone(const QPoint &pos);
    void slotSwitchMonitorInfo(bool show);
    void slotSwitchDropFrames(bool show);
    void slotGoToMarker(QAction *action);
    void slotSetVolume(int volume);
    void slotShowVolume();
    void slotEditMarker();
    void slotExtractCurrentZone();
    void onFrameDisplayed(const SharedFrame& frame);
    void slotStartDrag();
    void setZoom();
    void slotEnableEffectScene(bool enable);
    /** @brief split screen to compare clip with and without effect */
    void slotSwitchCompare(bool enable);
    void slotAdjustEffectCompare(double percent);

public slots:
    void slotOpenFile(const QString &);
    //void slotSetClipProducer(DocClipBase *clip, QPoint zone = QPoint(), bool forceUpdate = false, int position = -1);
    void updateClipProducer(Mlt::Producer *prod);
    void openClip(ClipController *controller);
    void openClipZone(ClipController *controller, int in, int out);
    void refreshMonitor(bool visible);
    void refreshMonitor();
    void slotSeek(int pos);
    void stop();
    void start();
    void slotPlay();
    void slotPlayZone();
    void slotLoopZone();
    /** @brief Loops the selected item (clip or transition). */
    void slotLoopClip();
    void slotForward(double speed = 0);
    void slotRewind(double speed = 0);
    void slotRewindOneFrame(int diff = 1);
    void slotForwardOneFrame(int diff = 1);
    void saveSceneList(const QString &path, const QDomElement &info = QDomElement());
    void slotStart();
    void slotEnd();
    void slotSetZoneStart();
    void slotSetZoneEnd();
    void slotZoneStart();
    void slotZoneEnd();
    void slotZoneMoved(int start, int end);
    void slotSeekToNextSnap();
    void slotSeekToPreviousSnap();
    void adjustRulerSize(int length, int offset = 0);
    void setTimePos(const QString &pos);
    QStringList getZoneInfo() const;
    /** @brief Display the on monitor effect scene (to adjust geometry over monitor). */
    void slotShowEffectScene(bool show = true, bool manuallyTriggered = false);
    bool effectSceneDisplayed();

    /** @brief Sets m_selectedClip to @param item. Used for looping it. */
    void slotSetSelectedClip(AbstractClipItem *item);
    void slotSetSelectedClip(ClipItem *item);
    void slotSetSelectedClip(Transition *item);
    void slotMouseSeek(int eventDelta, bool fast);
    void slotSwitchFullScreen();

signals:
    void renderPosition(int);
    void durationChanged(int);
    void refreshClipThumbnail(const QString &, bool);
    void adjustMonitorSize();
    void zoneUpdated(const QPoint&);
    //void saveZone(Render *, const QPoint&, DocClipBase *);
    /** @brief  Editing transitions / effects over the monitor requires the renderer to send frames as QImage.
     *      This causes a major slowdown, so we only enable it if required */
    void requestFrameForAnalysis(bool);
    /** @brief Request a zone extraction (ffmpeg transcoding). */
    void extractZone(const QString &id);
    void effectChanged(QRect r);
};

#endif
