/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "core.h"
#include "mainwindow.h"
#include "abstractpythoninterface.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <QDebug>
#include <QStandardPaths>
#include <QProcess>
#include <QAction>
#include <QGuiApplication>

PythonDependencyMessage::PythonDependencyMessage(QWidget *parent, AbstractPythonInterface *interface)
    : KMessageWidget(parent)
    , m_interface(interface)
{
    setWordWrap(true);
    connect(m_interface, &AbstractPythonInterface::setupError, this, [&](const QString &message){
        removeAction(m_installAction);
        doShowMessage(message, KMessageWidget::Warning);
    });

    connect(m_interface, &AbstractPythonInterface::checkVersionsResult, this, [&] (const QStringList &list) {
        if (list.isEmpty()) {
            if (m_interface->featureName().isEmpty()) {
                doShowMessage(i18n("Everything is properly configured."), KMessageWidget::Positive);
            } else {
                doShowMessage(i18n("%1 is properly configured.", m_interface->featureName()), KMessageWidget::Positive);
            }
        } else {
            if (m_interface->featureName().isEmpty()) {
                doShowMessage(i18n("Everything is configured: %1", list.join(QStringLiteral(", "))), KMessageWidget::Positive);
            } else {
                doShowMessage(i18n("%1 is configured: %2", m_interface->featureName(), list.join(QStringLiteral(", "))), KMessageWidget::Positive);
            }
        }
    });

    connect(m_interface, &AbstractPythonInterface::dependenciesMissing, this, [&](const QStringList &messages){
        m_installAction->setEnabled(true);
        m_installAction->setText(i18n("Install missing dependencies"));
        addAction(m_installAction);
        doShowMessage(messages.join(QStringLiteral("\n")), KMessageWidget::Warning);
    });

    connect(m_interface, &AbstractPythonInterface::proposeUpdate, this, [&](const QString &message){
        // only allow upgrading python modules once
        m_installAction->setText(i18n("Check for update"));
        m_installAction->setEnabled(true);
        addAction(m_installAction);
        doShowMessage(message, KMessageWidget::Warning);
    });

    connect(m_interface, &AbstractPythonInterface::dependenciesAvailable, this, [&](){
        if (!m_updated) {
            // only allow upgrading python modules once
            m_installAction->setText(i18n("Check for update"));
            m_installAction->setEnabled(true);
            addAction(m_installAction);
        }
        if (text().isEmpty()) {
            hide();
        }
    });

    m_installAction = new QAction(i18n("Install missing dependencies"), this);
    connect(m_installAction, &QAction::triggered, this, [&]() {
        if (!m_interface->missingDependencies().isEmpty()) {
            m_installAction->setEnabled(false);
            doShowMessage(i18n("Installing modules..."), KMessageWidget::Information);
            qApp->processEvents();
            m_interface->installMissingDependencies();
            m_interface->checkDependencies();
            removeAction(m_installAction);
            if (m_interface->missingDependencies().isEmpty()) {
                m_interface->checkVersions();
            }
        } else {
            // upgrade
            m_updated = true;
            m_installAction->setEnabled(false);
            doShowMessage(i18n("Updating modules..."), KMessageWidget::Information);
            qApp->processEvents();
            m_interface->updateDependencies();
            removeAction(m_installAction);
            doShowMessage(i18n("Checking configuration..."), KMessageWidget::Information);
            m_interface->checkVersions();
        }
    });
}

void PythonDependencyMessage::doShowMessage(const QString &message, KMessageWidget::MessageType messageType) {
    if (message.isEmpty()) {
        animatedHide();
    } else {
        setMessageType(messageType);
        setText(message);
        animatedShow();
    }
}

AbstractPythonInterface::AbstractPythonInterface(QObject *parent)
    : QObject{parent}
    , m_dependencies()
    , m_versions(new QMap<QString, QString>())
    , m_scripts(new QMap<QString, QString>())
{
    addScript(QStringLiteral("checkpackages.py"));
}

bool AbstractPythonInterface::checkSetup()
{
    if (!(m_pyExec.isEmpty() || m_pip3Exec.isEmpty() || m_scripts->values().contains(QStringLiteral("")))) {
        return true;
    }
#ifdef Q_OS_WIN
    m_pyExec = QStandardPaths::findExecutable(QStringLiteral("python"));
    m_pip3Exec = QStandardPaths::findExecutable(QStringLiteral("pip"));
#else
    m_pyExec = QStandardPaths::findExecutable(QStringLiteral("python3"));
    m_pip3Exec = QStandardPaths::findExecutable(QStringLiteral("pip3"));
#endif
    if (m_pyExec.isEmpty()) {
        emit setupError(i18n("Cannot find python3, please install it on your system.\n"
                                   "If already installed, check it is installed in a directory"
                                   "listed in PATH environment variable"));
        return false;
    }
    if (m_pip3Exec.isEmpty()) {
        emit setupError(i18n("Cannot find pip3, please install it on your system.\n"
                                   "If already installed, check it is installed in a directory"
                                   "listed in PATH environment variable"));
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

QString AbstractPythonInterface::locateScript(const QString &script) {
    QString path = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/%1").arg(script));
    if (path.isEmpty()) {
        emit setupError(i18n("The %1 script was not found, check your install.", script));
    }
    return path;
}

void AbstractPythonInterface::addDependency(const QString &pipname, const QString &purpose) {
    m_dependencies.insert(pipname, purpose);
}

void AbstractPythonInterface::addScript(const QString &script) {
    m_scripts->insert(script, QStringLiteral(""));
}

void AbstractPythonInterface::checkDependencies() {
    QString output = runPackageScript(QStringLiteral("--check"));
    if (output.isEmpty()) {
        return;
    }
    m_missing.clear();
    QStringList messages;
    for (auto i: m_dependencies.keys()) {
        if (output.contains(i)) {
            m_missing.append(i);
            if (m_dependencies.value(i).isEmpty()) {
                messages.append(xi18n("The <application>%1</application> python module is required.", i));
            } else {
                messages.append(xi18n("The <application>%1</application> python module is required for %2.", i, m_dependencies.value(i)));
            }
        }
    }
    if (messages.isEmpty()) {
        emit dependenciesAvailable();
    } else {
        emit dependenciesMissing(messages);
    }
}

QStringList AbstractPythonInterface::missingDependencies(const QStringList &filter) {
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


void AbstractPythonInterface::installMissingDependencies() {
    runPackageScript(QStringLiteral("--install"));
}

void AbstractPythonInterface::updateDependencies() {
    runPackageScript(QStringLiteral("--upgrade"));
}

void AbstractPythonInterface::proposeMaybeUpdate(const QString &dependency, const QString &minVersion)
{
    checkVersions(false);
    QString currentVersion = m_versions->value(dependency);
    if (currentVersion.isEmpty()) {
        emit setupError(i18n("Error while checking version of module %1", dependency));
        return;
    }
    if (versionToInt(currentVersion) < versionToInt(minVersion)) {
        emit proposeUpdate(i18n("At least version %1 of module %2 is required, "
                                "but your current version is %3", minVersion, dependency, currentVersion));
    } else {
        emit proposeUpdate(i18n("Please consider to update your setup."));
    }
}

int AbstractPythonInterface::versionToInt(const QString &version) {
    QStringList v = version.split(QStringLiteral("."));
    return QT_VERSION_CHECK(v.length() > 0 ? v.at(0).toInt() : 0,
                            v.length() > 1 ? v.at(1).toInt() : 0,
                            v.length() > 2 ? v.at(2).toInt() : 0);
}

void AbstractPythonInterface::checkVersions(bool signalOnResult)
{
    QString output = runPackageScript(QStringLiteral("--details"));
    if (output.isEmpty()) {
        return;
    }
    QStringList raw = output.split(QStringLiteral("Version: "));
    QStringList versions;
    for (int i=0; i < raw.count(); i++) {
        QString name = raw.at(i);
        int pos = name.indexOf(QStringLiteral("Name:"));
        if (pos > -1) {
            if (pos != 0) {
                name.remove(0, pos);
            }
            name = name.simplified().section(QLatin1Char(' '), 1, 1);
            QString version = raw.at(i+1);
            version = version.simplified().section(QLatin1Char(' '), 0, 0);
            versions.append(QString("%1 %2").arg(name, version));
            if (m_versions->contains(name)) {
                (*m_versions)[name.toLower()] = version;
            } else {
                m_versions->insert(name.toLower(), version);
            }
        }
    }
    if (signalOnResult) {
        emit checkVersionsResult(versions);
    }
}

QString AbstractPythonInterface::runPackageScript(const QString &mode) {
    if (m_dependencies.keys().isEmpty()) {
        qWarning() << "No dependencies specified";
        emit setupError(i18n("Internal Error: Cannot find dependency list"));
        return {};
    }
    if (!checkSetup()) {
        return {};
    }
    return runScript(QStringLiteral("checkpackages.py"), m_dependencies.keys(), mode);
}

QString AbstractPythonInterface::runScript(const QString &script, QStringList args, const QString &firstarg) {
    QString scriptpath = m_scripts->value(script);
    if (m_pyExec.isEmpty() || scriptpath.isEmpty()) {
        return {};
    }

    if (!firstarg.isEmpty()) {
        args.prepend(firstarg);
    }
    args.prepend(scriptpath);
    QProcess scriptJob;
    scriptJob.start(m_pyExec, args);
    scriptJob.waitForFinished();
    if (scriptJob.exitStatus() != QProcess::NormalExit || scriptJob.exitCode() != 0) {
        KMessageBox::detailedError(pCore->window(), i18n("Error while running python3 script:\n %1", scriptpath),
                                   scriptJob.readAllStandardError());
        return {};
    }
    return scriptJob.readAllStandardOutput();
}
