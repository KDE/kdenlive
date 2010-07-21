/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <QRect>
#include <QTime>
#include "renderer.h"
#include "rgbparade.h"
#include "rgbparadegenerator.h"

RGBParade::RGBParade(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
        AbstractScopeWidget(projMonitor, clipMonitor, parent)
{
    ui = new Ui::RGBParade_UI();
    ui->setupUi(this);
    m_rgbParadeGenerator = new RGBParadeGenerator();
}

RGBParade::~RGBParade()
{
    delete ui;
    delete m_rgbParadeGenerator;
}

QString RGBParade::widgetName() const { return "RGB Parade"; }

QRect RGBParade::scopeRect()
{
    QPoint topleft(offset, ui->line->y() + 2*offset);
    return QRect(topleft, QPoint(this->size().width() - offset, this->size().height() - offset) - topleft);
}

QImage RGBParade::renderHUD(uint) { return QImage(); }
QImage RGBParade::renderScope(uint accelerationFactor)
{
    QTime start = QTime::currentTime();
    start.start();
    QImage parade = m_rgbParadeGenerator->calculateRGBParade(m_scopeRect.size(), m_activeRender->extractFrame(m_activeRender->seekFramePosition()),
                                                    true, accelerationFactor);
    emit signalScopeRenderingFinished(start.elapsed(), accelerationFactor);
    return parade;
}
QImage RGBParade::renderBackground(uint) { return QImage(); }

bool RGBParade::isHUDDependingOnInput() const { return false; }
bool RGBParade::isScopeDependingOnInput() const { return true; }
bool RGBParade::isBackgroundDependingOnInput() const { return false; }
