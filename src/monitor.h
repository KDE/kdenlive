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

class MonitorManager;
class Render;
class SmallRuler;
class DocClipBase;

class MonitorRefresh : public QWidget {
    Q_OBJECT
public:
    MonitorRefresh(QWidget* parent);
    virtual void paintEvent(QPaintEvent * event);
    void setRenderer(Render* render);

private:
    Render *m_renderer;
};

class Overlay : public QLabel {
    Q_OBJECT
public:
    Overlay(QWidget* parent);
    virtual void paintEvent(QPaintEvent * event);
    void setOverlayText(const QString &, bool isZone = true);

private:
    bool m_isZone;
};

class Monitor : public QWidget {
    Q_OBJECT

public:
    Monitor(QString name, MonitorManager *manager, QWidget *parent = 0);
    virtual ~Monitor();
    Render *render;
    void resetProfile();
    QString name() const;
    void resetSize();
    bool isActive() const;
    void pause();
    void setupMenu(QMenu *goMenu, QAction *playZone, QAction *loopZone, QMenu *markerMenu = NULL);
    QDomDocument sceneList();
    DocClipBase *activeClip();
    GenTime position();
    void checkOverlay();

protected:
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseReleaseEvent(QMouseEvent * event);
    virtual void wheelEvent(QWheelEvent * event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual QStringList mimeTypes() const;
    /*    virtual void dragMoveEvent(QDragMoveEvent * event);
        virtual Qt::DropActions supportedDropActions() const;*/

//    virtual void resizeEvent(QResizeEvent * event);
//    virtual void paintEvent(QPaintEvent * event);

private:
    Ui::Monitor_UI ui;
    MonitorManager *m_monitorManager;
    MonitorRefresh *m_monitorRefresh;
    QString m_name;
    double m_scale;
    int m_length;
    int m_position;
    SmallRuler *m_ruler;
    KIcon m_playIcon;
    KIcon m_pauseIcon;
    bool m_isActive;
    KRestrictedLine *m_timePos;
    QAction *m_playAction;
    QMenu *m_contextMenu;
    QMenu *m_configMenu;
    QMenu *m_playMenu;
    DocClipBase *m_currentClip;
    QPoint m_DragStartPosition;
    bool m_dragStarted;
    Overlay *m_overlay;
    GenTime getSnapForPos(bool previous);

private slots:
    void adjustRulerSize(int length);
    void seekCursor(int pos);
    void rendererStopped(int pos);
    void slotExtractCurrentFrame();
    void slotSetThumbFrame();
    void slotSetSizeOneToOne();
    void slotSetSizeOneToTwo();
    void slotSaveZone();
    void slotSeek();

public slots:
    void slotOpenFile(const QString &);
    void slotSetXml(DocClipBase *clip, const int position = -1);
    void initMonitor();
    void refreshMonitor(bool visible);
    void slotSeek(int pos);
    void stop();
    void start();
    void activateMonitor();
    void slotPlay();
    void slotPlayZone();
    void slotLoopZone();
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

signals:
    void renderPosition(int);
    void durationChanged(int);
    void refreshClipThumbnail(const QString &);
    void adjustMonitorSize();
    void zoneUpdated(QPoint);
    void saveZone(Render *, QPoint);
};

#endif
