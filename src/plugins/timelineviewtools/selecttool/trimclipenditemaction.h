/*
Copyright (C) 2013  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TRIMCLIPENDITEMACTION_H
#define TRIMCLIPENDITEMACTION_H

#include "core/timelineview/tool/abstractclipitemaction.h"


class TrimClipEndItemAction : public AbstractClipItemAction
{
    Q_OBJECT

public:
    TrimClipEndItemAction(TimelineClipItem *clipItem, QEvent *initialEvent, QObject *parent = 0);
    ~TrimClipEndItemAction();

protected:
    void mouseMove(QGraphicsSceneMouseEvent *event);
    void mousePress(QGraphicsSceneMouseEvent *event);
    void mouseRelease(QGraphicsSceneMouseEvent *event);

private:
    int m_original;
    int m_boundLeft;
    int m_boundRight;
};

#endif
