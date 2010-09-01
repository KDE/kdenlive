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
#include <QMouseEvent>
#include <QPainter>
#include <QPoint>

#include "renderer.h"
#include "waveform.h"
#include "waveformgenerator.h"


const QSize Waveform::m_textWidth(35,0);

Waveform::Waveform(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
    AbstractScopeWidget(projMonitor, clipMonitor, true, parent)
{
    ui = new Ui::Waveform_UI();
    ui->setupUi(this);

    ui->paintMode->addItem(i18n("Yellow"), QVariant(WaveformGenerator::PaintMode_Yellow));
    ui->paintMode->addItem(i18n("White"), QVariant(WaveformGenerator::PaintMode_White));
    ui->paintMode->addItem(i18n("Green"), QVariant(WaveformGenerator::PaintMode_Green));


    m_aRec601 = new QAction(i18n("Rec. 601"), this);
    m_aRec601->setCheckable(true);
    m_aRec709 = new QAction(i18n("Rec. 709"), this);
    m_aRec709->setCheckable(true);
    m_agRec = new QActionGroup(this);
    m_agRec->addAction(m_aRec601);
    m_agRec->addAction(m_aRec709);
    m_menu->addSeparator()->setText(i18n("Luma mode"));
    m_menu->addAction(m_aRec601);
    m_menu->addAction(m_aRec709);


    bool b = true;
    b &= connect(ui->paintMode, SIGNAL(currentIndexChanged(int)), this, SLOT(forceUpdateScope()));
    b &= connect(this, SIGNAL(signalMousePositionChanged()), this, SLOT(forceUpdateHUD()));
    b &= connect(m_aRec601, SIGNAL(toggled(bool)), this, SLOT(forceUpdateScope()));
    b &= connect(m_aRec709, SIGNAL(toggled(bool)), this, SLOT(forceUpdateScope()));
    Q_ASSERT(b);

    init();
    m_waveformGenerator = new WaveformGenerator();
}

Waveform::~Waveform()
{
    writeConfig();

    delete m_waveformGenerator;
    delete m_aRec601;
    delete m_aRec709;
    delete m_agRec;
}

void Waveform::readConfig()
{
    AbstractScopeWidget::readConfig();

    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    ui->paintMode->setCurrentIndex(scopeConfig.readEntry("paintmode", 0));
    m_aRec601->setChecked(scopeConfig.readEntry("rec601", false));
    m_aRec709->setChecked(!m_aRec601->isChecked());
}

void Waveform::writeConfig()
{
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    scopeConfig.writeEntry("paintmode", ui->paintMode->currentIndex());
    scopeConfig.writeEntry("rec601", m_aRec601->isChecked());
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

    if (scopeRect().height() > 0 && m_mouseWithinWidget) {
        // Draw a horizontal line through the current mouse position
        // and show the value of the waveform there
        davinci.drawLine(0, y, scopeRect().size().width()-m_textWidth.width(), y);

        // Make the value stick to the line unless it is at the top/bottom of the scope
        const int top = 30;
        const int bottom = 20;
        int valY = y+5;
        if (valY < top) {
            valY = top;
        } else if (valY > scopeRect().height()-bottom) {
            valY = scopeRect().height()-bottom;
        }
        int val = 255*(1-(float)y/scopeRect().height());
        davinci.drawText(x, valY, QVariant(val).toString());
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
    WaveformGenerator::Rec rec = m_aRec601->isChecked() ? WaveformGenerator::Rec_601 : WaveformGenerator::Rec_709;
    QImage wave = m_waveformGenerator->calculateWaveform(scopeRect().size() - m_textWidth, qimage, (WaveformGenerator::PaintMode) paintmode,
                                                         true, rec, accelFactor);

    emit signalScopeRenderingFinished(start.elapsed(), 1);
    return wave;
}

QImage Waveform::renderBackground(uint)
{
    emit signalBackgroundRenderingFinished(0, 1);
    return QImage();
}
