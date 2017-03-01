/***************************************************************************
 *   Copyright (C) 2014 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#ifndef LUMALIFTGAINWIDGET_H
#define LUMALIFTGAINWIDGET_H

#include <QWidget>
#include <QDomNodeList>
#include <QLocale>

class ColorWheel;

/**
 * @class ChooseColorWidget
 * @brief Provides options to choose 3 colors.
 * @author Jean-Baptiste Mardelle
 */

class LumaLiftGain : public QWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the widget.
    * @param text (optional) What the color will be used for
    * @param color (optional) initial color
    * @param alphaEnabled (optional) Should transparent colors be enabled */
    explicit LumaLiftGain(const QDomNodeList &nodes, QWidget *parent = nullptr);
    void updateEffect(QDomElement &effect);

private:
    QLocale m_locale;
    ColorWheel *m_lift;
    ColorWheel *m_gamma;
    ColorWheel *m_gain;

signals:
    /** @brief Emitted whenever a different color was chosen. */
    void valueChanged();
};

#endif
