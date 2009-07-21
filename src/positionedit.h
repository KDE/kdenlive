/***************************************************************************
                          geomeytrval.h  -  description
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

#ifndef POSITONEDIT_H
#define POSITONEDIT_H


#include <QWidget>

#include "ui_positionval_ui.h"
#include "timecode.h"


class PositionEdit : public QWidget
{
    Q_OBJECT
public:
    explicit PositionEdit(const QString name, int pos, int min, int max, const Timecode tc, QWidget* parent = 0);
    int getPosition() const;
    void setPosition(int pos);

private:
    Ui::Positionval_UI m_ui;
    Timecode m_tc;

private slots:
    void slotUpdateTimecode();
    void slotUpdatePosition();

signals:
    void parameterChanged();
};

#endif
