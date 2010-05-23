/* This file is part of the KDE project
   Copyright (c) 2010 Jean-Baptiste Mardelle <jb@kdenlive.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#ifndef TIMECODEDISPLAY_H_
#define TIMECODEDISPLAY_H_

#include "ui_timecodedisplay_ui.h"
#include "timecode.h"

#include <KRestrictedLine>


/**
 * @short A widget for qreal values with a popup slider
 *
 * TimecodeDisplay combines a numerical input and a dropdown slider in a way that takes up as
 * little screen space as possible.
 *
 * It allows the user to either enter a floating point value or quickly set the value using a slider
 *
 * One signal is emitted when the value changes. The signal is even emitted when the slider
 * is moving. The second argument of the signal however tells you if the value is final or not. A
 * final value is produced by entering a value numerically or by releasing the slider.
 *
 * The input of the numerical line edit is constrained to numbers and decimal signs.
 */
class TimecodeDisplay : public QWidget, public Ui::TimecodeDisplay_UI
{

    Q_OBJECT

public:

    /**
     * Constructor for the widget, where value is set to 0
     *
     * @param parent parent QWidget
     */
    TimecodeDisplay(Timecode t, QWidget *parent = 0);

    /**
     * Destructor
     */
    virtual ~TimecodeDisplay();

    /**
     * The minimum value that can be entered.
     * default is 0
     */
    int minimum() const;

    /**
     * The maximum value that can be entered.
     * default is 100
     */
    int maximum() const;

    /**
     * Sets the minimum value that can be entered.
     * @param min the minimum value
     */
    void setMinimum(int min);

    /**
     * Sets the maximum value that can be entered.
     * @param max the maximum value
     */
    void setMaximum(int max);

    /**
    * The value shown.
    */
    int value() const;

    //virtual QSize minimumSizeHint() const; ///< reimplemented from QComboBox
    //virtual QSize sizeHint() const; ///< reimplemented from QComboBox

private:
    /** timecode for widget */
    Timecode m_timecode;
    /** Should we display the timecode in frames or in format hh:mm:ss:ff */
    bool m_frametimecode;
    int m_minimum;
    int m_maximum;

public slots:

    /**
    * Sets the value.
    * The value actually set is forced to be within the legal range: minimum <= value <= maximum
    * @param value the new value
    */
    void setValue(int value);
    void setValue(const QString &value);
    void slotPrepareTimeCodeFormat(Timecode t);

private slots:
    void slotValueUp();
    void slotValueDown();

signals:

    /**
     * Emitted every time the value changes (by calling setValue() or
     * by user interaction).
     * @param value the new value
     * @param final if the value is final ie not produced during sliding (on slider release it's final)
     */
    void valueChanged(int value, bool final);
    void editingFinished();

protected:
    virtual void keyPressEvent(QKeyEvent *e);
    virtual void wheelEvent(QWheelEvent *e);

};

#endif
