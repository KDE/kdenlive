/***************************************************************************
                          keyframeedit.h  -  description
                             -------------------
    begin                : 22 Jun 2009
    copyright            : (C) 2008 by Jean-Baptiste Mardelle
    email                : jb@kdenlive.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KEYFRAMEEDIT_H
#define KEYFRAMEEDIT_H


#include <QWidget>
#include <QDomElement>


#include "ui_keyframeeditor_ui.h"
#include "definitions.h"
#include "keyframehelper.h"

//class QGraphicsScene;

class KeyframeEdit : public QWidget
{
    Q_OBJECT
public:
    explicit KeyframeEdit(int maxFrame, double fps, int minValue, int maxValue, QWidget* parent = 0);

private:
    Ui::KeyframeEditor_UI m_ui;
    double m_fps;

public slots:


private slots:

signals:
    void parameterChanged();
    void seekToPos(int);
};

#endif
