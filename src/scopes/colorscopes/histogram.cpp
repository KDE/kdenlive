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
#include <QElapsedTimer>

#include "klocalizedstring.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QtWidgets/QButtonGroup>

Histogram::Histogram(QWidget *parent)
    : AbstractGfxScopeWidget(false, parent)
{
    m_ui = new Ui::Histogram_UI();
    m_ui->setupUi(this);

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

    QButtonGroup scaleGroup;
    scaleGroup.addButton(m_ui->rbLinear);
    scaleGroup.addButton(m_ui->rbLogarithmic);

    connect(m_ui->cbY, &QAbstractButton::toggled, this, &AbstractScopeWidget::forceUpdateScope);
    connect(m_ui->cbS, &QAbstractButton::toggled, this, &AbstractScopeWidget::forceUpdateScope);
    connect(m_ui->cbR, &QAbstractButton::toggled, this, &AbstractScopeWidget::forceUpdateScope);
    connect(m_ui->cbG, &QAbstractButton::toggled, this, &AbstractScopeWidget::forceUpdateScope);
    connect(m_ui->cbB, &QAbstractButton::toggled, this, &AbstractScopeWidget::forceUpdateScope);
    connect(m_aUnscaled, &QAction::toggled, this, &Histogram::forceUpdateScope);
    connect(m_aRec601, &QAction::toggled, this, &Histogram::forceUpdateScope);
    connect(m_aRec709, &QAction::toggled, this, &Histogram::forceUpdateScope);
    connect(m_ui->rbLogarithmic, &QAbstractButton::toggled, this, &Histogram::forceUpdateScope);

    init();
    m_histogramGenerator = new HistogramGenerator();
}

Histogram::~Histogram()
{
    writeConfig();
    delete m_histogramGenerator;
    delete m_ui;
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
    m_ui->cbY->setChecked(scopeConfig.readEntry("yEnabled", true));
    m_ui->cbS->setChecked(scopeConfig.readEntry("sEnabled", false));
    m_ui->cbR->setChecked(scopeConfig.readEntry("rEnabled", true));
    m_ui->cbG->setChecked(scopeConfig.readEntry("gEnabled", true));
    m_ui->cbB->setChecked(scopeConfig.readEntry("bEnabled", true));
    m_aRec601->setChecked(scopeConfig.readEntry("rec601", false));
    m_aRec709->setChecked(!m_aRec601->isChecked());
    m_ui->rbLogarithmic->setChecked(scopeConfig.readEntry("logScale", false));
}

void Histogram::writeConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());
    scopeConfig.writeEntry("yEnabled", m_ui->cbY->isChecked());
    scopeConfig.writeEntry("sEnabled", m_ui->cbS->isChecked());
    scopeConfig.writeEntry("rEnabled", m_ui->cbR->isChecked());
    scopeConfig.writeEntry("gEnabled", m_ui->cbG->isChecked());
    scopeConfig.writeEntry("bEnabled", m_ui->cbB->isChecked());
    scopeConfig.writeEntry("rec601", m_aRec601->isChecked());
    scopeConfig.writeEntry("logScale", m_ui->rbLogarithmic->isChecked());
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
    // qCDebug(KDENLIVE_LOG) << "According to the spacer, the top left point is " << m_ui->verticalSpacer->geometry().x() << '/' <<
    // m_ui->verticalSpacer->geometry().y();
    QPoint topleft(offset, offset + m_ui->verticalSpacer->geometry().y());
    return QRect(topleft, this->rect().size() - QSize(topleft.x() + offset, topleft.y() + offset));
}

QImage Histogram::renderHUD(uint)
{
    emit signalHUDRenderingFinished(0, 1);
    return QImage();
}
QImage Histogram::renderGfxScope(uint accelFactor, const QImage &qimage)
{
    QElapsedTimer timer;
    timer.start();
    const int componentFlags =
        (m_ui->cbY->isChecked() ? 1 : 0) * HistogramGenerator::ComponentY | (m_ui->cbS->isChecked() ? 1 : 0) * HistogramGenerator::ComponentSum |
        (m_ui->cbR->isChecked() ? 1 : 0) * HistogramGenerator::ComponentR | (m_ui->cbG->isChecked() ? 1 : 0) * HistogramGenerator::ComponentG |
        (m_ui->cbB->isChecked() ? 1 : 0) * HistogramGenerator::ComponentB;

    ITURec rec = m_aRec601->isChecked() ? ITURec::Rec_601 : ITURec::Rec_709;

    QImage histogram = m_histogramGenerator->calculateHistogram(m_scopeRect.size(), qimage, componentFlags, rec, m_aUnscaled->isChecked(), m_ui->rbLogarithmic->isChecked(), accelFactor);

    emit signalScopeRenderingFinished(uint(timer.elapsed()), accelFactor);
    return histogram;
}
QImage Histogram::renderBackground(uint)
{
    emit signalBackgroundRenderingFinished(0, 1);
    return QImage();
}
