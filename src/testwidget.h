/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef TESTWIDGET_H
#define TESTWIDGET_H

#include <QWidget>
#include "abstractscopewidget.h"
#include "ui_testwidget_ui.h"

class AbstractScopeWidget;
class TestWidget_UI;

class TestWidget : public AbstractScopeWidget {
    Q_OBJECT
public:
    TestWidget(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent = 0);
    virtual ~TestWidget();

    QString widgetName() const;

    /// Implemented methods ///
    QImage renderHUD();
    QImage renderScope();
    QImage renderBackground();
    bool isHUDDependingOnInput() const;
    bool isScopeDependingOnInput() const;
    bool isBackgroundDependingOnInput() const;

protected:
    QRect scopeRect();

private:
    Ui::TestWidget_UI *ui;

    QAction *m_aTest;

};

#endif // TESTWIDGET_H
