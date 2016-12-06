/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
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

#ifndef CHOOSECOLORWIDGET_H
#define CHOOSECOLORWIDGET_H

#include <QWidget>

class KColorButton;

/**
 * @class ChooseColorWidget
 * @brief Provides options to choose a color.
 * @author Till Theato
 */

class ChooseColorWidget : public QWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the widget.
    * @param text (optional) What the color will be used for
    * @param color (optional) initial color
    * @param alphaEnabled (optional) Should transparent colors be enabled */
    explicit ChooseColorWidget(const QString &text = QString(), const QString &color = QStringLiteral("0xffffffff"), bool alphaEnabled = false, QWidget *parent = Q_NULLPTR);

    /** @brief Gets the chosen color. */
    QString getColor() const;

private:
    KColorButton *m_button;

public slots:
    void slotColorModified(const QColor &color);

private slots:
    /** @brief Updates the different color choosing options to have all selected @param color. */
    void setColor(const QColor &color);

signals:
    /** @brief Emitted whenever a different color was chosen. */
    void modified(QColor = QColor());
    void displayMessage(const QString &, int);
    /** @brief When user wants to pick a color, it's better to disable filter so we get proper color values. */
    void disableCurrentFilter(bool);
};

#endif
