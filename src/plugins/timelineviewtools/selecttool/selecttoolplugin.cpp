/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "selecttoolplugin.h"
#include "selectclipitemtool.h"
#include "core/core.h"
#include "core/mainwindow.h"
#include "core/timelineview/timelinewidget.h"
#include "core/timelineview/tool/toolmanager.h"
#include "core/timelineview/tool/scenetool.h"
#include <KPluginFactory>
#include <KActionCollection>
#include <KAction>
#include <KLocalizedString>


K_PLUGIN_FACTORY( SelectToolPluginFactory, registerPlugin<SelectToolPlugin>(); )
K_EXPORT_PLUGIN( SelectToolPluginFactory( "kdenliveselecttool" ) )

SelectToolPlugin::SelectToolPlugin(QObject* parent, const QVariantList &args) :
    QObject(parent),
    KXMLGUIClient(pCore->window())
{
    setXMLFile("selecttool_ui.rc");

    KAction* action = new KAction(KIcon("kdenlive-select-tool"), i18n("Selection Tool"), this);
//     action->setShortcut(i18nc("Selection tool shortcut", "s"));
//     action->setCheckable(true);
//     action->setChecked(true);
    actionCollection()->addAction("select_tool", action);
//     connect(action, SIGNAL(triggered(bool)

    m_tool = new SceneTool(this);
    m_tool->setClipTool(new SelectClipItemTool(this));

    // temporary HACK
    pCore->window()->timelineWidget()->toolManager()->setActiveTool(m_tool);
}
