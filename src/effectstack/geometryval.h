/***************************************************************************
                          geomeytrval.h  -  description
                             -------------------
    begin                : 03 Aug 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef GEOMETRYVAL_H
#define GEOMETRYVAL_H

#include "keyframehelper.h"

#include "definitions.h"
#include "timecodedisplay.h"
#include "ui_geometryval_ui.h"

#include <QWidget>
#include <QDomElement>
#include <QGraphicsPathItem>

#include <mlt++/Mlt.h>

class GraphicsSceneRectMove;
class QGraphicsRectItem;
class QGraphicsView;

namespace Mlt
{
class Profile;
}

class Geometryval : public QWidget, public Ui::Geometryval
{
    Q_OBJECT
public:
    explicit Geometryval(const Mlt::Profile *profile, const Timecode &t, const QPoint &frame_size, int startPoint = 0, QWidget *parent = Q_NULLPTR);
    virtual ~Geometryval();
    QDomElement getParamDesc();
    QString getValue() const;
    void setFrameSize(const QPoint &p);
    /** @brief Updates the timecode display according to settings (frame number or hh:mm:ss:ff) */
    void updateTimecodeFormat();
    void slotUpdateRange(int inPoint, int outPoint);

private:
    const Mlt::Profile *m_profile;
    int m_realWidth;
    GraphicsSceneRectMove *m_scene;
    QGraphicsRectItem *m_paramRect;
    Mlt::Geometry *m_geom;
    KeyframeHelper *m_helper;
    QGraphicsPathItem *m_path;
    QMenu *m_configMenu;
    QAction *m_syncAction;
    QAction *m_editOptions;
    QAction *m_reset;
    bool m_fixedMode;
    QPoint m_frameSize;
    void updateTransitionPath();
    double m_dar;
    int m_startPoint;
    QGraphicsView *m_sceneview;
    TimecodeDisplay m_timePos;
    bool keyframeSelected();

public slots:
    void setupParam(const QDomElement &, int minframe, int maxframe);
    /** @brief Updates position of the local timeline to @param relTimelinePos.  */
    void slotSyncPosition(int relTimelinePos);

private slots:
    void slotNextFrame();
    void slotPreviousFrame();
    void slotPositionChanged(int pos = -1, bool seek = true);
    void slotDeleteFrame(int pos = -1);
    void slotAddFrame(int pos = -1);
    void slotUpdateTransitionProperties();
    void slotTransparencyChanged(int transp);
    void slotResizeCustom();
    void slotResizeOriginal();
    void slotAlignRight();
    void slotAlignLeft();
    void slotAlignTop();
    void slotAlignBottom();
    void slotAlignHCenter();
    void slotAlignVCenter();
    void slotSyncCursor();
    void slotResetPosition();
    void slotKeyframeMoved(int);
    void slotSwitchOptions();
    void slotUpdateGeometry();
    void slotGeometryX(int value);
    void slotGeometryY(int value);
    void slotGeometryWidth(int value);
    void slotGeometryHeight(int value);

signals:
    void parameterChanged();
    void seekToPos(int);
};

#endif
