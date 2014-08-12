/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QObject>
#include <KUrl>
#include "kdenlivecore_export.h"

class Project;
class AbstractProjectPart;
class TrackView;
class KdenliveDoc;
class KAction;
class KUrl;
class KAutoSaveFile;
class KRecentFilesAction;


/**
 * @class ProjectManager
 * @brief Takes care of interaction with projects.
 */


class KDENLIVECORE_EXPORT ProjectManager : public QObject
{
    Q_OBJECT

public:
    /** @brief Sets up actions to interact for project interaction (undo, redo, open, save, ...) and creates an empty project. */
    explicit ProjectManager(QObject* parent = 0);
    virtual ~ProjectManager();

    /** @brief Returns a pointer to the currently opened project. A project should always be open. */
    KdenliveDoc *current();
    TrackView *currentTrackView();

    /** @brief Opens or creates a file.
     * Command line argument passed in Url has
     * precedence, then "openlastproject", then just a plain empty file.
     * If opening Url fails, openlastproject will _not_ be used.
     */
    void init(const KUrl &projectUrl, const QString &clipList);

    void doOpenFile(const KUrl &url, KAutoSaveFile *stale);
    void recoverFiles(const QList<KAutoSaveFile *> &staleFiles, const KUrl &originUrl);

    KRecentFilesAction *recentFilesAction();

public slots:
    void newFile(bool showProjectSettings = true, bool force = false);
    /** @brief Shows file open dialog. */
    void openFile();
    void openLastFile();

    /** @brief Checks whether a URL is available to save to.
    * @return Whether the file was saved. */
    bool saveFile();

    /** @brief Shows a save file dialog for saving the project.
    * @return Whether the file was saved. */
    bool saveFileAs();

    /** @brief Set properties to match outputFileName and save the document.
    * @param outputFileName The URL to save to / The document's URL.
    * @return Whether we had success. */
    bool saveFileAs(const QString &outputFileName);
    /** @brief Close currently opened document. Returns false if something went wrong (cannot save modifications, ...). */
    bool closeCurrentDocument(bool saveChanges = true);

    /** @brief Prepares opening @param url.
    *
    * Checks if already open and whether backup exists */
    void openFile(const KUrl &url);

private slots:
    void slotRevert();
    /** @brief Open the project's backupdialog. */
    void slotOpenBackup(const KUrl &url = KUrl());

signals:
//     void projectOpened(Project *project);

private:
    void connectDocumentInfo(KdenliveDoc *doc);
    /** @brief Checks that the Kdenlive mime type is correctly installed.
    * @param open If set to true, this will return the mimetype allowed for file opening (adds .tar.gz format)
    * @return The mimetype */
    QString getMimeType(bool open = true);

    KdenliveDoc *m_project;
    TrackView *m_trackView;

    KAction *m_fileRevert;
    KUrl m_startUrl;
    KRecentFilesAction *m_recentFilesAction;
};

#endif
