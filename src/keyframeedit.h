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
    explicit KeyframeEdit(QDomElement e, int max, Timecode tc, QWidget* parent = 0);
    void setupParam(QDomElement e = QDomElement());

private:
    Ui::KeyframeEditor_UI m_ui;
    QDomElement m_param;
    int m_max;
    Timecode m_timecode;
    int m_previousPos;

public slots:


private slots:
    void slotDeleteKeyframe();
    void slotAddKeyframe();
    void slotGenerateParams(QTreeWidgetItem *item = NULL, int column = -1);
    void slotAdjustKeyframeInfo();
    void slotAdjustKeyframeValue(int value);
    void slotSaveCurrentParam(QTreeWidgetItem *item, int column);

signals:
    void parameterChanged();
    void seekToPos(int);
};

#endif
