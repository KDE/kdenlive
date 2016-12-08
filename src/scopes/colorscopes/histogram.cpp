/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "histogram.h"
#include "histogramgenerator.h"
#include <QTime>

#include <KSharedConfig>
#include <KConfigGroup>
#include "klocalizedstring.h"

Histogram::Histogram(QWidget *parent) :
    AbstractGfxScopeWidget(false, parent)
{
    ui = new Ui::Histogram_UI();
    ui->setupUi(this);

    m_aUnscaled = new QAction(i18n("Unscaled"), this);
    m_aUnscaled->setCheckable(true);

    m_aRec601 = new QAction(i18n("Rec. 601"), this);
    m_aRec601->setCheckable(true);
    m_aRec709 = new QAction(i18n("Rec. 709"), this);
    m_aRec709->setCheckable(true);
    m_agRec = new QActionGroup(this);
    m_agRec->addAction(m_aRec601);
    m_agRec->addAction(m_aRec709);

    m_menu->addSeparator();
    m_menu->addAction(m_aUnscaled);
    m_menu->addSeparator()->setText(i18n("Luma mode"));
    m_menu->addAction(m_aRec601);
    m_menu->addAction(m_aRec709);

    connect(ui->cbY, &QAbstractButton::toggled, this, &AbstractScopeWidget::forceUpdateScope);
    connect(ui->cbS, &QAbstractButton::toggled, this, &AbstractScopeWidget::forceUpdateScope);
    connect(ui->cbR, &QAbstractButton::toggled, this, &AbstractScopeWidget::forceUpdateScope);
    connect(ui->cbG, &QAbstractButton::toggled, this, &AbstractScopeWidget::forceUpdateScope);
    connect(ui->cbB, &QAbstractButton::toggled, this, &AbstractScopeWidget::forceUpdateScope);
    connect(m_aUnscaled, &QAction::toggled, this, &Histogram::forceUpdateScope);
    connect(m_aRec601, &QAction::toggled, this, &Histogram::forceUpdateScope);
    connect(m_aRec709, &QAction::toggled, this, &Histogram::forceUpdateScope);

    init();
    m_histogramGenerator = new HistogramGenerator();
}

Histogram::~Histogram()
{
    writeConfig();
    delete m_histogramGenerator;
    delete ui;
    delete m_aUnscaled;
    delete m_aRec601;
    delete m_aRec709;
    delete m_agRec;
}

void Histogram::readConfig()
{
    AbstractGfxScopeWidget::readConfig();

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());
    ui->cbY->setChecked(scopeConfig.readEntry("yEnabled", true));
    ui->cbS->setChecked(scopeConfig.readEntry("sEnabled", false));
    ui->cbR->setChecked(scopeConfig.readEntry("rEnabled", true));
    ui->cbG->setChecked(scopeConfig.readEntry("gEnabled", true));
    ui->cbB->setChecked(scopeConfig.readEntry("bEnabled", true));
    m_aRec601->setChecked(scopeConfig.readEntry("rec601", false));
    m_aRec709->setChecked(!m_aRec601->isChecked());
}

void Histogram::writeConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());
    scopeConfig.writeEntry("yEnabled", ui->cbY->isChecked());
    scopeConfig.writeEntry("sEnabled", ui->cbS->isChecked());
    scopeConfig.writeEntry("rEnabled", ui->cbR->isChecked());
    scopeConfig.writeEntry("gEnabled", ui->cbG->isChecked());
    scopeConfig.writeEntry("bEnabled", ui->cbB->isChecked());
    scopeConfig.writeEntry("rec601", m_aRec601->isChecked());
    scopeConfig.sync();
}

QString Histogram::widgetName() const
{
    return QStringLiteral("Histogram");
}

bool Histogram::isHUDDependingOnInput() const
{
    return false;
}
bool Histogram::isScopeDependingOnInput() const
{
    return true;
}
bool Histogram::isBackgroundDependingOnInput() const
{
    return false;
}

QRect Histogram::scopeRect()
{
    //qCDebug(KDENLIVE_LOG) << "According to the spacer, the top left point is " << ui->verticalSpacer->geometry().x() << '/' << ui->verticalSpacer->geometry().y();
    QPoint topleft(offset, offset + ui->verticalSpacer->geometry().y());
    return QRect(topleft, this->rect().size() - QSize(topleft.x() + offset, topleft.y() + offset));
}

QImage Histogram::renderHUD(uint)
{
    emit signalHUDRenderingFinished(0, 1);
    return QImage();
}
QImage Histogram::renderGfxScope(uint accelFactor, const QImage &qimage)
{
    QTime start = QTime::currentTime();
    start.start();
    const int componentFlags = (ui->cbY->isChecked() ? 1 : 0) * HistogramGenerator::ComponentY
                               | (ui->cbS->isChecked() ? 1 : 0) * HistogramGenerator::ComponentSum
                               | (ui->cbR->isChecked() ? 1 : 0) * HistogramGenerator::ComponentR
                               | (ui->cbG->isChecked() ? 1 : 0) * HistogramGenerator::ComponentG
                               | (ui->cbB->isChecked() ? 1 : 0) * HistogramGenerator::ComponentB;

    HistogramGenerator::Rec rec = m_aRec601->isChecked() ? HistogramGenerator::Rec_601 : HistogramGenerator::Rec_709;

    QImage histogram = m_histogramGenerator->calculateHistogram(m_scopeRect.size(), qimage, componentFlags,
                       rec, m_aUnscaled->isChecked(), accelFactor);

    emit signalScopeRenderingFinished(start.elapsed(), accelFactor);
    return histogram;
}
QImage Histogram::renderBackground(uint)
{
    emit signalBackgroundRenderingFinished(0, 1);
    return QImage();
}

