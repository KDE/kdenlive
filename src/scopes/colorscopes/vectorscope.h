/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef VECTORSCOPE_H
#define VECTORSCOPE_H

#include "abstractgfxscopewidget.h"
#include "ui_vectorscope_ui.h"

class ColorTools;

class Vectorscope_UI;
class VectorscopeGenerator;

enum BACKGROUND_MODE { BG_NONE = 0, BG_YUV = 1, BG_CHROMA = 2, BG_YPbPr = 3 };

/**
   \brief Displays the vectorscope of a frame.

   \see VectorscopeGenerator for more details about the vectorscope.
   */
class Vectorscope : public AbstractGfxScopeWidget
{
    Q_OBJECT

public:
    explicit Vectorscope(QWidget *parent = nullptr);
    ~Vectorscope() override;

    QString widgetName() const override;

protected:
    ///// Implemented methods /////
    QRect scopeRect() override;
    QImage renderHUD(uint accelerationFactor) override;
    QImage renderGfxScope(uint accelerationFactor, const QImage &) override;
    QImage renderBackground(uint accelerationFactor) override;
    bool isHUDDependingOnInput() const override;
    bool isScopeDependingOnInput() const override;
    bool isBackgroundDependingOnInput() const override;
    void readConfig() override;

    ///// Other /////
    void writeConfig();

private:
    Ui::Vectorscope_UI *m_ui;

    ColorTools *m_colorTools;

    QActionGroup *m_agColorSpace;
    QAction *m_aColorSpace_YUV;
    QAction *m_aColorSpace_YPbPr;
    QAction *m_aExportBackground;
    QAction *m_aAxisEnabled;
    QAction *m_a75PBox;
    QAction *m_aIQLines;

    VectorscopeGenerator *m_vectorscopeGenerator;

    /** How to represent the pixels on the scope (green, original color, ...) */
    int m_iPaintMode;

    /** Custom scaling of the vectorscope */
    float m_gain{1};

    QPoint m_centerPoint, m_pR75, m_pG75, m_pB75, m_pCy75, m_pMg75, m_pYl75;
    QPoint m_qR75, m_qG75, m_qB75, m_qCy75, m_qMg75, m_qYl75;
    /** Unlike the scopeRect, this rect represents the overall visible rectangle
        and not only the square touching the Vectorscope's circle. */
    QRect m_visibleRect;

    /** Updates the dimension. Only necessary when the widget has been resized. */
    void updateDimensions();
    int m_cw;

private slots:
    void slotGainChanged(int);
    void slotBackgroundChanged();
    void slotExportBackground();
    void slotColorSpaceChanged();
};

#endif // VECTORSCOPE_H
