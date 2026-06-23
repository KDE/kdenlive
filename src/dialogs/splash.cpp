/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "splash.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "project/projectmanager.h"
#include <QDebug>
#include <QFileInfo>
#include <ki18n_version.h>

#include <KColorSchemeManager>
#include <KColorSchemeModel>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <QDir>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

Splash::Splash(const QString version, const QStringList urls, const QStringList profileIds, const QStringList profileNames, bool showWelcome, bool firstRun,
               bool showCrashRecovery, bool wasUpgraded, QObject *parent)
    : QObject(parent)
    , m_showWelcome(showWelcome)
    , m_hasCrashRecovery(showCrashRecovery)
    , m_wasUpgraded(wasUpgraded)
{
    m_engine = new QQmlApplicationEngine(this);
    KLocalization::setupLocalizedContext(m_engine);
    qDebug() << ":::::::::: CREATING SPLASH SCREEN";
    QLocale locale;

    QStringList fileNames;
    QStringList fileDates;
    for (auto &s : urls) {
        QFileInfo info(s);
        fileNames.append(info.fileName());
        fileDates.append(locale.toString(info.lastModified(), "dd/MM/yy HH:mm:ss"));
    }
    if (firstRun) {
        // Default to dark palette on first run
        switchPalette(true);
    } else {
        // Initialize SchemeManager to ensure correct color scheme on start
        KColorSchemeManager::instance();
    }

    if (showWelcome) {
        bool isPalFps = ProjectManager::getDefaultProjectFormat().endsWith(QLatin1String("_25"));
        m_engine->setInitialProperties({{"version", version},
                                        {"fileNames", fileNames},
                                        {"fileDates", fileDates},
                                        {"urls", urls},
                                        {"profileIds", profileIds},
                                        {"profileNames", profileNames},
                                        {"palFps", isPalFps},
                                        {"firstRun", firstRun},
                                        {"crashRecovery", m_hasCrashRecovery},
                                        {"wasUpgraded", m_wasUpgraded}});
    } else {
        m_engine->setInitialProperties({{"version", version}, {"crashRecovery", m_hasCrashRecovery}, {"wasUpgraded", m_wasUpgraded}});
    }
    m_engine->load(showWelcome ? QUrl(QStringLiteral("qrc:/qt/qml/org/kde/kdenlive/Splash.qml"))
                               : QUrl(QStringLiteral("qrc:/qt/qml/org/kde/kdenlive/Simplesplash.qml")));
    m_rootObject = m_engine->rootObjects().at(0);
    if (showWelcome) {
        connect(m_rootObject, SIGNAL(openBlank()), this, SIGNAL(openBlank()));
        connect(m_rootObject, SIGNAL(openOtherFile()), this, SIGNAL(openOtherFile()));
        connect(m_rootObject, SIGNAL(closeApp()), this, SIGNAL(closeApp()));
        connect(m_rootObject, SIGNAL(openFile(QString)), this, SIGNAL(openFile(QString)));
        connect(m_rootObject, SIGNAL(openLink(QString)), this, SIGNAL(openLink(QString)));
        connect(m_rootObject, SIGNAL(openTemplate(QString)), this, SIGNAL(openTemplate(QString)));
        connect(m_rootObject, SIGNAL(showWelcome(bool)), this, SLOT(updateWelcomeDisplay(bool)));
        connect(m_rootObject, SIGNAL(switchPalette(bool)), this, SLOT(switchPalette(bool)));
        connect(m_rootObject, SIGNAL(clearHistory()), this, SLOT(clearHistory()));
        connect(m_rootObject, SIGNAL(clearProfiles()), this, SLOT(clearProfiles()));
        connect(m_rootObject, SIGNAL(forgetFile(QString)), this, SLOT(forgetFile(QString)));
        connect(m_rootObject, SIGNAL(forgetProfile(QString)), this, SLOT(forgetProfile(QString)));

        // All signals should also trigger a release lock
        connect(m_rootObject, SIGNAL(resetConfig()), this, SIGNAL(resetConfig()));
        connect(m_rootObject, SIGNAL(openBlank()), this, SIGNAL(releaseLock()));
        connect(m_rootObject, SIGNAL(openOtherFile()), this, SIGNAL(releaseLock()));
        connect(m_rootObject, SIGNAL(openFile(QString)), this, SIGNAL(releaseLock()));
        connect(m_rootObject, SIGNAL(openTemplate(QString)), this, SIGNAL(releaseLock()));
        connect(m_rootObject, SIGNAL(firstStart(QString, QString, bool, int, int)), this, SIGNAL(releaseLock()));
        connect(m_rootObject, SIGNAL(firstStart(QString, QString, bool, int, int)), this, SIGNAL(firstStart(QString, QString, bool, int, int)));
    } else {
        if (m_hasCrashRecovery || m_wasUpgraded) {
            connect(m_rootObject, SIGNAL(resetConfig()), this, SIGNAL(resetConfig()));
            connect(m_rootObject, SIGNAL(openLink(QString)), this, SIGNAL(openLink(QString)));
            connect(m_rootObject, SIGNAL(openBlank()), this, SIGNAL(openBlank()));
            connect(m_rootObject, SIGNAL(openBlank()), this, SIGNAL(releaseLock()));
        }
    }
    if (m_wasUpgraded) {
        const QStringList currentVersion = version.split(QLatin1Char('.'));
        if (currentVersion.length() == 3) {
            // Store latest version
            KdenliveSettings::setLastSeenVersionMajor(currentVersion.at(0).toInt());
            KdenliveSettings::setLastSeenVersionMinor(currentVersion.at(1).toInt());
            KdenliveSettings::setLastSeenVersionMicro(currentVersion.at(2).toInt());
        }
    }
    qApp->processEvents(QEventLoop::AllEvents);
}

bool Splash::welcomeDisplayed() const
{
    return m_showWelcome;
}

bool Splash::hasCrashRecovery() const
{
    return m_hasCrashRecovery;
}

bool Splash::hasEventLoop() const
{
    return m_hasCrashRecovery || m_showWelcome || (!m_showWelcome && m_wasUpgraded);
}

bool Splash::wasUpgraded() const
{
    return m_wasUpgraded;
}

void Splash::updateWelcomeDisplay(bool show)
{
    KdenliveSettings::setShowWelcome(show);
}

void Splash::fadeOutAndDelete()
{
    QMetaObject::invokeMethod(m_rootObject, "close");
    // Plan deletion
    QTimer::singleShot(100, this, &Splash::deleteLater);
}

void Splash::fadeOut()
{
    QMetaObject::invokeMethod(m_rootObject, "fade");
}

void Splash::setReady()
{
    QMetaObject::invokeMethod(m_rootObject, "enableActions");
}

void Splash::switchPalette(bool dark)
{
    KColorSchemeManager::instance()->activateSchemeId(dark ? QStringLiteral("BreezeDark") : QString());
}

void Splash::showProgressMessage(const QString &message, int)
{
    QMetaObject::invokeMethod(m_rootObject, "displayProgress", Qt::DirectConnection, Q_ARG(QVariant, message));
    qApp->processEvents();
}

void Splash::clearHistory()
{
    KConfigGroup history(KSharedConfig::openConfig(), "Recent Files");
    history.deleteGroup();
}

void Splash::forgetFile(const QString &path)
{
    KConfigGroup history(KSharedConfig::openConfig(), "Recent Files");
    auto entires = history.entryMap();
    QString entryName;
    for (auto i = entires.cbegin(), end = entires.cend(); i != end; ++i) {
        if (i.value() == path) {
            entryName = i.key();
            break;
        }
    }
    if (!entryName.isEmpty()) {
        history.deleteEntry(entryName);
        entryName.replace(QStringLiteral("File"), QStringLiteral("Name"));
        history.deleteEntry(entryName);
    }
}

void Splash::forgetProfile(const QString &path)
{
    KConfigGroup history(KSharedConfig::openConfig(), "Recent Profiles");
    QStringList profileIds = history.readEntry("recentProfiles").split(QLatin1Char(','));
    QStringList profileNames = history.readEntry("recentProfileNames").split(QLatin1Char(','));
    int ix = profileIds.indexOf(path);
    if (ix > -1) {
        profileIds.removeAt(ix);
        if (profileNames.size() > ix) {
            profileNames.removeAt(ix);
        }
        history.writeEntry("recentProfiles", profileIds);
        history.writeEntry("recentProfileNames", profileNames);
    }
}

void Splash::clearProfiles()
{
    KConfigGroup history(KSharedConfig::openConfig(), "Recent Profiles");
    history.deleteGroup();
}
