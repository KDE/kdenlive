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
#include <kselectaction.h>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QProgressBar>

class QValidator;
class QToolButton;
class QLineEdit;
class QAction;
class QMenu;
class KSelectAction;


class CustomLabel : public QProgressBar
{
    Q_OBJECT
public:
    CustomLabel(const QString &label, bool showSlider = true, int precision = 0, QWidget *parent = 0);
    void setProgressValue(double value);
    
protected:
    //virtual void mouseDoubleClickEvent(QMouseEvent * event);
    virtual void mousePressEvent(QMouseEvent * event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    //virtual void paintEvent(QPaintEvent *event);
    virtual void wheelEvent(QWheelEvent * event);

private:
    QPoint m_dragStartPosition;
    QPoint m_dragLastPosition;
    bool m_dragMode;
    double m_step;
    bool m_showSlider;
    double m_precision;
    double m_value;
    void slotValueInc(double factor = 1);
    void slotValueDec(double factor = 1);
    void setNewValue(double, bool);
    
signals:
    void valueChanged(double, bool);
    void setInTimeline();
    void resetValue();
};

/**
 * @brief A widget for modifing numbers by dragging, using the mouse wheel or entering them with the keyboard.
 */

class DragValue : public QWidget
{
    Q_OBJECT

public:
    DragValue(const QString &label, double defaultValue, int decimals, int id, const QString suffix, bool showSlider = true, QWidget* parent = 0);
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
    /** @brief Change the "inTimeline" property to paint the intimeline widget differently. */
    void setInTimelineProperty(bool intimeline);
    /** @brief Returns minimum size for QSpinBox, used to set all spinboxes to the same width. */
    int spinSize();
    /** @brief Sets the minimum size for QSpinBox, used to set all spinboxes to the same width. */
    void setSpinSize(int width);
    
public slots:
    /** @brief Sets the value (forced to be in the valid range) and emits valueChanged. */
    void setValue(double value, bool final = true);
    /** @brief Resets to default value */
    void slotReset();

signals:
    void valueChanged(int value, bool final = true);
    void valueChanged(double value, bool final = true);
    void inTimeline(int);



    /*
     * Private
     */

protected:
    /*virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);*/
    /** @brief Forwards tab focus to lineedit since it is disabled. */
    virtual void focusInEvent(QFocusEvent *e);
    //virtual void keyPressEvent(QKeyEvent *e);
    //virtual void wheelEvent(QWheelEvent *e);
    //virtual void paintEvent( QPaintEvent * event );

private slots:

    void slotEditingFinished();

    void slotSetScaleMode(int mode);
    void slotSetDirectUpdate(bool directUpdate);
    void slotShowContextMenu(const QPoint &pos);
    void slotSetValue(int value);
    void slotSetValue(double value);
    void slotSetInTimeline();

private:
    double m_maximum;
    double m_minimum;
    int m_decimals;
    double m_default;
    int m_id;
    QSpinBox *m_intEdit;
    QDoubleSpinBox *m_doubleEdit;

    QMenu *m_menu;
    KSelectAction *m_scale;
    QAction *m_directUpdate;
    CustomLabel *m_label;
};

#endif
