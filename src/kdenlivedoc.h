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


#ifndef KDENLIVEDOC_H
#define KDENLIVEDOC_H

#include <qdom.h>
#include <qstring.h>
#include <qmap.h>
#include <QList>
#include <QObject>
#include <QUndoGroup>

#include <KUndoStack>
#include <kurl.h>

#include "gentime.h"
#include "timecode.h"
#include "definitions.h"

class Render;
class ClipManager;
class DocClipBase;

class KdenliveDoc: public QObject {
Q_OBJECT public:

    KdenliveDoc(const KUrl &url, MltVideoProfile profile, QUndoGroup *undoGroup, QWidget *parent = 0);
    ~KdenliveDoc();
    QDomNodeList producersList();
    double fps() const;
    int width() const;
    int height() const;
    KUrl url() const;
    void backupMltPlaylist();
    Timecode timecode() const;
    QDomDocument toXml() const;
    void setRenderer(Render *render);
    KUndoStack *commandStack();
    QString producerName(int id);
    void setProducerDuration(int id, int duration);
    int getProducerDuration(int id);
    Render *renderer();
    ClipManager *clipManager();
    void addClip(const QDomElement &elem, const int clipId);
    void addFolder(const QString foldername, int clipId, bool edit);
    void deleteFolder(const QString foldername, int clipId);
    void slotAddClipFile(const KUrl url, const QString group, const int groupId = -1);
    void slotAddFolder(const QString folderName);
    void slotDeleteFolder(const QString folderName, const int id);
    void slotEditFolder(const QString folderName, const QString oldfolderName, int clipId);
    void slotAddColorClipFile(const QString name, const QString color, QString duration, const QString group, const int groupId = -1);
    void deleteClip(const uint clipId);
    int getFramePos(QString duration);
    DocClipBase *getBaseClip(int clipId);
    void updateClip(int id);
    void deleteProjectClip(QList <int> ids);
    void deleteProjectFolder(QMap <QString, int> map);
    /** Inform application of the audio thumbnails generation progress */
    void setThumbsProgress(KUrl url, int progress);
    QString profilePath() const;
    QString description() const;
    /** Returns the document format: PAL or NTSC */
    QString getDocumentStandard();
    void setUrl(KUrl url);
    QDomElement documentInfoXml();
    void setProfilePath(QString path);
    /** Set to true if document needs saving, false otherwise */
    void setModified(bool mod);
    int getFreeClipId();
    /** does the document need saving */
    bool isModified();

private:
    KUrl m_url;
    QDomDocument m_document;
    QString m_projectName;
    double m_fps;
    int m_width;
    int m_height;
    Timecode m_timecode;
    Render *m_render;
    KUndoStack *m_commandStack;
    QDomDocument generateSceneList();
    ClipManager *m_clipManager;
    MltVideoProfile m_profile;
    QString m_scenelist;
    /** tells whether current doc has been changed since last save event */
    bool m_modified;

public slots:

signals:
    void addProjectClip(DocClipBase *);
    void addProjectFolder(const QString, int, bool, bool edit = false);
    void signalDeleteProjectClip(int);
    void updateClipDisplay(int);
    void deletTimelineClip(int);
    void thumbsProgress(KUrl, int);
    /** emited when the document state has been modified (= needs saving or not) */
    void docModified(bool);
};

#endif
