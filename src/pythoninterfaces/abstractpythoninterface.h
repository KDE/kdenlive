/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once
#include "kdenlivesettings.h"

#include <KMessageWidget>
#include <QObject>
#include <QString>
#include <QMap>
#include <QPair>

class KJob;

class AbstractPythonInterface : public QObject
{
    Q_OBJECT
public:
    explicit AbstractPythonInterface(QObject *parent = nullptr);
    /** @brief Check if python and pip are installed, as well as all required scripts.
        If a check failed setupError() will be emitted with an error message that can be
        shown to the user.
        @returns wether all checks succeeded.
    */
    ~AbstractPythonInterface() override;
    /** @brief Check if python is found and use venv if requested. */
    bool checkPython(bool useVenv, bool calculateSize = false, bool forceInstall = false);
    bool checkSetup();
    /** @brief Check which versions of the dependencies are installed.
        @param Whether checkVersionsResult() will be emitted once the result is available.
    */
    void checkVersions(bool signalOnResult = true);
    void updateDependencies();
    /** @brief Returns a cached list of all missing dependencies
     *  To update the cache run checkDependencies().
     *  @param filter If this is empty all missing packages will be returned,
     *         otherwise only those of the filter (in case they are missing).
     */
    QStringList missingDependencies(const QStringList &filter = {});
    QString runScript(const QString &scriptpath, QStringList args = {}, const QString &firstarg = {}, bool concurrent = false, bool packageFeedback = false);
    QString pythonExec() { return KdenliveSettings::pythonPath(); };
    void proposeMaybeUpdate(const QString &dependency, const QString &minVersion);
    bool installDisabled() { return m_disableInstall; };
    void runConcurrentScript(const QString &script, QStringList args);
    /** @brief Delete the python venv folder. */
    bool removePythonVenv();
    /** @brief Python venv setup in progress. */
    bool installInProcess() const;
    /** @brief Returns true if the optional dependency was found. */
    bool optionalDependencyAvailable(const QString &dependency) const;

    friend class PythonDependencyMessage;

public Q_SLOTS:
    /** @brief Check if all dependencies are installed.
        If everything is okay dependenciesAvailable() will be emitted,
        otherwise dependenciesMissing() with a message that can be shown
        to the user telling wich dependencies are missing.
        To get a list of all missing dependencies use missingDependencies
        @returns wether all checks succeeded.
    */
    void checkDependencies(bool force = false, bool async = true);
    void checkDependenciesConcurrently();
    void checkVersionsConcurrently();

private:
    QStringList m_missing;
    QStringList m_optionalMissing;
    QMap<QString, QString> *m_versions;
    bool m_disableInstall;
    bool m_dependenciesChecked;

    void installMissingDependencies();
    QString locateScript(const QString &script);
    QString runPackageScript(const QString &mode, bool concurrent = false);
    int versionToInt(const QString &version);
    /** @brief Create a python virtualenv */
    bool setupVenv();
    void gotFolderSize(KJob *job);
    QString installPackage(const QStringList packageNames);

protected:
    QMap<QString, QString> m_dependencies;
    QStringList m_optionalDeps;
    QMap<QString, QString> *m_scripts;
    void addDependency(const QString &pipname, const QString &purpose, bool optional = false);
    void addScript(const QString &script);
    virtual QString featureName() { return {}; };
    bool m_installInProgress{false};

Q_SIGNALS:
    void setupError(const QString &message);
    void setupMessage(const QString &message, int messageType);
    void checkVersionsResult(const QStringList &versions);
    void dependenciesMissing(const QStringList &messages);
    void dependenciesAvailable();
    void proposeUpdate(const QString &message);
    void scriptFeedback(const QStringList message);
    void installFeedback(const QString &message);
    void gotPythonSize(const QString &message);
    void scriptGpuCheckFinished();
    void scriptFinished();
    void scriptStarted();
    void abortScript();
    void venvSetupChanged();
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
    bool m_updated;

};
