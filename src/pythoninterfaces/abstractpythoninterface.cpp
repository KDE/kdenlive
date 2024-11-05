/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "abstractpythoninterface.h"
#include "core.h"
#include "mainwindow.h"

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
                m_installAction->setText(m_interface->installMessage());
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
                if (m_interface->installMissingDependencies()) {
                    doShowMessage(i18n("Installing modules… this can take a while"), KMessageWidget::Information);
                    addAction(m_abortAction);
                    removeAction(m_installAction);
                    qApp->processEvents();
                } else {
                    m_installAction->setEnabled(true);
                }
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
    : QObject{parent} //, m_disableInstall(pCore->packageType() == LinuxPackageType::Flatpak)
    , m_dependenciesChecked(false)
    , m_dependencies()
{
    addScript(QStringLiteral("checkpackages.py"));
    addScript(QStringLiteral("checkgpu.py"));
}

AbstractPythonInterface::~AbstractPythonInterface() {}

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
    if (!(KdenliveSettings::pythonPath().isEmpty() || KdenliveSettings::pipPath().isEmpty() || m_scripts.values().contains(QStringLiteral("")))) {
        return true;
    }
    if (!checkPython(KdenliveSettings::usePythonVenv())) {
        return false;
    }

    QMapIterator<QString, QString> i(m_scripts);
    while (i.hasNext()) {
        i.next();
        if (i.value().isEmpty()) {
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
    QDir pluginDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    pluginDir.mkpath(QStringLiteral("."));

    QProcess envProcess;
    // For some reason, this fails in AppImage, but when extracting the Appimage it works...
    // No workaround found yet for AppImage
    QStringList args = {QStringLiteral("-m"), QStringLiteral("venv"), pluginDir.absoluteFilePath(QStringLiteral("venv"))};
    envProcess.start(pyExec, args);
    envProcess.waitForStarted();
    envProcess.waitForFinished(-1);
    QString pythonPath;
    if (envProcess.exitStatus() == QProcess::NormalExit) {
#ifdef Q_OS_WIN
        pythonPath = QStringLiteral("venv/Scripts/");
        QStringList pythonPaths = {pluginDir.absoluteFilePath(pythonPath)};
        KdenliveSettings::setPythonPath(QStandardPaths::findExecutable(QStringLiteral("python"), pythonPaths));
        KdenliveSettings::setPipPath(QStandardPaths::findExecutable(QStringLiteral("pip"), pythonPaths));
#else
        pythonPath = QStringLiteral("venv/bin/");
        QStringList pythonPaths = {pluginDir.absoluteFilePath(pythonPath)};
        KdenliveSettings::setPythonPath(QStandardPaths::findExecutable(QStringLiteral("python3"), pythonPaths));
        KdenliveSettings::setPipPath(QStandardPaths::findExecutable(QStringLiteral("pip3"), pythonPaths));
#endif
        if (!KdenliveSettings::pipPath().isEmpty() && QFile::exists(KdenliveSettings::pipPath())) {
            installPackage({QStringLiteral("importlib")});
            installInProgress = false;
            return true;
        }
        // ERROR READ JOB OUTPUT
        Q_EMIT setupError(i18n("Cannot create the python virtual environment:\n%1", envProcess.readAllStandardOutput()));
    } else {
        Q_EMIT setupError(i18n("Cannot create the python virtual environment:\n%1", envProcess.readAllStandardError()));
    }
    // Install failed, remove venv
    if (pluginDir.cd(QStringLiteral("venv"))) {
        if (pluginDir.dirName() == QLatin1String("venv")) {
            pluginDir.removeRecursively();
        }
    }
    installInProgress = false;
    return false;
}

const QString AbstractPythonInterface::locateScript(const QString &script)
{
    const QString path = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/%1").arg(script));
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
    m_scripts.insert(script, locateScript(script));
}

void AbstractPythonInterface::checkDependenciesConcurrently()
{
    (void)QtConcurrent::run(&AbstractPythonInterface::checkDependencies, this, false, false);
}

void AbstractPythonInterface::checkVersionsConcurrently()
{
    (void)QtConcurrent::run(&AbstractPythonInterface::checkVersions, this, true);
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
    const QString output = runPackageScript(QStringLiteral("--check"), false, false);
    QStringList missingDeps = output.split(QStringLiteral("Missing: "));
    QStringList outputMissing;
    for (auto &m : missingDeps) {
        outputMissing << m.simplified();
    }
    const QStringList deps = parseDependencies(m_dependencies.keys(), true);
    QStringList messages;
    if (!output.isEmpty()) {
        // We have missing dependencies
        for (auto i : deps) {
            if (outputMissing.contains(i)) {
                if (m_optionalDeps.contains(i)) {
                    m_optionalMissing << i;
                    continue;
                }
                m_missing.append(i);
                messages.append(xi18n("The <application>%1</application> python module is required.", i));
            }
        }
    }
    m_dependenciesChecked = true;
    if (messages.isEmpty()) {
        if (async) {
            checkVersionsConcurrently();
        } else {
            checkVersions(true);
        }
        Q_EMIT dependenciesAvailable();
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

bool AbstractPythonInterface::installMissingDependencies()
{
    if (KMessageBox::warningContinueCancel(pCore->window(), i18n("This requires an internet connection and will take several minutes to download all necessary "
                                                                 "dependencies. After that all processing will happen offline.")) != KMessageBox::Continue) {
        return false;
    }
    if (!checkPython(true, true, true)) {
        return false;
    }
    runPackageScript(QStringLiteral("--install"), true);
    return true;
}

void AbstractPythonInterface::updateDependencies()
{
    runPackageScript(QStringLiteral("--upgrade"), true);
}

void AbstractPythonInterface::runConcurrentScript(const QString &script, QStringList args, bool feedback)
{
    if (m_dependencies.keys().isEmpty()) {
        qWarning() << "No dependencies specified";
        Q_EMIT setupError(i18n("Internal Error: Cannot find dependency list"));
        return;
    }
    if (!checkSetup()) {
        qWarning() << "setup error for script: " << script;
        return;
    }
    (void)QtConcurrent::run(&AbstractPythonInterface::runScript, this, script, args, QString(), true, feedback);
}

void AbstractPythonInterface::proposeMaybeUpdate(const QString &dependency, const QString &minVersion)
{
    checkVersions(false);
    const QString currentVersion = m_versions.value(dependency);
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
    QStringList deps = parseDependencies(m_dependencies.keys(), true);
    const QStringList output = runPackageScript(QStringLiteral("--details"), false, false).split(QLatin1Char('\n'));
    QMutexLocker locker(&m_versionsMutex);
    QStringList versionsText;
    for (auto &o : output) {
        if (o.contains(QLatin1String("=="))) {
            const QString package = o.section(QLatin1String("=="), 0, 0).toLower();
            if (deps.contains(package)) {
                QString version = o.section(QLatin1String("=="), 1);
                if (version == QLatin1String("missing")) {
                    version = i18nc("@item:intext indicates a missing dependency", "missing (optional)");
                }
                m_versions.insert(package, version);
                versionsText.append(QStringLiteral("<b>%1</b> %2").arg(package, version));
            }
        }
    }
    if (m_versions.isEmpty()) {
        Q_EMIT setupMessage(i18nc("@label:textbox", "No version information available."), int(KMessageWidget::Warning));
        qDebug() << "::: CHECKING DEPENDENCIES... NO VERSION INFO AVAILABLE";
        return;
    }
    if (signalOnResult) {
        Q_EMIT checkVersionsResult(versionsText);
    }
}

QStringList AbstractPythonInterface::parseDependencies(const QStringList deps, bool split)
{
    // Extract requirements files
    if (split == false) {
        return deps;
    }
    QStringList packages;
    QStringList reqFiles;
    for (auto &d : deps) {
        if (d.endsWith(QLatin1String(".txt"))) {
            reqFiles << d;
        } else {
            packages << d;
        }
    }
    for (auto &r : reqFiles) {
        // This is a requirements.txt file, read content
        QStringList stringList;
        QFile textFile(r);
        if (textFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream textStream(&textFile);
            while (!textStream.atEnd()) {
                QString line = textStream.readLine();
                if (line.simplified().isEmpty())
                    continue;
                else if (!line.startsWith(QLatin1Char('#'))) {
                    if (line.contains(QLatin1Char('>'))) {
                        line = line.section(QLatin1Char('>'), 0, 0);
                    } else if (line.contains(QLatin1Char('<'))) {
                        line = line.section(QLatin1Char('<'), 0, 0);
                    } else if (line.contains(QLatin1Char('='))) {
                        line = line.section(QLatin1Char('='), 0, 0);
                    }
                    packages.append(line.simplified());
                }
            }
        }
    }
    return packages;
}

QString AbstractPythonInterface::runPackageScript(const QString &mode, bool concurrent, bool displayFeedback)
{
    if (m_dependencies.keys().isEmpty()) {
        qWarning() << "No dependencies specified";
        Q_EMIT setupError(i18n("Internal Error: Cannot find dependency list"));
        return {};
    }
    if (!checkSetup()) {
        return {};
    }
    QStringList deps = parseDependencies(m_dependencies.keys(), mode != QLatin1String("--install") && mode != QLatin1String("--upgrade"));

    if (concurrent) {
        (void)QtConcurrent::run(&AbstractPythonInterface::runScript, this, QStringLiteral("checkpackages.py"), deps, mode, concurrent, displayFeedback);
        return {};
    } else {
        return runScript(QStringLiteral("checkpackages.py"), deps, mode, concurrent, displayFeedback);
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
    const QString scriptpath = m_scripts.value(script);
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
    args = parseDependencies(args, firstarg != QLatin1String("--install") && firstarg != QLatin1String("--upgrade"));
    if (!firstarg.isEmpty()) {
        args.prepend(firstarg);
    }
    args.prepend(scriptpath);
    QProcess scriptJob;
    connect(this, &AbstractPythonInterface::abortScript, &scriptJob, &QProcess::kill, Qt::DirectConnection);

    if (packageFeedback) {
        if (concurrent) {
            scriptJob.setProcessChannelMode(QProcess::MergedChannels);
        }
        connect(&scriptJob, &QProcess::readyReadStandardOutput, [this, &scriptJob]() {
            const QString processData = QString::fromUtf8(scriptJob.readAllStandardOutput());
            if (!processData.isEmpty()) {
                Q_EMIT installFeedback(processData.simplified());
            }
        });
    }

    scriptJob.start(KdenliveSettings::pythonPath(), args);
    // Don't timeout
    qDebug() << "::: RUNNONG SCRIPT: " << KdenliveSettings::pythonPath() << " = " << args;
    scriptJob.waitForFinished(-1);

    if (!concurrent && (scriptJob.exitStatus() != QProcess::NormalExit || scriptJob.exitCode() != 0)) {
        const QString errorMessage = scriptJob.readAllStandardError();
        Q_EMIT setupError(i18n("Error while running python3 script:\n %1\n%2", scriptpath, errorMessage));
        return {};
    }

    if (concurrent && !packageFeedback) {
        const QString processData = QString::fromUtf8(scriptJob.readAllStandardOutput());
        Q_EMIT scriptFeedback(script, args, processData.split(QLatin1Char('\n'), Qt::SkipEmptyParts));
    }

    if (script.contains(QLatin1String("checkgpu.py")) || script.contains(QLatin1String("whisperquery")) || script.contains(QLatin1String("seamlessquery"))) {
        Q_EMIT concurrentScriptFinished(script, args);
    } else if (concurrent && (firstarg == QLatin1String("--install") || firstarg == QLatin1String("--upgrade"))) {
        Q_EMIT scriptFinished(args);
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

const QString AbstractPythonInterface::installMessage() const
{
    return i18n("Install missing dependencies");
}
