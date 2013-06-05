/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "toolmanager.h"
#include "scenetool.h"
#include "timelineview/timelineclipitem.h"
#include "timelineview/timelinescene.h"
#include <KServiceTypeTrader>
#include <KPluginInfo>
#include <QEvent>


ToolManager::ToolManager(QObject* parent) :
    QObject(parent),
    m_activeTool(NULL)
{
}

void ToolManager::clipEvent(TimelineClipItem* clipItem, QEvent* event)
{
    if (!m_activeTool) return;
    m_activeTool->clipEvent(clipItem, event);
}

void ToolManager::sceneEvent(TimelineScene* scene, QEvent* event)
{
    if (!m_activeTool) {
	
	return;
    }
    m_activeTool->sceneEvent(scene, event);
}

void ToolManager::setActiveTool(SceneTool* tool)
{
    m_activeTool = tool;
}
