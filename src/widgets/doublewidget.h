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

#ifndef DOUBLEWIDGET_H
#define DOUBLEWIDGET_H

#include <QWidget>

class DragValue;

/**
 * @class DoubleWidget
 * @brief Widget to choose a double parameter (for a effect) with the help of a slider and a spinbox.
 * @author Till Theato
 *
 * The widget does only handle integers, so the parameter has to be converted into the proper double range afterwards.
 */

class DoubleWidget : public QWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the parameter's GUI.
     * @param name Name of the parameter
     * @param value Value of the parameter
     * @param min Minimum value
     * @param max maximum value
     * @param factor value, if we want a range 0-1000 for a 0-1 parameter, we can set a factor of 1000
     * @param defaultValue Value used when using reset functionality
     * @param comment A comment explaining the parameter. Will be shown in a tooltip.
     * @param suffix (optional) Suffix to display in spinbox
     * @param parent (optional) Parent Widget */
    explicit DoubleWidget(const QString &name, double value, double min, double max, double factor, double defaultValue, const QString &comment, int id,
                          const QString &suffix = QString(), int decimals = 0, bool oddOnly = false, QWidget *parent = nullptr);
    ~DoubleWidget() override;

    /** @brief Gets the parameter's value. */
    double getValue();
    /** @brief Returns minimum size for QSpinBox, used to set all spinboxes to the same width. */
    int spinSize();
    void setSpinSize(int width);
    void enableEdit(bool enable);
    /** @brief Returns true if widget is currently being edited */
    bool hasEditFocus() const;

public slots:
    /** @brief Sets the value to @param value. */
    void setValue(double value);

    /** @brief Sets value to m_default. */
    void slotReset();

    /** @brief Shows/Hides the comment label. */
    void slotShowComment(bool show);

private slots:

    void slotSetValue(double value, bool final);

private:
    DragValue *m_dragVal;
    double m_factor;

signals:
    void valueChanged(double);

    // same signal as valueChanged, but add an extra boolean to tell if user is dragging value or not
    void valueChanging(double, bool);
};

#endif
