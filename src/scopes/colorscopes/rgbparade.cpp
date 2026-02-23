/*
    SPDX-FileCopyrightText: 2010 Simon Andreas Eugster <simon.eu@gmail.com>
    This file is part of kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "rgbparade.h"
#include "rgbparadegenerator.h"
#include <QActionGroup>
#include <QDebug>
#include <QElapsedTimer>
#include <QPainter>
#include <QRect>

#include "core.h"
#include "klocalizedstring.h"
#include <KConfigGroup>
#include <KSharedConfig>

RGBParade::RGBParade(QWidget *parent)
    : AbstractGfxScopeWidget(true, parent)
{
    // overwrite custom scopes palette from AbstractScopeWidget with global app palette to respect users theme preference
    setPalette(QPalette());

    m_ui = new Ui::RGBParade_UI();
    m_ui->setupUi(this);

    // Disable legacy right-click context menu; use hamburger menu instead
    setContextMenuPolicy(Qt::NoContextMenu);

    // Create settings menu
    m_settingsMenu = new QMenu(this);
    m_ui->hamburgerButton->setMenu(m_settingsMenu);

    // Paint mode submenu
    m_paintModeMenu = new QMenu(i18n("Paint Mode"), m_settingsMenu);
    m_settingsMenu->addMenu(m_paintModeMenu);

    m_aPaintModeRGB = new QAction(i18n("RGB"), this);
    m_aPaintModeRGB->setCheckable(true);
    connect(m_aPaintModeRGB, &QAction::toggled, this, &RGBParade::slotPaintModeChanged);

    m_aPaintModeWhite = new QAction(i18n("White"), this);
    m_aPaintModeWhite->setCheckable(true);
    connect(m_aPaintModeWhite, &QAction::toggled, this, &RGBParade::slotPaintModeChanged);

    m_agPaintMode = new QActionGroup(this);
    m_agPaintMode->addAction(m_aPaintModeRGB);
    m_agPaintMode->addAction(m_aPaintModeWhite);

    m_paintModeMenu->addAction(m_aPaintModeRGB);
    m_paintModeMenu->addAction(m_aPaintModeWhite);

    // Add separator and other options to settings menu
    m_settingsMenu->addSeparator();
    m_aAxis = new QAction(i18n("Draw axis"), this);
    m_aAxis->setCheckable(true);
    connect(m_aAxis, &QAction::changed, this, &RGBParade::forceUpdateScope);
    m_settingsMenu->addAction(m_aAxis);

    m_aGradRef = new QAction(i18n("Gradient reference line"), this);
    m_aGradRef->setCheckable(true);
    connect(m_aGradRef, &QAction::changed, this, &RGBParade::forceUpdateScope);
    m_settingsMenu->addAction(m_aGradRef);

    m_settingsMenu->addSeparator();
    m_settingsMenu->addAction(m_aRealtime);

    connect(this, &RGBParade::signalMousePositionChanged, this, &RGBParade::forceUpdateHUD);
    connect(pCore.get(), &Core::updatePalette, this, [this]() { forceUpdate(true); });

    m_rgbParadeGenerator = new RGBParadeGenerator();
    init();
}

RGBParade::~RGBParade()
{
    writeConfig();

    delete m_ui;
    delete m_rgbParadeGenerator;

    // Delete settings menu actions
    delete m_aPaintModeRGB;
    delete m_aPaintModeWhite;
    delete m_agPaintMode;

    delete m_aAxis;
    delete m_aGradRef;
}

void RGBParade::readConfig()
{
    AbstractGfxScopeWidget::readConfig();

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());

    // Read paint mode
    int paintMode = scopeConfig.readEntry("paintmode", 0);
    switch (paintMode) {
    case 0:
        m_aPaintModeRGB->setChecked(true);
        break;
    case 1:
        m_aPaintModeWhite->setChecked(true);
        break;
    default:
        m_aPaintModeRGB->setChecked(true);
        break;
    }
    slotPaintModeChanged();

    m_aAxis->setChecked(scopeConfig.readEntry("axis", false));
    m_aGradRef->setChecked(scopeConfig.readEntry("gradref", false));
}

void RGBParade::writeConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());

    // Write paint mode
    int paintMode = 0;
    if (m_aPaintModeRGB->isChecked())
        paintMode = 0;
    else if (m_aPaintModeWhite->isChecked())
        paintMode = 1;
    scopeConfig.writeEntry("paintmode", paintMode);

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
    QPoint topleft(offset, m_ui->hamburgerButton->geometry().bottom() + 2 * offset);
    return QRect(topleft, QPoint(this->size().width() - offset, this->size().height() - offset));
}

QImage RGBParade::renderHUD(uint)
{
    qreal scalingFactor = devicePixelRatioF();
    QImage hud(m_scopeRect.size() * scalingFactor, QImage::Format_ARGB32);
    hud.setDevicePixelRatio(scalingFactor);
    hud.fill(qRgba(0, 0, 0, 0));

    QPainter davinci;
    bool ok = davinci.begin(&hud);
    if (!ok) {
        qDebug() << "Could not initialise QPainter for RGB Parade HUD.";
        return hud;
    }

    if (scopeRect().height() <= 0) {
        return hud;
    }

    QFontMetrics fm = davinci.fontMetrics();

    const int hudScaleOffsetTop = 2 * offset;

    int x = scopeRect().width() - offset - fm.horizontalAdvance(QStringLiteral("255"));
    int y = m_mousePos.y() - scopeRect().y();
    bool mouseWithinScope = m_mouseWithinWidget && y >= RGBParadeGenerator::distBorder &&
                            y <= (scopeRect().height() - RGBParadeGenerator::distBottom - RGBParadeGenerator::distBorder);

    if (mouseWithinScope) {
        int val = 255 + int(255. * (1 - y) / (scopeRect().height() - RGBParadeGenerator::distBottom - 2 * RGBParadeGenerator::distBorder));

        if ((val >= 0) && (val <= 255)) {
            const int textHeight = fm.height();
            const int textMargin = 2;

            // Scale baseline positions
            const int scaleTopY = RGBParadeGenerator::distBorder + hudScaleOffsetTop;                                        // "255" position
            const int scaleBottomY = scopeRect().height() - RGBParadeGenerator::distBottom - RGBParadeGenerator::distBorder; // "0" position

            // Calculate the natural position for the current value text (centered on horizontal line)
            // drawText uses baseline positioning, so we need to adjust for centering
            int naturalValY = y + fm.ascent() - textHeight / 2;

            // Check if current value text would clip with scale values or widget edges
            // For top scale: check if current value overlaps with the "255" text area
            bool clipsWithTop = (naturalValY - textHeight / 2) < (scaleTopY + fm.descent() + textMargin);
            // For bottom scale: check if current value overlaps with the "0" text area
            bool clipsWithBottom = (naturalValY + textHeight / 2) > (scaleBottomY - fm.ascent() - textMargin);
            bool clipsWithWidgetTop = (naturalValY - textHeight / 2) < (RGBParadeGenerator::distBorder + textMargin);
            bool clipsWithWidgetBottom = (naturalValY + textHeight / 2) > (scopeRect().height() - RGBParadeGenerator::distBorder - textMargin);

            // Calculate final text position with edge clipping adjustments
            int finalValY = naturalValY;
            if (clipsWithWidgetTop) {
                finalValY = RGBParadeGenerator::distBorder + textMargin + textHeight / 2;
            } else if (clipsWithWidgetBottom) {
                finalValY = scopeRect().height() - RGBParadeGenerator::distBorder - textMargin - textHeight / 2;
            }

            // Draw scale values, only hide the ones that would clip
            davinci.setPen(palette().text().color());
            if (!clipsWithTop) {
                davinci.drawText(x, scaleTopY, QStringLiteral("255"));
            }
            if (!clipsWithBottom) {
                davinci.drawText(x, scaleBottomY, QStringLiteral("0"));
            }

            // Always draw the current value and the horizontal line
            davinci.setPen(palette().highlight().color());
            davinci.drawText(x, finalValY, QVariant(val).toString());
            davinci.drawLine(RGBParadeGenerator::distBorder, y, scopeRect().size().width() - RGBParadeGenerator::distRight - RGBParadeGenerator::distBorder, y);
        }
    } else {
        // When mouse is not within scope, always show scale values
        davinci.setPen(palette().text().color());
        davinci.drawText(x, scopeRect().height() - RGBParadeGenerator::distBottom - RGBParadeGenerator::distBorder, QStringLiteral("0"));
        davinci.drawText(x, RGBParadeGenerator::distBorder + hudScaleOffsetTop, QStringLiteral("255"));
    }

    Q_EMIT signalHUDRenderingFinished(1, 1);
    return hud;
}

QImage RGBParade::renderGfxScope(uint accelerationFactor, const QImage &qimage)
{
    QElapsedTimer timer;
    timer.start();

    QImage parade = m_rgbParadeGenerator->calculateRGBParade(m_scopeRect.size(), devicePixelRatioF(), qimage, RGBParadeGenerator::PaintMode(m_iPaintMode),
                                                             m_aAxis->isChecked(), m_aGradRef->isChecked(), accelerationFactor, palette());
    Q_EMIT signalScopeRenderingFinished(uint(timer.elapsed()), accelerationFactor);
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

void RGBParade::showSettingsMenu()
{
    m_settingsMenu->exec(m_ui->hamburgerButton->mapToGlobal(m_ui->hamburgerButton->rect().bottomLeft()));
}

void RGBParade::slotPaintModeChanged()
{
    if (m_aPaintModeRGB->isChecked()) {
        m_iPaintMode = RGBParadeGenerator::PaintMode_RGB;
    } else if (m_aPaintModeWhite->isChecked()) {
        m_iPaintMode = RGBParadeGenerator::PaintMode_White;
    }
    forceUpdateScope();
}
