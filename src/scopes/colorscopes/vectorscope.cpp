/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/


#include <QAction>
#include <QColor>
#include <QDebug>
#include <QMouseEvent>
#include <QMenu>
#include <QPainter>

#include <QTime>

#include "colorplaneexport.h"
#include "colortools.h"
#include "vectorscope.h"
#include "colorcorrection/vectorscopegenerator.h"

const float P75 = .75;
const unsigned char DEFAULT_Y = 255;

const QPointF YUV_R(-.147,  .615);
const QPointF YUV_G(-.289, -.515);
const QPointF YUV_B(.437, -.100);
const QPointF YUV_Cy(.147, -.615);
const QPointF YUV_Mg(.289,  .515);
const QPointF YUV_Yl(-.437,  .100);

const QPointF YPbPr_R(-.169, .5);
const QPointF YPbPr_G(-.331, -.419);
const QPointF YPbPr_B(.5, -.081);
const QPointF YPbPr_Cy(.169, -.5);
const QPointF YPbPr_Mg(.331, .419);
const QPointF YPbPr_Yl(-.5, .081);


Vectorscope::Vectorscope(QWidget *parent) :
    AbstractGfxScopeWidget(true, parent),
    m_gain(1)
{
    ui = new Ui::Vectorscope_UI();
    ui->setupUi(this);

    m_colorTools = new ColorTools();
    m_colorPlaneExport = new ColorPlaneExport(this);
    m_vectorscopeGenerator = new VectorscopeGenerator();

    ui->paintMode->addItem(i18n("Green 2"), QVariant(VectorscopeGenerator::PaintMode_Green2));
    ui->paintMode->addItem(i18n("Green"), QVariant(VectorscopeGenerator::PaintMode_Green));
    ui->paintMode->addItem(i18n("Black"), QVariant(VectorscopeGenerator::PaintMode_Black));
    ui->paintMode->addItem(i18n("Modified YUV (Chroma)"), QVariant(VectorscopeGenerator::PaintMode_Chroma));
    ui->paintMode->addItem(i18n("YUV"), QVariant(VectorscopeGenerator::PaintMode_YUV));
    ui->paintMode->addItem(i18n("Original Color"), QVariant(VectorscopeGenerator::PaintMode_Original));

    ui->backgroundMode->addItem(i18n("None"), QVariant(BG_NONE));
    ui->backgroundMode->addItem(i18n("YUV"), QVariant(BG_YUV));
    ui->backgroundMode->addItem(i18n("Modified YUV (Chroma)"), QVariant(BG_CHROMA));
    ui->backgroundMode->addItem(i18n("YPbPr"), QVariant(BG_YPbPr));

    ui->sliderGain->setMinimum(0);
    ui->sliderGain->setMaximum(40);

    bool b = true;
    b &= connect(ui->backgroundMode, SIGNAL(currentIndexChanged(int)), this, SLOT(slotBackgroundChanged()));
    b &= connect(ui->sliderGain, SIGNAL(valueChanged(int)), this, SLOT(slotGainChanged(int)));
    b &= connect(ui->paintMode, SIGNAL(currentIndexChanged(int)), this, SLOT(forceUpdateScope()));
    b &= connect(this, SIGNAL(signalMousePositionChanged()), this, SLOT(forceUpdateHUD()));
    ui->sliderGain->setValue(0);


    ///// Build context menu /////

    m_menu->addSeparator()->setText(i18n("Tools"));;

    m_aExportBackground = new QAction(i18n("Export background"), this);
    m_menu->addAction(m_aExportBackground);
    b &= connect(m_aExportBackground, SIGNAL(triggered()), this, SLOT(slotExportBackground()));

    m_menu->addSeparator()->setText(i18n("Drawing options"));

    m_a75PBox = new QAction(i18n("75% box"), this);
    m_a75PBox->setCheckable(true);
    m_menu->addAction(m_a75PBox);
    b &= connect(m_a75PBox, SIGNAL(changed()), this, SLOT(forceUpdateBackground()));

    m_aAxisEnabled = new QAction(i18n("Draw axis"), this);
    m_aAxisEnabled->setCheckable(true);
    m_menu->addAction(m_aAxisEnabled);
    b &= connect(m_aAxisEnabled, SIGNAL(changed()), this, SLOT(forceUpdateBackground()));

    m_aIQLines = new QAction(i18n("Draw I/Q lines"), this);
    m_aIQLines->setCheckable(true);
    m_menu->addAction(m_aIQLines);
    b &= connect(m_aIQLines, SIGNAL(changed()), this, SLOT(forceUpdateBackground()));

    m_menu->addSeparator()->setText(i18n("Color Space"));
    m_aColorSpace_YPbPr = new QAction(i18n("YPbPr"), this);
    m_aColorSpace_YPbPr->setCheckable(true);
    m_aColorSpace_YUV = new QAction(i18n("YUV"), this);
    m_aColorSpace_YUV->setCheckable(true);
    m_agColorSpace = new QActionGroup(this);
    m_agColorSpace->addAction(m_aColorSpace_YPbPr);
    m_agColorSpace->addAction(m_aColorSpace_YUV);
    m_menu->addAction(m_aColorSpace_YPbPr);
    m_menu->addAction(m_aColorSpace_YUV);
    b &= connect(m_aColorSpace_YPbPr, SIGNAL(toggled(bool)), this, SLOT(slotColorSpaceChanged()));
    b &= connect(m_aColorSpace_YUV, SIGNAL(toggled(bool)), this, SLOT(slotColorSpaceChanged()));

    Q_ASSERT(b);

    // To make the 1.0x text show
    slotGainChanged(ui->sliderGain->value());

    init();
}

Vectorscope::~Vectorscope()
{
    writeConfig();

    delete m_colorTools;
    delete m_colorPlaneExport;
    delete m_vectorscopeGenerator;

    delete m_aColorSpace_YPbPr;
    delete m_aColorSpace_YUV;
    delete m_aExportBackground;
    delete m_aAxisEnabled;
    delete m_a75PBox;
    delete m_agColorSpace;
    delete ui;
}

QString Vectorscope::widgetName() const { return QString("Vectorscope"); }

void Vectorscope::readConfig()
{
    AbstractGfxScopeWidget::readConfig();

    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    m_a75PBox->setChecked(scopeConfig.readEntry("75PBox", false));
    m_aAxisEnabled->setChecked(scopeConfig.readEntry("axis", false));
    m_aIQLines->setChecked(scopeConfig.readEntry("iqlines", false));
    ui->backgroundMode->setCurrentIndex(scopeConfig.readEntry("backgroundmode").toInt());
    ui->paintMode->setCurrentIndex(scopeConfig.readEntry("paintmode").toInt());
    ui->sliderGain->setValue(scopeConfig.readEntry("gain", 1));
    m_aColorSpace_YPbPr->setChecked(scopeConfig.readEntry("colorspace_ypbpr", false));
    m_aColorSpace_YUV->setChecked(!m_aColorSpace_YPbPr->isChecked());
}

void Vectorscope::writeConfig()
{
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    scopeConfig.writeEntry("75PBox", m_a75PBox->isChecked());
    scopeConfig.writeEntry("axis", m_aAxisEnabled->isChecked());
    scopeConfig.writeEntry("iqlines", m_aIQLines->isChecked());
    scopeConfig.writeEntry("backgroundmode", ui->backgroundMode->currentIndex());
    scopeConfig.writeEntry("paintmode", ui->paintMode->currentIndex());
    scopeConfig.writeEntry("gain", ui->sliderGain->value());
    scopeConfig.writeEntry("colorspace_ypbpr", m_aColorSpace_YPbPr->isChecked());
    scopeConfig.sync();
}

QRect Vectorscope::scopeRect()
{
    // Distance from top/left/right
    int offset = 6;

    // We want to paint below the controls area. The line is the lowest element.
    QPoint topleft(offset, ui->verticalSpacer->geometry().y()+offset);
    QPoint bottomright(ui->horizontalSpacer->geometry().right()-offset, this->size().height()-offset);

    m_visibleRect = QRect(topleft, bottomright);

    QRect scopeRect(topleft, bottomright);

    // Circle Width: min of width and height
    cw = (scopeRect.height() < scopeRect.width()) ? scopeRect.height() : scopeRect.width();
    scopeRect.setWidth(cw);
    scopeRect.setHeight(cw);


    m_centerPoint = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), QPointF(0,0));
    pR75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75*VectorscopeGenerator::scaling*YUV_R);
    pG75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75*VectorscopeGenerator::scaling*YUV_G);
    pB75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75*VectorscopeGenerator::scaling*YUV_B);
    pCy75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75*VectorscopeGenerator::scaling*YUV_Cy);
    pMg75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75*VectorscopeGenerator::scaling*YUV_Mg);
    pYl75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75*VectorscopeGenerator::scaling*YUV_Yl);
    qR75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75*VectorscopeGenerator::scaling*YPbPr_R);
    qG75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75*VectorscopeGenerator::scaling*YPbPr_G);
    qB75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75*VectorscopeGenerator::scaling*YPbPr_B);
    qCy75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75*VectorscopeGenerator::scaling*YPbPr_Cy);
    qMg75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75*VectorscopeGenerator::scaling*YPbPr_Mg);
    qYl75 = m_vectorscopeGenerator->mapToCircle(scopeRect.size(), P75*VectorscopeGenerator::scaling*YPbPr_Yl);

    return scopeRect;
}

bool Vectorscope::isHUDDependingOnInput() const { return false; }
bool Vectorscope::isScopeDependingOnInput() const { return true; }
bool Vectorscope::isBackgroundDependingOnInput() const { return false; }

QImage Vectorscope::renderHUD(uint)
{

    QImage hud;
    QLocale locale;
    if (m_mouseWithinWidget) {
        // Mouse moved: Draw a circle over the scope

        hud = QImage(m_visibleRect.size(), QImage::Format_ARGB32);
        hud.fill(qRgba(0, 0, 0, 0));

        QPainter davinci(&hud);
        QPoint widgetCenterPoint = m_scopeRect.topLeft() + m_centerPoint;

        int dx = -widgetCenterPoint.x()+m_mousePos.x();
        int dy =  widgetCenterPoint.y()-m_mousePos.y();

        QPoint reference = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(1,0));

        float r = sqrt(dx*dx + dy*dy);
        float percent = (float) 100*r/VectorscopeGenerator::scaling/m_gain/(reference.x() - widgetCenterPoint.x());

        switch (ui->backgroundMode->itemData(ui->backgroundMode->currentIndex()).toInt()) {
        case BG_NONE:
            davinci.setPen(penLight);
            break;
        default:
            if (r > cw/2) {
                davinci.setPen(penLight);
            } else {
                davinci.setPen(penDark);
            }
            break;
        }
        davinci.drawEllipse(m_centerPoint, (int)r, (int)r);
        davinci.setPen(penThin);
        davinci.drawText(QPoint(m_scopeRect.width()-40, m_scopeRect.height()), i18n("%1 \%", locale.toString(percent, 'f', 0)));

        float angle = copysign(acos(dx/r)*180/M_PI, dy);
        davinci.drawText(QPoint(10, m_scopeRect.height()), i18n("%1°", locale.toString(angle, 'f', 1)));

//        m_circleEnabled = false;
    } else {
        hud = QImage(0, 0, QImage::Format_ARGB32);
    }
    emit signalHUDRenderingFinished(0, 1);
    return hud;
}

QImage Vectorscope::renderGfxScope(uint accelerationFactor, const QImage qimage)
{
    QTime start = QTime::currentTime();
    QImage scope;

    if (cw <= 0) {
        qDebug() << "Scope size not known yet. Aborting.";
    } else {

        VectorscopeGenerator::ColorSpace colorSpace = m_aColorSpace_YPbPr->isChecked() ?
                                                      VectorscopeGenerator::ColorSpace_YPbPr : VectorscopeGenerator::ColorSpace_YUV;
        VectorscopeGenerator::PaintMode paintMode = (VectorscopeGenerator::PaintMode) ui->paintMode->itemData(ui->paintMode->currentIndex()).toInt();
        scope = m_vectorscopeGenerator->calculateVectorscope(m_scopeRect.size(),
                                                             qimage,
                                                             m_gain, paintMode, colorSpace,
                                                             m_aAxisEnabled->isChecked(), accelerationFactor);

    }

    unsigned int mseconds = start.msecsTo(QTime::currentTime());
    emit signalScopeRenderingFinished(mseconds, accelerationFactor);
    return scope;
}

QImage Vectorscope::renderBackground(uint)
{
    QTime start = QTime::currentTime();
    start.start();

    QImage bg(m_visibleRect.size(), QImage::Format_ARGB32);
    bg.fill(qRgba(0,0,0,0));

    // Set up tools
    QPainter davinci(&bg);
    davinci.setRenderHint(QPainter::Antialiasing, true);

    QPoint vinciPoint;
    QPoint vinciPoint2;

    // Draw the color plane (if selected)
    QImage colorPlane;
    switch (ui->backgroundMode->itemData(ui->backgroundMode->currentIndex()).toInt()) {
    case BG_YUV:
        colorPlane = m_colorTools->yuvColorWheel(m_scopeRect.size(), (unsigned char) 128, 1/VectorscopeGenerator::scaling, false, true);
        davinci.drawImage(0, 0, colorPlane);
        break;
    case BG_CHROMA:
        colorPlane = m_colorTools->yuvColorWheel(m_scopeRect.size(), (unsigned char) 255, 1/VectorscopeGenerator::scaling, true, true);
        davinci.drawImage(0, 0, colorPlane);
        break;
    case BG_YPbPr:
        colorPlane = m_colorTools->yPbPrColorWheel(m_scopeRect.size(), (unsigned char) 128, 1/VectorscopeGenerator::scaling, true);
        davinci.drawImage(0, 0, colorPlane);
        break;
    }



    // Draw I/Q lines (from the YIQ color space; Skin tones lie on the I line)
    // Positions are calculated by transforming YIQ:[0 1 0] or YIQ:[0 0 1] to YUV/YPbPr.
    if (m_aIQLines->isChecked()) {

        switch (ui->backgroundMode->itemData(ui->backgroundMode->currentIndex()).toInt()) {
        case BG_NONE:
            davinci.setPen(penLightDots);
            break;
        default:
            davinci.setPen(penDarkDots);
            break;
        }

        if (m_aColorSpace_YUV->isChecked()) {
            vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(-.544,.838));
            vinciPoint2 = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(.544,-.838));
        } else {
            vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(-.675,.737));
            vinciPoint2 = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(.675,-.737));
        }

        davinci.drawLine(vinciPoint, vinciPoint2);
        davinci.setPen(penThick);
        davinci.drawText(vinciPoint - QPoint(11, 5), "I");


        switch (ui->backgroundMode->itemData(ui->backgroundMode->currentIndex()).toInt()) {
        case BG_NONE:
            davinci.setPen(penLightDots);
            break;
        default:
            davinci.setPen(penDarkDots);
            break;
        }

        if (m_aColorSpace_YUV->isChecked()) {
            vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(.838, .544));
            vinciPoint2 = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(-.838,-.544));
        } else {
            vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(.908, .443));
            vinciPoint2 = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(-.908,-.443));
        }

        davinci.drawLine(vinciPoint, vinciPoint2);
        davinci.setPen(penThick);
        davinci.drawText(vinciPoint - QPoint(-7, 2), "Q");



    }

    // Draw the vectorscope circle
    davinci.setPen(penThick);
    davinci.drawEllipse(0, 0, cw, cw);

    // Draw RGB/CMY points with 100% chroma
    if (m_aColorSpace_YUV->isChecked()) {
        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling*YUV_R);
        davinci.drawEllipse(vinciPoint, 4,4);
        davinci.drawText(vinciPoint-QPoint(20, -10), "R");

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling*YUV_G);
        davinci.drawEllipse(vinciPoint, 4,4);
        davinci.drawText(vinciPoint-QPoint(20, 0), "G");

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling*YUV_B);
        davinci.drawEllipse(vinciPoint, 4,4);
        davinci.drawText(vinciPoint+QPoint(15, 10), "B");

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling*YUV_Cy);
        davinci.drawEllipse(vinciPoint, 4,4);
        davinci.drawText(vinciPoint+QPoint(15, -5), "Cy");

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling*YUV_Mg);
        davinci.drawEllipse(vinciPoint, 4,4);
        davinci.drawText(vinciPoint+QPoint(15, 10), "Mg");

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling*YUV_Yl);
        davinci.drawEllipse(vinciPoint, 4,4);
        davinci.drawText(vinciPoint-QPoint(25, 0), "Yl");
    } else {
        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling*YPbPr_R);
        davinci.drawEllipse(vinciPoint, 4,4);
        davinci.drawText(vinciPoint-QPoint(20, -10), "R");

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling*YPbPr_G);
        davinci.drawEllipse(vinciPoint, 4,4);
        davinci.drawText(vinciPoint-QPoint(20, 0), "G");

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling*YPbPr_B);
        davinci.drawEllipse(vinciPoint, 4,4);
        davinci.drawText(vinciPoint+QPoint(15, 10), "B");

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling*YPbPr_Cy);
        davinci.drawEllipse(vinciPoint, 4,4);
        davinci.drawText(vinciPoint+QPoint(15, -5), "Cy");

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling*YPbPr_Mg);
        davinci.drawEllipse(vinciPoint, 4,4);
        davinci.drawText(vinciPoint+QPoint(15, 10), "Mg");

        vinciPoint = m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), VectorscopeGenerator::scaling*YPbPr_Yl);
        davinci.drawEllipse(vinciPoint, 4,4);
        davinci.drawText(vinciPoint-QPoint(25, 0), "Yl");
    }

    switch (ui->backgroundMode->itemData(ui->backgroundMode->currentIndex()).toInt()) {
    case BG_NONE:
        davinci.setPen(penLight);
        break;
    default:
        davinci.setPen(penDark);
        break;
    }

    // Draw axis
    if (m_aAxisEnabled->isChecked()) {
        davinci.drawLine(m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(0,-.9)), m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(0,.9)));
        davinci.drawLine(m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(-.9,0)), m_vectorscopeGenerator->mapToCircle(m_scopeRect.size(), QPointF(.9,0)));
    }

    // Draw center point
    switch (ui->backgroundMode->itemData(ui->backgroundMode->currentIndex()).toInt()) {
    case BG_CHROMA:
        davinci.setPen(penDark);
        break;
    default:
        davinci.setPen(penThin);
        break;
    }
    davinci.drawEllipse(m_centerPoint, 5,5);


    // Draw 75% box
    if (m_a75PBox->isChecked()) {
        if (m_aColorSpace_YUV->isChecked()) {
            davinci.drawLine(pR75, pYl75);
            davinci.drawLine(pYl75, pG75);
            davinci.drawLine(pG75, pCy75);
            davinci.drawLine(pCy75, pB75);
            davinci.drawLine(pB75, pMg75);
            davinci.drawLine(pMg75, pR75);
        } else {
            davinci.drawLine(qR75, qYl75);
            davinci.drawLine(qYl75, qG75);
            davinci.drawLine(qG75, qCy75);
            davinci.drawLine(qCy75, qB75);
            davinci.drawLine(qB75, qMg75);
            davinci.drawLine(qMg75, qR75);
        }
    }

    // Draw RGB/CMY points with 75% chroma (for NTSC)
    davinci.setPen(penThin);
    if (m_aColorSpace_YUV->isChecked()) {
        davinci.drawEllipse(pR75, 3,3);
        davinci.drawEllipse(pG75, 3,3);
        davinci.drawEllipse(pB75, 3,3);
        davinci.drawEllipse(pCy75, 3,3);
        davinci.drawEllipse(pMg75, 3,3);
        davinci.drawEllipse(pYl75, 3,3);
    } else {
        davinci.drawEllipse(qR75, 3,3);
        davinci.drawEllipse(qG75, 3,3);
        davinci.drawEllipse(qB75, 3,3);
        davinci.drawEllipse(qCy75, 3,3);
        davinci.drawEllipse(qMg75, 3,3);
        davinci.drawEllipse(qYl75, 3,3);
    }

    // Draw realtime factor (number of skipped pixels)
    if (m_aRealtime->isChecked()) {
        davinci.setPen(penThin);
        davinci.drawText(QPoint(m_scopeRect.width()-40, m_scopeRect.height()-15), QVariant(m_accelFactorScope).toString().append("x"));
    }

    emit signalBackgroundRenderingFinished(start.elapsed(), 1);
    return bg;
}




///// Slots /////

void Vectorscope::slotGainChanged(int newval)
{
    QLocale locale;
    m_gain = 1 + (float)newval/10;
    ui->lblGain->setText(locale.toString(m_gain, 'f', 1) + 'x');
    forceUpdateScope();
}

void Vectorscope::slotExportBackground()
{
    qDebug() << "Exporting background";
    m_colorPlaneExport->show();

}

void Vectorscope::slotBackgroundChanged()
{
    // Background changed, switch to a suitable color mode now
    int index;
    switch (ui->backgroundMode->itemData(ui->backgroundMode->currentIndex()).toInt()) {
    case BG_YUV:
        index = ui->paintMode->findData(QVariant(VectorscopeGenerator::PaintMode_Black));
        if (index >= 0) {
            ui->paintMode->setCurrentIndex(index);
        }
        break;

    case BG_NONE:
        if (ui->paintMode->itemData(ui->paintMode->currentIndex()).toInt() == VectorscopeGenerator::PaintMode_Black) {
            index = ui->paintMode->findData(QVariant(VectorscopeGenerator::PaintMode_Green2));
            ui->paintMode->setCurrentIndex(index);
        }
        break;
    }
    forceUpdateBackground();
}

void Vectorscope::slotColorSpaceChanged()
{
    int index;
    if (m_aColorSpace_YPbPr->isChecked()) {
        if (ui->backgroundMode->itemData(ui->backgroundMode->currentIndex()).toInt() == BG_YUV) {
            index = ui->backgroundMode->findData(QVariant(BG_YPbPr));
            if (index >= 0) {
                ui->backgroundMode->setCurrentIndex(index);
            }
        }
    } else {
        if (ui->backgroundMode->itemData(ui->backgroundMode->currentIndex()).toInt() == BG_YPbPr) {
            index = ui->backgroundMode->findData(QVariant(BG_YUV));
            if (index >= 0) {
                ui->backgroundMode->setCurrentIndex(index);
            }
        }
    }
    forceUpdate();
}
