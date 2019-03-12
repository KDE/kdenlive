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
 Two mechanisms are provided: color-picking directly on the screen and choosing from a list
 * @author Till Theato
 */

class ChooseColorWidget : public QWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the widget.
     * @param name (optional) What the color will be used for (name of the parameter)
     * @param color (optional) initial color
     * @param comment (optional) Comment about the parameter
     * @param alphaEnabled (optional) Should transparent colors be enabled
     * @param parent(optional) Parent widget
     */
    explicit ChooseColorWidget(const QString &name = QString(), const QString &color = QStringLiteral("0xffffffff"), const QString &comment = QString(),
                               bool alphaEnabled = false, QWidget *parent = nullptr);

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

    void disableCurrentFilter(bool);
    void valueChanged();
};

#endif
