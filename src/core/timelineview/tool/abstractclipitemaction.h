/*
Copyright (C) 2013  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ABSTRACTCLIPITEMACTION_H
#define ABSTRACTCLIPITEMACTION_H

#include <QObject>
#include "kdenlivecore_export.h"

class TimelineClipItem;
class QGraphicsSceneMouseEvent;


class KDENLIVECORE_EXPORT AbstractClipItemAction : public QObject
{
    Q_OBJECT

public:
    AbstractClipItemAction(TimelineClipItem *clipItem, QEvent* initialEvent, QObject *parent = 0);
    virtual ~AbstractClipItemAction();

public slots:
    virtual void slotEvent(QEvent *event);

signals:
    void finished();

protected:
    virtual void mouseMove(QGraphicsSceneMouseEvent *event);
    virtual void mousePress(QGraphicsSceneMouseEvent *event);
    virtual void mouseRelease(QGraphicsSceneMouseEvent *event);

    TimelineClipItem * m_clip;
};

#endif
