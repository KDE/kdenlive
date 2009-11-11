/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#include "subprojectitem.h"
#include "timecode.h"
#include "definitions.h"
#include "kdenlivesettings.h"
#include "docclipbase.h"

#include <KDebug>
#include <KLocale>
#include <KIcon>

const int DurationRole = Qt::UserRole + 1;

SubProjectItem::SubProjectItem(QTreeWidgetItem * parent, int in, int out) :
        QTreeWidgetItem(parent, PROJECTSUBCLIPTYPE), m_in(in), m_out(out)
{
    setSizeHint(0, QSize(65, 30));
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    QString name = Timecode::getStringTimecode(in, KdenliveSettings::project_fps());
    setText(0, name);
    GenTime duration = GenTime(out - in, KdenliveSettings::project_fps());
    if (duration != GenTime()) setData(0, DurationRole, Timecode::getEasyTimecode(duration, KdenliveSettings::project_fps()));
    //setFlags(Qt::NoItemFlags);
    //kDebug() << "Constructed with clipId: " << m_clipId;
}


SubProjectItem::~SubProjectItem()
{
}

int SubProjectItem::numReferences() const
{
    return 0;
}

QDomElement SubProjectItem::toXml() const
{
    //return m_clip->toXML();
    return QDomElement();
}

QPoint SubProjectItem::zone() const
{
    QPoint z(m_in, m_out);
    return z;
}

DocClipBase *SubProjectItem::referencedClip()
{
    return NULL; //m_clip;
}


