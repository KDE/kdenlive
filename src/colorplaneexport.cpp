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
#include <QDebug>
#include <KMessageBox>

const QString EXTENSION_PNG = ".png";
const int SCALE_RANGE = 80;

ColorPlaneExport::ColorPlaneExport(QWidget *parent) :
    QDialog(parent)
{
    setupUi(this);

    m_colorTools = new ColorTools();

    tResX->setText("800");
    tResY->setText("800");

    cbColorspace->addItem(i18n("YUV UV plane"), QVariant(CPE_YUV));
    cbColorspace->addItem(i18n("YUV Y plane"), QVariant(CPE_YUV_Y));
    cbColorspace->addItem(i18n("Modified YUV (Chroma)"), QVariant(CPE_YUV_MOD));
    cbColorspace->addItem(i18n("RGB plane, one component varying"), QVariant(CPE_RGB_CURVE));

    sliderColor->setSliderPosition(128);

    // 0  -> 1
    // 50 -> 0.5
    // 80 -> 0.2
    sliderScaling->setInvertedAppearance(true);
    sliderScaling->setRange(0, 80);
    sliderScaling->setSliderPosition(50);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(slotExportPlane()));
    connect(tResX, SIGNAL(textChanged(QString)), this, SLOT(slotValidate()));
    connect(tResY, SIGNAL(textChanged(QString)), this, SLOT(slotValidate()));
    connect(kurlrequester, SIGNAL(textChanged(QString)), this, SLOT(slotValidate()));
    connect(sliderColor, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateDisplays()));
    connect(sliderScaling, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateDisplays()));
    connect(cbColorspace, SIGNAL(currentIndexChanged(int)), this, SLOT(slotColormodeChanged()));

    kurlrequester->setUrl(KUrl("/tmp/yuv-plane.png"));

    slotColormodeChanged();
    slotValidate();
}

ColorPlaneExport::~ColorPlaneExport()
{
    delete m_colorTools;
}



///// Helper functions /////

void ColorPlaneExport::enableSliderScaling(const bool &enable)
{
    sliderScaling->setEnabled(enable);
    lblScaling->setEnabled(enable);
    lblScaleNr->setEnabled(enable);
}

void ColorPlaneExport::enableSliderColor(const bool &enable)
{
    sliderColor->setEnabled(enable);
    lblSliderName->setEnabled(enable);
    lblColNr->setEnabled(enable);
}

void ColorPlaneExport::enableCbVariant(const bool &enable)
{
   cbVariant->setEnabled(enable);
   lblVariant->setEnabled(enable);
   if (!enable) {
       while (cbVariant->count() > 0) {
           cbVariant->removeItem(0);
       }
   }
}



///// Slots /////

void ColorPlaneExport::slotUpdateDisplays()
{
    m_scaling = 1 - (float)sliderScaling->value()/100;

    lblScaleNr->setText("0..." + QString::number(m_scaling, 'f', 2));

    switch (cbColorspace->itemData(cbColorspace->currentIndex()).toInt()) {
    case CPE_YUV_Y:
        lblColNr->setText(i18n("%1°", QString::number(sliderColor->value())));
        break;
    default:
        lblColNr->setText(QString::number(sliderColor->value()));
        break;
    }

    lblSize->setText(i18n("%1 px", QVariant(tResX->text()).toInt()*QVariant(tResY->text()).toInt()));
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
        ok = kurlrequester->text().trimmed().length() > 0;
        qDebug() << "File given: " << ok;
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
    qDebug() << "Exporting plane now to " <<  kurlrequester->text();
    QString lower = kurlrequester->text().toLower();
    qDebug() << "Lower: " << lower;
    if (!lower.endsWith(".png") && !lower.endsWith(".jpg") && !lower.endsWith(".tif") && !lower.endsWith(".tiff")) {
        if (KMessageBox::questionYesNo(this, i18n("File has no extension. Add extension (%1)?", EXTENSION_PNG)) == KMessageBox::Yes) {
            kurlrequester->setUrl(KUrl(kurlrequester->text() + ".png"));
        }
    }
    QImage img;
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
        img = m_colorTools->rgbCurvePlane(size, (ColorTools::ColorsRGB) (cbVariant->itemData(cbVariant->currentIndex()).toInt()));
        break;
    }
    img.save(kurlrequester->text());
}

void ColorPlaneExport::slotColormodeChanged()
{
    qDebug() << "Color mode changed to " << cbColorspace->itemData(cbColorspace->currentIndex()).toInt();
    switch (cbColorspace->itemData(cbColorspace->currentIndex()).toInt()) {
    case CPE_YUV:
    case CPE_YUV_MOD:
        enableSliderScaling(true);
        enableSliderColor(true);
        enableCbVariant(false);
        sliderColor->setRange(0,255);
        sliderColor->setPageStep(128);
        lblSliderName->setText(i18n("Y value"));
        lblSliderName->setToolTip(i18n("The Y value describes the brightness of the colors."));
        break;
    case CPE_YUV_Y:
        qDebug() << "Changing slider range.";
        enableSliderScaling(true);
        enableSliderColor(true);
        enableCbVariant(false);
        sliderColor->setMaximum(321);
        sliderColor->setRange(0,179);
        sliderColor->setPageStep(90);
        lblSliderName->setText(i18n("UV angle"));
        lblSliderName->setToolTip(i18n("Angle through the UV plane, with all possible Y values."));
        break;
    case CPE_RGB_CURVE:
        // deliberately fall through
    default:
        enableSliderScaling(false);
        enableSliderColor(false);
        enableCbVariant(true);
        cbVariant->addItem(i18n("Red"), QVariant(ColorTools::COL_R));
        cbVariant->addItem(i18n("Green"), QVariant(ColorTools::COL_G));
        cbVariant->addItem(i18n("Blue"), QVariant(ColorTools::COL_B));
        break;
    }
    this->update();
    slotUpdateDisplays();
}
