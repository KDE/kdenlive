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

#ifndef POSITIONEDIT_H
#define POSITIONEDIT_H


#include "timecode.h"

#include <QWidget>

class QSlider;
class TimecodeDisplay;

class PositionEdit : public QWidget
{
    Q_OBJECT
public:
    explicit PositionEdit(const QString &name, int pos, int min, int max, const Timecode& tc, QWidget* parent = Q_NULLPTR);
    ~PositionEdit();
    int getPosition() const;
    void setPosition(int pos);
    void updateTimecodeFormat();
    bool isValid() const;

public slots:
    void setRange(int min, int max, bool absolute = false);

private:
    TimecodeDisplay *m_display;
    QSlider *m_slider;

private slots:
    void slotUpdatePosition();

signals:
    void parameterChanged(int pos = 0);
};

#endif
