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
class Histogram : public AbstractGfxScopeWidget {
    Q_OBJECT

public:
    explicit Histogram(QWidget *parent = Q_NULLPTR);
    ~Histogram();
    QString widgetName() const;

protected:
    virtual void readConfig();
    void writeConfig();

private:
    HistogramGenerator *m_histogramGenerator;
    QAction *m_aUnscaled;
    QAction *m_aRec601;
    QAction *m_aRec709;
    QActionGroup *m_agRec;

    QRect scopeRect();
    bool isHUDDependingOnInput() const;
    bool isScopeDependingOnInput() const;
    bool isBackgroundDependingOnInput() const;
    QImage renderHUD(uint accelerationFactor);
    QImage renderGfxScope(uint accelerationFactor, const QImage &);
    QImage renderBackground(uint accelerationFactor);
    Ui::Histogram_UI *ui;

};

#endif // HISTOGRAM_H
