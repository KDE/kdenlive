/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef SELECTCLIPITEMTOOL_H
#define SELECTCLIPITEMTOOL_H

#include "core/timelineview/tool/abstractclipitemtool.h"


class SelectClipItemTool : public AbstractClipItemTool
{
    Q_OBJECT

public:
    explicit SelectClipItemTool(QObject* parent = 0);

    virtual void clipEvent(TimelineClipItem *clipItem, QEvent *event);
};

#endif
