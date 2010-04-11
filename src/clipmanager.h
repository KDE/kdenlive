/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef CLIPMANAGER_H
#define CLIPMANAGER_H

/**ClipManager manages the list of clips in a document
  *@author Jean-Baptiste Mardelle
  */

#include <qdom.h>
#include <QPixmap>
#include <QObject>

#include <KUrl>
#include <KUndoStack>
#include <KDirWatch>
#include <klocale.h>

#include "gentime.h"
#include "definitions.h"


class KdenliveDoc;
class DocClipBase;
class AbstractGroupItem;

namespace Mlt
{
class Producer;
};

class ClipManager: public QObject
{
Q_OBJECT public:

    ClipManager(KdenliveDoc *doc);
    virtual ~ ClipManager();
    void addClip(DocClipBase *clip);
    DocClipBase *getClipAt(int pos);
    void deleteClip(const QString &clipId);
    void slotAddClipFile(const KUrl url, const QString group, const QString &groupId);
    void slotAddClipList(const KUrl::List urls, const QString group, const QString &groupId);
    void slotAddTextClipFile(const QString titleName, int out, const QString xml, const QString group, const QString &groupId);
    void slotAddTextTemplateClip(QString titleName, const KUrl path, const QString group, const QString &groupId);
    void slotAddColorClipFile(const QString name, const QString color, QString duration, const QString group, const QString &groupId);
    void slotAddSlideshowClipFile(const QString name, const QString path, int count, const QString duration, const bool loop, const bool fade, const QString &luma_duration, const QString &luma_file, const int softness, const QString group, const QString &groupId);
    DocClipBase *getClipById(QString clipId);
    const QList <DocClipBase *> getClipByResource(QString resource);
    void slotDeleteClips(QStringList ids);
    void setThumbsProgress(const QString &message, int progress);
    void checkAudioThumbs();
    QList <DocClipBase*> documentClipList() const;
    QMap <QString, QString> documentFolderList() const;
    int getFreeClipId();
    int getFreeFolderId();
    int lastClipId() const;
    void startAudioThumbsGeneration();
    void endAudioThumbsGeneration(const QString &requestedId);
    void askForAudioThumb(const QString &id);
    QString projectFolder() const;
    void clearUnusedProducers();
    void resetProducersList(const QList <Mlt::Producer *> prods);
    void addFolder(const QString&, const QString&);
    void deleteFolder(const QString&);
    void clear();
    AbstractGroupItem *createGroup();
    void removeGroup(AbstractGroupItem *group);
    QDomElement groupsXml() const;
    int clipsCount() const;

public slots:
    void updatePreviewSettings();

private slots:
    void slotClipModified(const QString &path);
    void slotClipMissing(const QString &path);
    void slotClipAvailable(const QString &path);

private:   // Private attributes
    /** the list of clips in the document */
    QList <DocClipBase*> m_clipList;
    /** the list of groups in the document */
    QList <AbstractGroupItem *> m_groupsList;
    QMap <QString, QString> m_folderList;
    QList <QString> m_audioThumbsQueue;
    /** the document undo stack*/
    KdenliveDoc *m_doc;
    int m_clipIdCounter;
    int m_folderIdCounter;
    bool m_audioThumbsEnabled;
    QString m_generatingAudioId;
    KDirWatch m_fileWatcher;

signals:
    void reloadClip(const QString &);
    void missingClip(const QString &);
    void availableClip(const QString &);
    void checkAllClips();
};

#endif
