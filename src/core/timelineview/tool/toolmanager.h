/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TOOLMANAGER_H
#define TOOLMANAGER_H

#include <QObject>
#include "kdenlivecore_export.h"

class SceneTool;
class TimelineClipItem;
class TimelineScene;
class QEvent;


class KDENLIVECORE_EXPORT ToolManager : public QObject
{
    Q_OBJECT

public:
    explicit ToolManager(QObject* parent = 0);

    void clipEvent(TimelineClipItem *clipItem, QEvent *event);
    void sceneEvent(TimelineScene *scene, QEvent *event);

    void setActiveTool(SceneTool *tool);

private:
    SceneTool *m_activeTool;
    
};

#endif
