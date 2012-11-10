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


#ifndef SUBPROJECTITEM_H
#define SUBPROJECTITEM_H

#include <QTreeWidgetItem>
#include <QTreeWidget>
#include <QDomElement>

#include <KUrl>

#include "gentime.h"
#include "definitions.h"

class DocClipBase;

/** \brief Represents a clip or a folder in the projecttree
 *
 * This class represents a clip or folder in the projecttree and in the document(?) */
class SubProjectItem : public QTreeWidgetItem
{
public:
    SubProjectItem(double display_ratio, QTreeWidgetItem * parent, int in, int out, QString description = QString());
    virtual ~SubProjectItem();
    QDomElement toXml() const;
    int numReferences() const;
    DocClipBase *referencedClip();
    QPoint zone() const;
    void setZone(QPoint p);
    QString description() const;
    void setDescription(QString desc);
    static int itemDefaultHeight();

    /** Make sure folders appear on top of the tree widget */
    virtual bool operator<(const QTreeWidgetItem &other)const {
        int column = treeWidget()->sortColumn();
        if (other.type() != PROJECTFOLDERTYPE)
            return text(column).toLower() < other.text(column).toLower();
        else return false;
    }

private:
    int m_in;
    int m_out;
    QString m_description;
};

#endif
