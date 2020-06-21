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

   Instances of KdenliveDoc classes are created by void MainWindow::newFile(bool showProjectSettings, bool force)
*/
#ifndef KDENLIVEDOC_H
#define KDENLIVEDOC_H

#include <QAction>
#include <QDir>
#include <QList>
#include <QMap>
#include <memory>
#include <qdom.h>

#include <kautosavefile.h>

#include "definitions.h"
#include "gentime.h"
#include "timecode.h"

class MainWindow;
class TrackInfo;
class ProjectClip;
class MarkerListModel;
class Render;
class ProfileParam;

class QUndoGroup;
class QUndoCommand;
class DocUndoStack;

namespace Mlt {
class Profile;
}

class KdenliveDoc : public QObject
{
    Q_OBJECT
public:
    KdenliveDoc(const QUrl &url, QString projectFolder, QUndoGroup *undoGroup, const QString &profileName, const QMap<QString, QString> &properties,
                const QMap<QString, QString> &metadata, const QPair<int, int> &tracks, int audioChannels, bool *openBackup, MainWindow *parent = nullptr);
    ~KdenliveDoc() override;
    friend class LoadJob;
    /** @brief Get current document's producer. */
    const QByteArray getAndClearProjectXml();
    double fps() const;
    int width() const;
    int height() const;
    QUrl url() const;
    KAutoSaveFile *m_autosave;
    Timecode timecode() const;
    std::shared_ptr<DocUndoStack> commandStack();

    int getFramePos(const QString &duration);
    /** @brief Get a list of all clip ids that are inside a folder. */
    QStringList getBinFolderClipIds(const QString &folderId) const;

    const QString description() const;
    void setUrl(const QUrl &url);

    /** @brief Defines whether the document needs to be saved. */
    bool isModified() const;

    /** @brief Returns the project folder, used to store project temporary files. */
    QString projectTempFolder() const;
    /** @brief Returns the folder used to store project data files (titles, etc). */
    QString projectDataFolder() const;
    void setZoom(int horizontal, int vertical = -1);
    QPoint zoom() const;
    double dar() const;
    /** @brief Returns the project file xml. */
    QDomDocument xmlSceneList(const QString &scene);
    /** @brief Saves the project file xml to a file. */
    bool saveSceneList(const QString &path, const QString &scene);
    /** @brief Saves only the MLT xml to a file for preview rendering. */
    void saveMltPlaylist(const QString &fileName);
    void cacheImage(const QString &fileId, const QImage &img) const;
    void setProjectFolder(const QUrl &url);
    void setZone(int start, int end);
    QPoint zone() const;
    /** @brief Returns target tracks (video, audio). */
    QPair<int, int> targetTracks() const;
    /** @brief Load document guides from properties. */
    void loadDocumentGuides();
    void setDocumentProperty(const QString &name, const QString &value);
    virtual const QString getDocumentProperty(const QString &name, const QString &defaultValue = QString()) const;

    /** @brief Gets the list of renderer properties saved into the document. */
    QMap<QString, QString> getRenderProperties() const;
    /** @brief Read the display ratio from an xml project file. */
    static double getDisplayRatio(const QString &path);
    /** @brief Backup the project file */
    void backupLastSavedVersion(const QString &path);
    /** @brief Returns the document metadata (author, copyright, ...) */
    const QMap<QString, QString> metadata() const;
    /** @brief Set the document metadata (author, copyright, ...) */
    void setMetadata(const QMap<QString, QString> &meta);
    /** @brief Get all document properties that need to be saved */
    QMap<QString, QString> documentProperties();
    bool useProxy() const;
    bool useExternalProxy() const;
    bool autoGenerateProxy(int width) const;
    bool autoGenerateImageProxy(int width) const;
    /** @brief Saves effects embedded in project file. */
    void saveCustomEffects(const QDomNodeList &customeffects);
    void resetProfile(bool reloadThumbs);
    /** @brief Returns true if the profile file has changed. */
    bool profileChanged(const QString &profile) const;
    /** @brief Get an action from main actioncollection. */
    QAction *getAction(const QString &name);
    /** @brief Add an action to main actioncollection. */
    void doAddAction(const QString &name, QAction *a, const QKeySequence &shortcut);
    void invalidatePreviews(QList<int> chunks);
    void previewProgress(int p);
    /** @brief Select most appropriate rendering profile for timeline preview based on fps / size. */
    void selectPreviewProfile();
    void displayMessage(const QString &text, MessageType type = DefaultMessage, int timeOut = 0);
    /** @brief Get a cache directory for this project. */
    QDir getCacheDir(CacheType type, bool *ok) const;
    /** @brief Create standard cache dirs for the project */
    void initCacheDirs();
    /** @brief Get a list of all proxy hash used in this project */
    QStringList getProxyHashList();
    /** @brief Move project data files to new url */
    void moveProjectData(const QString &src, const QString &dest);

    /** @brief Returns a pointer to the guide model */
    std::shared_ptr<MarkerListModel> getGuideModel() const;

    // TODO REFAC: delete */
    Render *renderer();
    /** @brief Returns MLT's root (base path) */
    const QString documentRoot() const;
    /** @brief Returns true if timeline preview settings changed*/
    bool updatePreviewSettings(const QString &profile);
    /** @brief Returns the recommended proxy profile parameters */
    QString getAutoProxyProfile();
    /** @brief Returns the number of clips in this project (useful to show loading progress) */
    int clipsCount() const;
    /** @brief Returns a list of project tags (color / description) */
    QMap <QString, QString> getProjectTags();
    /** @brief Returns the number of audio channels for this project */
    int audioChannels() const;

    /**
     * If the document used a decimal point different than “.”, it is stored in this property.
     * @return Original decimal point, or an empty string if it was “.” already
     */
    QString &modifiedDecimalPoint();

private:
    QUrl m_url;
    QDomDocument m_document;
    int m_clipsCount;
    /** @brief MLT's root (base path) that is stripped from urls in saved xml */
    QString m_documentRoot;
    Timecode m_timecode;
    std::shared_ptr<DocUndoStack> m_commandStack;
    QString m_searchFolder;

    /** @brief Tells whether the current document has been changed after being saved. */
    bool m_modified;

    /** @brief The default recommended proxy extension */
    QString m_proxyExtension;

    /** @brief The default recommended proxy params */
    QString m_proxyParams;

    /** @brief Tells whether the current document was modified by Kdenlive on opening, and a backup should be created on save. */
    enum DOCSTATUS { CleanProject, ModifiedProject, UpgradedProject };
    DOCSTATUS m_documentOpenStatus;

    /** @brief The project folder, used to store project files (titles, effects...). */
    QString m_projectFolder;
    QList<int> m_undoChunks;
    QMap<QString, QString> m_documentProperties;
    QMap<QString, QString> m_documentMetadata;
    std::shared_ptr<MarkerListModel> m_guideModel;

    QString m_modifiedDecimalPoint;

    QString searchFileRecursively(const QDir &dir, const QString &matchSize, const QString &matchHash) const;

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
    void updateProjectProfile(bool reloadProducers = false, bool reloadThumbs = false);
    /** @brief initialize proxy settings based on hw status */
    void initProxySettings();

public slots:
    void slotCreateTextTemplateClip(const QString &group, const QString &groupId, QUrl path);

    /** @brief Sets the document as modified or up to date.
     * @description  If crash recovery is turned on, a timer calls KdenliveDoc::slotAutoSave() \n
     * Emits docModified connected to MainWindow::slotUpdateDocumentState \n
     * @param mod (optional) true if the document has to be saved */
    void setModified(bool mod = true);
    void slotProxyCurrentItem(bool doProxy, QList<std::shared_ptr<ProjectClip>> clipList = QList<std::shared_ptr<ProjectClip>>(), bool force = false,
                              QUndoCommand *masterCommand = nullptr);
    /** @brief Saves the current project at the autosave location.
     * @description The autosave files are in ~/.kde/data/stalefiles/kdenlive/ */
    void slotAutoSave(const QString &scene);
    /** @brief Groups were changed, save to MLT. */
    void groupsChanged(const QString &groups);

private slots:
    void slotModified();
    void switchProfile(std::unique_ptr<ProfileParam> &profile, const QString &id, const QDomElement &xml);
    void slotSwitchProfile(const QString &profile_path, bool reloadThumbs);
    /** @brief Check if we did a new action invalidating more recent undo items. */
    void checkPreviewStack(int ix);
    /** @brief Guides were changed, save to MLT. */
    void guidesChanged();

signals:
    void resetProjectList();

    /** @brief Informs that the document status has been changed.
     *
     * If the document has been modified, it's called with true as an argument. */
    void docModified(bool);
    void selectLastAddedClip(const QString &);
    /** @brief When creating a backup file, also save a thumbnail of current timeline */
    void saveTimelinePreview(const QString &path);
    /** @brief Trigger the autosave timer start */
    void startAutoSave();
    /** @brief Current doc created effects, reload list */
    void reloadEffects(const QStringList &paths);
    /** @brief Fps was changed, update timeline (changed = 1 means no change) */
    void updateFps(double changed);
    /** @brief If a command is pushed when we are in the middle of undo stack, invalidate further undo history */
    void removeInvalidUndo(int ix);
    /** @brief Update compositing info */
    void updateCompositionMode(int);
};

#endif
