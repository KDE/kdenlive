/*
Copyright (C) 2020  Julius Künzel <jk.kdedev@smartlab.uber.space>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "dockareaorientationmanager.h"
#include "core.h"
#include "mainwindow.h"
#include <klocalizedstring.h>

DockAreaOrientationManager::DockAreaOrientationManager(QObject *parent)
    : QObject(parent)
{
    m_verticalAction = new QAction(QIcon::fromTheme(QStringLiteral("object-columns")), i18n("Arrange Dock Areas In Columns"), this);
    pCore->window()->addAction(QStringLiteral("vertical_dockareaorientation"), m_verticalAction);
    connect(m_verticalAction, &QAction::triggered, this, &DockAreaOrientationManager::slotVerticalOrientation);

    m_horizontalAction = new QAction(QIcon::fromTheme(QStringLiteral("object-rows")), i18n("Arrange Dock Areas In Rows"), this);
    pCore->window()->addAction(QStringLiteral("horizontal_dockareaorientation"), m_horizontalAction);
    connect(m_horizontalAction, &QAction::triggered, this, &DockAreaOrientationManager::slotHorizontalOrientation);
}

void DockAreaOrientationManager::slotVerticalOrientation()
{
    // Use the corners for left and right DockWidgetArea
    pCore->window()->setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
    pCore->window()->setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    pCore->window()->setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
    pCore->window()->setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
}

void DockAreaOrientationManager::slotHorizontalOrientation()
{
    // Use the corners for top and bottom DockWidgetArea
    pCore->window()->setCorner(Qt::TopRightCorner, Qt::TopDockWidgetArea);
    pCore->window()->setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);
    pCore->window()->setCorner(Qt::TopLeftCorner, Qt::TopDockWidgetArea);
    pCore->window()->setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
}
