/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <QMenu>
#include <QPainter>
#include <QRect>
#include <QTime>
#include "renderer.h"
#include "rgbparade.h"
#include "rgbparadegenerator.h"

RGBParade::RGBParade(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
        AbstractScopeWidget(projMonitor, clipMonitor, true, parent)
{
    ui = new Ui::RGBParade_UI();
    ui->setupUi(this);

    ui->paintMode->addItem(i18n("RGB"), QVariant(RGBParadeGenerator::PaintMode_RGB));
    ui->paintMode->addItem(i18n("White"), QVariant(RGBParadeGenerator::PaintMode_White));

    bool b = true;

    m_menu->addSeparator();
    m_aAxis = new QAction(i18n("Draw axis"), this);
    m_aAxis->setCheckable(true);
    m_menu->addAction(m_aAxis);
    b &= connect(m_aAxis, SIGNAL(changed()), this, SLOT(forceUpdateScope()));

    m_aGradRef = new QAction(i18n("Gradient reference line"), this);
    m_aGradRef->setCheckable(true);
    m_menu->addAction(m_aGradRef);
    b &= connect(m_aGradRef, SIGNAL(changed()), this, SLOT(forceUpdateScope()));

    b &= connect(ui->paintMode, SIGNAL(currentIndexChanged(int)), this, SLOT(forceUpdateScope()));
    b &= connect(this, SIGNAL(signalMousePositionChanged()), this, SLOT(forceUpdateHUD()));
    Q_ASSERT(b);

    m_rgbParadeGenerator = new RGBParadeGenerator();
    init();
}

RGBParade::~RGBParade()
{
    writeConfig();

    delete ui;
    delete m_rgbParadeGenerator;
    delete m_aAxis;
    delete m_aGradRef;
}

void RGBParade::readConfig()
{
    AbstractScopeWidget::readConfig();

    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    ui->paintMode->setCurrentIndex(scopeConfig.readEntry("paintmode", 0));
    m_aAxis->setChecked(scopeConfig.readEntry("axis", true));
    m_aGradRef->setChecked(scopeConfig.readEntry("gradref", false));
}

void RGBParade::writeConfig()
{
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    scopeConfig.writeEntry("paintmode", ui->paintMode->currentIndex());
    scopeConfig.writeEntry("axis", m_aAxis->isChecked());
    scopeConfig.writeEntry("gradref", m_aGradRef->isChecked());
    scopeConfig.sync();
}


QString RGBParade::widgetName() const { return "RGB Parade"; }

QRect RGBParade::scopeRect()
{
    QPoint topleft(offset, ui->verticalSpacer->geometry().y() + 2*offset);
    return QRect(topleft, QPoint(this->size().width() - offset, this->size().height() - offset));
}

QImage RGBParade::renderHUD(uint)
{
    if (scopeRect().height() > 0 && m_mouseWithinWidget) {
        QImage hud(m_scopeRect.size(), QImage::Format_ARGB32);
        hud.fill(qRgba(0,0,0,0));

        QPainter davinci(&hud);
        davinci.setPen(penLight);

        int x = scopeRect().width()-30;
        int y = m_mousePos.y() - scopeRect().y();

        // Draw a horizontal line through the current mouse position
        // and show the value of the waveform there
        davinci.drawLine(0, y, scopeRect().size().width()-RGBParadeGenerator::distRight, y);

        int val = 255*(1-((float)y/(scopeRect().height()-RGBParadeGenerator::distBottom)));
        davinci.drawText(x, scopeRect().height()/2, QVariant(val).toString());

        emit signalHUDRenderingFinished(1, 1);
        return hud;
    } else {
        emit signalHUDRenderingFinished(1, 1);
        return QImage();
    }
}
QImage RGBParade::renderScope(uint accelerationFactor, QImage qimage)
{
    QTime start = QTime::currentTime();
    start.start();

    int paintmode = ui->paintMode->itemData(ui->paintMode->currentIndex()).toInt();
    QImage parade = m_rgbParadeGenerator->calculateRGBParade(m_scopeRect.size(), qimage, (RGBParadeGenerator::PaintMode) paintmode,
                                                    m_aAxis->isChecked(), m_aGradRef->isChecked(), accelerationFactor);
    emit signalScopeRenderingFinished(start.elapsed(), accelerationFactor);
    return parade;
}
QImage RGBParade::renderBackground(uint) { return QImage(); }

bool RGBParade::isHUDDependingOnInput() const { return false; }
bool RGBParade::isScopeDependingOnInput() const { return true; }
bool RGBParade::isBackgroundDependingOnInput() const { return false; }
