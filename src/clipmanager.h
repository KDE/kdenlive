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
#include <klocale.h>

#include "gentime.h"
#include "definitions.h"


class KdenliveDoc;
class DocClipBase;

class ClipManager: public QObject {
Q_OBJECT public:


    ClipManager(KdenliveDoc *doc);
    virtual ~ ClipManager();
    void addClip(DocClipBase *clip);
    DocClipBase *getClipAt(int pos);
    void deleteClip(uint clipId);
    void slotAddClipFile(const KUrl url, const QString group, const int groupId);
    void slotAddTextClipFile(const QString path, const QString xml, const QString group, const int groupId);
    void slotAddColorClipFile(const QString name, const QString color, QString duration, const QString group, const int groupId);
    void slotAddSlideshowClipFile(const QString name, const QString path, int count, const QString duration, const bool loop, const bool fade, const QString &luma_duration, const QString &luma_file, const int softness, const QString group, const int groupId);
    DocClipBase *getClipById(int clipId);
    void slotDeleteClip(uint clipId);
    void setThumbsProgress(const QString &message, int progress);
    void checkAudioThumbs();
    QList <DocClipBase*> documentClipList();
    int getFreeClipId();

private:   // Private attributes
    /** the list of clips in the document */
    QList <DocClipBase*> m_clipList;
    /** the document undo stack*/
    KdenliveDoc *m_doc;
    int m_clipIdCounter;
    bool m_audioThumbsEnabled;

};

#endif
