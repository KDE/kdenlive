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


#ifndef PROJECTITEM_H
#define PROJECTITEM_H

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
class ProjectItem : public QTreeWidgetItem
{
public:
    ProjectItem(QTreeWidget * parent, DocClipBase *clip);
    ProjectItem(QTreeWidgetItem * parent, DocClipBase *clip);
    virtual ~ProjectItem();
    QDomElement toXml() const;
    int numReferences() const;

    void setProperties(const QMap < QString, QString > &attributes, const QMap < QString, QString > &metadata);

    /** \brief The id of the clip or folder.
     *
     * The clipId is used both to identify clips and folders (groups) */
    const QString &clipId() const;
    QStringList names() const;
    const KUrl clipUrl() const;
    int clipMaxDuration() const;
    CLIPTYPE clipType() const;
    void changeDuration(int frames);
    DocClipBase *referencedClip();
    void setProperties(QMap <QString, QString> props);
    void setProperty(const QString &key, const QString &value);
    void clearProperty(const QString &key);
    QString getClipHash() const;

    virtual bool operator<(const QTreeWidgetItem &other)const {
       int column = treeWidget()->sortColumn();
       if (other.type() != PROJECTFOLDERTYPE)
	  return text(column).toLower() < other.text(column).toLower();
       else return false;
    }

private:
    CLIPTYPE m_clipType;
    QString m_clipId;
    void slotSetToolTip();
    DocClipBase *m_clip;
};

#endif
