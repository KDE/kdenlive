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
#include <QProcess>

#include <KUrl>

#include "gentime.h"
#include "definitions.h"
#include "project/jobs/abstractclipjob.h"


class DocClipBase;

/** \brief Represents a clip or a folder in the projecttree
 *
 * This class represents a clip or folder in the projecttree and in the document(?) */
class ProjectItem : public QTreeWidgetItem
{
public:
    ProjectItem(QTreeWidget * parent, DocClipBase *clip, const QSize &pixmapSize);
    ProjectItem(QTreeWidgetItem * parent, DocClipBase *clip, const QSize &pixmapSize);
    virtual ~ProjectItem();
    QDomElement toXml() const;
    int numReferences() const;

    void setProperties(const QMap < QString, QString > &attributes, const QMap < QString, QString > &metadata);

    /** \brief The id of the clip or folder.
     *
     * The clipId is used both to identify clips and folders (groups) */
    const QString &clipId() const;
    const KUrl clipUrl() const;
    int clipMaxDuration() const;
    ClipType clipType() const;
    void changeDuration(int frames);
    DocClipBase *referencedClip();
    void setProperties(const QMap <QString, QString> &props);
    void setProperty(const QString &key, const QString &value);
    void clearProperty(const QString &key);
    QString getClipHash() const;
    static int itemDefaultHeight();
    void slotSetToolTip();
    /** \brief Set the status of the clip job. */
    void setJobStatus(JOBTYPE jobType, ClipJobStatus status, int progress = 0, const QString &statusMessage = QString());
    /** \brief Set the status of a clip job if it is of the specified job type. */
    void setConditionalJobStatus(ClipJobStatus status, JOBTYPE requestedJobType);
    /** \brief Returns the proxy status for this clip (true means there is a proxy clip). */
    bool hasProxy() const;
    /** \brief Returns true if the proxy for this clip is ready. */
    bool isProxyReady() const;
    /** \brief Returns true if there is a job currently running for this clip. */
    bool isJobRunning() const;
    /** \brief Returns true if we are currently creating the proxy for this clip. */
    bool isProxyRunning() const;
    /** \brief Returns true if the thumbnail for this clip has been loaded. */
    bool hasPixmap() const;
    /** \brief Sets the thumbnail for this clip. */
    void setPixmap(const QPixmap& p);

    virtual bool operator<(const QTreeWidgetItem &other)const {
        int column = treeWidget()->sortColumn();
        if (other.type() != ProjectFoldeType)
            return text(column).toLower() < other.text(column).toLower();
        else return false;
    }

private:
    ClipType m_clipType;
    DocClipBase *m_clip;
    QString m_clipId;
    bool m_pixmapSet;
    /** @brief Setup basic properties */
    void buildItem(const QSize &pixmapSize);
    /** @brief Check if an xml project file has proxies */
    bool playlistHasProxies(const QString& path);
};

#endif
