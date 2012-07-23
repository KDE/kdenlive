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

#include "timecode.h"
#include "gentime.h"

#include <QAbstractSpinBox>

/**
 * @class TimecodeDisplay
 * @brief A widget for inserting a timecode value.
 * @author Jean-Baptiste Mardelle
 *
 * TimecodeDisplay can be used to insert eigther frames
 * or a timecode in the format HH:MM:SS:FF
 */
class TimecodeDisplay : public QAbstractSpinBox
{
    Q_OBJECT

public:
    /** @brief Constructor for the widget, sets value to 0.
    * @param t Timecode object used to setup correct input (frames or HH:MM:SS:FF)
    * @param parent parent Widget */
    explicit TimecodeDisplay(Timecode t, QWidget *parent = 0);

    /** @brief Returns the minimum value, which can be entered.
    * default is 0 */
    int minimum() const;

    /** @brief Returns the maximum value, which can be entered.
    * default is no maximum (-1) */
    int maximum() const;

    /** @brief Sets the minimum maximum value that can be entered.
    * @param min the minimum value
    * @param max the maximum value */
    void setRange(int min, int max);

    /** @brief Returns the current input in frames. */
    int getValue() const;

    /** @brief Returns the current input as a GenTime object. */
    GenTime gentime() const;

    /** @brief Returs the widget's timecode object. */
    Timecode timecode() const;

    /** @brief Sets value's format to frames or HH:MM:SS:FF according to @param frametimecode.
    * @param frametimecode true = frames, false = HH:MM:SS:FF
    * @param init true = force the change, false = update only if the frametimecode param changed */
    void setTimeCodeFormat(bool frametimecode, bool init = false);

    /** @brief Sets timecode for current project.
     * @param t the new timecode */
    void updateTimeCode(Timecode t);

    virtual void stepBy(int steps);

private:
    /** timecode for widget */
    Timecode m_timecode;
    /** Should we display the timecode in frames or in format hh:mm:ss:ff */
    bool m_frametimecode;
    int m_minimum;
    int m_maximum;
    int m_value;

public slots:
    /** @brief Sets the value.
    * @param value the new value
    * The value actually set is forced to be within the legal range: minimum <= value <= maximum */
    void setValue(int value);
    void setValue(const QString &value);
    void setValue(GenTime value);

    /** @brief Sets value's format accorrding to Kdenlive's settings.
    * @param t (optional, if already existing) Timecode object to use */
    void slotUpdateTimeCodeFormat();

private slots:
    void slotEditingFinished();

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
    virtual void mouseReleaseEvent(QMouseEvent *);
//    virtual void wheelEvent(QWheelEvent *e);
    virtual QAbstractSpinBox::StepEnabled stepEnabled () const;

};

#endif
