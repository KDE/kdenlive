/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once
#include "kdenlivesettings.h"

#include <KMessageWidget>

#include <QFutureWatcher>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QString>

class AbstractPythonInterface : public QObject
{
    Q_OBJECT
public:
    struct PythonExec
    {
        QString python;
        QString pip;
    };
    enum InstallStatus { Unknown, NotInstalled, Installed, InProgress, MissingDependencies, Broken };

    explicit AbstractPythonInterface(QObject *parent = nullptr);
    /** @brief Check if python and pip are installed, as well as all required scripts.
        If a check failed setupError() will be emitted with an error message that can be
        shown to the user.
        @returns whether all checks succeeded.
    */
    ~AbstractPythonInterface() override;
    /** @brief Check if the Python venv is setup correctly, if not create it if requested.
     *  @returns true if the venv is setup properly or was created successfully, otherwise false
     */
    bool checkVenv(bool calculateSize = false, bool forceInstall = false);
    /** @brief Check which versions of the dependencies are installed.
        @param Whether checkVersionsResult() will be emitted once the result is available.
    */
    void checkVersions(bool signalOnResult = true);
    void calculateVenvSize();
    void updateDependencies();
    /** @brief Returns a cached list of all missing dependencies
     *  To update the cache run checkDependencies().
     *  @param filter If this is empty all missing packages will be returned,
     *         otherwise only those of the filter (in case they are missing).
     */
    QStringList missingDependencies(const QStringList &filter = {});
    /** @brief Install an additional requirements file. */
    virtual bool installRequirements(const QString reqFile);
    QString runScript(const QString &script, QStringList args = {}, const QString &firstarg = {}, bool concurrent = false, bool packageFeedback = false);
    virtual PythonExec venvPythonExecs(bool checkPip = false);
    virtual bool useSystemPython();
    QString systemPythonExec();
    void proposeMaybeUpdate(const QString &dependency, const QString &minVersion);
    void runConcurrentScript(const QString &script, QStringList args, bool feedback = false);
    /** @brief Python venv setup in progress. */
    bool installInProcess() const;
    /** @brief Returns true if the optional dependency was found. */
    bool optionalDependencyAvailable(const QString &dependency) const;
    /** @brief The text that will appear on the install button when a dependency is missing. */
    virtual const QString installMessage() const;
    /** @brief The path to the binary location for this virtual environment. */
    const QString getVenvBinPath();
    /** @brief The virtual environments dir name. */
    virtual const QString getVenvPath();
    /** @brief Add a special dependency. */
    void addDependency(const QString &pipname, const QString &purpose, bool optional = false);
    /** @brief Get a script path ba name. */
    const QString getScript(const QString &scriptName) const;
    /** @brief Delete the virtual environment. */
    void deleteVenv();
    /** @brief User readable list of dependencies. */
    const QStringList listDependencies();
    void setStatus(InstallStatus status);
    InstallStatus status() const;
    virtual QString featureName() { return {}; };

    friend class PythonDependencyMessage;

public Q_SLOTS:
    /** @brief Check if all dependencies are installed.
        If everything is okay dependenciesAvailable() will be emitted,
        otherwise dependenciesMissing() with a message that can be shown
        to the user telling which dependencies are missing.
        To get a list of all missing dependencies use missingDependencies
        @returns whether all checks succeeded.
    */
    bool checkDependencies(bool force = false, bool async = true);
    void checkDependenciesConcurrently();
    void checkVersionsConcurrently();
    /** @brief Ensure all dependenciew are installed. */
    bool installMissingDependencies();
    bool checkSetup(bool requestInstall = false, bool *newInstall = nullptr);
    /** @brief Try to update the venv if something is broken */
    void rebuildVenv();

private:
    QStringList m_missing;
    QStringList m_optionalMissing;
    QMap<QString, QString> m_versions;
    bool m_dependenciesChecked{false};
    QMutex m_versionsMutex;
    QFutureWatcher<void> m_watcher;
    QFutureWatcher<void> m_depsWatcher;
    QFutureWatcher<void> m_versionWatcher;
    QFuture<void> m_depsJob;
    QFuture<void> m_versionJob;
    QFuture<void> m_scriptJob;
    const QString locateScript(const QString &script);
    QString runPackageScript(QString mode, bool concurrent = false, bool displayFeedback = true, bool forceInstall = false);
    int versionToInt(const QString &version);
    /** @brief Create a python virtualenv */
    bool setupVenv();
    QString installPackage(const QStringList packageNames);
    QStringList parseDependencies(QStringList deps, bool split);

protected:
    QMap<QString, QString> m_dependencies;
    QStringList m_optionalDeps;
    QMap<QString, QString> m_scripts;
    void addScript(const QString &script);
    InstallStatus m_installStatus{Unknown};

Q_SIGNALS:
    void setupError(const QString &message);
    void setupMessage(const QString &message, KMessageWidget::MessageType messageType = KMessageWidget::Information);
    void checkVersionsResult(const QStringList &versions);
    void dependenciesMissing(const QStringList &messages);
    void dependenciesAvailable();
    void proposeUpdate(const QString &message);
    void scriptFeedback(const QString &script, const QStringList args, const QStringList message);
    void installFeedback(const QString &message);
    void gotPythonSize(const QString &message);
    void concurrentScriptFinished(const QString &script, const QStringList &args);
    void scriptFinished(const QStringList &args);
    void scriptStarted();
    void abortScript();
    void venvSetupChanged();
    void installStatusChanged();
};

class PythonDependencyMessage : public KMessageWidget {
    Q_OBJECT

public:
    PythonDependencyMessage(QWidget *parent, AbstractPythonInterface *interface, bool setupErrorOnly = false);

public Q_SLOTS:
    void checkAfterInstall();
    void doShowMessage(const QString &message, KMessageWidget::MessageType messageType = KMessageWidget::Information);

private:
    AbstractPythonInterface * m_interface;
    QAction *m_installAction{nullptr};
    QAction *m_abortAction{nullptr};
    bool m_updated{false};
};
