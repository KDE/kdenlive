/***************************************************************************
 *   Copyright (C) 2010 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef RGBPARADE_H
#define RGBPARADE_H

#include "abstractgfxscopewidget.h"
#include "ui_rgbparade_ui.h"

class QImage;
class RGBParade_UI;
class RGBParadeGenerator;

/**
 * \brief Displays the RGB waveform of a frame.
 * This is the same as the Waveform, but for each colour channel separately.
 */
class RGBParade : public AbstractGfxScopeWidget
{
public:
    explicit RGBParade(QWidget *parent = Q_NULLPTR);
    ~RGBParade();
    QString widgetName() const;

protected:
    virtual void readConfig();
    void writeConfig();
    QRect scopeRect();

private:
    Ui::RGBParade_UI *ui;
    RGBParadeGenerator *m_rgbParadeGenerator;

    QAction *m_aAxis;
    QAction *m_aGradRef;

    bool isHUDDependingOnInput() const;
    bool isScopeDependingOnInput() const;
    bool isBackgroundDependingOnInput() const;

    QImage renderHUD(uint accelerationFactor);
    QImage renderGfxScope(uint accelerationFactor, const QImage &);
    QImage renderBackground(uint accelerationFactor);
};

#endif // RGBPARADE_H
