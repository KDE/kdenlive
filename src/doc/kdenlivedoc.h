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

/*! \class KdenliveDoc
    \brief Represents a kdenlive project file

   Instances of KdeliveDoc classes are created by void MainWindow::newFile(bool showProjectSettings, bool force)
*/
#ifndef KDENLIVEDOC_H
#define KDENLIVEDOC_H

#include <QtXml/qdom.h>
#include <QMap>
#include <QList>
#include <QDir>
#include <QObject>

#include <QUrl>
#include <kautosavefile.h>

#include "gentime.h"
#include "timecode.h"
#include "definitions.h"
#include "timeline/guide.h"

class Render;
class ClipManager;
class DocClipBase;
class MainWindow;
class TrackInfo;
class NotesPlugin;
class ProjectClip;
class ClipController;

class QTextEdit;
class QProgressDialog;
class QUndoGroup;
class QTimer;
class QUndoStack;

class KdenliveDoc: public QObject
{
    Q_OBJECT
public:

    KdenliveDoc(const QUrl &url, const QUrl &projectFolder, QUndoGroup *undoGroup, const QString &profileName, const QMap <QString, QString>& properties, const QMap <QString, QString>& metadata, const QPoint &tracks, Render *render, NotesPlugin *notes, bool *openBackup, MainWindow *parent = 0, QProgressDialog *progressDialog = 0);
    ~KdenliveDoc();
    QDomNodeList producersList();
    double fps() const;
    int width() const;
    int height() const;
    QUrl url() const;
    KAutoSaveFile *m_autosave;
    Timecode timecode() const;
    QDomDocument toXml();
    //void setRenderer(Render *render);
    QUndoStack *commandStack();
    Render *renderer();
    QDomDocument m_guidesXml;
    QDomElement guidesXml() const;
    ClipManager *clipManager();

    /** @brief Adds a clip to the project tree.
     * @return false if the user aborted the operation, true otherwise */
    bool addClip(QDomElement elem, const QString &clipId, bool createClipItem = true);

    /** @brief Updates information about a clip.
     * @param elem the <kdenlive_producer />
     * @param orig the potential <producer />
     * @param clipId the producer id
     * @return false if the user aborted the operation (in case the clip wasn't
     *     there yet), true otherwise
     *
     * If the clip wasn't added before, it tries to add it to the project. */
    bool addClipInfo(QDomElement elem, QDomElement orig, const QString &clipId);
    void deleteClip(const QString &clipId);
    int getFramePos(const QString &duration);
    ProjectClip *getBinClip(const QString &clipId);
    ClipController *getClipController(const QString &clipId);
    DocClipBase *getBaseClip(const QString &clipId);
    void updateClip(const QString &id);

    /** @brief Informs Kdenlive of the audio thumbnails generation progress. */
    void setThumbsProgress(const QString &message, int progress);
    const QString &profilePath() const;
    MltVideoProfile mltProfile() const;
    const QString description() const;
    void setUrl(const QUrl &url);

    /** @brief Updates the project profile.
     * @return true if frame rate was changed */
    bool setProfilePath(QString path);

    /** @brief Defines whether the document needs to be saved. */
    bool isModified() const;

    /** @brief Returns the project folder, used to store project files. */
    QUrl projectFolder() const;
    void syncGuides(const QList <Guide *> &guides);
    void setZoom(int horizontal, int vertical);
    QPoint zoom() const;
    double dar() const;
    double projectDuration() const;
    /** @brief Returns the project file xml. */
    QDomDocument xmlSceneList(const QString &scene, const QStringList &expandedFolders);
    /** @brief Saves the project file xml to a file. */
    bool saveSceneList(const QString &path, const QString &scene, const QStringList &expandedFolders, bool autosave = false);
    int tracksCount() const;
    TrackInfo trackInfoAt(int ix) const;
    void insertTrack(int ix, const TrackInfo &type);
    void deleteTrack(int ix);
    void setTrackType(int ix, const TrackInfo &type);
    const QList <TrackInfo> tracksList() const;

    /** @brief Gets the number of audio and video tracks and returns them as a QPoint with x = video, y = audio. */
    QPoint getTracksCount() const;

    void switchTrackVideo(int ix, bool hide);
    void switchTrackAudio(int ix, bool hide);
    void switchTrackLock(int ix, bool lock);
    bool isTrackLocked(int ix) const;

    /** @brief Sets the duration of track @param ix to @param duration.
     * This does not! influence the actual track but only the value in its TrackInfo. */
    void setTrackDuration(int ix, int duration);

    /** @brief Returns the duration of track @param ix.
     *
     * The returned duration might differ from the actual track duration!
     * It is the one stored in the track's TrackInfo. */
    int trackDuration(int ix);
    void cacheImage(const QString &fileId, const QImage &img) const;
    void setProjectFolder(QUrl url);
    void setZone(int start, int end);
    QPoint zone() const;
    int setSceneList();
    void setDocumentProperty(const QString &name, const QString &value);
    const QString getDocumentProperty(const QString &name) const;
    void addClipList(const QList<QUrl> &urls, const QMap<QString, QString> &data = QMap<QString, QString>());

    /** @brief Gets the list of renderer properties saved into the document. */
    QMap <QString, QString> getRenderProperties() const;
    void addTrackEffect(int ix, QDomElement effect);
    void removeTrackEffect(int ix, const QDomElement &effect);
    void setTrackEffect(int trackIndex, int effectIndex, QDomElement effect);
    const EffectsList getTrackEffects(int ix);
    /** @brief Enable / disable an effect in Kdenlive's xml list. */
    void enableTrackEffects(int trackIndex, const QList<int> &effectIndexes, bool disable);
    QDomElement getTrackEffect(int trackIndex, int effectIndex) const;
    /** @brief Check if a track already contains a specific effect. */
    int hasTrackEffect(int trackIndex, const QString &tag, const QString &id) const;
    /** @brief Get a list of folder id's that were opened on last save. */
    QStringList getExpandedFolders();
    /** @brief Read the display ratio from an xml project file. */
    static double getDisplayRatio(const QString &path);
    /** @brief Backup the project file */
    void backupLastSavedVersion(const QString &path);
    /** @brief Returns the document metadata (author, copyright, ...) */
    const QMap <QString, QString> metadata() const;
    /** @brief Set the document metadata (author, copyright, ...) */
    void setMetadata(const QMap <QString, QString>& meta);
    void slotUpdateClipProperties(const QString &id, QMap <QString, QString> properties);
    /** @brief Get frame size of the renderer */
    const QSize getRenderSize();
    
private:
    QUrl m_url;
    QDomDocument m_document;
    double m_fps;
    int m_width;
    int m_height;
    Timecode m_timecode;
    Render *m_render;
    QTextEdit *m_notesWidget;
    QUndoStack *m_commandStack;
    ClipManager *m_clipManager;
    MltVideoProfile m_profile;
    QTimer *m_autoSaveTimer;
    QString m_searchFolder;

    /** @brief Tells whether the current document has been changed after being saved. */
    bool m_modified;

    /** @brief The project folder, used to store project files (titles, effects...). */
    QUrl m_projectFolder;
    QMap <QString, QString> m_documentProperties;
    QMap <QString, QString> m_documentMetadata;

    QList <TrackInfo> m_tracksList;
    void setNewClipResource(const QString &id, const QString &path);
    QString searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash) const;
    void moveProjectData(const QUrl &url);
    /**
     * @brief check for issues with the clips in the project
     * Instansiates DocumentChecker objects to do this task.
     * @param infoproducers
     * @return
     */
    bool checkDocumentClips(QDomNodeList infoproducers);

    /** @brief Creates a new project. */
    QDomDocument createEmptyDocument(int videotracks, int audiotracks);
    QDomDocument createEmptyDocument(const QList<TrackInfo> &tracks);
    /** @brief Saves effects embedded in project file.
    *   @return True if effects were imported.  */
    bool saveCustomEffects(const QDomNodeList &customeffects);

    /** @brief Updates the project folder location entry in the kdenlive file dialogs to point to the current project folder. */
    void updateProjectFolderPlacesEntry();
    /** @brief Only keep some backup files, delete some */
    void cleanupBackupFiles();

public slots:
    void slotCreateXmlClip(const QString &name, const QDomElement &xml, const QString &group, const QString &groupId);
    void slotCreateColorClip(const QString &name, const QString &color, const QString &duration, const QString &group, const QString &groupId);
    void slotCreateSlideshowClipFile(const QMap<QString, QString> &properties, const QString &group, const QString &groupId);
    /**
     * @brief Create a title clip.
     *  Instansiates TitleWidget objects
     * @param group
     * @param groupId
     * @param templatePath
     */
    void slotCreateTextClip(QString group, const QString &groupId, const QString &templatePath = QString());
    void slotCreateTextTemplateClip(const QString &group, const QString &groupId, QUrl path);

    /** @brief Sets the document as modified or up to date.
     * @description  If crash recovery is turned on, a timer calls KdenliveDoc::slotAutoSave() \n
     * Emits docModified conected to MainWindow::slotUpdateDocumentState \n
     * @param mod (optional) true if the document has to be saved */
    void setModified(bool mod = true);
    void checkProjectClips(bool displayRatioChanged = false, bool fpsChanged = false);
    void slotProxyCurrentItem(bool doProxy);

private slots:
    /** @brief Saves the current project at the autosave location.
     * @description The autosave files are in ~/.kde/data/stalefiles/kdenlive/ */
    void slotAutoSave();

signals:
    void resetProjectList();
    void addProjectClip(DocClipBase *, bool getInfo = true);
    void signalDeleteProjectClip(const QString &);
    void updateClipDisplay(const QString&);
    void progressInfo(const QString &, int);

    /** @brief Informs that the document status has been changed.
     *
     * If the document has been modified, it's called with true as an argument. */
    void docModified(bool);
    void selectLastAddedClip(const QString &);
    void guidesUpdated();
    /** @brief When creating a backup file, also save a thumbnail of current timeline */
    void saveTimelinePreview(const QString &path);
};

#endif

