/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "abstractpythoninterface.h"
#include "core.h"
#include "mainwindow.h"
#include "utils/KMessageBox_KdenliveCompat.h"

#include <KIO/DirectorySizeJob>
#include <KLocalizedString>
#include <KMessageBox>
#include <QAction>
#include <QDebug>
#include <QGuiApplication>
#include <QMutex>
#include <QProcess>
#include <QStandardPaths>
#include <QtConcurrent>

static QMutex mutex;
static bool installInProgress;

PythonDependencyMessage::PythonDependencyMessage(QWidget *parent, AbstractPythonInterface *interface, bool setupErrorOnly)
    : KMessageWidget(parent)
    , m_interface(interface)
{
    setWordWrap(true);
    setCloseButtonVisible(false);
    hide();
    m_installAction = new QAction(i18n("Install missing dependencies"), this);
    m_abortAction = new QAction(i18n("Abort installation"), this);
    connect(m_abortAction, &QAction::triggered, m_interface, &AbstractPythonInterface::abortScript);
    connect(m_interface, &AbstractPythonInterface::setupError, this, [&](const QString &message) {
        removeAction(m_installAction);
        removeAction(m_abortAction);
        doShowMessage(message, KMessageWidget::Warning);
    });
    connect(m_interface, &AbstractPythonInterface::setupMessage, this,
            [&](const QString &message, int messageType) { doShowMessage(message, KMessageWidget::MessageType(messageType)); });

    if (!setupErrorOnly) {
        connect(m_interface, &AbstractPythonInterface::checkVersionsResult, this, [&](const QStringList &list) {
            removeAction(m_abortAction);
            if (list.isEmpty()) {
                if (m_interface->featureName().isEmpty()) {
                    doShowMessage(i18n("Everything is properly configured."), KMessageWidget::Positive);
                } else {
                    doShowMessage(i18n("%1 is properly configured.", m_interface->featureName()), KMessageWidget::Positive);
                }
            } else {
                if (m_interface->featureName().isEmpty()) {
                    doShowMessage(i18n("Everything is configured:<br>%1", list.join(QStringLiteral(", "))), KMessageWidget::Positive);
                } else {
                    doShowMessage(i18n("%1 is configured:<br>%2", m_interface->featureName(), list.join(QStringLiteral(", "))), KMessageWidget::Positive);
                }
            }
        });

        connect(m_interface, &AbstractPythonInterface::dependenciesMissing, this, [&](const QStringList &messages) {
            if (!m_interface->installDisabled()) {
                m_installAction->setEnabled(true);
                removeAction(m_abortAction);
                m_installAction->setText(i18n("Install missing dependencies"));
                addAction(m_installAction);
            }

            doShowMessage(messages.join(QStringLiteral("\n")), KMessageWidget::Warning);
        });

        if (!m_interface->installDisabled()) {
            connect(m_interface, &AbstractPythonInterface::proposeUpdate, this, [&](const QString &message) {
                // only allow upgrading python modules once
                m_installAction->setText(i18n("Check for update"));
                m_installAction->setEnabled(true);
                removeAction(m_abortAction);
                addAction(m_installAction);
                doShowMessage(message, KMessageWidget::Warning);
            });
        }

        connect(m_interface, &AbstractPythonInterface::dependenciesAvailable, this, [&]() {
            if (!m_updated && !m_interface->installDisabled()) {
                // only allow upgrading python modules once
                m_installAction->setText(i18n("Check for update"));
                m_installAction->setEnabled(true);
                removeAction(m_abortAction);
                addAction(m_installAction);
            }
            if (text().isEmpty()) {
                hide();
            }
        });

        connect(m_installAction, &QAction::triggered, this, [&]() {
            if (!m_interface->missingDependencies().isEmpty()) {
                m_installAction->setEnabled(false);
                doShowMessage(i18n("Installing modules… this can take a while"), KMessageWidget::Information);
                addAction(m_abortAction);
                qApp->processEvents();
                m_interface->installMissingDependencies();
                removeAction(m_installAction);
            } else {
                // upgrade
                m_updated = true;
                m_installAction->setEnabled(false);
                addAction(m_abortAction);
                doShowMessage(i18n("Updating modules…"), KMessageWidget::Information);
                qApp->processEvents();
                m_interface->updateDependencies();
                removeAction(m_installAction);
            }
        });
    }
}

void PythonDependencyMessage::doShowMessage(const QString &message, KMessageWidget::MessageType messageType)
{
    if (message.isEmpty()) {
        hide();
    } else {
        setMessageType(messageType);
        setText(message);
        show();
    }
}

void PythonDependencyMessage::checkAfterInstall()
{
    doShowMessage(i18n("Checking configuration…"), KMessageWidget::Information);
    m_interface->checkDependencies(true, false);
}

AbstractPythonInterface::AbstractPythonInterface(QObject *parent)
    : QObject{parent}
    , m_versions(new QMap<QString, QString>())
    , m_disableInstall(pCore->packageType() == QStringLiteral("flatpak"))
    , m_dependenciesChecked(false)
    , m_dependencies()
    , m_scripts(new QMap<QString, QString>())
{
    addScript(QStringLiteral("checkpackages.py"));
    addScript(QStringLiteral("checkgpu.py"));
}

AbstractPythonInterface::~AbstractPythonInterface()
{
    delete m_versions;
    delete m_scripts;
}

bool AbstractPythonInterface::checkPython(bool useVenv, bool calculateSize, bool forceInstall)
{
    if (installInProgress) {
        return false;
    }
    if (!calculateSize && useVenv == KdenliveSettings::usePythonVenv() && !KdenliveSettings::pythonPath().isEmpty() && !KdenliveSettings::pipPath().isEmpty() &&
        QFile::exists(KdenliveSettings::pythonPath())) {
        // Already setup
        if (KdenliveSettings::pythonPath().contains(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))) {
            if (useVenv) {
                // Everything ok, using venv python
                qDebug() << "::::: CHECKING PYTHON WITH VENV EVEYTHING OK...";
                return true;
            }
        } else if (!useVenv) {
            // Everything ok, using system python
            qDebug() << "::::: CHECKING PYTHON NO VENV EVEYTHING OK...";
            return true;
        }
    }
    qDebug() << "::::: CHECKING PYTHON, RQST VENV: " << useVenv;
    Q_EMIT setupMessage(i18nc("@label:textbox", "Checking setup…"), int(KMessageWidget::Information));
    QMutexLocker bk(&mutex);
    QStringList pythonPaths;
    if (useVenv) {
#ifdef Q_OS_WIN
        const QString pythonPath = QStringLiteral("venv/Scripts/");
#else
        const QString pythonPath = QStringLiteral("venv/bin/");
#endif
        QDir pluginDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
        if (!pluginDir.exists(pythonPath)) {
            // Setup venv
            if (forceInstall) {
                if (!setupVenv()) {
                    return false;
                }
            } else {
                // Venv folder not found, disable
                useVenv = false;
            }
        }
        if (pluginDir.exists(pythonPath)) {
            pythonPaths << pluginDir.absoluteFilePath(pythonPath);
        }
    }
#ifdef Q_OS_WIN
    KdenliveSettings::setPythonPath(QStandardPaths::findExecutable(QStringLiteral("python"), pythonPaths));
    KdenliveSettings::setPipPath(QStandardPaths::findExecutable(QStringLiteral("pip"), pythonPaths));
#else
    KdenliveSettings::setPythonPath(QStandardPaths::findExecutable(QStringLiteral("python3"), pythonPaths));
    KdenliveSettings::setPipPath(QStandardPaths::findExecutable(QStringLiteral("pip3"), pythonPaths));
#endif
    if (KdenliveSettings::pythonPath().isEmpty()) {
        Q_EMIT setupError(i18n("Cannot find python3, please install it on your system.\n"
                               "If already installed, check it is installed in a directory "
                               "listed in PATH environment variable"));
        return false;
    }
    if (KdenliveSettings::pipPath().isEmpty() && !m_disableInstall) {
        Q_EMIT setupError(i18n("Cannot find pip3, please install it on your system.\n"
                               "If already installed, check it is installed in a directory "
                               "listed in PATH environment variable"));
        return false;
    }
    if (useVenv != KdenliveSettings::usePythonVenv()) {
        KdenliveSettings::setUsePythonVenv(useVenv);
        Q_EMIT venvSetupChanged();
    }
    Q_EMIT setupMessage(i18n("Using python from %1", QFileInfo(KdenliveSettings::pythonPath()).absolutePath()), int(KMessageWidget::Information));
    if (calculateSize) {
        // Calculate venv size
        QDir pluginDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
        if (pluginDir.cd(QStringLiteral("venv"))) {
            KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(pluginDir.absolutePath()));
            connect(job, &KIO::DirectorySizeJob::result, this, &AbstractPythonInterface::gotFolderSize);
        } else {
            Q_EMIT gotPythonSize(i18n("No python venv found"));
        }
    }
    return true;
}

void AbstractPythonInterface::gotFolderSize(KJob *job)
{
    auto *sourceJob = static_cast<KIO::DirectorySizeJob *>(job);
    KIO::filesize_t total = sourceJob->totalSize();
    if (sourceJob->totalFiles() == 0) {
        total = 0;
    }
    Q_EMIT gotPythonSize(i18n("Python venv size: %1", KIO::convertSize(total)));
}

bool AbstractPythonInterface::checkSetup()
{
    if (!(KdenliveSettings::pythonPath().isEmpty() || KdenliveSettings::pipPath().isEmpty() || m_scripts->values().contains(QStringLiteral("")))) {
        return true;
    }
    if (!checkPython(KdenliveSettings::usePythonVenv())) {
        return false;
    }

    for (int i = 0; i < m_scripts->count(); i++) {
        QString key = m_scripts->keys()[i];
        (*m_scripts)[key] = locateScript(key);
        if ((*m_scripts)[key].isEmpty()) {
            return false;
        }
    }
    return true;
}

bool AbstractPythonInterface::setupVenv()
{
    // First check if python and venv are available
    QString pyExec;
#ifdef Q_OS_WIN
    pyExec = QStandardPaths::findExecutable(QStringLiteral("python"));
#else
    pyExec = QStandardPaths::findExecutable(QStringLiteral("python3"));
#endif
    // Check that the system python is found
    if (pyExec.isEmpty()) {
        Q_EMIT setupError(i18n("Cannot find python3, please install it on your system.\n"
                               "If already installed, check it is installed in a directory "
                               "listed in PATH environment variable"));
        return false;
    }
    // Use system python to check for venv
    installInProgress = true;
    KdenliveSettings::setPythonPath(pyExec);
    const QString missingDeps = runScript(QStringLiteral("checkpackages.py"), {"virtualenv"}, QStringLiteral("--check"), false);
    if (!missingDeps.isEmpty()) {
        Q_EMIT setupError(i18n("Cannot find python virtualenv, please install it on your system. Defaulting to system python."));
        installInProgress = false;
        return false;
    }
    QDir pluginDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    pluginDir.mkpath(QStringLiteral("."));

    QProcess envProcess;
    // For some reason, this fails in AppImage, but when extracting the Appimage it works...
    // No workaround found yet for AppImage
    QStringList args = {QStringLiteral("-m"), QStringLiteral("venv"), pluginDir.absoluteFilePath(QStringLiteral("venv"))};
    envProcess.start(pyExec, args);
    envProcess.waitForStarted();
    envProcess.waitForFinished(-1);
    if (envProcess.exitStatus() == QProcess::NormalExit) {
#ifdef Q_OS_WIN
        const QString pythonPath = QStringLiteral("venv/Scripts/");
        QStringList pythonPaths = {pluginDir.absoluteFilePath(pluginDir.absoluteFilePath(pythonPath))};
        KdenliveSettings::setPythonPath(QStandardPaths::findExecutable(QStringLiteral("python"), pythonPaths));
        KdenliveSettings::setPipPath(QStandardPaths::findExecutable(QStringLiteral("pip"), pythonPaths));
#else
        const QString pythonPath = QStringLiteral("venv/bin/");
        QStringList pythonPaths = {pluginDir.absoluteFilePath(pluginDir.absoluteFilePath(pythonPath))};
        KdenliveSettings::setPythonPath(QStandardPaths::findExecutable(QStringLiteral("python3"), pythonPaths));
        KdenliveSettings::setPipPath(QStandardPaths::findExecutable(QStringLiteral("pip3"), pythonPaths));
#endif
        if (!KdenliveSettings::pipPath().isEmpty()) {
            installPackage({QStringLiteral("importlib")});
            installInProgress = false;
            return true;
        }
    }
    installInProgress = false;
    return false;
}

QString AbstractPythonInterface::locateScript(const QString &script)
{
    QString path = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/%1").arg(script));
    if (path.isEmpty()) {
        Q_EMIT setupError(i18n("The %1 script was not found, check your install.", script));
    }
    return path;
}

void AbstractPythonInterface::addDependency(const QString &pipname, const QString &purpose, bool optional)
{
    m_dependencies.insert(pipname, purpose);
    if (optional) {
        m_optionalDeps << pipname;
    }
}

void AbstractPythonInterface::addScript(const QString &script)
{
    m_scripts->insert(script, QStringLiteral(""));
}

void AbstractPythonInterface::checkDependenciesConcurrently()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QtConcurrent::run(this, &AbstractPythonInterface::checkDependencies, false, false);
#else
    (void)QtConcurrent::run(&AbstractPythonInterface::checkDependencies, this, false, false);
#endif
}

void AbstractPythonInterface::checkVersionsConcurrently()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QtConcurrent::run(this, &AbstractPythonInterface::checkVersions, true);
#else
    (void)QtConcurrent::run(&AbstractPythonInterface::checkVersions, this, true);
#endif
}

void AbstractPythonInterface::checkDependencies(bool force, bool async)
{
    if (!force && m_dependenciesChecked) {
        // Don't check twice if dependecies are satisfied
        return;
    }
    // Force check, reset flag
    m_missing.clear();
    m_optionalMissing.clear();
    const QString output = runPackageScript(QStringLiteral("--check"));
    QStringList missingDeps = output.split(QStringLiteral("Missing: "));
    QStringList outputMissing;
    for (auto &m : missingDeps) {
        outputMissing << m.simplified();
    }
    QStringList messages;
    if (!output.isEmpty()) {
        // We have missing dependencies
        for (auto i : m_dependencies.keys()) {
            if (outputMissing.contains(i)) {
                if (m_optionalDeps.contains(i)) {
                    m_optionalMissing << i;
                    continue;
                }
                m_missing.append(i);
                if (m_dependencies.value(i).isEmpty()) {
                    messages.append(xi18n("The <application>%1</application> python module is required.", i));
                } else {
                    messages.append(xi18n("The <application>%1</application> python module is required for %2.", i, m_dependencies.value(i)));
                }
            }
        }
    }
    m_dependenciesChecked = true;
    if (messages.isEmpty()) {
        Q_EMIT dependenciesAvailable();
        if (async) {
            checkVersionsConcurrently();
        } else {
            checkVersions(true);
        }
    } else {
        Q_EMIT dependenciesMissing(messages);
    }
}

QStringList AbstractPythonInterface::missingDependencies(const QStringList &filter)
{
    if (filter.isEmpty()) {
        return m_missing;
    }
    QStringList filtered;
    for (auto item : filter) {
        if (m_missing.contains(item)) {
            filtered.append(item);
        }
    }
    return filtered;
};

void AbstractPythonInterface::installMissingDependencies()
{
    if (!KdenliveSettings::usePythonVenv()) {
        QDir pluginDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
        if (KMessageBox::questionTwoActions(
                pCore->window(),
                i18n("Kdenlive can install the missing python modules in a virtual environment under %1.\nThis way, it won't touch your system libraries.",
                     pluginDir.absoluteFilePath(QStringLiteral("venv"))),
                i18n("Python environment"), KGuiItem(i18n("Use virtual environment (recommended)")),
                KGuiItem(i18n("Use system install"))) == KMessageBox::PrimaryAction) {
            if (!checkPython(true, true, true)) {
                return;
            }
        }
    }
    runPackageScript(QStringLiteral("--install"), true);
}

void AbstractPythonInterface::updateDependencies()
{
    runPackageScript(QStringLiteral("--upgrade"), true);
}

void AbstractPythonInterface::runConcurrentScript(const QString &script, QStringList args)
{
    if (m_dependencies.keys().isEmpty()) {
        qWarning() << "No dependencies specified";
        Q_EMIT setupError(i18n("Internal Error: Cannot find dependency list"));
        return;
    }
    if (!checkSetup()) {
        return;
    }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QtConcurrent::run(this, &AbstractPythonInterface::runScript, script, args, QString(), true, false);
#else
    (void)QtConcurrent::run(&AbstractPythonInterface::runScript, this, script, args, QString(), true, false);
#endif
}

void AbstractPythonInterface::proposeMaybeUpdate(const QString &dependency, const QString &minVersion)
{
    checkVersions(false);
    QString currentVersion = m_versions->value(dependency);
    if (currentVersion.isEmpty()) {
        Q_EMIT setupError(i18n("Error while checking version of module %1", dependency));
        return;
    }
    if (versionToInt(currentVersion) < versionToInt(minVersion)) {
        Q_EMIT proposeUpdate(i18n("At least version %1 of module %2 is required, "
                                  "but your current version is %3",
                                  minVersion, dependency, currentVersion));
    } else {
        Q_EMIT proposeUpdate(i18n("Please consider to update your setup."));
    }
}

int AbstractPythonInterface::versionToInt(const QString &version)
{
    QStringList v = version.split(QStringLiteral("."));
    return QT_VERSION_CHECK(v.length() > 0 ? v.at(0).toInt() : 0, v.length() > 1 ? v.at(1).toInt() : 0, v.length() > 2 ? v.at(2).toInt() : 0);
}

void AbstractPythonInterface::checkVersions(bool signalOnResult)
{
    if (installDisabled()) {
        return;
    }
    QString output = runPackageScript(QStringLiteral("--details"));
    if (output.isEmpty() || !output.contains(QStringLiteral("Version: "))) {
        Q_EMIT setupMessage(i18nc("@label:textbox", "No version information available."), int(KMessageWidget::Warning));
        qDebug() << "::: CHECKING DEPENDENCIES... NO VERSION INFO AVAILABLE";
        return;
    }
    QStringList raw = output.split(QStringLiteral("Version: "));
    QStringList versions;
    for (int i = 0; i < raw.count(); i++) {
        QString name = raw.at(i);
        int pos = name.indexOf(QStringLiteral("Name:"));
        if (pos > -1) {
            if (pos != 0) {
                name.remove(0, pos);
            }
            name = name.simplified().section(QLatin1Char(' '), 1, 1);
            QString version = raw.at(i + 1);
            version = version.simplified().section(QLatin1Char(' '), 0, 0);
            if (version == QLatin1String("missing")) {
                version = i18nc("@item:intext indicates a missing dependency", "missing (optional)");
            }
            versions.append(QString("<b>%1</b> %2").arg(name, version));
            if (m_versions->contains(name)) {
                (*m_versions)[name.toLower()] = version;
            } else {
                m_versions->insert(name.toLower(), version);
            }
        }
    }
    if (signalOnResult) {
        Q_EMIT checkVersionsResult(versions);
    }
}

QString AbstractPythonInterface::runPackageScript(const QString &mode, bool concurrent)
{
    if (m_dependencies.keys().isEmpty()) {
        qWarning() << "No dependencies specified";
        Q_EMIT setupError(i18n("Internal Error: Cannot find dependency list"));
        return {};
    }
    if (!checkSetup()) {
        return {};
    }
    if (concurrent) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QtConcurrent::run(this, &AbstractPythonInterface::runScript, QStringLiteral("checkpackages.py"), m_dependencies.keys(), mode, concurrent, true);
#else
        (void)QtConcurrent::run(&AbstractPythonInterface::runScript, this, QStringLiteral("checkpackages.py"), m_dependencies.keys(), mode, concurrent, true);
#endif
        return {};
    } else {
        return runScript(QStringLiteral("checkpackages.py"), m_dependencies.keys(), mode, concurrent, true);
    }
}

QString AbstractPythonInterface::installPackage(const QStringList packageNames)
{
    if (!checkSetup()) {
        return {};
    }
    return runScript(QStringLiteral("checkpackages.py"), packageNames, "--install", false, true);
}

QString AbstractPythonInterface::runScript(const QString &script, QStringList args, const QString &firstarg, bool concurrent, bool packageFeedback)
{
    QString scriptpath = m_scripts->value(script);
    if (KdenliveSettings::pythonPath().isEmpty() || scriptpath.isEmpty()) {
        if (KdenliveSettings::pythonPath().isEmpty()) {
            Q_EMIT setupError(i18n("Python exec not found"));
        } else {
            Q_EMIT setupError(i18n("Failed to find script file %1", script));
        }
        return {};
    }
    if (concurrent && (firstarg == QLatin1String("--install") || firstarg == QLatin1String("--upgrade"))) {
        Q_EMIT scriptStarted();
    }
    if (!firstarg.isEmpty()) {
        args.prepend(firstarg);
    }
    args.prepend(scriptpath);
    QProcess scriptJob;
    if (concurrent) {
        if (packageFeedback) {
            connect(&scriptJob, &QProcess::readyReadStandardOutput, [this, &scriptJob]() {
                const QString processData = QString::fromUtf8(scriptJob.readAll());
                if (!processData.isEmpty()) {
                    Q_EMIT installFeedback(processData.simplified());
                }
            });
        } else {
            connect(&scriptJob, &QProcess::readyReadStandardOutput, [this, &scriptJob]() {
                const QString processData = QString::fromUtf8(scriptJob.readAll());
                Q_EMIT scriptFeedback(processData.split(QLatin1Char('\n'), Qt::SkipEmptyParts));
            });
        }
    }
    connect(this, &AbstractPythonInterface::abortScript, &scriptJob, &QProcess::kill, Qt::DirectConnection);
    scriptJob.start(KdenliveSettings::pythonPath(), args);
    // Don't timeout
    qDebug() << "::: RUNNONG SCRIPT: " << KdenliveSettings::pythonPath() << " = " << args;
    scriptJob.waitForFinished(-1);
    if (!concurrent && (scriptJob.exitStatus() != QProcess::NormalExit || scriptJob.exitCode() != 0)) {
        qDebug() << "::::: WARNING ERRROR EXIT STATUS: " << scriptJob.exitCode();
        const QString errorMessage = scriptJob.readAllStandardError();
        Q_EMIT setupError(i18n("Error while running python3 script:\n %1\n%2", scriptpath, errorMessage));
        qWarning() << " SCRIPT ERROR: " << errorMessage;
        return {};
    }
    if (script == QLatin1String("checkgpu.py")) {
        Q_EMIT scriptGpuCheckFinished();
    } else if (concurrent && (firstarg == QLatin1String("--install") || firstarg == QLatin1String("--upgrade"))) {
        Q_EMIT scriptFinished();
    }
    return scriptJob.readAllStandardOutput();
}

bool AbstractPythonInterface::removePythonVenv()
{
    QDir pluginDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    if (!pluginDir.exists(QStringLiteral("venv")) || !pluginDir.absolutePath().contains(QStringLiteral("kdenlive"))) {
        return false;
    }
    if (!pluginDir.cd(QStringLiteral("venv"))) {
        return false;
    }
    if (KMessageBox::warningContinueCancel(pCore->window(),
                                           i18n("This will delete the python virtual environment from:<br/><b>%1</b><br/>The environment will be recreated "
                                                "and modules downloaded whenever you reenable the python virtual environment.",
                                                pluginDir.absolutePath())) != KMessageBox::Continue) {
        return false;
    }
    return pluginDir.removeRecursively();
}

bool AbstractPythonInterface::installInProcess() const
{
    return installInProgress;
}

bool AbstractPythonInterface::optionalDependencyAvailable(const QString &dependency) const
{
    return !m_optionalMissing.contains(dependency);
}
