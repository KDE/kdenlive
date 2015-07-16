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
#include <QTimer>
#include <QUrl>

#include <kautosavefile.h>
#include <KDirWatch>

#include "gentime.h"
#include "timecode.h"
#include "definitions.h"
#include "timeline/guide.h"
#include "mltcontroller/effectscontroller.h"

class Render;
class ClipManager;
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

namespace Mlt {
    class Profile;
}

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
    QUndoStack *commandStack();
    Render *renderer();
    ClipManager *clipManager();
    QString groupsXml() const;

    /** @brief Adds a clip to the project tree.
     * @return false if the user aborted the operation, true otherwise */
    bool addClip(QDomElement elem, const QString &clipId);

    /** @brief Updates information about a clip.
     * @param elem the <kdenlive_producer />
     * @param orig the potential <producer />
     * @param clipId the producer id
     * @return false if the user aborted the operation (in case the clip wasn't
     *     there yet), true otherwise
     *
     * If the clip wasn't added before, it tries to add it to the project. */
    //bool addClipInfo(QDomElement elem, QDomElement orig, const QString &clipId);
    void deleteClip(const QString &clipId);
    int getFramePos(const QString &duration);
    /** @brief Get a bin's clip from its id. */
    ProjectClip *getBinClip(const QString &clipId);
    /** @brief Get a list of all clip ids that are inside a folder. */
    QStringList getBinFolderClipIds(const QString &folderId) const;
    ClipController *getClipController(const QString &clipId);
    void updateClip(const QString &id);

    /** @brief Informs Kdenlive of the audio thumbnails generation progress. */
    void setThumbsProgress(const QString &message, int progress);
    const QString &profilePath() const;
    /** @brief Returns current project profile. */
    MltVideoProfile mltProfile() const;
    ProfileInfo getProfileInfo() const;
    //Mlt::Profile *profile();
    const QString description() const;
    void setUrl(const QUrl &url);

    /** @brief Updates the project profile.
     * @return true if frame rate was changed */
    bool setProfilePath(QString path);

    /** @brief Defines whether the document needs to be saved. */
    bool isModified() const;

    /** @brief Returns the project folder, used to store project files. */
    QUrl projectFolder() const;
    void setZoom(int horizontal, int vertical);
    QPoint zoom() const;
    double dar() const;
    double projectDuration() const;
    /** @brief Returns the project file xml. */
    QDomDocument xmlSceneList(const QString &scene);
    /** @brief Saves the project file xml to a file. */
    bool saveSceneList(const QString &path, const QString &scene);
    void cacheImage(const QString &fileId, const QImage &img) const;
    void setProjectFolder(QUrl url);
    void setZone(int start, int end);
    QPoint zone() const;
    int setSceneList();
    void setDocumentProperty(const QString &name, const QString &value);
    const QString getDocumentProperty(const QString &name) const;

    /** @brief Gets the list of renderer properties saved into the document. */
    QMap <QString, QString> getRenderProperties() const;
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
    void slotUpdateClipProperties(const QString &id, QMap <QString, QString> properties, bool refreshPropertiesPanel);
    /** @brief Get frame size of the renderer (profile)*/
    const QSize getRenderSize() const;
    /** @brief Add url to the file watcher so that we monitor changes */
    void watchFile(const QUrl &url);
    /** @brief Get all document properties that need to be saved */
    const QMap <QString, QString> documentProperties();
    bool useProxy() const;
    QString documentNotes() const;
    /** @brief Saves effects embedded in project file. */
    void saveCustomEffects(const QDomNodeList &customeffects);
    void resetProfile();

private:
    QUrl m_url;
    QDomDocument m_document;
    KDirWatch m_fileWatcher;
    /** Timer used to reload clips when they have been externally modified */
    QTimer m_modifiedTimer;
    /** List of the clip IDs that need to be reloaded after being externally modified */
    QMap <QString, QTime> m_modifiedClips;
    int m_width;
    int m_height;
    Timecode m_timecode;
    Render *m_render;
    QTextEdit *m_notesWidget;
    QUndoStack *m_commandStack;
    ClipManager *m_clipManager;
    MltVideoProfile m_profile;
    QString m_searchFolder;

    /** @brief Tells whether the current document has been changed after being saved. */
    bool m_modified;

    /** @brief The project folder, used to store project files (titles, effects...). */
    QUrl m_projectFolder;
    QMap <QString, QString> m_documentProperties;
    QMap <QString, QString> m_documentMetadata;

    void setNewClipResource(const QString &id, const QString &path);
    QString searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash) const;
    void moveProjectData(const QUrl &url);
    /**
     * @brief check for issues with the clips in the project
     * Instansiates DocumentChecker objects to do this task.
     * @param infoproducers
     * @return
     */
    bool checkDocumentClips();

    /** @brief Creates a new project. */
    QDomDocument createEmptyDocument(int videotracks, int audiotracks);
    QDomDocument createEmptyDocument(const QList<TrackInfo> &tracks);

    /** @brief Updates the project folder location entry in the kdenlive file dialogs to point to the current project folder. */
    void updateProjectFolderPlacesEntry();
    /** @brief Only keep some backup files, delete some */
    void cleanupBackupFiles();
    /** @brief Load document properties from the xml file */
    void loadDocumentProperties();
    /** @brief update document properties to reflect a change in the current profile */
    void updateProjectProfile();

public slots:
    void slotCreateXmlClip(const QString &name, const QDomElement &xml, const QString &group, const QString &groupId);
    void slotCreateTextTemplateClip(const QString &group, const QString &groupId, QUrl path);

    /** @brief Sets the document as modified or up to date.
     * @description  If crash recovery is turned on, a timer calls KdenliveDoc::slotAutoSave() \n
     * Emits docModified conected to MainWindow::slotUpdateDocumentState \n
     * @param mod (optional) true if the document has to be saved */
    void setModified(bool mod = true);
    void checkProjectClips(bool displayRatioChanged = false, bool fpsChanged = false);
    void slotProxyCurrentItem(bool doProxy);
    /** @brief Saves the current project at the autosave location.
     * @description The autosave files are in ~/.kde/data/stalefiles/kdenlive/ */
    void slotAutoSave();

private slots:
    void slotClipModified(const QString &path);
    void slotClipMissing(const QString &path);
    void slotClipAvailable(const QString &path);
    void slotProcessModifiedClips();
    void slotModified();
    void slotSetDocumentNotes(const QString &notes);

signals:
    void resetProjectList();
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
    /** @brief Trigger the autosave timer start */
    void startAutoSave();
    /** @brief Current doc created effects, reload list */
    void reloadEffects();
};

#endif

