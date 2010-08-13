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
#include "renderer.h"
#include "vectorscope.h"
#include "vectorscopegenerator.h"

const float P75 = .75;
const unsigned char DEFAULT_Y = 255;

const QPointF YUV_R(-.147,  .615);
const QPointF YUV_G(-.289, -.515);
const QPointF YUV_B(.437, -.100);
const QPointF YUV_Cy(.147, -.615);
const QPointF YUV_Mg(.289,  .515);
const QPointF YUV_Yl(-.437,  .100);

const QPen penThick(QBrush(QColor(250,250,250)), 2, Qt::SolidLine);
const QPen penThin(QBrush(QColor(250,250,250)), 1, Qt::SolidLine);
const QPen penLight(QBrush(QColor(200,200,250,150)), 1, Qt::SolidLine);
const QPen penDark(QBrush(QColor(0,0,20,250)), 1, Qt::SolidLine);


Vectorscope::Vectorscope(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent) :
    AbstractScopeWidget(projMonitor, clipMonitor, parent),
    m_gain(1),
    m_circleEnabled(false)
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

    ui->sliderGain->setMinimum(0);
    ui->sliderGain->setMaximum(40);

    bool b = true;
    b &= connect(ui->backgroundMode, SIGNAL(currentIndexChanged(int)), this, SLOT(slotBackgroundChanged()));
    b &= connect(ui->sliderGain, SIGNAL(valueChanged(int)), this, SLOT(slotGainChanged(int)));
    b &= connect(ui->paintMode, SIGNAL(currentIndexChanged(int)), this, SLOT(forceUpdateScope()));
    ui->sliderGain->setValue(0);


    ///// Build context menu /////

    m_menu->addSeparator();

    m_aExportBackground = new QAction(i18n("Export background"), this);
    m_menu->addAction(m_aExportBackground);
    b &= connect(m_aExportBackground, SIGNAL(triggered()), this, SLOT(slotExportBackground()));

    m_a75PBox = new QAction(i18n("75% box"), this);
    m_a75PBox->setCheckable(true);
    m_a75PBox->setChecked(false);
    m_menu->addAction(m_a75PBox);
    b &= connect(m_a75PBox, SIGNAL(changed()), this, SLOT(forceUpdateBackground()));

    m_aAxisEnabled = new QAction(i18n("Draw axis"), this);
    m_aAxisEnabled->setCheckable(true);
    m_aAxisEnabled->setChecked(false);
    m_menu->addAction(m_aAxisEnabled);
    b &= connect(m_aAxisEnabled, SIGNAL(changed()), this, SLOT(forceUpdateBackground()));

    Q_ASSERT(b);


    this->setMouseTracking(true);
    slotGainChanged(ui->sliderGain->value());

    init();
}

Vectorscope::~Vectorscope()
{
    writeConfig();

    delete m_colorTools;
    delete m_colorPlaneExport;
    delete m_vectorscopeGenerator;

    delete m_aExportBackground;
    delete m_aAxisEnabled;
    delete m_a75PBox;
}

QString Vectorscope::widgetName() const { return QString("Vectorscope"); }

void Vectorscope::readConfig()
{
    AbstractScopeWidget::readConfig();

    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    m_a75PBox->setChecked(scopeConfig.readEntry("75PBox", false));
    m_aAxisEnabled->setChecked(scopeConfig.readEntry("axis", false));
}

void Vectorscope::writeConfig()
{
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup scopeConfig(config, configName());
    scopeConfig.writeEntry("75PBox", m_a75PBox->isChecked());
    scopeConfig.writeEntry("axis", m_aAxisEnabled->isChecked());
    scopeConfig.sync();
}

QRect Vectorscope::scopeRect()
{
    // Distance from top/left/right
    int offset = 6;

    // We want to paint below the controls area. The line is the lowest element.
    QPoint topleft(offset, ui->verticalSpacer->geometry().y()+offset);
    QPoint bottomright(ui->horizontalSpacer->geometry().right()-offset, this->size().height()-offset);

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

    return scopeRect;
}

bool Vectorscope::isHUDDependingOnInput() const { return false; }
bool Vectorscope::isScopeDependingOnInput() const { return true; }
bool Vectorscope::isBackgroundDependingOnInput() const { return false; }

QImage Vectorscope::renderHUD(uint)
{

    QImage hud;

    if (m_circleEnabled) {
        // Mouse moved: Draw a circle over the scope

        hud = QImage(m_scopeRect.size(), QImage::Format_ARGB32);
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
            davinci.setPen(penDark);
            break;
        }
        davinci.drawEllipse(m_centerPoint, (int)r, (int)r);
        davinci.setPen(penThin);
        davinci.drawText(QPoint(m_scopeRect.width()-40, m_scopeRect.height()), i18n("%1 \%", QString::number(percent, 'f', 0)));

        float angle = copysign(acos(dx/r)*180/M_PI, dy);
        davinci.drawText(QPoint(10, m_scopeRect.height()), i18n("%1°", QString::number(angle, 'f', 1)));

        m_circleEnabled = false;
    } else {
        hud = QImage(0, 0, QImage::Format_ARGB32);
    }
    emit signalHUDRenderingFinished(0, 1);
    return hud;
}

QImage Vectorscope::renderScope(uint accelerationFactor, QImage qimage)
{
    QTime start = QTime::currentTime();
    QImage scope;

    if (cw <= 0) {
        qDebug() << "Scope size not known yet. Aborting.";
    } else {

        scope = m_vectorscopeGenerator->calculateVectorscope(m_scopeRect.size(),
                                                             qimage,
                                                             m_gain, (VectorscopeGenerator::PaintMode) ui->paintMode->itemData(ui->paintMode->currentIndex()).toInt(),
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

    QImage bg;
    switch (ui->backgroundMode->itemData(ui->backgroundMode->currentIndex()).toInt()) {
    case BG_YUV:
        qDebug() << "YUV background.";
        bg = m_colorTools->yuvColorWheel(m_scopeRect.size(), (unsigned char) 128, 1/VectorscopeGenerator::scaling, false, true);
        break;
    case BG_CHROMA:
        bg = m_colorTools->yuvColorWheel(m_scopeRect.size(), (unsigned char) 255, 1/VectorscopeGenerator::scaling, true, true);
        break;
    default:
        qDebug() << "No background.";
        bg = QImage(cw, cw, QImage::Format_ARGB32);
        bg.fill(qRgba(0,0,0,0));
        break;
    }


    // Draw the vectorscope circle
    QPainter davinci(&bg);
    QPoint vinciPoint;


    davinci.setRenderHint(QPainter::Antialiasing, true);

    davinci.setPen(penThick);
    davinci.drawEllipse(0, 0, cw, cw);

    // Draw RGB/CMY points with 100% chroma
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
        davinci.drawLine(pR75, pYl75);
        davinci.drawLine(pYl75, pG75);
        davinci.drawLine(pG75, pCy75);
        davinci.drawLine(pCy75, pB75);
        davinci.drawLine(pB75, pMg75);
        davinci.drawLine(pMg75, pR75);
    }

    // Draw RGB/CMY points with 75% chroma (for NTSC)
    davinci.setPen(penThin);
    davinci.drawEllipse(pR75, 3,3);
    davinci.drawEllipse(pG75, 3,3);
    davinci.drawEllipse(pB75, 3,3);
    davinci.drawEllipse(pCy75, 3,3);
    davinci.drawEllipse(pMg75, 3,3);
    davinci.drawEllipse(pYl75, 3,3);

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
    m_gain = 1 + (float)newval/10;
    ui->lblGain->setText(QString::number(m_gain, 'f', 1) + "x");
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



///// Events /////

void Vectorscope::mouseMoveEvent(QMouseEvent *event)
{
    // Draw a circle around the center,
    // showing percentage number of the radius length

    m_circleEnabled = true;
    m_mousePos = event->pos();
    forceUpdateHUD();
}

void Vectorscope::leaveEvent(QEvent *event)
{
    // Repaint the HUD without the circle

    m_circleEnabled = false;
    QWidget::leaveEvent(event);
    forceUpdateHUD();
}
