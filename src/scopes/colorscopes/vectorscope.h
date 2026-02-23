/*
    SPDX-FileCopyrightText: 2010 Simon Andreas Eugster <simon.eu@gmail.com>
    This file is part of kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractgfxscopewidget.h"
#include "ui_vectorscope_ui.h"

class ColorTools;

class Vectorscope_UI;
class VectorscopeGenerator;

enum BACKGROUND_MODE { BG_DARK = 0, BG_YUV = 1, BG_CHROMA = 2, BG_YPbPr = 3 };

/**
 *  @brief Displays the vectorscope of a frame.
 *
 *  @see VectorscopeGenerator for more details about the vectorscope.
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

    // Settings menu
    QMenu *m_settingsMenu;
    QMenu *m_paintModeMenu;
    QMenu *m_backgroundModeMenu;

    // Paint mode actions
    QActionGroup *m_agPaintMode;
    QAction *m_aPaintModeGreen2;
    QAction *m_aPaintModeGreen;
    QAction *m_aPaintModeBlack;
    QAction *m_aPaintModeChroma;
    QAction *m_aPaintModeYUV;
    QAction *m_aPaintModeOriginal;

    // Background mode actions
    QActionGroup *m_agBackgroundMode;
    QAction *m_aBackgroundDark;
    QAction *m_aBackgroundYUV;
    QAction *m_aBackgroundChroma;
    QAction *m_aBackgroundYPbPr;

    // Gain action (removed - now using UI widgets)
    // QAction *m_aGain;

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

    /** Background mode */
    BACKGROUND_MODE m_backgroundMode;

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

private Q_SLOTS:
    void slotGainChanged(int);
    void slotBackgroundChanged();
    void slotExportBackground();
    void slotColorSpaceChanged();
    void showSettingsMenu();
    void showGainDialog();
    void slotPaintModeChanged();
    void slotBackgroundModeChanged();
    void slotGainSliderChanged(int);
    void slotGainButtonClicked();
};
