/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <KMessageWidget>
#include <QObject>
#include <QString>
#include <QMap>
#include <QPair>

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
    QString pythonExec() { return m_pyExec; };
    void proposeMaybeUpdate(const QString &dependency, const QString &minVersion);
    bool installDisabled() { return m_disableInstall; };
    void runConcurrentScript(const QString &script, QStringList args);

    friend class PythonDependencyMessage;

public Q_SLOTS:
    /** @brief Check if all dependencies are installed.
        If everything is okay dependenciesAvailable() will be emitted,
        otherwise dependenciesMissing() with a message that can be shown
        to the user telling wich dependencies are missing.
        To get a list of all missing dependencies use missingDependencies
        @returns wether all checks succeeded.
    */
    void checkDependencies();

private:
    QString m_pyExec;
    QString m_pip3Exec;
    QMap<QString, QString> m_dependencies;
    QStringList m_missing;
    QMap<QString, QString> *m_versions;
    bool m_disableInstall;
    bool m_dependenciesChecked;

    void installMissingDependencies();
    QString locateScript(const QString &script);
    QString runPackageScript(const QString &mode, bool concurrent = false);
    int versionToInt(const QString &version);

protected:
    QMap<QString, QString> *m_scripts;
    void addDependency(const QString &pipname, const QString &purpose);
    void addScript(const QString &script);
    virtual QString featureName() { return {}; };

Q_SIGNALS:
    void setupError(const QString &message);
    void checkVersionsResult(const QStringList &versions);
    void dependenciesMissing(const QStringList &messages);
    void dependenciesAvailable();
    void proposeUpdate(const QString &message);
    void scriptFeedback(const QStringList message);
    void installFeedback(const QString &message);
    void scriptGpuCheckFinished();
    void scriptFinished();
    void scriptStarted();
    void abortScript();
};

class PythonDependencyMessage : public KMessageWidget {
    Q_OBJECT

public:
    PythonDependencyMessage(QWidget *parent, AbstractPythonInterface *interface);

public Q_SLOTS:
    void checkAfterInstall();

private Q_SLOTS:
    void doShowMessage(const QString &message, KMessageWidget::MessageType messageType = KMessageWidget::Information);

private:
    AbstractPythonInterface * m_interface;
    QAction *m_installAction{nullptr};
    QAction *m_abortAction{nullptr};
    bool m_updated;

};
