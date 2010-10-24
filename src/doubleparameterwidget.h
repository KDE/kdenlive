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


#ifndef DOUBLEPARAMETERWIDGET_H
#define DOUBLEPARAMETERWIDGET_H

#include <QWidget>

class QLabel;
class QSlider;
class QSpinBox;

/**
 * @class DoubleParameterWidget
 * @brief Widget to choose a double parameter (for a effect) with the help of a slider and a spinbox.
 * @author Till Theato
 *
 * The widget does only handle integers, so the parameter has to be converted into the proper double range afterwards.
 */

class DoubleParameterWidget : public QWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the parameter's GUI.
    * @param name Name of the parameter
    * @param value Value of the parameter
    * @param min Minimum value
    * @param max maximum value
    * @param defaultValue Value used when using reset functionality
    * @param suffix (optional) Suffix to display in spinbox
    * @param parent (optional) Parent Widget */
    DoubleParameterWidget(const QString &name, int value, int min, int max, int defaultValue, const QString suffix = QString(), QWidget* parent = 0);
    /** @brief Updates the label to display @param name. */
    void setName(const QString &name);
    /** @brief Gets the parameter's value. */
    int getValue();

public slots:
    /** @brief Sets the value to @param value. */
    void setValue(int value);

    /** @brief Sets value to m_default. */
    void slotReset();

private:
    int m_default;
    QLabel *m_name;
    QSlider *m_slider;
    QSpinBox *m_spinBox;
    
signals:
    void valueChanged(int);
};

#endif
