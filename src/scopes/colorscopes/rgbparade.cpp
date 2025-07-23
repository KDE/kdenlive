/*
    SPDX-FileCopyrightText: 2010 Simon Andreas Eugster <simon.eu@gmail.com>
    This file is part of kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "rgbparade.h"
#include "rgbparadegenerator.h"
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

    connect(m_ui->paintMode, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &RGBParade::forceUpdateScope);
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

    int paintmode = m_ui->paintMode->itemData(m_ui->paintMode->currentIndex()).toInt();
    QImage parade = m_rgbParadeGenerator->calculateRGBParade(m_scopeRect.size(), devicePixelRatioF(), qimage, RGBParadeGenerator::PaintMode(paintmode),
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
