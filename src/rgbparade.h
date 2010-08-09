/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef RGBPARADE_H
#define RGBPARADE_H

#include <QObject>
#include "abstractscopewidget.h"
#include "ui_rgbparade_ui.h"

class Monitor;
class QImage;
class RGBParade_UI;
class RGBParadeGenerator;

class RGBParade : public AbstractScopeWidget
{
public:
    RGBParade(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent = 0);
    ~RGBParade();
    QString widgetName() const;

protected:
    QRect scopeRect();

private:
    Ui::RGBParade_UI *ui;
    RGBParadeGenerator *m_rgbParadeGenerator;

    bool isHUDDependingOnInput() const;
    bool isScopeDependingOnInput() const;
    bool isBackgroundDependingOnInput() const;

    QImage renderHUD(uint accelerationFactor);
    QImage renderScope(uint accelerationFactor, QImage);
    QImage renderBackground(uint accelerationFactor);
};

#endif // RGBPARADE_H
