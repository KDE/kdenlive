/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef HISTOGRAM_H
#define HISTOGRAM_H

#include "abstractgfxscopewidget.h"
#include "ui_histogram_ui.h"

class HistogramGenerator;

/**
 *  \brief Displays the histogram of frames.
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
};

#endif // HISTOGRAM_H
