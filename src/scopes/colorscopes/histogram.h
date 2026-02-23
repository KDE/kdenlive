/*
    SPDX-FileCopyrightText: 2010 Simon Andreas Eugster <simon.eu@gmail.com>
    This file is part of kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractgfxscopewidget.h"
#include "ui_histogram_ui.h"

class HistogramGenerator;

/**
 *  @brief Displays the histogram of frames.
 */
class Histogram : public AbstractGfxScopeWidget
{
    Q_OBJECT

public:
    explicit Histogram(QWidget *parent = nullptr);
    ~Histogram() override;
    QString widgetName() const override;

protected:
    void readConfig() override;
    void writeConfig();

private:
    HistogramGenerator *m_histogramGenerator;

    // Settings menu
    QMenu *m_settingsMenu;
    QMenu *m_componentsMenu;
    QMenu *m_scaleMenu;

    // Component actions
    QAction *m_aComponentY;
    QAction *m_aComponentS;
    QAction *m_aComponentR;
    QAction *m_aComponentG;
    QAction *m_aComponentB;

    // Scale actions
    QActionGroup *m_agScale;
    QAction *m_aScaleLinear;
    QAction *m_aScaleLogarithmic;

    /** Components flags */
    int m_components;

    /** Scale type */
    bool m_logScale;

    QAction *m_aUnscaled;
    QAction *m_aRec601;
    QAction *m_aRec709;
    QActionGroup *m_agRec;

    QRect scopeRect() override;
    bool isHUDDependingOnInput() const override;
    bool isScopeDependingOnInput() const override;
    bool isBackgroundDependingOnInput() const override;
    QImage renderHUD(uint accelerationFactor) override;
    QImage renderGfxScope(uint accelerationFactor, const QImage &) override;
    QImage renderBackground(uint accelerationFactor) override;
    Ui::Histogram_UI *m_ui;

private Q_SLOTS:
    void showSettingsMenu();
    void slotComponentsChanged();
    void slotScaleChanged();
};
