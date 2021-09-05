/*
 *   SPDX-FileCopyrightText: 2010 Simon Andreas Eugster (simon.eu@gmail.com)      *
 *   This file is part of kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 ***************************************************************************/

#ifndef RGBPARADE_H
#define RGBPARADE_H

#include "abstractgfxscopewidget.h"
#include "ui_rgbparade_ui.h"

class QImage;
class RGBParade_UI;
class RGBParadeGenerator;

/**
 * @brief Displays the RGB waveform of a frame.
 * This is the same as the Waveform, but for each colour channel separately.
 */
class RGBParade : public AbstractGfxScopeWidget
{
public:
    explicit RGBParade(QWidget *parent = nullptr);
    ~RGBParade() override;
    QString widgetName() const override;

protected:
    void readConfig() override;
    void writeConfig();
    QRect scopeRect() override;

private:
    Ui::RGBParade_UI *m_ui;
    RGBParadeGenerator *m_rgbParadeGenerator;

    QAction *m_aAxis;
    QAction *m_aGradRef;

    bool isHUDDependingOnInput() const override;
    bool isScopeDependingOnInput() const override;
    bool isBackgroundDependingOnInput() const override;

    QImage renderHUD(uint accelerationFactor) override;
    QImage renderGfxScope(uint accelerationFactor, const QImage &) override;
    QImage renderBackground(uint accelerationFactor) override;
};

#endif // RGBPARADE_H
