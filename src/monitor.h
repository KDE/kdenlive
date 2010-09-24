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

#include <QLabel>

#include <KIcon>
#include <KAction>
#include <KRestrictedLine>
#include <QDomElement>

#include "gentime.h"
#include "ui_monitor_ui.h"
#include "timecodedisplay.h"
#ifdef Q_WS_MAC
#include "videoglwidget.h"
#endif

class MonitorManager;
class Render;
class SmallRuler;
class DocClipBase;
class MonitorScene;
class AbstractClipItem;
class Transition;
class ClipItem;
class QGraphicsView;
class QGraphicsPixmapItem;

class MonitorRefresh : public QWidget
{
    Q_OBJECT
public:
    MonitorRefresh(QWidget *parent = 0);
    virtual void paintEvent(QPaintEvent *event);
    void setRenderer(Render* render);

private:
    Render *m_renderer;
};

class Overlay : public QLabel
{
    Q_OBJECT
public:
    Overlay(QWidget* parent);
    void setOverlayText(const QString &, bool isZone = true);

private:
    bool m_isZone;
};

class Monitor : public QWidget
{
    Q_OBJECT

public:
    Monitor(QString name, MonitorManager *manager, QString profile = QString(), QWidget *parent = 0);
    virtual ~Monitor();
    Render *render;
    void resetProfile(const QString profile);
    QString name() const;
    void resetSize();
    bool isActive() const;
    void pause();
    void setupMenu(QMenu *goMenu, QAction *playZone, QAction *loopZone, QMenu *markerMenu = NULL, QAction *loopClip = NULL);
    const QString sceneList();
    DocClipBase *activeClip();
    GenTime position();
    void checkOverlay();
    void updateTimecodeFormat();
    void updateMarkers(DocClipBase *source);
    MonitorScene *getEffectScene();

protected:
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseReleaseEvent(QMouseEvent * event);

    /** @brief Move to another position on mouse wheel event.
     *
     * Moves towards the end of the clip/timeline on mouse wheel down/back, the
     * opposite on mouse wheel up/forward.
     * Ctrl + wheel moves by a second, without Ctrl it moves by a single frame. */
    virtual void wheelEvent(QWheelEvent * event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual QStringList mimeTypes() const;
    /*virtual void dragMoveEvent(QDragMoveEvent * event);
    virtual Qt::DropActions supportedDropActions() const;*/

    //virtual void resizeEvent(QResizeEvent * event);
    //virtual void paintEvent(QPaintEvent * event);

private:
    Ui::Monitor_UI m_ui;
    QString m_name;
    MonitorManager *m_monitorManager;
    DocClipBase *m_currentClip;
    SmallRuler *m_ruler;
    Overlay *m_overlay;
    bool m_isActive;
    double m_scale;
    int m_length;
    bool m_dragStarted;
    MonitorRefresh *m_monitorRefresh;
    KIcon m_playIcon;
    KIcon m_pauseIcon;
    TimecodeDisplay *m_timePos;
    QAction *m_playAction;
    /** Has to be available so we can enable and disable it. */
    QAction *m_loopClipAction;
    QMenu *m_contextMenu;
    QMenu *m_configMenu;
    QMenu *m_playMenu;
    QMenu *m_markerMenu;
    QPoint m_DragStartPosition;
    MonitorScene *m_effectScene;
    QGraphicsView *m_effectView;
    /** Selected clip/transition in timeline. Used for looping it. */
    AbstractClipItem *m_selectedClip;
    /** true if selected clip is transition, false = selected clip is clip.
     *  Necessary because sometimes we get two signals, e.g. we get a clip and we get selected transition = NULL. */
    bool m_loopClipTransition;
#ifdef Q_WS_MAC
    VideoGLWidget *m_glWidget;
#endif

    GenTime getSnapForPos(bool previous);

private slots:
    void seekCursor(int pos);
    void rendererStopped(int pos);
    void slotExtractCurrentFrame();
    void slotSetThumbFrame();
    void slotSetSizeOneToOne();
    void slotSetSizeOneToTwo();
    void slotSaveZone();
    void slotSeek();
    void setClipZone(QPoint pos);
    void slotSwitchMonitorInfo(bool show);
    void slotSwitchDropFrames(bool show);
    void slotGoToMarker(QAction *action);

public slots:
    void slotOpenFile(const QString &);
    void slotSetXml(DocClipBase *clip, QPoint zone = QPoint(), const int position = -1);
    void initMonitor();
    void refreshMonitor(bool visible = true);
    void slotSeek(int pos);
    void stop();
    void start();
    void activateMonitor();
    void slotPlay();
    void slotPlayZone();
    void slotLoopZone();
    /** @brief Loops the selected item (clip or transition). */
    void slotLoopClip();
    void slotForward(double speed = 0);
    void slotRewind(double speed = 0);
    void slotRewindOneFrame(int diff = 1);
    void slotForwardOneFrame(int diff = 1);
    void saveSceneList(QString path, QDomElement info = QDomElement());
    void slotStart();
    void slotEnd();
    void slotSetZoneStart();
    void slotSetZoneEnd();
    void slotZoneStart();
    void slotZoneEnd();
    void slotZoneMoved(int start, int end);
    void slotSeekToNextSnap();
    void slotSeekToPreviousSnap();
    void adjustRulerSize(int length);
    void setTimePos(const QString &pos);
    QStringList getZoneInfo() const;
    void slotEffectScene(bool show = true);
    bool effectSceneDisplayed();

    /** @brief Sets m_selectedClip to @param item. Used for looping it. */
    void slotSetSelectedClip(AbstractClipItem *item);
    void slotSetSelectedClip(ClipItem *item);
    void slotSetSelectedClip(Transition *item);

signals:
    void renderPosition(int);
    void durationChanged(int);
    void refreshClipThumbnail(const QString &);
    void adjustMonitorSize();
    void zoneUpdated(QPoint);
    void saveZone(Render *, QPoint);
    /** @brief  Editing transitions / effects over the monitor requires the renderer to send frames as QImage.
     *      This causes a major slowdown, so we only enable it if required */
    void requestFrameForAnalysis(bool);
};

#endif
