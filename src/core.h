/*
Copyright (C) 2014  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef CORE_H
#define CORE_H

#include "kdenlivecore_export.h"
#include "undohelper.hpp"
#include "definitions.h"
#include <QObject>
#include <QTabWidget>
#include <QUrl>
#include <memory>

class Bin;
class BinController;
class KdenliveDoc;
class LibraryWidget;
class MainWindow;
class MltConnection;
class MonitorManager;
class ProducerQueue;
class ProfileModel;
class ProjectManager;
class Monitor;

namespace Mlt {
class Repository;
}

#define EXIT_RESTART (42)
#define pCore Core::self()

/**
 * @class Core
 * @brief Singleton that provides access to the different parts of Kdenlive
 *
 * Needs to be initialize before any widgets are created in MainWindow.
 * Plugins should be loaded after the widget setup.
 */

class /*KDENLIVECORE_EXPORT*/ Core : public QObject
{
    Q_OBJECT

public:
    Core(const Core &) = delete;
    Core &operator=(const Core &) = delete;
    Core(Core &&) = delete;
    Core &operator=(Core &&) = delete;

    virtual ~Core();

    /**
     * @brief Setup the basics of the application, in particular the connection
     * with Mlt
     * @param MltPath (optional) path to MLT environment
     */
    static void build(const QString &MltPath = QString());

    /**
     * @brief Init the GUI part of the app and show the main window
     * @param Url (optional) file to open
     * If Url is present, it will be opened, otherwhise, if openlastproject is
     * set, latest project will be opened. If no file is open after trying this,
     * a default new file will be created. */
    void initGUI(const QUrl &Url);

    /** @brief Returns a pointer to the singleton object. */
    static std::unique_ptr<Core> &self();

    /** @brief Returns a pointer to the main window. */
    MainWindow *window();

    /** @brief Returns a pointer to the project manager. */
    ProjectManager *projectManager();
    /** @brief Returns a pointer to the project manager. */
    KdenliveDoc *currentDoc();
    /** @brief Returns a pointer to the monitor manager. */
    MonitorManager *monitorManager();
    /** @brief Returns a pointer to the project bin controller. */
    std::shared_ptr<BinController> binController();
    /** @brief Returns a pointer to the project bin. */
    Bin *bin();
    /** @brief Returns a pointer to the producer queue. */
    ProducerQueue *producerQueue();
    /** @brief Returns a pointer to the library. */
    LibraryWidget *library();

    /** @brief Returns a pointer to MLT's repository */
    std::unique_ptr<Mlt::Repository> &getMltRepository();

    /** @brief Returns a pointer to the current profile */
    std::unique_ptr<ProfileModel> &getCurrentProfile() const;
    /** @brief Define the active profile 
     *  @returns true if profile exists, false if not found
     */
    bool setCurrentProfile(const QString &profilePath);
    /** @brief Returns Sample Aspect Ratio of current profile */
    double getCurrentSar() const;
    /** @brief Returns Display Aspect Ratio of current profile */
    double getCurrentDar() const;

    /** @brief Returns frame rate of current profile */
    double getCurrentFps() const;

    /** @brief Returns the frame size (width x height) of current profile */
    QSize getCurrentFrameSize() const;
    /** @brief Returns the frame display size (width x height) of current profile */
    QSize getCurrentFrameDisplaySize() const;
    /** @brief Request project monitor refresh */
    void requestMonitorRefresh();
    /** @brief Request project monitor refresh if current position is inside range*/
    void refreshProjectRange(QSize range);
    /** @brief Request project monitor refresh if referenced item is under cursor */
    void refreshProjectItem(int itemId);
    /** @brief Returns a reference to a monitor (clip or project monitor) */
    Monitor *getMonitor(int id);

    /** @brief This function must be called whenever the profile used changes */
    void profileChanged();

    /** @brief Create and push and undo object based on the corresponding functions
        Note that if you class permits and requires it, you should use the macro PUSH_UNDO instead*/
    void pushUndo(const Fun &undo, const Fun &redo, const QString &text);
    void pushUndo(QUndoCommand *command);
    /** @brief display a user info/warning message in statusbar */
    void displayMessage(const QString &message, MessageType type, int timeout);

private:
    explicit Core();
    static std::unique_ptr<Core> m_self;

    /** @brief Makes sure Qt's locale and system locale settings match. */
    void initLocale();

    MainWindow *m_mainWindow;
    ProjectManager *m_projectManager;
    MonitorManager *m_monitorManager;
    std::shared_ptr<BinController> m_binController;
    ProducerQueue *m_producerQueue;
    Bin *m_binWidget;
    LibraryWidget *m_library;
    /** @brief Current project's profile path */
    QString m_currentProfile;

    std::unique_ptr<MltConnection> m_mltConnection;

    QString m_profile;

signals:
    void coreIsReady();
    void updateLibraryPath();
};

#endif
