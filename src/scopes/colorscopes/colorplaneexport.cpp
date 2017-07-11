/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "colorplaneexport.h"
#include "klocalizedstring.h"
#include <KMessageBox>
//#define DEBUG_CTE
#ifdef DEBUG_CTE
#include "kdenlive_debug.h"
#endif

const QString EXTENSION_PNG = QStringLiteral(".png");

ColorPlaneExport::ColorPlaneExport(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);

    m_colorTools = new ColorTools();

    tResX->setText(QStringLiteral("800"));
    tResY->setText(QStringLiteral("800"));

    cbColorspace->addItem(i18n("YUV UV plane"), QVariant(ColorPlaneExport::CPE_YUV));
    cbColorspace->addItem(i18n("YUV Y plane"), QVariant(ColorPlaneExport::CPE_YUV_Y));
    cbColorspace->addItem(i18n("Modified YUV (Chroma)"), QVariant(ColorPlaneExport::CPE_YUV_MOD));
    cbColorspace->addItem(i18n("YCbCr CbCr plane"), QVariant(ColorPlaneExport::CPE_YPbPr));
    cbColorspace->addItem(i18n("RGB plane, one component varying"), QVariant(ColorPlaneExport::CPE_RGB_CURVE));
    cbColorspace->addItem(i18n("HSV Hue Shift"), QVariant(ColorPlaneExport::CPE_HSV_HUESHIFT));
    cbColorspace->addItem(i18n("HSV Saturation"), QVariant(ColorPlaneExport::CPE_HSV_SATURATION));

    sliderColor->setSliderPosition(128);

    // 0  -> 1
    // 50 -> 0.5
    // 80 -> 0.2
    sliderScaling->setInvertedAppearance(true);
    sliderScaling->setRange(0, 80);
    sliderScaling->setSliderPosition(50);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &ColorPlaneExport::slotExportPlane);
    connect(tResX, &QLineEdit::textChanged, this, &ColorPlaneExport::slotValidate);
    connect(tResY, &QLineEdit::textChanged, this, &ColorPlaneExport::slotValidate);
    connect(kurlrequester, &KUrlRequester::textChanged, this, &ColorPlaneExport::slotValidate);
    connect(sliderColor, &QAbstractSlider::valueChanged, this, &ColorPlaneExport::slotUpdateDisplays);
    connect(sliderScaling, &QAbstractSlider::valueChanged, this, &ColorPlaneExport::slotUpdateDisplays);
    connect(cbColorspace, SIGNAL(currentIndexChanged(int)), this, SLOT(slotColormodeChanged()));

    kurlrequester->setUrl(QUrl(QStringLiteral("/tmp/yuv-plane.png")));

    slotColormodeChanged();
    slotValidate();
}

ColorPlaneExport::~ColorPlaneExport()
{
    delete m_colorTools;
}

///// Helper functions /////

void ColorPlaneExport::enableSliderScaling(bool enable)
{
    sliderScaling->setEnabled(enable);
    lblScaling->setEnabled(enable);
    lblScaleNr->setEnabled(enable);
}

void ColorPlaneExport::enableSliderColor(bool enable)
{
    sliderColor->setEnabled(enable);
    lblSliderName->setEnabled(enable);
    lblColNr->setEnabled(enable);
}

void ColorPlaneExport::enableCbVariant(bool enable)
{
    cbVariant->setEnabled(enable);
    lblVariant->setEnabled(enable);
    if (!enable) {
        cbVariant->clear();
    }
}

///// Slots /////

void ColorPlaneExport::slotUpdateDisplays()
{
    m_scaling = 1 - (float)sliderScaling->value() / 100;

    switch (cbColorspace->itemData(cbColorspace->currentIndex()).toInt()) {
    case CPE_RGB_CURVE:
        lblScaleNr->setText(QChar(0xb1) + QString::number(sliderScaling->value(), 'f', 2));
        break;
    case CPE_HSV_HUESHIFT:
        lblScaleNr->setText(QString::number(sliderScaling->value()));
        break;
    default:
        lblScaleNr->setText("0..." + QString::number(m_scaling, 'f', 2));
        break;
    }

    switch (cbColorspace->itemData(cbColorspace->currentIndex()).toInt()) {
    case CPE_YUV_Y:
        lblColNr->setText(i18n("%1°", QString::number(sliderColor->value())));
        break;
    default:
        lblColNr->setText(QString::number(sliderColor->value()));
        break;
    }

    lblSize->setText(i18n("%1 px", tResX->text().toInt() * tResY->text().toInt()));
}

void ColorPlaneExport::slotValidate()
{
    bool ok;
    int nr;

    nr = QVariant(tResX->text()).toInt(&ok);
    ok = ok && nr > 0;
    if (ok) {
        nr = QVariant(tResY->text()).toInt(&ok);
        ok = ok && nr > 0;
    }
    if (ok) {
        ok = !kurlrequester->text().trimmed().isEmpty();
#ifdef DEBUG_CPE
        qCDebug(KDENLIVE_LOG) << "File given: " << ok;
#endif
    }

    if (ok) {
        buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    } else {
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel);
    }

    slotUpdateDisplays();
}

void ColorPlaneExport::slotExportPlane()
{
#ifdef DEBUG_CPE
    qCDebug(KDENLIVE_LOG) << "Exporting plane now to " << kurlrequester->text();
#endif
    QString lower = kurlrequester->text().toLower();
#ifdef DEBUG_CPE
    qCDebug(KDENLIVE_LOG) << "Lower: " << lower;
#endif
    if (!lower.endsWith(QLatin1String(".png")) && !lower.endsWith(QLatin1String(".jpg")) && !lower.endsWith(QLatin1String(".tif")) &&
        !lower.endsWith(QLatin1String(".tiff"))) {
        if (KMessageBox::questionYesNo(this, i18n("File has no extension. Add extension (%1)?", EXTENSION_PNG)) == KMessageBox::Yes) {
            kurlrequester->setUrl(QUrl(kurlrequester->text() + QStringLiteral(".png")));
        }
    }
    QImage img;
    QColor col;
    QSize size(QVariant(tResX->text()).toInt(), QVariant(tResY->text()).toInt());
    switch (cbColorspace->itemData(cbColorspace->currentIndex()).toInt()) {
    case CPE_YUV:
        img = m_colorTools->yuvColorWheel(size, sliderColor->value(), m_scaling, false, false);
        break;
    case CPE_YUV_Y:
        img = m_colorTools->yuvVerticalPlane(size, sliderColor->value(), m_scaling);
        break;
    case CPE_YUV_MOD:
        img = m_colorTools->yuvColorWheel(size, sliderColor->value(), m_scaling, true, false);
        break;
    case CPE_RGB_CURVE:
        img = m_colorTools->rgbCurvePlane(size, (ColorTools::ColorsRGB)(cbVariant->itemData(cbVariant->currentIndex()).toInt()),
                                          (double)sliderScaling->value() / 255);
        break;
    case CPE_YPbPr:
        img = m_colorTools->yPbPrColorWheel(size, sliderColor->value(), m_scaling, false);
        break;
    case CPE_HSV_HUESHIFT:
        img = m_colorTools->hsvHueShiftPlane(size, sliderColor->value(), sliderScaling->value(), -180, 180);
        break;
    case CPE_HSV_SATURATION:
        col.setHsv(0, 0, sliderColor->value());
        img = m_colorTools->hsvCurvePlane(size, col, ColorTools::COM_H, ColorTools::COM_S);
        break;
    default:
        Q_ASSERT(false);
    }
    img.save(kurlrequester->text());
}

void ColorPlaneExport::slotColormodeChanged()
{
#ifdef DEBUG_CPE
    qCDebug(KDENLIVE_LOG) << "Color mode changed to " << cbColorspace->itemData(cbColorspace->currentIndex()).toInt();
#endif
    lblScaling->setText(i18n("Scaling"));
    sliderScaling->setInvertedAppearance(true);
    switch (cbColorspace->itemData(cbColorspace->currentIndex()).toInt()) {
    case CPE_YUV:
    case CPE_YUV_MOD:
    case CPE_YPbPr:
        enableSliderScaling(true);
        enableSliderColor(true);
        enableCbVariant(false);
        sliderColor->setRange(0, 255);
        sliderColor->setPageStep(128);
        lblSliderName->setText(i18n("Y value"));
        lblSliderName->setToolTip(i18n("The Y value describes the brightness of the colors."));
        break;
    case CPE_YUV_Y:
#ifdef DEBUG_CPE
        qCDebug(KDENLIVE_LOG) << "Changing slider range.";
#endif
        enableSliderScaling(true);
        enableSliderColor(true);
        enableCbVariant(false);
        sliderColor->setMaximum(321);
        sliderColor->setRange(0, 179);
        sliderColor->setPageStep(90);
        lblSliderName->setText(i18n("UV angle"));
        lblSliderName->setToolTip(i18n("Angle through the UV plane, with all possible Y values."));
        break;
    case CPE_RGB_CURVE:
        enableSliderScaling(true);
        enableSliderColor(false);
        enableCbVariant(true);
        sliderScaling->setRange(1, 255);
        sliderScaling->setValue(255);
        cbVariant->addItem(i18n("Red"), QVariant((int)ColorTools::ColorsRGB::R));
        cbVariant->addItem(i18n("Green"), QVariant((int)ColorTools::ColorsRGB::G));
        cbVariant->addItem(i18n("Blue"), QVariant((int)ColorTools::ColorsRGB::B));
        cbVariant->addItem(i18n("Luma"), QVariant((int)ColorTools::ColorsRGB::Luma));
        break;
    case CPE_HSV_HUESHIFT:
        enableSliderScaling(true);
        enableSliderColor(true);
        enableCbVariant(false);
        sliderScaling->setRange(0, 255);
        sliderScaling->setValue(200);
        sliderScaling->setInvertedAppearance(false);
        sliderColor->setRange(0, 255);
        sliderColor->setValue(200);
        lblSliderName->setText(i18n("HSV Saturation"));
        lblScaling->setText(i18n("HSV Value"));
        break;
    case CPE_HSV_SATURATION:
        enableSliderScaling(false);
        enableSliderColor(true);
        sliderColor->setRange(0, 255);
        sliderColor->setValue(200);
        lblSliderName->setText(i18n("HSV Value"));
        break;
    default:
        enableSliderScaling(false);
        enableSliderColor(false);
        enableCbVariant(true);
        cbVariant->addItem(i18n("Red"), QVariant((int)ColorTools::ColorsRGB::R));
        cbVariant->addItem(i18n("Green"), QVariant((int)ColorTools::ColorsRGB::G));
        cbVariant->addItem(i18n("Blue"), QVariant((int)ColorTools::ColorsRGB::B));
        cbVariant->addItem(i18n("Luma"), QVariant((int)ColorTools::ColorsRGB::Luma));
        break;
    }
    this->update();
    slotUpdateDisplays();
}
