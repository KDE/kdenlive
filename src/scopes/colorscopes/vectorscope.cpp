/*
    SPDX-FileCopyrightText: 2010 Simon Andreas Eugster <simon.eu@gmail.com>
    This file is part of kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "vectorscope.h"
#include "colorplaneexport.h"
#include "utils/colortools.h"
#include "vectorscopegenerator.h"

#include "core.h"
#include "kdenlive_debug.h"
#include "klocalizedstring.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <QAction>
#include <QActionGroup>
#include <QElapsedTimer>
#include <QInputDialog>
#include <QPainter>
#include <cmath>
const double P75 = .75;

const QPointF YUV_R(-.147, .615);
const QPointF YUV_G(-.289, -.515);
const QPointF YUV_B(.437, -.100);
const QPointF YUV_Cy(.147, -.615);
const QPointF YUV_Mg(.289, .515);
const QPointF YUV_Yl(-.437, .100);

const QPointF YPbPr_R(-.169, .5);
const QPointF YPbPr_G(-.331, -.419);
const QPointF YPbPr_B(.5, -.081);
const QPointF YPbPr_Cy(.169, -.5);
const QPointF YPbPr_Mg(.331, .419);
const QPointF YPbPr_Yl(-.5, .081);

Vectorscope::Vectorscope(QWidget *parent)
    : AbstractGfxScopeWidget(true, parent)

{
    // overwrite custom scopes palette from AbstractScopeWidget with global app palette to respect users theme preference
    setPalette(QPalette());

    m_ui = new Ui::Vectorscope_UI();
    m_ui->setupUi(this);

    // Disable legacy right-click context menu; use hamburger menu instead
    setContextMenuPolicy(Qt::NoContextMenu);

    m_colorTools = new ColorTools();
    m_vectorscopeGenerator = new VectorscopeGenerator();

    // Create settings menu
    m_settingsMenu = new QMenu(this);
    m_ui->hamburgerButton->setMenu(m_settingsMenu);

    // Paint mode submenu
    m_paintModeMenu = new QMenu(i18n("Paint Mode"), m_settingsMenu);
    m_settingsMenu->addMenu(m_paintModeMenu);

    m_aPaintModeGreen2 = new QAction(i18n("Green 2"), this);
    m_aPaintModeGreen2->setCheckable(true);
    connect(m_aPaintModeGreen2, &QAction::toggled, this, &Vectorscope::slotPaintModeChanged);

    m_aPaintModeGreen = new QAction(i18n("Green"), this);
    m_aPaintModeGreen->setCheckable(true);
    connect(m_aPaintModeGreen, &QAction::toggled, this, &Vectorscope::slotPaintModeChanged);

    m_aPaintModeBlack = new QAction(i18n("Black"), this);
    m_aPaintModeBlack->setCheckable(true);
    connect(m_aPaintModeBlack, &QAction::toggled, this, &Vectorscope::slotPaintModeChanged);

    m_aPaintModeChroma = new QAction(i18n("Modified YUV (Chroma)"), this);
    m_aPaintModeChroma->setCheckable(true);
    connect(m_aPaintModeChroma, &QAction::toggled, this, &Vectorscope::slotPaintModeChanged);

    m_aPaintModeYUV = new QAction(i18n("YUV"), this);
    m_aPaintModeYUV->setCheckable(true);
    connect(m_aPaintModeYUV, &QAction::toggled, this, &Vectorscope::slotPaintModeChanged);

    m_aPaintModeOriginal = new QAction(i18n("Original Color"), this);
    m_aPaintModeOriginal->setCheckable(true);
    connect(m_aPaintModeOriginal, &QAction::toggled, this, &Vectorscope::slotPaintModeChanged);

    m_agPaintMode = new QActionGroup(this);
    m_agPaintMode->addAction(m_aPaintModeGreen2);
    m_agPaintMode->addAction(m_aPaintModeGreen);
    m_agPaintMode->addAction(m_aPaintModeBlack);
    m_agPaintMode->addAction(m_aPaintModeChroma);
    m_agPaintMode->addAction(m_aPaintModeYUV);
    m_agPaintMode->addAction(m_aPaintModeOriginal);

    m_paintModeMenu->addAction(m_aPaintModeGreen2);
    m_paintModeMenu->addAction(m_aPaintModeGreen);
    m_paintModeMenu->addAction(m_aPaintModeBlack);
    m_paintModeMenu->addAction(m_aPaintModeChroma);
    m_paintModeMenu->addAction(m_aPaintModeYUV);
    m_paintModeMenu->addAction(m_aPaintModeOriginal);

    // Background mode submenu
    m_backgroundModeMenu = new QMenu(i18n("Background"), m_settingsMenu);
    m_settingsMenu->addMenu(m_backgroundModeMenu);

    m_aBackgroundDark = new QAction(i18n("Black"), this);
    m_aBackgroundDark->setCheckable(true);
    connect(m_aBackgroundDark, &QAction::toggled, this, &Vectorscope::slotBackgroundModeChanged);

    m_aBackgroundYUV = new QAction(i18n("YUV"), this);
    m_aBackgroundYUV->setCheckable(true);
    connect(m_aBackgroundYUV, &QAction::toggled, this, &Vectorscope::slotBackgroundModeChanged);

    m_aBackgroundChroma = new QAction(i18n("Modified YUV (Chroma)"), this);
    m_aBackgroundChroma->setCheckable(true);
    connect(m_aBackgroundChroma, &QAction::toggled, this, &Vectorscope::slotBackgroundModeChanged);

    m_aBackgroundYPbPr = new QAction(i18n("YPbPr"), this);
    m_aBackgroundYPbPr->setCheckable(true);
    connect(m_aBackgroundYPbPr, &QAction::toggled, this, &Vectorscope::slotBackgroundModeChanged);

    m_agBackgroundMode = new QActionGroup(this);
    m_agBackgroundMode->addAction(m_aBackgroundDark);
    m_agBackgroundMode->addAction(m_aBackgroundYUV);
    m_agBackgroundMode->addAction(m_aBackgroundChroma);
    m_agBackgroundMode->addAction(m_aBackgroundYPbPr);

    m_backgroundModeMenu->addAction(m_aBackgroundDark);
    m_backgroundModeMenu->addAction(m_aBackgroundYUV);
    m_backgroundModeMenu->addAction(m_aBackgroundChroma);
    m_backgroundModeMenu->addAction(m_aBackgroundYPbPr);

    // Connect gain control widgets
    connect(m_ui->gainSlider, &QSlider::valueChanged, this, &Vectorscope::slotGainSliderChanged);
    connect(m_ui->gainButton, &QToolButton::clicked, this, &Vectorscope::slotGainButtonClicked);

    connect(this, &Vectorscope::signalMousePositionChanged, this, &Vectorscope::forceUpdateHUD);
    connect(pCore.get(), &Core::updatePalette, this, [this]() { forceUpdate(true); });

    ///// Build hamburger settings menu /////

    // Tools
    m_settingsMenu->addSeparator()->setText(i18n("Tools"));

    m_aExportBackground = new QAction(i18n("Export background"), this);
    connect(m_aExportBackground, &QAction::triggered, this, &Vectorscope::slotExportBackground);
    m_settingsMenu->addAction(m_aExportBackground);

    // Drawing options
    m_settingsMenu->addSeparator()->setText(i18n("Drawing options"));

    m_a75PBox = new QAction(i18n("75% box"), this);
    m_a75PBox->setCheckable(true);
    connect(m_a75PBox, &QAction::changed, this, &Vectorscope::forceUpdateBackground);
    m_settingsMenu->addAction(m_a75PBox);

    m_aAxisEnabled = new QAction(i18n("Draw axis"), this);
    m_aAxisEnabled->setCheckable(true);
    connect(m_aAxisEnabled, &QAction::changed, this, &Vectorscope::forceUpdateBackground);
    m_settingsMenu->addAction(m_aAxisEnabled);

    m_aIQLines = new QAction(i18n("Draw I/Q lines"), this);
    m_aIQLines->setCheckable(true);
    connect(m_aIQLines, &QAction::changed, this, &Vectorscope::forceUpdateBackground);
    m_settingsMenu->addAction(m_aIQLines);

    // Color Space
    m_settingsMenu->addSeparator()->setText(i18n("Color Space"));
    m_aColorSpace_YPbPr = new QAction(i18n("YPbPr"), this);
    m_aColorSpace_YPbPr->setCheckable(true);
    connect(m_aColorSpace_YPbPr, &QAction::toggled, this, &Vectorscope::slotColorSpaceChanged);
    m_aColorSpace_YUV = new QAction(i18n("YUV"), this);
    m_aColorSpace_YUV->setCheckable(true);
    connect(m_aColorSpace_YUV, &QAction::toggled, this, &Vectorscope::slotColorSpaceChanged);
    m_agColorSpace = new QActionGroup(this);
    m_agColorSpace->addAction(m_aColorSpace_YPbPr);
    m_agColorSpace->addAction(m_aColorSpace_YUV);
    m_settingsMenu->addAction(m_aColorSpace_YPbPr);
    m_settingsMenu->addAction(m_aColorSpace_YUV);

    m_settingsMenu->addSeparator();
    m_settingsMenu->addAction(m_aAutoRefresh);
    m_settingsMenu->addAction(m_aRealtime);
    m_menu->removeAction(m_aAutoRefresh);
    m_menu->removeAction(m_aRealtime);

    init();
}

Vectorscope::~Vectorscope()
{
    writeConfig();

    delete m_colorTools;
    delete m_vectorscopeGenerator;

    delete m_aColorSpace_YPbPr;
    delete m_aColorSpace_YUV;
    delete m_aExportBackground;
    delete m_aAxisEnabled;
    delete m_a75PBox;
    delete m_agColorSpace;

    // Delete settings menu actions
    delete m_aPaintModeGreen2;
    delete m_aPaintModeGreen;
    delete m_aPaintModeBlack;
    delete m_aPaintModeChroma;
    delete m_aPaintModeYUV;
    delete m_aPaintModeOriginal;
    delete m_agPaintMode;

    delete m_aBackgroundDark;
    delete m_aBackgroundYUV;
    delete m_aBackgroundChroma;
    delete m_aBackgroundYPbPr;
    delete m_agBackgroundMode;

    delete m_ui;
}

QString Vectorscope::widgetName() const
{
    return QStringLiteral("Vectorscope");
}

void Vectorscope::readConfig()
{
    AbstractGfxScopeWidget::readConfig();

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());
    m_a75PBox->setChecked(scopeConfig.readEntry("75PBox", false));
    m_aAxisEnabled->setChecked(scopeConfig.readEntry("axis", false));
    m_aIQLines->setChecked(scopeConfig.readEntry("iqlines", false));

    // Read paint mode
    int paintMode = scopeConfig.readEntry("paintmode", 0);
    switch (paintMode) {
    case 0:
        m_aPaintModeGreen2->setChecked(true);
        break;
    case 1:
        m_aPaintModeGreen->setChecked(true);
        break;
    case 2:
        m_aPaintModeBlack->setChecked(true);
        break;
    case 3:
        m_aPaintModeChroma->setChecked(true);
        break;
    case 4:
        m_aPaintModeYUV->setChecked(true);
        break;
    case 5:
        m_aPaintModeOriginal->setChecked(true);
        break;
    default:
        m_aPaintModeGreen2->setChecked(true);
        break;
    }
    slotPaintModeChanged();

    // Read background mode
    int backgroundMode = scopeConfig.readEntry("backgroundmode", 0);
    switch (backgroundMode) {
    case 0:
        m_aBackgroundDark->setChecked(true);
        break;
    case 1:
        m_aBackgroundYUV->setChecked(true);
        break;
    case 2:
        m_aBackgroundChroma->setChecked(true);
        break;
    case 3:
        m_aBackgroundYPbPr->setChecked(true);
        break;
    default:
        m_aBackgroundDark->setChecked(true);
        break;
    }
    slotBackgroundModeChanged();

    m_gain = scopeConfig.readEntry("gain", 1.0);

    // Update slider and label to match the gain value
    int sliderValue = int(m_gain * 10.0); // Convert gain (0.1-5.0) to slider range (1-50)
    sliderValue = qBound(1, sliderValue, 50);
    m_ui->gainSlider->setValue(sliderValue);
    m_ui->gainLabel->setText(QStringLiteral("%1x").arg(m_gain, 0, 'f', 1));

    m_aColorSpace_YPbPr->setChecked(scopeConfig.readEntry("colorspace_ypbpr", false));
    m_aColorSpace_YUV->setChecked(!m_aColorSpace_YPbPr->isChecked());
}

void Vectorscope::writeConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup scopeConfig(config, configName());
    scopeConfig.writeEntry("75PBox", m_a75PBox->isChecked());
    scopeConfig.writeEntry("axis", m_aAxisEnabled->isChecked());
    scopeConfig.writeEntry("iqlines", m_aIQLines->isChecked());

    // Write paint mode
    int paintMode = 0;
    if (m_aPaintModeGreen2->isChecked())
        paintMode = 0;
    else if (m_aPaintModeGreen->isChecked())
        paintMode = 1;
    else if (m_aPaintModeBlack->isChecked())
        paintMode = 2;
    else if (m_aPaintModeChroma->isChecked())
        paintMode = 3;
    else if (m_aPaintModeYUV->isChecked())
        paintMode = 4;
    else if (m_aPaintModeOriginal->isChecked())
        paintMode = 5;
    scopeConfig.writeEntry("paintmode", paintMode);

    // Write background mode
    int backgroundMode = 0;
    if (m_aBackgroundDark->isChecked())
        backgroundMode = 0;
    else if (m_aBackgroundYUV->isChecked())
        backgroundMode = 1;
    else if (m_aBackgroundChroma->isChecked())
        backgroundMode = 2;
    else if (m_aBackgroundYPbPr->isChecked())
        backgroundMode = 3;
    scopeConfig.writeEntry("backgroundmode", backgroundMode);

    scopeConfig.writeEntry("gain", m_gain);
    scopeConfig.writeEntry("colorspace_ypbpr", m_aColorSpace_YPbPr->isChecked());
    scopeConfig.sync();
}

QRect Vectorscope::scopeRect()
{
    // Distance from top/left/right
    int border = 6;

    // We want to paint below the hamburger button area
    QPoint topleft(border, m_ui->hamburgerButton->geometry().bottom() + border);
    QPoint bottomright(this->size().width() - border, this->size().height() - border);

    m_visibleRect = QRect(topleft, bottomright);

    QRect scopeRect(topleft, bottomright);

    // Circle Width: min of width and height
    m_cw = (scopeRect.height() < scopeRect.width()) ? scopeRect.height() : scopeRect.width();
    scopeRect.setWidth(m_cw);
    scopeRect.setHeight(m_cw);

    m_centerPoint = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), QPointF(0, 0));
    m_pR75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75 * VectorscopeGenerator::scaling * YUV_R);
    m_pG75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75 * VectorscopeGenerator::scaling * YUV_G);
    m_pB75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75 * VectorscopeGenerator::scaling * YUV_B);
    m_pCy75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75 * VectorscopeGenerator::scaling * YUV_Cy);
    m_pMg75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75 * VectorscopeGenerator::scaling * YUV_Mg);
    m_pYl75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75 * VectorscopeGenerator::scaling * YUV_Yl);
    m_qR75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75 * VectorscopeGenerator::scaling * YPbPr_R);
    m_qG75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75 * VectorscopeGenerator::scaling * YPbPr_G);
    m_qB75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75 * VectorscopeGenerator::scaling * YPbPr_B);
    m_qCy75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75 * VectorscopeGenerator::scaling * YPbPr_Cy);
    m_qMg75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75 * VectorscopeGenerator::scaling * YPbPr_Mg);
    m_qYl75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75 * VectorscopeGenerator::scaling * YPbPr_Yl);

    return scopeRect;
}

bool Vectorscope::isHUDDependingOnInput() const
{
    return false;
}
bool Vectorscope::isScopeDependingOnInput() const
{
    return true;
}
bool Vectorscope::isBackgroundDependingOnInput() const
{
    return false;
}

QImage Vectorscope::renderHUD(uint)
{

    QImage hud;
    QLocale locale; // Used for UI → OK
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    if (m_mouseWithinWidget) {
        // Mouse moved: Draw a circle over the scope

        qreal dpr = devicePixelRatioF();
        hud = QImage(m_visibleRect.size() * dpr, QImage::Format_ARGB32);
        hud.setDevicePixelRatio(dpr);
        hud.fill(qRgba(0, 0, 0, 0));

        QPainter davinci;
        bool ok = davinci.begin(&hud);
        if (!ok) {
            qDebug() << "Could not initialise QPainter for Vectorscope HUD.";
            return hud;
        }
        QPoint widgetCenterPoint = m_scopeRect.topLeft() + m_centerPoint;

        int dx = -widgetCenterPoint.x() + m_mousePos.x();
        int dy = widgetCenterPoint.y() - m_mousePos.y();

        QPoint reference = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(1, 0));

        float r = sqrtf(dx * dx + dy * dy);
        float percent = 100.f * r / float(VectorscopeGenerator::scaling) / m_gain / (reference.x() - widgetCenterPoint.x());

        QPen penHighlight(QBrush(palette().highlight().color()), penLight.width(), Qt::SolidLine);

        switch (m_backgroundMode) {
        case BG_DARK:
            davinci.setPen(penHighlight);
            break;
        default:
            if (r > m_cw / 2.0f) {
                davinci.setPen(penHighlight);
            } else {
                davinci.setPen(penDark);
            }
            break;
        }
        davinci.setRenderHint(QPainter::Antialiasing, true);
        davinci.drawEllipse(m_centerPoint, int(r), int(r));
        davinci.setRenderHint(QPainter::Antialiasing, false);
        davinci.setPen(penHighlight);
        davinci.drawText(QPoint(m_scopeRect.width() - 40, m_scopeRect.height()), i18n("%1%", locale.toString(percent, 'f', 0)));

        float angle = float(copysignf(std::acos(dx / r) * 180.f / float(M_PI), dy));
        davinci.drawText(QPoint(10, m_scopeRect.height()), i18n("%1°", locale.toString(angle, 'f', 1)));

    } else {
        hud = QImage(0, 0, QImage::Format_ARGB32);
    }
    Q_EMIT signalHUDRenderingFinished(0, 1);
    return hud;
}

QImage Vectorscope::renderGfxScope(uint accelerationFactor, const QImage &qimage)
{
    QElapsedTimer timer;
    timer.start();
    QImage scope;

    if (m_cw <= 0) {
        qCDebug(KDENLIVE_LOG) << "Scope size not known yet. Aborting.";
    } else {

        VectorscopeGenerator::ColorSpace colorSpace =
            m_aColorSpace_YPbPr->isChecked() ? VectorscopeGenerator::ColorSpace_YPbPr : VectorscopeGenerator::ColorSpace_YUV;
        VectorscopeGenerator::PaintMode paintMode = VectorscopeGenerator::PaintMode(m_iPaintMode);
        qreal dpr = devicePixelRatioF();
        scope = m_vectorscopeGenerator->calculateVectorscope(m_scopeRect.size() * dpr, dpr, qimage, m_gain, paintMode, colorSpace, m_aAxisEnabled->isChecked(),
                                                             accelerationFactor);
    }
    Q_EMIT signalScopeRenderingFinished(uint(timer.elapsed()), accelerationFactor);
    return scope;
}

QImage Vectorscope::renderBackground(uint)
{
    QElapsedTimer timer;
    timer.start();

    qreal scalingFactor = devicePixelRatioF();
    QImage bg(m_visibleRect.size() * scalingFactor, QImage::Format_ARGB32);
    bg.setDevicePixelRatio(scalingFactor);
    bg.fill(qRgba(0, 0, 0, 0));

    // Set up tools
    QPainter davinci;
    bool ok = davinci.begin(&bg);
    if (!ok) {
        qDebug() << "Could not initialise QPainter for Vectorscope background.";
        return bg;
    }
    davinci.setRenderHint(QPainter::Antialiasing, true);

    QPoint vinciPoint;
    QPoint vinciPoint2;

    // Alignment correction
    // ColorTools colorWheel functions and VectorscopeGenerator::mapToCircle are not anti-aliased.
    // As we're drawing the circle border anti-aliased we need to compensate for that as we might spill outside the anti-aliased circle otherwise.
    // When drawing the I/Q lines they could also spill outside the circle in case of fractional HiDPI scaling and potential rounding differences between
    // drawing the circle and the lines. In the following we'll be insetting these drawing calls by 1px and then drawing the circle border with at least 2px
    // width.
    QSize alignmentCorrection(2, 2);
    int alignmentCorrectionOffset = 1;
    QPoint alignmentCorrectionPoint(alignmentCorrectionOffset, alignmentCorrectionOffset);

    // Draw the color plane (if selected)
    QImage colorPlane;
    switch (m_backgroundMode) {
    case BG_YUV:
        colorPlane = m_colorTools->yuvColorWheel(m_scopeRect.size() - alignmentCorrection, 128, 1 / float(VectorscopeGenerator::scaling), false, true);
        break;
    case BG_CHROMA:
        colorPlane = m_colorTools->yuvColorWheel(m_scopeRect.size() - alignmentCorrection, 255, 1 / float(VectorscopeGenerator::scaling), true, true);
        break;
    case BG_YPbPr:
        colorPlane = m_colorTools->yPbPrColorWheel(m_scopeRect.size() - alignmentCorrection, 128, 1 / float(VectorscopeGenerator::scaling), true);
        break;
    case BG_DARK:
        colorPlane = m_colorTools->FixedColorCircle(m_scopeRect.size() - alignmentCorrection, qRgba(25, 25, 23, 255));
        break;
    }
    davinci.drawImage(alignmentCorrectionOffset, alignmentCorrectionOffset, colorPlane);

    // Draw I/Q lines (from the YIQ color space; Skin tones lie on the I line)
    // Positions are calculated by transforming YIQ:[0 1 0] or YIQ:[0 0 1] to YUV/YPbPr.
    if (m_aIQLines->isChecked()) {
        QPen iqTextLabelPen(QBrush(palette().text().color()), 2, Qt::SolidLine);

        switch (m_backgroundMode) {
        case BG_DARK:
            davinci.setPen(penLightDots);
            break;
        default:
            davinci.setPen(penDarkDots);
            break;
        }

        if (m_aColorSpace_YUV->isChecked()) {
            vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size() - alignmentCorrection, QPointF(-.544, .838));
            vinciPoint2 = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size() - alignmentCorrection, QPointF(.544, -.838));
        } else {
            vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size() - alignmentCorrection, QPointF(-.675, .737));
            vinciPoint2 = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size() - alignmentCorrection, QPointF(.675, -.737));
        }

        davinci.drawLine(vinciPoint + alignmentCorrectionPoint, vinciPoint2 + alignmentCorrectionPoint);
        davinci.setPen(iqTextLabelPen);
        davinci.drawText(vinciPoint - QPoint(11, 5), QStringLiteral("I"));

        switch (m_backgroundMode) {
        case BG_DARK:
            davinci.setPen(penLightDots);
            break;
        default:
            davinci.setPen(penDarkDots);
            break;
        }

        if (m_aColorSpace_YUV->isChecked()) {
            vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size() - alignmentCorrection, QPointF(.838, .544));
            vinciPoint2 = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size() - alignmentCorrection, QPointF(-.838, -.544));
        } else {
            vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size() - alignmentCorrection, QPointF(.908, .443));
            vinciPoint2 = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size() - alignmentCorrection, QPointF(-.908, -.443));
        }

        davinci.drawLine(vinciPoint + alignmentCorrectionPoint, vinciPoint2 + alignmentCorrectionPoint);
        davinci.setPen(iqTextLabelPen);
        davinci.drawText(vinciPoint - QPoint(-7, 2), QStringLiteral("Q"));
    }

    // Draw the vectorscope circle
    bool isDarkTheme = palette().color(QPalette::Window).lightness() < palette().color(QPalette::WindowText).lightness();
    if (isDarkTheme) {
        davinci.setPen(penThick);
    } else {
        davinci.setPen(QPen(palette().color(QPalette::Dark), penThick.width(), Qt::SolidLine));
    }
    qreal penWidthHalf = davinci.pen().widthF() / 2;
    davinci.drawEllipse(QRectF(penWidthHalf, penWidthHalf, m_cw - davinci.pen().widthF(), m_cw - davinci.pen().widthF()));

    // Draw RGB/CMY points with 100% chroma
    davinci.setPen(penThick);
    if (m_aColorSpace_YUV->isChecked()) {
        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling * YUV_R);
        davinci.drawEllipse(vinciPoint, 4, 4);
        davinci.drawText(vinciPoint - QPoint(20, -10), QStringLiteral("R"));

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling * YUV_G);
        davinci.drawEllipse(vinciPoint, 4, 4);
        davinci.drawText(vinciPoint - QPoint(20, 0), QStringLiteral("G"));

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling * YUV_B);
        davinci.drawEllipse(vinciPoint, 4, 4);
        davinci.drawText(vinciPoint + QPoint(15, 10), QStringLiteral("B"));

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling * YUV_Cy);
        davinci.drawEllipse(vinciPoint, 4, 4);
        davinci.drawText(vinciPoint + QPoint(15, -5), QStringLiteral("Cy"));

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling * YUV_Mg);
        davinci.drawEllipse(vinciPoint, 4, 4);
        davinci.drawText(vinciPoint + QPoint(15, 10), QStringLiteral("Mg"));

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling * YUV_Yl);
        davinci.drawEllipse(vinciPoint, 4, 4);
        davinci.drawText(vinciPoint - QPoint(25, 0), QStringLiteral("Yl"));
    } else {
        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling * YPbPr_R);
        davinci.drawEllipse(vinciPoint, 4, 4);
        davinci.drawText(vinciPoint - QPoint(20, -10), QStringLiteral("R"));

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling * YPbPr_G);
        davinci.drawEllipse(vinciPoint, 4, 4);
        davinci.drawText(vinciPoint - QPoint(20, 0), QStringLiteral("G"));

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling * YPbPr_B);
        davinci.drawEllipse(vinciPoint, 4, 4);
        davinci.drawText(vinciPoint + QPoint(15, 10), QStringLiteral("B"));

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling * YPbPr_Cy);
        davinci.drawEllipse(vinciPoint, 4, 4);
        davinci.drawText(vinciPoint + QPoint(15, -5), QStringLiteral("Cy"));

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling * YPbPr_Mg);
        davinci.drawEllipse(vinciPoint, 4, 4);
        davinci.drawText(vinciPoint + QPoint(15, 10), QStringLiteral("Mg"));

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling * YPbPr_Yl);
        davinci.drawEllipse(vinciPoint, 4, 4);
        davinci.drawText(vinciPoint - QPoint(25, 0), QStringLiteral("Yl"));
    }

    // Draw axis
    if (m_aAxisEnabled->isChecked()) {
        switch (m_backgroundMode) {
        case BG_DARK:
            davinci.setPen(penLight);
            break;
        default:
            davinci.setPen(penDark);
            break;
        }
        davinci.drawLine(m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(0, -.9)),
                         m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(0, .9)));
        davinci.drawLine(m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(-.9, 0)),
                         m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(.9, 0)));
    }

    // Draw center point
    switch (m_backgroundMode) {
    case BG_CHROMA:
        davinci.setPen(penDark);
        break;
    default:
        davinci.setPen(penThin);
        break;
    }
    davinci.drawEllipse(m_centerPoint, 5, 5);

    // Draw 75% box
    if (m_a75PBox->isChecked()) {
        if (m_aColorSpace_YUV->isChecked()) {
            davinci.drawLine(m_pR75, m_pYl75);
            davinci.drawLine(m_pYl75, m_pG75);
            davinci.drawLine(m_pG75, m_pCy75);
            davinci.drawLine(m_pCy75, m_pB75);
            davinci.drawLine(m_pB75, m_pMg75);
            davinci.drawLine(m_pMg75, m_pR75);
        } else {
            davinci.drawLine(m_qR75, m_qYl75);
            davinci.drawLine(m_qYl75, m_qG75);
            davinci.drawLine(m_qG75, m_qCy75);
            davinci.drawLine(m_qCy75, m_qB75);
            davinci.drawLine(m_qB75, m_qMg75);
            davinci.drawLine(m_qMg75, m_qR75);
        }
    }

    // Draw RGB/CMY points with 75% chroma (for NTSC)
    davinci.setPen(penThin);
    if (m_aColorSpace_YUV->isChecked()) {
        davinci.drawEllipse(m_pR75, 3, 3);
        davinci.drawEllipse(m_pG75, 3, 3);
        davinci.drawEllipse(m_pB75, 3, 3);
        davinci.drawEllipse(m_pCy75, 3, 3);
        davinci.drawEllipse(m_pMg75, 3, 3);
        davinci.drawEllipse(m_pYl75, 3, 3);
    } else {
        davinci.drawEllipse(m_qR75, 3, 3);
        davinci.drawEllipse(m_qG75, 3, 3);
        davinci.drawEllipse(m_qB75, 3, 3);
        davinci.drawEllipse(m_qCy75, 3, 3);
        davinci.drawEllipse(m_qMg75, 3, 3);
        davinci.drawEllipse(m_qYl75, 3, 3);
    }

    // Draw realtime factor (number of skipped pixels)
    if (m_aRealtime->isChecked()) {
        davinci.setPen(penThin);
        davinci.drawText(QPoint(m_scopeRect.width() - 40, m_scopeRect.height() - 15), QVariant(m_accelFactorScope).toString().append(QStringLiteral("×")));
    }

    Q_EMIT signalBackgroundRenderingFinished(uint(timer.elapsed()), 1);
    return bg;
}

///// Slots /////

void Vectorscope::slotGainChanged(int newval)
{
    m_gain = 1 + newval / 10.f;
    forceUpdateScope();
}

void Vectorscope::slotGainSliderChanged(int value)
{
    m_gain = value / 10.0; // Convert slider value (1-50) to gain (0.1-5.0)
    m_ui->gainLabel->setText(QStringLiteral("%1x").arg(m_gain, 0, 'f', 1));
    forceUpdateScope();
}

void Vectorscope::slotGainButtonClicked()
{
    // Reset gain to 1.0 when button is clicked
    m_ui->gainSlider->setValue(10); // 10 corresponds to 1.0x gain
}

void Vectorscope::slotExportBackground()
{
    QPointer<ColorPlaneExport> colorPlaneExportDialog = new ColorPlaneExport(this);
    colorPlaneExportDialog->exec();
    delete colorPlaneExportDialog;
}

void Vectorscope::slotBackgroundChanged()
{
    // Background changed, switch to a suitable color mode now
    switch (m_backgroundMode) {
    case BG_YUV:
        if (m_iPaintMode != VectorscopeGenerator::PaintMode_Black) {
            m_aPaintModeBlack->setChecked(true);
            slotPaintModeChanged();
        }
        break;

    case BG_DARK:
        if (m_iPaintMode == VectorscopeGenerator::PaintMode_Black) {
            m_aPaintModeGreen2->setChecked(true);
            slotPaintModeChanged();
        }
        break;
    }
    forceUpdateBackground();
}

void Vectorscope::slotColorSpaceChanged()
{
    if (m_aColorSpace_YPbPr->isChecked()) {
        if (m_backgroundMode == BG_YUV) {
            m_aBackgroundYPbPr->setChecked(true);
            slotBackgroundModeChanged();
        }
    } else {
        if (m_backgroundMode == BG_YPbPr) {
            m_aBackgroundYUV->setChecked(true);
            slotBackgroundModeChanged();
        }
    }
    forceUpdate();
}

void Vectorscope::showSettingsMenu()
{
    m_settingsMenu->exec(m_ui->hamburgerButton->mapToGlobal(m_ui->hamburgerButton->rect().bottomLeft()));
}

void Vectorscope::showGainDialog()
{
    bool ok;
    double gain = QInputDialog::getDouble(this, i18n("Gain"), i18n("Gain value:"), m_gain, 0.1, 5.0, 1, &ok);
    if (ok) {
        m_gain = gain;
        forceUpdateScope();
    }
}

void Vectorscope::slotPaintModeChanged()
{
    if (m_aPaintModeGreen2->isChecked()) {
        m_iPaintMode = VectorscopeGenerator::PaintMode_Green2;
    } else if (m_aPaintModeGreen->isChecked()) {
        m_iPaintMode = VectorscopeGenerator::PaintMode_Green;
    } else if (m_aPaintModeBlack->isChecked()) {
        m_iPaintMode = VectorscopeGenerator::PaintMode_Black;
    } else if (m_aPaintModeChroma->isChecked()) {
        m_iPaintMode = VectorscopeGenerator::PaintMode_Chroma;
    } else if (m_aPaintModeYUV->isChecked()) {
        m_iPaintMode = VectorscopeGenerator::PaintMode_YUV;
    } else if (m_aPaintModeOriginal->isChecked()) {
        m_iPaintMode = VectorscopeGenerator::PaintMode_Original;
    }
    forceUpdateScope();
}

void Vectorscope::slotBackgroundModeChanged()
{
    if (m_aBackgroundDark->isChecked()) {
        m_backgroundMode = BG_DARK;
    } else if (m_aBackgroundYUV->isChecked()) {
        m_backgroundMode = BG_YUV;
    } else if (m_aBackgroundChroma->isChecked()) {
        m_backgroundMode = BG_CHROMA;
    } else if (m_aBackgroundYPbPr->isChecked()) {
        m_backgroundMode = BG_YPbPr;
    }
    forceUpdateBackground();
}
