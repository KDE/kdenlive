/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "timelineview.h"
#include "timelinescene.h"
#include "timelinepositionbar.h"
#include "project/timeline.h"
#include "project/producerwrapper.h"
#include "monitor/monitormodel.h"
#include <QWheelEvent>
#include <QScrollBar>


TimelineView::TimelineView(QWidget* parent) :
    QGraphicsView(parent),
    m_scene(0),
    m_zoomLevel(-1)
{
    setFrameShape(QFrame::NoFrame);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);

    setMouseTracking(true);
}

TimelineView::~TimelineView()
{
}

void TimelineView::setScene(TimelineScene* scene)
{
    QGraphicsView::setScene(scene);
    m_scene = scene;

    connect(m_scene, SIGNAL(heightChanged(int)), this, SLOT(setHeight(int)));

    m_scene->positionTracks();
    setZoom(m_scene->timeline()->producer()->get_int("timelineview_zoom"));
}

double TimelineView::scale() const
{
    return (double) /*frameWidth()*/90 / TimelinePositionBar::comboScale[m_zoomLevel];
}

void TimelineView::setZoom(int level)
{
    if (!m_scene) {
        return;
    }

    // not so nice to hardcode it here
    level = qBound(0, level, 13);

    if (level == m_zoomLevel) {
        return;
    }

    m_zoomLevel = level;
    if (m_scene) {
        m_scene->timeline()->producer()->set("timelineview_zoom", m_zoomLevel);
    }

    double scale = this->scale();

    QMatrix scaleMatrix;
    scaleMatrix = scaleMatrix.scale(scale, transform().m22());

    setMatrix(scaleMatrix);

    int diff = sceneRect().width() - m_scene->timeline()->duration();
    if (diff * scaleMatrix.m11() < 50) {
        if (scaleMatrix.m11() < 0.4) {
            setSceneRect(0, 0, (m_scene->timeline()->duration() + 100 / scaleMatrix.m11()), sceneRect().height());
        } else {
            setSceneRect(0, 0, (m_scene->timeline()->duration() + 300), sceneRect().height());
        }
    }

    emit zoomChanged(level);

//     double verticalPos = mapToScene(QPoint(0, viewport()->height() / 2)).y();
//     centerOn(QPointF(cursorPos(), verticalPos));
}

void TimelineView::zoomIn()
{
    setZoom(m_zoomLevel - 1);
}

void TimelineView::zoomOut()
{
    setZoom(m_zoomLevel + 1);
}

void TimelineView::zoomFit()
{

}

void TimelineView::setHeight(int height)
{
    setSceneRect(0, 0, sceneRect().width(), height);
}

void TimelineView::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() == Qt::ControlModifier) {
        if (event->delta() > 0) {
            zoomIn();
        } else {
            zoomOut();
        }
    } else {
        if (event->delta() > 0) {
            horizontalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepSub);
        } else {
            horizontalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
        }
    }
}

void TimelineView::mousePressEvent(QMouseEvent* event)
{
    QGraphicsView::mousePressEvent(event);
    m_scene->timeline()->monitor()->activate();
    // HACK: use only one consumer. using two is buggy
//     m_scene->timeline()->monitor()->setProducer(m_scene->timeline()->producer());
}

#include "timelineview.moc"
