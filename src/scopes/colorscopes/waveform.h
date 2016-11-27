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

#include "abstractgfxscopewidget.h"

#include "ui_waveform_ui.h"

class Waveform_UI;
class WaveformGenerator;

/**
  \brief Displays the waveform of a frame.

   For further explanations of the waveform see the WaveformGenerator class.
*/
class Waveform : public AbstractGfxScopeWidget {
    Q_OBJECT

public:
    explicit Waveform(QWidget *parent = Q_NULLPTR);
    ~Waveform();

    QString widgetName() const;

protected:
    virtual void readConfig();
    void writeConfig();

private:
    Ui::Waveform_UI *ui;
    WaveformGenerator *m_waveformGenerator;

    QAction *m_aRec601;
    QAction *m_aRec709;
    QActionGroup *m_agRec;

    static const QSize m_textWidth;
    static const int m_paddingBottom;

    QImage m_waveform;

    /// Implemented methods ///
    QRect scopeRect();
    QImage renderHUD(uint);
    QImage renderGfxScope(uint, const QImage &);
    QImage renderBackground(uint);
    bool isHUDDependingOnInput() const;
    bool isScopeDependingOnInput() const;
    bool isBackgroundDependingOnInput() const;

};

#endif // WAVEFORM_H
