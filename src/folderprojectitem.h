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


#ifndef FOLDERPROJECTITEM_H
#define FOLDERPROJECTITEM_H

#include <QTreeWidgetItem>
#include <QTreeWidget>


/** \brief Represents a clip or a folder in the projecttree
 *
 * This class represents a clip or folder in the projecttree and in the document(?) */
class FolderProjectItem : public QTreeWidgetItem
{
public:
    FolderProjectItem(QTreeWidget* parent, const QStringList & strings, const QString &clipId);
    virtual ~FolderProjectItem();
    QString clipId() const;
    const QString groupName() const;
    void setGroupName(const QString name);

private:
    QString m_groupName;
    QString m_clipId;
};

#endif
