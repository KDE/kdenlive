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

const int Waveform::m_paddingBottom(20);

Waveform::Waveform(QWidget *parent)
    : AbstractGfxScopeWidget(true, parent)
{
    // overwrite custom scopes palette from AbstractScopeWidget with global app palette to respect users theme preference
    setPalette(QPalette());

    QFontMetrics fm = QFontMetrics(font());
    m_textWidth = fm.horizontalAdvance(QStringLiteral("255"));

    m_ui = new Ui::Waveform_UI();
    m_ui->setupUi(this);

    // Disable legacy right-click context menu; use hamburger menu instead
    setContextMenuPolicy(Qt::NoContextMenu);

    // Create settings menu and its actions
    m_settingsMenu = new QMenu(this);
    m_ui->hamburgerButton->setMenu(m_settingsMenu);

    // Paint mode submenu
    m_paintModeMenu = new QMenu(i18n("Paint Mode"), m_settingsMenu);
    m_settingsMenu->addMenu(m_paintModeMenu);

    m_aPaintModeYellow = new QAction(i18n("Yellow"), this);
    m_aPaintModeYellow->setCheckable(true);
    connect(m_aPaintModeYellow, &QAction::toggled, this, &Waveform::slotPaintModeChanged);

    m_aPaintModeWhite = new QAction(i18n("White"), this);
    m_aPaintModeWhite->setCheckable(true);
    connect(m_aPaintModeWhite, &QAction::toggled, this, &Waveform::slotPaintModeChanged);

    m_aPaintModeGreen = new QAction(i18n("Green"), this);
    m_aPaintModeGreen->setCheckable(true);
    connect(m_aPaintModeGreen, &QAction::toggled, this, &Waveform::slotPaintModeChanged);

    m_agPaintMode = new QActionGroup(this);
    m_agPaintMode->addAction(m_aPaintModeYellow);
    m_agPaintMode->addAction(m_aPaintModeWhite);
    m_agPaintMode->addAction(m_aPaintModeGreen);

    m_paintModeMenu->addAction(m_aPaintModeYellow);
    m_paintModeMenu->addAction(m_aPaintModeWhite);
    m_paintModeMenu->addAction(m_aPaintModeGreen);

    m_aRec601 = new QAction(i18n("Rec. 601"), this);
    m_aRec601->setCheckable(true);
    connect(m_aRec601, &QAction::toggled, this, &Waveform::forceUpdateScope);

    m_aRec709 = new QAction(i18n("Rec. 709"), this);
    m_aRec709->setCheckable(true);
    connect(m_aRec709, &QAction::toggled, this, &Waveform::forceUpdateScope);

    m_agRec = new QActionGroup(this);
    m_agRec->addAction(m_aRec601);
    m_agRec->addAction(m_aRec709);

    m_settingsMenu->addSeparator()->setText(i18n("Luma mode"));
    m_settingsMenu->addAction(m_aRec601);
    m_settingsMenu->addAction(m_aRec709);

    m_settingsMenu->addSeparator();
    m_settingsMenu->addAction(m_aRealtime);

    connect(this, &Waveform::signalMousePositionChanged, this, &Waveform::forceUpdateHUD);
    connect(pCore.get(), &Core::updatePalette, this, [this]() { forceUpdate(true); });

    init();
    m_waveformGenerator = new WaveformGenerator();
}

Waveform::~Waveform()
{
    writeConfig();

    delete m_waveformGenerator;

    // Delete settings menu actions
    delete m_aPaintModeYellow;
    delete m_aPaintModeWhite;
    delete m_aPaintModeGreen;
    delete m_agPaintMode;

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

    // Read paint mode
    int paintMode = scopeConfig.readEntry("paintmode", 0);
    switch (paintMode) {
    case 0:
        m_aPaintModeYellow->setChecked(true);
        break;
    case 1:
        m_aPaintModeWhite->setChecked(true);
        break;
    case 2:
        m_aPaintModeGreen->setChecked(true);
        break;
    default:
        m_aPaintModeYellow->setChecked(true);
        break;
    }
    slotPaintModeChanged();

    m_aRec601->setChecked(scopeConfig.readEntry("rec601", false));
    m_aRec709->setChecked(!m_aRec601->isChecked());
}

void Waveform::writeConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());

    // Write paint mode
    int paintMode = 0;
    if (m_aPaintModeYellow->isChecked())
        paintMode = 0;
    else if (m_aPaintModeWhite->isChecked())
        paintMode = 1;
    else if (m_aPaintModeGreen->isChecked())
        paintMode = 2;
    scopeConfig.writeEntry("paintmode", paintMode);

    scopeConfig.writeEntry("rec601", m_aRec601->isChecked());
    scopeConfig.sync();
}

QRect Waveform::scopeRect()
{
    QPoint topleft(offset, m_ui->hamburgerButton->geometry().bottom() + 2 * offset);
    return QRect(topleft, QPoint(this->size().width() - offset, this->size().height() - offset));
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

    if (scopeRect().height() <= 0) {
        return hud;
    }

    QFontMetrics fm = davinci.fontMetrics();

    const int hudScaleOffsetTop = 2 * offset;

    const int rightX = scopeRect().width() - offset - m_textWidth;
    const int x = m_mousePos.x() - scopeRect().x();
    const int y = m_mousePos.y() - scopeRect().y();

    bool mouseWithinScope =
        m_mouseWithinWidget && y >= WaveformGenerator::distBorder && y <= (scopeRect().height() - m_paddingBottom - WaveformGenerator::distBorder);

    if (mouseWithinScope) {
        int val = 255 + int(255. * (1 - y) / (scopeRect().height() - m_paddingBottom - 2 * WaveformGenerator::distBorder));

        if (val >= 0 && val <= 255) {
            const int textHeight = fm.height();
            const int textMargin = 2;

            // Scale baseline positions
            const int scaleTopY = WaveformGenerator::distBorder + hudScaleOffsetTop;                         // "255" position
            const int scaleBottomY = scopeRect().height() - m_paddingBottom - WaveformGenerator::distBorder; // "0" position

            // Calculate the natural position for the current value text (centered on horizontal line)
            // drawText uses baseline positioning, so we need to adjust for centering
            int naturalValY = y + fm.ascent() - textHeight / 2;

            // Check if current value text would clip with scale values or widget edges
            // For top scale: check if current value overlaps with the "255" text area
            bool clipsWithTop = (naturalValY - textHeight / 2) < (scaleTopY + fm.descent() + textMargin);
            // For bottom scale: check if current value overlaps with the "0" text area
            bool clipsWithBottom = (naturalValY + textHeight / 2) > (scaleBottomY - fm.ascent() - textMargin);
            bool clipsWithWidgetTop = (naturalValY - textHeight / 2) < textMargin;
            bool clipsWithWidgetBottom = (naturalValY + textHeight / 2) > (scopeRect().height() - textMargin);

            // Calculate final text position with edge clipping adjustments
            int finalValY = naturalValY;
            if (clipsWithWidgetTop) {
                finalValY = textMargin + textHeight / 2;
            } else if (clipsWithWidgetBottom) {
                finalValY = scopeRect().height() - textMargin - textHeight / 2;
            }

            // Draw scale values, only hide the ones that would clip
            davinci.setPen(palette().text().color());
            if (!clipsWithTop) {
                davinci.drawText(rightX, scaleTopY, QStringLiteral("255"));
            }
            if (!clipsWithBottom) {
                davinci.drawText(rightX, scaleBottomY, QStringLiteral("0"));
            }

            // Always draw the current value and the horizontal line
            davinci.setPen(palette().highlight().color());
            davinci.drawText(rightX, finalValY, QVariant(val).toString());
            davinci.drawLine(WaveformGenerator::distBorder, y, scopeRect().size().width() - m_textWidth - 2 * offset - WaveformGenerator::distBorder, y);
        }

        if (scopeRect().width() > 0) {
            // Draw a vertical line and the x position of the source clip
            const int profileWidth = pCore->getCurrentProfile()->width();

            const int drawingAreaWidth = scopeRect().width() - 2 * offset - m_textWidth - 2 * WaveformGenerator::distBorder;

            const int clipX = ((x - WaveformGenerator::distBorder) * (profileWidth - 1)) / (drawingAreaWidth - 1);

            if (clipX >= 0 && clipX <= profileWidth) {
                int valX = x - 15;
                if (valX < 0) {
                    valX = 0;
                }
                if (valX >= drawingAreaWidth) {
                    valX = drawingAreaWidth - 1;
                }

                davinci.drawLine(x, y, x, scopeRect().height() - m_paddingBottom);
                davinci.drawText(valX, scopeRect().height() - offset, QVariant(clipX).toString() + QStringLiteral(" px"));
            }
        }
    } else {
        // When mouse is not within scope, always show scale values
        davinci.setPen(palette().text().color());
        davinci.drawText(rightX, scopeRect().height() - m_paddingBottom - WaveformGenerator::distBorder, QStringLiteral("0"));
        davinci.drawText(rightX, WaveformGenerator::distBorder + hudScaleOffsetTop, QStringLiteral("255"));
    }

    Q_EMIT signalHUDRenderingFinished(0, 1);
    return hud;
}

QImage Waveform::renderGfxScope(uint accelFactor, const QImage &qimage)
{
    QElapsedTimer timer;
    timer.start();

    ITURec rec = m_aRec601->isChecked() ? ITURec::Rec_601 : ITURec::Rec_709;
    qreal scalingFactor = devicePixelRatioF();
    QImage wave = m_waveformGenerator->calculateWaveform((scopeRect().size() - QSize(m_textWidth + 2 * offset, 0) - QSize(0, m_paddingBottom)), scalingFactor,
                                                         qimage, WaveformGenerator::PaintMode(m_iPaintMode), true, rec, accelFactor);

    Q_EMIT signalScopeRenderingFinished(uint(timer.elapsed()), 1);
    return wave;
}

QImage Waveform::renderBackground(uint)
{
    Q_EMIT signalBackgroundRenderingFinished(0, 1);
    return QImage();
}

void Waveform::showSettingsMenu()
{
    m_ui->hamburgerButton->setDown(true);
    connect(m_settingsMenu, &QMenu::aboutToHide, this, [this]() { m_ui->hamburgerButton->setDown(false); });
    m_settingsMenu->exec(m_ui->hamburgerButton->mapToGlobal(m_ui->hamburgerButton->rect().bottomLeft()));
}

void Waveform::slotPaintModeChanged()
{
    if (m_aPaintModeYellow->isChecked()) {
        m_iPaintMode = WaveformGenerator::PaintMode_Yellow;
    } else if (m_aPaintModeWhite->isChecked()) {
        m_iPaintMode = WaveformGenerator::PaintMode_White;
    } else if (m_aPaintModeGreen->isChecked()) {
        m_iPaintMode = WaveformGenerator::PaintMode_Green;
    }
    forceUpdateScope();
}
