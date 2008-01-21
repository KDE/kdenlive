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

#include "gentime.h"
#include "docclipbase.h"


class ProjectItem : public QTreeWidgetItem
{
  public:
    ProjectItem(QTreeWidget * parent, const QStringList & strings, QDomElement xml, int clipId);
    ProjectItem(QTreeWidgetItem * parent, const QStringList & strings, QDomElement xml, int clipId);
    ProjectItem(QTreeWidget * parent, const QStringList & strings, int clipId);
    virtual ~ProjectItem();
    QDomElement toXml() const;

    void setProperties(const QMap < QString, QString > &attributes, const QMap < QString, QString > &metadata);
    int clipId() const;
    QStringList names() const;
    bool isGroup() const;
    const QString groupName() const;
    const KUrl clipUrl() const;
    int clipMaxDuration() const;

  private:
    QDomElement m_element;
    GenTime m_duration;
    bool m_durationKnown;
    DocClipBase::CLIPTYPE m_clipType;
    int m_clipId;
    void slotSetToolTip();
    bool m_isGroup;
    QString m_groupName;
};

#endif
