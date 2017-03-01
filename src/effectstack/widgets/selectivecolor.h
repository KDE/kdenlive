/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SELECTIVECOLORWIDGET_H
#define SELECTIVECOLORWIDGET_H

#include "ui_selectivecolor_ui.h"

#include <QWidget>
#include <QLocale>
#include <QDomElement>

/**
 * @class SelectiveColor
 * @brief Provides options to adjust CMYK factor of a color range.
 * @author Jean-Baptiste Mardelle
 */

class SelectiveColor : public QWidget, public Ui::SelectiveColor
{
    Q_OBJECT
public:
    /** @brief Sets up the widget.
    * @param text (optional) What the color will be used for
    * @param color (optional) initial color
    * @param alphaEnabled (optional) Should transparent colors be enabled */
    explicit SelectiveColor(const QDomElement &effect, QWidget *parent = nullptr);
    ~SelectiveColor();
    void addParam(QDomElement &effect, QString name);
    void updateEffect(QDomElement &effect);

private:
    QLocale m_locale;

private slots:
    void updateValues();
    void effectChanged();

signals:
    /** @brief Emitted whenever a different color was chosen. */
    void valueChanged();
};

#endif
