/***************************************************************************
                          positionedit.h  -  description
                             -------------------
    begin                : 03 Aug 2008
    copyright            : (C) 2008 by Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef POSITIONWIDGET_H
#define POSITIONWIDGET_H

#include "timecode.h"
#include <QString>
#include <QWidget>

class QSlider;
class TimecodeDisplay;

/*@brief This class is used to display a parameter with time value */
class PositionWidget : public QWidget
{
    Q_OBJECT
public:
    /** @brief Sets up the parameter's GUI.
     * @param name Name of the parameter
     * @param pos Initial position (in time)
     * @param min Minimum time value
     * @param max maximum time value
     * @param tc Timecode object to define time format
     * @param comment (optional) A comment explaining the parameter. Will be shown in a tooltip.
     * @param parent (optional) Parent Widget */
    explicit PositionWidget(const QString &name, int pos, int min, int max, const Timecode &tc, const QString &comment = QString(), QWidget *parent = nullptr);
    ~PositionWidget() override;
    /** @brief get current position
     */
    int getPosition() const;
    /** @brief set position
     */
    void setPosition(int pos);
    /** @brief Call this when the timecode has been changed project-wise
     */
    void updateTimecodeFormat();
    /** @brief checks that the allowed time interval is valid
     */
    bool isValid() const;

public slots:
    /** @brief change the range of the widget
        @param min New minimum value
        @param max New maximum value
        @param absolute If true, the range is shifted to start in 0
     */
    void setRange(int min, int max, bool absolute = false);

    void slotShowComment(bool show);

private:
    TimecodeDisplay *m_display;
    QSlider *m_slider;

private slots:
    void slotUpdatePosition();

signals:
    void valueChanged();
};

#endif
