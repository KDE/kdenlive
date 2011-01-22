/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#ifndef DRAGVALUE_H_
#define DRAGVALUE_H_

#include <QWidget>

class QValidator;
class QToolButton;
class QLineEdit;

/**
 * @brief A widget for modifing numbers by dragging, using the mouse wheel or entering them with the keyboard.
 */

class DragValue : public QWidget
{
    Q_OBJECT

public:
    DragValue(QWidget* parent = 0);
    virtual ~DragValue();

    /** @brief Returns the precision = number of decimals */
    int precision() const;
    /** @brief Returns the maximum value */
    qreal minimum() const;
    /** @brief Returns the minimum value */
    qreal maximum() const;

    /** @brief Sets the precision (number of decimals) to @param precision. */
    void setPrecision(int precision);
    /** @brief Sets the minimum value. */
    void setMinimum(qreal min);
    /** @brief Sets the maximum value. */
    void setMaximum(qreal max);
    /** @brief Sets minimum and maximum value. */
    void setRange(qreal min, qreal max);
    /** @brief Sets the size of a step (when dragging or using the mouse wheel). */
    void setStep(qreal step);

    /** @brief Returns the current value */
    qreal value() const;
    
public slots:
    /** @brief Sets the value (forced to be in the valid range) and emits valueChanged. */
    void setValue(qreal value, bool final = true);

signals:
    void valueChanged(qreal value, bool final);




    /*
     * Private
     */

protected:
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    /** @brief Forwards tab focus to lineedit since it is disabled. */
    virtual void focusInEvent(QFocusEvent *e);
    //virtual void keyPressEvent(QKeyEvent *e);
    virtual void wheelEvent(QWheelEvent *e);

private slots:
    void slotValueInc();
    void slotValueDec();
    void slotEditingFinished();

private:
    qreal m_maximum;
    qreal m_minimum;
    int m_precision;
    qreal m_step;
    QLineEdit *m_edit;
    /*QToolButton *m_buttonInc;
    QToolButton *m_buttonDec;*/
    QPoint m_dragStartPosition;
    QPoint m_dragLastPosition;
    bool m_dragMode;
    bool m_finalValue;

    /** @brief Sets the maximum width of the widget so that there is enough space for the widest possible value. */
    void updateMaxWidth();
};

#endif
