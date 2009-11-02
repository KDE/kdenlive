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


#include "folderprojectitem.h"

#include <KDebug>
#include <KLocale>
#include <KIcon>


FolderProjectItem::FolderProjectItem(QTreeWidget * parent, const QStringList & strings, const QString &clipId) :
        QTreeWidgetItem(parent, strings, PROJECTFOLDERTYPE),
        m_groupName(strings.at(1)),
        m_clipId(clipId)
{
    setSizeHint(0, QSize(65, QFontInfo(font(1)).pixelSize() * 2));
    setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    setIcon(0, KIcon("folder").pixmap(sizeHint(0)));
    setToolTip(1, "<b>" + i18n("Folder"));
    //setFlags(Qt::NoItemFlags);
    //kDebug() << "Constructed with clipId: " << m_clipId;
}


FolderProjectItem::~FolderProjectItem()
{
}

QString FolderProjectItem::clipId() const
{
    return m_clipId;
}

const QString FolderProjectItem::groupName() const
{
    return m_groupName;
}

void FolderProjectItem::setGroupName(const QString name)
{
    m_groupName = name;
    setText(1, name);
}



