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

class KTextEdit;
class KProgressDialog;

class KdenliveDoc: public QObject
{
Q_OBJECT public:

    KdenliveDoc(const KUrl &url, const KUrl &projectFolder, QUndoGroup *undoGroup, QString profileName, const QPoint tracks, Render *render, KTextEdit *notes, MainWindow *parent = 0, KProgressDialog *progressDialog = 0);
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

    /** @brief Adds a clip to the project tree.
     * @return false if the user aborted the operation, true otherwise */
    bool addClip(QDomElement elem, QString clipId, bool createClipItem = true);

    /** @brief Updates information about a clip.
     * @param elem the <kdenlive_producer />
     * @param orig the potential <producer />
     * @param clipId the producer id
     * @return false if the user aborted the operation (in case the clip wasn't
     *     there yet), true otherwise
     *
     * If the clip wasn't added before, it tries to add it to the project. */
    bool addClipInfo(QDomElement elem, QDomElement orig, QString clipId);
    void slotAddClipFile(const KUrl url, const QString group, const QString &groupId = QString());
    void slotAddClipList(const KUrl::List urls, const QString group, const QString &groupId = QString());
    void deleteClip(const QString &clipId);
    int getFramePos(QString duration);
    DocClipBase *getBaseClip(const QString &clipId);
    void updateClip(const QString id);

    /** @brief Informs Kdenlive of the audio thumbnails generation progress. */
    void setThumbsProgress(const QString &message, int progress);
    const QString &profilePath() const;
    MltVideoProfile mltProfile() const;
    const QString description() const;
    void setUrl(KUrl url);

    /** @brief Updates the project profile.
     * @return true if frame rate was changed */
    bool setProfilePath(QString path);
    const QString getFreeClipId();

    /** @brief Defines whether the document needs to be saved. */
    bool isModified() const;

    /** @brief Returns the project folder, used to store project files. */
    KUrl projectFolder() const;
    void syncGuides(QList <Guide *> guides);
    void setZoom(int horizontal, int vertical);
    QPoint zoom() const;
    double dar() const;
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
    int setSceneList();
    void updatePreviewSettings();
    bool isTrackLocked(int ix) const;
    void setDocumentProperty(const QString &name, const QString &value);
    const QString getDocumentProperty(const QString &name) const;

    /** @brief Gets the list of renderer properties saved into the document. */
    QMap <QString, QString> getRenderProperties() const;
    void addTrackEffect(int ix, QDomElement effect);
    void removeTrackEffect(int ix, QDomElement effect);
    void setTrackEffect(int trackIndex, int effectIndex, QDomElement effect);
    const EffectsList getTrackEffects(int ix);
    QDomElement getTrackEffect(int trackIndex, int effectIndex) const;

private:
    KUrl m_url;
    QDomDocument m_document;
    double m_fps;
    int m_width;
    int m_height;
    Timecode m_timecode;
    Render *m_render;
    KTextEdit *m_notesWidget;
    QUndoStack *m_commandStack;
    ClipManager *m_clipManager;
    MltVideoProfile m_profile;
    QTimer *m_autoSaveTimer;
    QString m_searchFolder;

    /** @brief Tells whether the current document has been changed after being saved. */
    bool m_modified;

    /** @brief The project folder, used to store project files (titles, effects...). */
    KUrl m_projectFolder;
    QMap <QString, QString> m_documentProperties;

    QList <TrackInfo> m_tracksList;
    void setNewClipResource(const QString &id, const QString &path);
    QString searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash) const;
    void moveProjectData(KUrl url);
    bool checkDocumentClips(QDomNodeList infoproducers);

    /** @brief Creates a new project. */
    QDomDocument createEmptyDocument(int videotracks, int audiotracks);
    QDomDocument createEmptyDocument(QList <TrackInfo> tracks);
    /** @brief Saves effects embedded in project file.
    *   @return True if effects were imported.  */
    bool saveCustomEffects(QDomNodeList customeffects);

public slots:
    void slotCreateXmlClip(const QString &name, const QDomElement xml, QString group, const QString &groupId);
    void slotCreateColorClip(const QString &name, const QString &color, const QString &duration, QString group, const QString &groupId);
    void slotCreateSlideshowClipFile(const QString name, const QString path,
                                     int count, const QString duration,
                                     const bool loop, const bool crop,
                                     const bool fade, const QString &luma_duration,
                                     const QString &luma_file, const int softness,
                                     const QString &animation, QString group,
                                     const QString &groupId);
    void slotCreateTextClip(QString group, const QString &groupId, const QString &templatePath = QString());
    void slotCreateTextTemplateClip(QString group, const QString &groupId, KUrl path);

    /** @brief Sets the document as modified or up to date.
     * @param mod (optional) true if the document has to be saved */
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

    /** @brief Informs that the document status has been changed.
     *
     * If the document has been modified, it's called with true as an argument. */
    void docModified(bool);
    void selectLastAddedClip(const QString &);
    void guidesUpdated();
};

#endif
