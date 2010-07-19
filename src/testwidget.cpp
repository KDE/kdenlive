/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "testwidget.h"
#include "ui_testwidget_ui.h"

#include <QMenu>
#include <QDebug>

TestWidget::TestWidget(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
    AbstractScopeWidget(projMonitor, clipMonitor, parent)
{
    ui = new Ui::TestWidget_UI();
    ui->setupUi(this);

    m_aTest = new QAction("Hallo. ", this);
    m_menu->addAction(m_aTest);
}

TestWidget::~TestWidget()
{
    delete ui;
    delete m_aTest;
}

///// Implemented Methods /////

QImage TestWidget::renderHUD()
{
    emit signalHUDRenderingFinished();
    return QImage();
}

QImage TestWidget::renderScope()
{
    emit signalScopeRenderingFinished();
    return QImage();
}

QImage TestWidget::renderBackground()
{
    emit signalBackgroundRenderingFinished();
    return QImage();
}


QRect TestWidget::scopeRect()
{
    return QRect(QPoint(offset, ui->line->y() + 2*offset), this->rect().bottomRight() - QPoint(offset, offset));
}

QString TestWidget::widgetName() const
{
    return "Testwidget";
}
