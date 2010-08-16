/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef WAVEFORM_H
#define WAVEFORM_H

#include "abstractscopewidget.h"

#include "ui_waveform_ui.h"

class Waveform_UI;
class WaveformGenerator;

class Waveform : public AbstractScopeWidget {
    Q_OBJECT

public:
    Waveform(Monitor *projMonitor, Monitor *clipMonitor, QWidget *parent = 0);
    ~Waveform();

    virtual QString widgetName() const;

protected:
    virtual void readConfig();
    void writeConfig();

private:
    Ui::Waveform_UI *ui;

    WaveformGenerator *m_waveformGenerator;

    bool initialDimensionUpdateDone;

    QImage m_waveform;

    /// Implemented methods ///
    QRect scopeRect();
    QImage renderHUD(uint);
    QImage renderScope(uint, QImage);
    QImage renderBackground(uint);
    bool isHUDDependingOnInput() const;
    bool isScopeDependingOnInput() const;
    bool isBackgroundDependingOnInput() const;

};

#endif // WAVEFORM_H
