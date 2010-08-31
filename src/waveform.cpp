/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <QMouseEvent>
#include <QPainter>
#include <QPoint>
#include <QDebug>

#include "renderer.h"
#include "waveform.h"
#include "waveformgenerator.h"


const QSize Waveform::m_textWidth(35,0);

Waveform::Waveform(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
    AbstractScopeWidget(projMonitor, clipMonitor, parent)
{
    ui = new Ui::Waveform_UI();
    ui->setupUi(this);

    ui->paintMode->addItem(i18n("Yellow"), QVariant(WaveformGenerator::PaintMode_Yellow));
    ui->paintMode->addItem(i18n("Green"), QVariant(WaveformGenerator::PaintMode_Green));

    bool b = true;
    b &= connect(ui->paintMode, SIGNAL(currentIndexChanged(int)), this, SLOT(forceUpdateScope()));
    Q_ASSERT(b);

    // Track the mouse also when no button held down
    this->setMouseTracking(true);

    init();
    m_waveformGenerator = new WaveformGenerator();
}

Waveform::~Waveform()
{
    writeConfig();

    delete m_waveformGenerator;
}

void Waveform::readConfig()
{
    AbstractScopeWidget::readConfig();

    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    ui->paintMode->setCurrentIndex(scopeConfig.readEntry("paintmode", 0));
}

void Waveform::writeConfig()
{
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    scopeConfig.writeEntry("paintmode", ui->paintMode->currentIndex());
    scopeConfig.sync();
}


QRect Waveform::scopeRect()
{
    // Distance from top/left/right
    int offset = 6;

    QPoint topleft(offset, ui->verticalSpacer->geometry().y()+offset);

    return QRect(topleft, this->size() - QSize(offset+topleft.x(), offset+topleft.y()));
}


///// Implemented methods /////

QString Waveform::widgetName() const { return QString("Waveform"); }
bool Waveform::isHUDDependingOnInput() const { return false; }
bool Waveform::isScopeDependingOnInput() const { return true; }
bool Waveform::isBackgroundDependingOnInput() const { return false; }

QImage Waveform::renderHUD(uint)
{
    QImage hud(m_scopeRect.size(), QImage::Format_ARGB32);
    hud.fill(qRgba(0,0,0,0));
    QPainter davinci(&hud);

    davinci.setPen(penLight);

    int x = scopeRect().width()-m_textWidth.width()+3;
    int y = m_mousePos.y() - scopeRect().y();

    if (scopeRect().height() > 0 && m_lineEnabled) {
        davinci.drawLine(0, y, scopeRect().size().width()-m_textWidth.width(), y);
        int val = 255*(1-(float)y/scopeRect().height());
        davinci.drawText(x, scopeRect().height()/2, QVariant(val).toString());
    }
    davinci.drawText(x, scopeRect().height(), "0");
    davinci.drawText(x, 10, "255");

    emit signalHUDRenderingFinished(0, 1);
    return hud;
}

QImage Waveform::renderScope(uint accelFactor, QImage qimage)
{
    QTime start = QTime::currentTime();
    start.start();

    int paintmode = ui->paintMode->itemData(ui->paintMode->currentIndex()).toInt();
    QImage wave = m_waveformGenerator->calculateWaveform(scopeRect().size() - m_textWidth, qimage, (WaveformGenerator::PaintMode) paintmode,
                                                         true, accelFactor);

    emit signalScopeRenderingFinished(start.elapsed(), 1);
    return wave;
}

QImage Waveform::renderBackground(uint)
{
    emit signalBackgroundRenderingFinished(0, 1);
    return QImage();
}


///// Events /////

void Waveform::mouseMoveEvent(QMouseEvent *event)
{
    // Note: Mouse tracking has to be enabled
    m_lineEnabled = true;
    m_mousePos = event->pos();
    forceUpdateHUD();
}

void Waveform::leaveEvent(QEvent *event)
{
    // Repaint the HUD without the circle

    m_lineEnabled = false;
    QWidget::leaveEvent(event);
    forceUpdateHUD();
}
