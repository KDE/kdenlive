/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "rgbparade.h"
#include "rgbparadegenerator.h"
#include <QPainter>
#include <QRect>
#include <QElapsedTimer>

#include "klocalizedstring.h"
#include <KConfigGroup>
#include <KSharedConfig>

RGBParade::RGBParade(QWidget *parent)
    : AbstractGfxScopeWidget(true, parent)
{
    m_ui = new Ui::RGBParade_UI();
    m_ui->setupUi(this);

    m_ui->paintMode->addItem(i18n("RGB"), QVariant(RGBParadeGenerator::PaintMode_RGB));
    m_ui->paintMode->addItem(i18n("White"), QVariant(RGBParadeGenerator::PaintMode_White));

    m_menu->addSeparator();
    m_aAxis = new QAction(i18n("Draw axis"), this);
    m_aAxis->setCheckable(true);
    m_menu->addAction(m_aAxis);
    connect(m_aAxis, &QAction::changed, this, &RGBParade::forceUpdateScope);

    m_aGradRef = new QAction(i18n("Gradient reference line"), this);
    m_aGradRef->setCheckable(true);
    m_menu->addAction(m_aGradRef);
    connect(m_aGradRef, &QAction::changed, this, &RGBParade::forceUpdateScope);

    connect(m_ui->paintMode, SIGNAL(currentIndexChanged(int)), this, SLOT(forceUpdateScope()));
    connect(this, &RGBParade::signalMousePositionChanged, this, &RGBParade::forceUpdateHUD);

    m_rgbParadeGenerator = new RGBParadeGenerator();
    init();
}

RGBParade::~RGBParade()
{
    writeConfig();

    delete m_ui;
    delete m_rgbParadeGenerator;
    delete m_aAxis;
    delete m_aGradRef;
}

void RGBParade::readConfig()
{
    AbstractGfxScopeWidget::readConfig();

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());
    m_ui->paintMode->setCurrentIndex(scopeConfig.readEntry("paintmode", 0));
    m_aAxis->setChecked(scopeConfig.readEntry("axis", false));
    m_aGradRef->setChecked(scopeConfig.readEntry("gradref", false));
}

void RGBParade::writeConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());
    scopeConfig.writeEntry("paintmode", m_ui->paintMode->currentIndex());
    scopeConfig.writeEntry("axis", m_aAxis->isChecked());
    scopeConfig.writeEntry("gradref", m_aGradRef->isChecked());
    scopeConfig.sync();
}

QString RGBParade::widgetName() const
{
    return QStringLiteral("RGB Parade");
}

QRect RGBParade::scopeRect()
{
    QPoint topleft(offset, m_ui->verticalSpacer->geometry().y() + 2 * offset);
    return QRect(topleft, QPoint(this->size().width() - offset, this->size().height() - offset));
}

QImage RGBParade::renderHUD(uint)
{
    QImage hud(m_scopeRect.size(), QImage::Format_ARGB32);
    hud.fill(qRgba(0, 0, 0, 0));

    QPainter davinci(&hud);
    davinci.setPen(penLight);

    int x = scopeRect().width() - 30;

    davinci.drawText(x, scopeRect().height() - RGBParadeGenerator::distBottom, QStringLiteral("0"));
    davinci.drawText(x, 10, QStringLiteral("255"));

    if (scopeRect().height() > 0 && m_mouseWithinWidget) {

        int y = m_mousePos.y() - scopeRect().y();

        // Draw a horizontal line through the current mouse position
        // and show the value of the waveform there
        davinci.drawLine(0, y, scopeRect().size().width() - RGBParadeGenerator::distRight, y);

        // Make the value stick to the line unless it is at the top/bottom of the scope
        const int top = 30;
        const int bottom = 20 + RGBParadeGenerator::distBottom;

        int valY = y + 5;
        if (valY < top) {
            valY = top;
        } else if (valY > scopeRect().height() - bottom) {
            valY = scopeRect().height() - bottom;
        }

	int val = 255 + int(255. * (1. - (float)y) / ((float)scopeRect().height() - RGBParadeGenerator::distBottom));
	if((val >= 0) && (val <= 255)) {
        	davinci.drawText(x, valY, QVariant(val).toString());
	}

    }

    emit signalHUDRenderingFinished(1, 1);
    return hud;
}

QImage RGBParade::renderGfxScope(uint accelerationFactor, const QImage &qimage)
{
    QElapsedTimer timer;
    timer.start();

    int paintmode = m_ui->paintMode->itemData(m_ui->paintMode->currentIndex()).toInt();
    QImage parade = m_rgbParadeGenerator->calculateRGBParade(m_scopeRect.size(), qimage, (RGBParadeGenerator::PaintMode)paintmode, m_aAxis->isChecked(),
                                                             m_aGradRef->isChecked(), accelerationFactor);
    emit signalScopeRenderingFinished((uint)timer.elapsed(), accelerationFactor);
    return parade;
}

QImage RGBParade::renderBackground(uint)
{
    return QImage();
}

bool RGBParade::isHUDDependingOnInput() const
{
    return false;
}

bool RGBParade::isScopeDependingOnInput() const
{
    return true;
}

bool RGBParade::isBackgroundDependingOnInput() const
{
    return false;
}
