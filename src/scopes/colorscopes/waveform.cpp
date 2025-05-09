/*
    SPDX-FileCopyrightText: 2010 Simon Andreas Eugster <simon.eu@gmail.com>
    This file is part of kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "waveform.h"
#include "waveformgenerator.h"
// For reading out the project resolution
#include "core.h"
#include "profiles/profilemodel.hpp"

#include "klocalizedstring.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QActionGroup>
#include <QElapsedTimer>
#include <QPainter>
#include <QPoint>

const QSize Waveform::m_textWidth(35, 0);
const int Waveform::m_paddingBottom(20);

Waveform::Waveform(QWidget *parent)
    : AbstractGfxScopeWidget(true, parent)
{
    // overwrite custom scopes palette from AbstractScopeWidget with global app palette to respect users theme preference
    setPalette(QPalette());

    m_ui = new Ui::Waveform_UI();
    m_ui->setupUi(this);

    m_ui->paintMode->addItem(i18n("Yellow"), QVariant(WaveformGenerator::PaintMode_Yellow));
    m_ui->paintMode->addItem(i18n("White"), QVariant(WaveformGenerator::PaintMode_White));
    m_ui->paintMode->addItem(i18n("Green"), QVariant(WaveformGenerator::PaintMode_Green));

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

    connect(m_ui->paintMode, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &Waveform::forceUpdateScope);
    connect(this, &Waveform::signalMousePositionChanged, this, &Waveform::forceUpdateHUD);
    connect(m_aRec601, &QAction::toggled, this, &Waveform::forceUpdateScope);
    connect(m_aRec709, &QAction::toggled, this, &Waveform::forceUpdateScope);
    connect(pCore.get(), &Core::updatePalette, this, [this]() { forceUpdate(true); });

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
    delete m_ui;
}

void Waveform::readConfig()
{
    AbstractGfxScopeWidget::readConfig();

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());
    m_ui->paintMode->setCurrentIndex(scopeConfig.readEntry("paintmode", 0));
    m_aRec601->setChecked(scopeConfig.readEntry("rec601", false));
    m_aRec709->setChecked(!m_aRec601->isChecked());
}

void Waveform::writeConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());
    scopeConfig.writeEntry("paintmode", m_ui->paintMode->currentIndex());
    scopeConfig.writeEntry("rec601", m_aRec601->isChecked());
    scopeConfig.sync();
}

QRect Waveform::scopeRect()
{
    // Distance from top/left/right
    int border = 6;
    QPoint topleft(border, m_ui->verticalSpacer->geometry().y() + border);

    return QRect(topleft, this->size() - QSize(border + topleft.x(), border + topleft.y()));
}

///// Implemented methods /////

QString Waveform::widgetName() const
{
    return QStringLiteral("Waveform");
}
bool Waveform::isHUDDependingOnInput() const
{
    return false;
}
bool Waveform::isScopeDependingOnInput() const
{
    return true;
}
bool Waveform::isBackgroundDependingOnInput() const
{
    return false;
}

QImage Waveform::renderHUD(uint)
{
    qreal scalingFactor = devicePixelRatioF();
    QImage hud(m_scopeRect.size() * scalingFactor, QImage::Format_ARGB32);
    hud.setDevicePixelRatio(scalingFactor);
    hud.fill(qRgba(0, 0, 0, 0));

    QPainter davinci;
    bool ok = davinci.begin(&hud);
    if (!ok) {
        qDebug() << "Could not initialise QPainter for Waveform HUD.";
        return hud;
    }
    davinci.setPen(palette().text().color());

    //    qCDebug(KDENLIVE_LOG) << values.value("width");

    const int rightX = scopeRect().width() - m_textWidth.width() + 3;
    const int x = m_mousePos.x() - scopeRect().x();
    const int y = m_mousePos.y() - scopeRect().y();

    if (scopeRect().height() > 0 && m_mouseWithinWidget) {
        int val = 255 - (255 * y) / scopeRect().height();

        if (val >= 0 && val <= 255) {
            // Draw a horizontal line through the current mouse position
            // and show the value of the waveform there
            davinci.drawLine(0, y, scopeRect().size().width() - m_textWidth.width(), y);

            // Make the value stick to the line unless it is at the top/bottom of the scope
            int valY = y + 5;
            const int top = 30;
            const int bottom = 20;
            if (valY < top) {
                valY = top;
            } else if (valY > scopeRect().height() - bottom) {
                valY = scopeRect().height() - bottom;
            }
            davinci.drawText(rightX, valY, QVariant(val).toString());
        }

        if (scopeRect().width() > 0) {
            // Draw a vertical line and the x position of the source clip
            const int profileWidth = pCore->getCurrentProfile()->width();

            const int clipX = (x * (profileWidth - 1)) / (scopeRect().width() - m_textWidth.width() - 1);

            if (clipX >= 0 && clipX <= profileWidth) {
                int valX = x - 15;
                if (valX < 0) {
                    valX = 0;
                }
                if (valX > scopeRect().width() - 55 - m_textWidth.width()) {
                    valX = scopeRect().width() - 55 - m_textWidth.width();
                }

                davinci.drawLine(x, y, x, scopeRect().height() - m_paddingBottom);
                davinci.drawText(valX, scopeRect().height() - 5, QVariant(clipX).toString() + QStringLiteral(" px"));
            }
        }
    }
    davinci.drawText(rightX, scopeRect().height() - m_paddingBottom, QStringLiteral("0"));
    davinci.drawText(rightX, 10, QStringLiteral("255"));

    Q_EMIT signalHUDRenderingFinished(0, 1);
    return hud;
}

QImage Waveform::renderGfxScope(uint accelFactor, const QImage &qimage)
{
    QElapsedTimer timer;
    timer.start();

    const int paintmode = m_ui->paintMode->itemData(m_ui->paintMode->currentIndex()).toInt();
    ITURec rec = m_aRec601->isChecked() ? ITURec::Rec_601 : ITURec::Rec_709;
    qreal scalingFactor = devicePixelRatioF();
    QImage wave = m_waveformGenerator->calculateWaveform((scopeRect().size() - m_textWidth - QSize(0, m_paddingBottom)), scalingFactor, qimage,
                                                         WaveformGenerator::PaintMode(paintmode), true, rec, accelFactor, palette());

    Q_EMIT signalScopeRenderingFinished(uint(timer.elapsed()), 1);
    return wave;
}

QImage Waveform::renderBackground(uint)
{
    Q_EMIT signalBackgroundRenderingFinished(0, 1);
    return QImage();
}
