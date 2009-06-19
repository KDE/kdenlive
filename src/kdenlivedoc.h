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
#include <QString>
#include <QMap>
#include <QList>
#include <QDir>
#include <QObject>
#include <QUndoGroup>
#include <QUndoStack>
#include <QTimer>

#include <KUrl>
#include <kautosavefile.h>

#include "gentime.h"
#include "timecode.h"
#include "definitions.h"
#include "guide.h"

class Render;
class ClipManager;
class DocClipBase;
class MainWindow;
class TrackInfo;

class KdenliveDoc: public QObject
{
Q_OBJECT public:

    KdenliveDoc(const KUrl &url, const KUrl &projectFolder, QUndoGroup *undoGroup, QString profileName, const QPoint tracks, Render *render, MainWindow *parent = 0);
    ~KdenliveDoc();
    QDomNodeList producersList();
    double fps() const;
    int width() const;
    int height() const;
    KUrl url() const;
    KAutoSaveFile *m_autosave;
    Timecode timecode() const;
    QDomDocument toXml();
    //void setRenderer(Render *render);
    QUndoStack *commandStack();
    QString producerName(const QString &id);
    Render *renderer();
    QDomDocument m_guidesXml;
    QDomElement guidesXml() const;
    ClipManager *clipManager();
    void addClip(QDomElement elem, QString clipId, bool createClipItem = true);
    void addClipInfo(QDomElement elem, QDomElement orig, QString clipId);
    void slotAddClipFile(const KUrl url, const QString group, const QString &groupId = QString());
    void slotAddClipList(const KUrl::List urls, const QString group, const QString &groupId = QString());
    void deleteClip(const QString &clipId);
    int getFramePos(QString duration);
    DocClipBase *getBaseClip(const QString &clipId);
    void updateClip(const QString &id);
    void deleteProjectClip(QList <QString> ids);
    /** Inform application of the audio thumbnails generation progress */
    void setThumbsProgress(const QString &message, int progress);
    const QString &profilePath() const;
    MltVideoProfile mltProfile() const;
    const QString description() const;
    void setUrl(KUrl url);
    void setProfilePath(QString path);
    const QString getFreeClipId();
    /** does the document need saving */
    bool isModified() const;
    /** Returns project folder, used to store project files (titles, effects,...) */
    KUrl projectFolder() const;
    /** Used to inform main app of the current document loading progress */
    void loadingProgressed();
    void syncGuides(QList <Guide *> guides);
    void setZoom(int factor);
    int zoom() const;
    double dar();
    double projectDuration() const;
    bool saveSceneList(const QString &path, const QString &scene);
    int tracksCount() const;
    TrackInfo trackInfoAt(int ix) const;
    void insertTrack(int ix, TrackInfo type);
    void deleteTrack(int ix);
    void setTrackType(int ix, TrackInfo type);
    const QList <TrackInfo> tracksList() const;
    QPoint getTracksCount() const;
    void switchTrackVideo(int ix, bool hide);
    void switchTrackAudio(int ix, bool hide);
    void switchTrackLock(int ix, bool lock);
    void cachePixmap(const QString &fileId, const QPixmap &pix) const;
    void setProjectFolder(KUrl url);
    QString getLadspaFile() const;
    void setZone(int start, int end);
    QPoint zone() const;
    void setSceneList();
    void updatePreviewSettings();
    bool isTrackLocked(int ix) const;

private:
    KUrl m_url;
    QDomDocument m_document;
    double m_fps;
    int m_zoom;
    /** Cursor position at document opening */
    int m_startPos;
    int m_width;
    int m_height;
    Timecode m_timecode;
    Render *m_render;
    QUndoStack *m_commandStack;
    ClipManager *m_clipManager;
    MltVideoProfile m_profile;
    QTimer *m_autoSaveTimer;
    QString m_searchFolder;
    /** tells whether current doc has been changed since last save event */
    bool m_modified;
    /** Project folder, used to store project files (titles, effects,...) */
    KUrl m_projectFolder;
    double m_documentLoadingStep;
    double m_documentLoadingProgress;
    bool m_abortLoading;
    int m_zoneStart;
    int m_zoneEnd;

    QList <TrackInfo> m_tracksList;

    QDomDocument createEmptyDocument(const int videotracks, const int audiotracks);

    void setNewClipResource(const QString &id, const QString &path);
    QString searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash) const;
    void moveProjectData(KUrl url);
    bool checkDocumentClips(QDomNodeList infoproducers);

public slots:
    void slotCreateTextClip(QString group, const QString &groupId, const QString &templatePath = QString());
    void slotCreateTextTemplateClip(QString group, const QString &groupId, KUrl path);
    /** Set to true if document needs saving, false otherwise */
    void setModified(bool mod = true);
    void checkProjectClips();

private slots:
    void slotAutoSave();

signals:
    void resetProjectList();
    void addProjectClip(DocClipBase *, bool getInfo = true);
    void signalDeleteProjectClip(const QString &);
    void updateClipDisplay(const QString&);
    void deleteTimelineClip(const QString&);
    void progressInfo(const QString &, int);
    /** emitted when the document state has been modified (= needs saving or not) */
    void docModified(bool);
    void selectLastAddedClip(const QString &);
    void guidesUpdated();
};

#endif
