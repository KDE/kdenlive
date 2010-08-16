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

    ui->paintMode->addItem(i18n("RGB"), QVariant(RGBParadeGenerator::PaintMode_RGB));
    ui->paintMode->addItem(i18n("RGB 2"), QVariant(RGBParadeGenerator::PaintMode_RGB2));

    bool b = true;
    b &= connect(ui->paintMode, SIGNAL(currentIndexChanged(int)), this, SLOT(forceUpdateScope()));
    Q_ASSERT(b);

    m_rgbParadeGenerator = new RGBParadeGenerator();
    init();
}

RGBParade::~RGBParade()
{
    writeConfig();

    delete ui;
    delete m_rgbParadeGenerator;
}

void RGBParade::readConfig()
{
    AbstractScopeWidget::readConfig();

    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    ui->paintMode->setCurrentIndex(scopeConfig.readEntry("paintmode", 0));
}

void RGBParade::writeConfig()
{
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    scopeConfig.writeEntry("paintmode", ui->paintMode->currentIndex());
    scopeConfig.sync();
}


QString RGBParade::widgetName() const { return "RGB Parade"; }

QRect RGBParade::scopeRect()
{
    QPoint topleft(offset, ui->verticalSpacer->geometry().y() + 2*offset);
    return QRect(topleft, QPoint(this->size().width() - offset, this->size().height() - offset));
}

QImage RGBParade::renderHUD(uint) { return QImage(); }
QImage RGBParade::renderScope(uint accelerationFactor, QImage qimage)
{
    QTime start = QTime::currentTime();
    start.start();

    int paintmode = ui->paintMode->itemData(ui->paintMode->currentIndex()).toInt();
    QImage parade = m_rgbParadeGenerator->calculateRGBParade(m_scopeRect.size(), qimage, (RGBParadeGenerator::PaintMode) paintmode,
                                                    true, accelerationFactor);
    emit signalScopeRenderingFinished(start.elapsed(), accelerationFactor);
    return parade;
}
QImage RGBParade::renderBackground(uint) { return QImage(); }

bool RGBParade::isHUDDependingOnInput() const { return false; }
bool RGBParade::isScopeDependingOnInput() const { return true; }
bool RGBParade::isBackgroundDependingOnInput() const { return false; }
