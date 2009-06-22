/***************************************************************************
                          geomeytrval.cpp  -  description
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

#include "keyframeedit.h"
#include "kdenlivesettings.h"

#include <KDebug>


KeyframeEdit::KeyframeEdit(Timecode tc, QWidget* parent) :
        QWidget(parent),
        m_timecode(tc)
{
    m_ui.setupUi(this);
    m_ui.keyframe_list->setHeaderLabels(QStringList() << i18n("Position") << i18n("Value"));
    m_ui.button_add->setIcon(KIcon("document-new"));
    m_ui.button_delete->setIcon(KIcon("edit-delete"));
    setEnabled(false);
}

void KeyframeEdit::setupParam(int maxFrame, int minValue, int maxValue, QString keyframes)
{
    m_maxFrame = maxFrame;
    m_min = minValue;
    m_max = maxValue;
    m_ui.keyframe_list->clear();
    QStringList frames = keyframes.split(";");
    for (int i = 0; i < frames.count(); i++) {
        QString framePos = m_timecode.getTimecodeFromFrames(frames.at(i).section(':', 0, 0).toInt());
        m_ui.keyframe_list->addTopLevelItem(new QTreeWidgetItem(QStringList() << framePos << frames.at(i).section(':', 1, 1)));
    }
}


