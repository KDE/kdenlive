/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "trackheadercontainer.h"
#include "trackheaderwidget.h"
#include "project/timeline.h"
#include "project/timelinetrack.h"
#include <QVBoxLayout>
#include <QLayout>


TrackHeaderContainer::TrackHeaderContainer(QWidget* parent) :
    QScrollArea(parent)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // ?
    setFixedWidth(70);
    setFrameShape(QFrame::NoFrame);

    m_container = new QWidget(this);

    QVBoxLayout *layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    setWidget(m_container);
    setWidgetResizable(true);
}

void TrackHeaderContainer::setTimeline(Timeline* timeline)
{
    QLayoutItem *layoutItem;
    while ((layoutItem = m_container->layout()->takeAt(0)) != 0) {
        QWidget *widget = layoutItem->widget();
        delete layoutItem;
        if (widget) {
            widget->deleteLater();
        }
    }

    foreach (TimelineTrack *track, timeline->tracks()) {
        TrackHeaderWidget *trackHeaderWidget = new TrackHeaderWidget(track, m_container);
        m_container->layout()->addWidget(trackHeaderWidget);
    }
    static_cast<QVBoxLayout*>(m_container->layout())->addStretch();
}

void TrackHeaderContainer::adjustHeight(int min, int max)
{
    m_container->layout()->setContentsMargins(0, 0, 0, (max > 0) * QApplication::style()->pixelMetric(QStyle::PM_ScrollBarExtent));
}
