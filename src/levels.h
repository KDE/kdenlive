/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef LEVELS_H
#define LEVELS_H

#include "abstractscopewidget.h"
#include "ui_levels_ui.h"

class LevelsGenerator;

class Levels : public AbstractScopeWidget {
    Q_OBJECT

public:
    Levels(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent = 0);
    ~Levels();
    QString widgetName() const;


private:
    LevelsGenerator *m_levelsGenerator;
    QAction *m_aUnscaled;

    QRect scopeRect();
    bool isHUDDependingOnInput() const;
    bool isScopeDependingOnInput() const;
    bool isBackgroundDependingOnInput() const;
    QImage renderHUD(uint accelerationFactor);
    QImage renderScope(uint accelerationFactor);
    QImage renderBackground(uint accelerationFactor);
    Ui::Levels_UI *ui;

};

#endif // LEVELS_H
