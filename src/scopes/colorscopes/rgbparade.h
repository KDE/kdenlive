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
    explicit RGBParade(QWidget *parent = nullptr);
    ~RGBParade();
    QString widgetName() const Q_DECL_OVERRIDE;

protected:
    void readConfig() Q_DECL_OVERRIDE;
    void writeConfig();
    QRect scopeRect() Q_DECL_OVERRIDE;

private:
    Ui::RGBParade_UI *ui;
    RGBParadeGenerator *m_rgbParadeGenerator;

    QAction *m_aAxis;
    QAction *m_aGradRef;

    bool isHUDDependingOnInput() const Q_DECL_OVERRIDE;
    bool isScopeDependingOnInput() const Q_DECL_OVERRIDE;
    bool isBackgroundDependingOnInput() const Q_DECL_OVERRIDE;

    QImage renderHUD(uint accelerationFactor) Q_DECL_OVERRIDE;
    QImage renderGfxScope(uint accelerationFactor, const QImage &) Q_DECL_OVERRIDE;
    QImage renderBackground(uint accelerationFactor) Q_DECL_OVERRIDE;
};

#endif // RGBPARADE_H
