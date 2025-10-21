/*
    SPDX-FileCopyrightText: 2010 Simon Andreas Eugster <simon.eu@gmail.com>
    This file is part of kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "histogram.h"
#include "histogramgenerator.h"
#include <QElapsedTimer>

#include "core.h"
#include "klocalizedstring.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QActionGroup>
#include <QButtonGroup>

Histogram::Histogram(QWidget *parent)
    : AbstractGfxScopeWidget(false, parent)
{
    // overwrite custom scopes palette from AbstractScopeWidget with global app palette to respect users theme preference
    setPalette(QPalette());

    m_ui = new Ui::Histogram_UI();
    m_ui->setupUi(this);

    // Disable legacy right-click context menu; use hamburger menu instead
    setContextMenuPolicy(Qt::NoContextMenu);

    // Create settings menu
    m_settingsMenu = new QMenu(this);
    m_ui->hamburgerButton->setMenu(m_settingsMenu);

    // Components submenu
    m_componentsMenu = new QMenu(i18n("Components"), m_settingsMenu);
    m_settingsMenu->addMenu(m_componentsMenu);

    m_aComponentY = new QAction(i18n("Y (Luma)"), this);
    m_aComponentY->setCheckable(true);
    connect(m_aComponentY, &QAction::toggled, this, &Histogram::slotComponentsChanged);

    m_aComponentS = new QAction(i18n("Sum"), this);
    m_aComponentS->setCheckable(true);
    connect(m_aComponentS, &QAction::toggled, this, &Histogram::slotComponentsChanged);

    m_aComponentR = new QAction(i18n("R"), this);
    m_aComponentR->setCheckable(true);
    connect(m_aComponentR, &QAction::toggled, this, &Histogram::slotComponentsChanged);

    m_aComponentG = new QAction(i18n("G"), this);
    m_aComponentG->setCheckable(true);
    connect(m_aComponentG, &QAction::toggled, this, &Histogram::slotComponentsChanged);

    m_aComponentB = new QAction(i18n("B"), this);
    m_aComponentB->setCheckable(true);
    connect(m_aComponentB, &QAction::toggled, this, &Histogram::slotComponentsChanged);

    m_componentsMenu->addAction(m_aComponentY);
    m_componentsMenu->addAction(m_aComponentS);
    m_componentsMenu->addAction(m_aComponentR);
    m_componentsMenu->addAction(m_aComponentG);
    m_componentsMenu->addAction(m_aComponentB);

    // Scale submenu
    m_scaleMenu = new QMenu(i18n("Scale"), m_settingsMenu);
    m_settingsMenu->addMenu(m_scaleMenu);

    m_aScaleLinear = new QAction(i18n("Linear"), this);
    m_aScaleLinear->setCheckable(true);
    connect(m_aScaleLinear, &QAction::toggled, this, &Histogram::slotScaleChanged);

    m_aScaleLogarithmic = new QAction(i18n("Logarithmic"), this);
    m_aScaleLogarithmic->setCheckable(true);
    connect(m_aScaleLogarithmic, &QAction::toggled, this, &Histogram::slotScaleChanged);

    m_agScale = new QActionGroup(this);
    m_agScale->addAction(m_aScaleLinear);
    m_agScale->addAction(m_aScaleLogarithmic);

    m_scaleMenu->addAction(m_aScaleLinear);
    m_scaleMenu->addAction(m_aScaleLogarithmic);

    m_aUnscaled = new QAction(i18n("Unscaled"), this);
    m_aUnscaled->setCheckable(true);
    connect(m_aUnscaled, &QAction::toggled, this, &Histogram::forceUpdateScope);

    m_aRec601 = new QAction(i18n("Rec. 601"), this);
    m_aRec601->setCheckable(true);
    connect(m_aRec601, &QAction::toggled, this, &Histogram::forceUpdateScope);

    m_aRec709 = new QAction(i18n("Rec. 709"), this);
    m_aRec709->setCheckable(true);
    connect(m_aRec709, &QAction::toggled, this, &Histogram::forceUpdateScope);

    m_agRec = new QActionGroup(this);
    m_agRec->addAction(m_aRec601);
    m_agRec->addAction(m_aRec709);

    // Populate hamburger settings menu with remaining options
    m_settingsMenu->addSeparator();
    m_settingsMenu->addAction(m_aUnscaled);
    m_settingsMenu->addSeparator()->setText(i18n("Luma mode"));
    m_settingsMenu->addAction(m_aRec601);
    m_settingsMenu->addAction(m_aRec709);

    m_settingsMenu->addSeparator();
    m_settingsMenu->addAction(m_aRealtime);

    connect(pCore.get(), &Core::updatePalette, this, &Histogram::forceUpdateScope);

    init();
    m_histogramGenerator = new HistogramGenerator();
}

Histogram::~Histogram()
{
    writeConfig();
    delete m_histogramGenerator;

    // Delete settings menu actions
    delete m_aComponentY;
    delete m_aComponentS;
    delete m_aComponentR;
    delete m_aComponentG;
    delete m_aComponentB;

    delete m_aScaleLinear;
    delete m_aScaleLogarithmic;
    delete m_agScale;

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

    // Read components
    m_aComponentY->setChecked(scopeConfig.readEntry("yEnabled", true));
    m_aComponentS->setChecked(scopeConfig.readEntry("sEnabled", false));
    m_aComponentR->setChecked(scopeConfig.readEntry("rEnabled", true)); // codespell:ignore renabled
    m_aComponentG->setChecked(scopeConfig.readEntry("gEnabled", true));
    m_aComponentB->setChecked(scopeConfig.readEntry("bEnabled", true));
    slotComponentsChanged();

    // Read scale
    bool logScale = scopeConfig.readEntry("logScale", false);
    if (logScale) {
        m_aScaleLogarithmic->setChecked(true);
    } else {
        m_aScaleLinear->setChecked(true);
    }
    slotScaleChanged();

    m_aRec601->setChecked(scopeConfig.readEntry("rec601", false));
    m_aRec709->setChecked(!m_aRec601->isChecked());
}

void Histogram::writeConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());
    scopeConfig.writeEntry("yEnabled", m_aComponentY->isChecked());
    scopeConfig.writeEntry("sEnabled", m_aComponentS->isChecked());
    scopeConfig.writeEntry("rEnabled", m_aComponentR->isChecked()); // codespell:ignore renabled
    scopeConfig.writeEntry("gEnabled", m_aComponentG->isChecked());
    scopeConfig.writeEntry("bEnabled", m_aComponentB->isChecked());
    scopeConfig.writeEntry("rec601", m_aRec601->isChecked());
    scopeConfig.writeEntry("logScale", m_aScaleLogarithmic->isChecked());
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
    QPoint topleft(offset, offset + m_ui->hamburgerButton->geometry().bottom());
    return QRect(topleft, this->rect().size() - QSize(topleft.x() + offset, topleft.y() + offset));
}

QImage Histogram::renderHUD(uint)
{
    Q_EMIT signalHUDRenderingFinished(0, 1);
    return QImage();
}
QImage Histogram::renderGfxScope(uint accelFactor, const QImage &qimage)
{
    QElapsedTimer timer;
    timer.start();

    ITURec rec = m_aRec601->isChecked() ? ITURec::Rec_601 : ITURec::Rec_709;

    qreal scalingFactor = devicePixelRatioF();
    QImage histogram = m_histogramGenerator->calculateHistogram(m_scopeRect.size(), scalingFactor, qimage, m_components, rec, m_aUnscaled->isChecked(),
                                                                m_logScale, accelFactor, palette());

    Q_EMIT signalScopeRenderingFinished(uint(timer.elapsed()), accelFactor);
    return histogram;
}
QImage Histogram::renderBackground(uint)
{
    Q_EMIT signalBackgroundRenderingFinished(0, 1);
    return QImage();
}

void Histogram::showSettingsMenu()
{
    m_settingsMenu->exec(m_ui->hamburgerButton->mapToGlobal(m_ui->hamburgerButton->rect().bottomLeft()));
}

void Histogram::slotComponentsChanged()
{
    m_components = 0;
    if (m_aComponentY->isChecked()) m_components |= HistogramGenerator::ComponentY;
    if (m_aComponentS->isChecked()) m_components |= HistogramGenerator::ComponentSum;
    if (m_aComponentR->isChecked()) m_components |= HistogramGenerator::ComponentR;
    if (m_aComponentG->isChecked()) m_components |= HistogramGenerator::ComponentG;
    if (m_aComponentB->isChecked()) m_components |= HistogramGenerator::ComponentB;
    forceUpdateScope();
}

void Histogram::slotScaleChanged()
{
    m_logScale = m_aScaleLogarithmic->isChecked();
    forceUpdateScope();
}
