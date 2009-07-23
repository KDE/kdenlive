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


#include <QWidget>
#include <QDomElement>
#include <QGraphicsPathItem>

#include <mlt++/Mlt.h>

#include "ui_geometryval_ui.h"
#include "definitions.h"
#include "keyframehelper.h"
#include "ui_geometryposition_ui.h"

//class QGraphicsScene;
class GraphicsSceneRectMove;
class QGraphicsRectItem;
class QMouseEvent;


class Geometryval : public QWidget
{
    Q_OBJECT
public:
    explicit Geometryval(const MltVideoProfile profile, QPoint frame_size, QWidget* parent = 0);
    virtual ~Geometryval();
    QDomElement getParamDesc();
    void setFrameSize(QPoint p);

private:
    Ui::Geometryval m_ui;
    MltVideoProfile m_profile;
    GraphicsSceneRectMove *m_scene;
    QDomElement m_param;
    QGraphicsRectItem *m_paramRect;
    Mlt::Geometry *m_geom;
    KeyframeHelper *m_helper;
    QGraphicsPathItem *m_path;
    QMenu *m_configMenu;
    QMenu *m_scaleMenu;
    QMenu *m_alignMenu;
    QAction *m_syncAction;
    QAction *m_editGeom;
    bool m_fixedMode;
    QPoint m_frameSize;
    Ui::GeometryPosition_UI m_view;
    void updateTransitionPath();

public slots:
    void setupParam(const QDomElement, int, int);

private slots:
    void slotNextFrame();
    void slotPreviousFrame();
    void slotPositionChanged(int pos, bool seek = true);
    void slotDeleteFrame();
    void slotAddFrame();
    void slotUpdateTransitionProperties();
    void slotTransparencyChanged(int transp);
    void slotResize50();
    void slotResize100();
    void slotResize200();
    void slotResizeCustom();
    void slotResizeOriginal();
    void slotAlignRight();
    void slotAlignLeft();
    void slotAlignTop();
    void slotAlignBottom();
    void slotAlignCenter();
    void slotAlignHCenter();
    void slotAlignVCenter();
    void slotSyncCursor();
    void slotGeometry();
    void slotResetPosition();
    void slotKeyframeMoved(int);

signals:
    void parameterChanged();
    void seekToPos(int);
};

#endif
