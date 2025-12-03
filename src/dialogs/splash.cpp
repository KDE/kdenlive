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

#if KI18N_VERSION >= QT_VERSION_CHECK(6, 8, 0)
#include <KLocalizedQmlContext>
#else
#include <KLocalizedContext>
#endif
#include <KColorSchemeManager>
#include <KColorSchemeModel>
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
#if KI18N_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    KLocalization::setupLocalizedContext(m_engine);
#else
    m_engine->rootContext()->setContextObject(new KLocalizedContext(this));
#endif
    qDebug() << ":::::::::: CREATING SPLASH SCREEN SPLASH";
    QLocale locale;

#if KI18N_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    KConfigGroup cg(KSharedConfig::openConfig(), "UiSettings");
    const QString theme = cg.readEntry("ColorSchemePath");
    QString schemeId;
    int max = KColorSchemeManager::instance()->model()->rowCount();
    for (int i = 0; i < max; ++i) {
        QModelIndex index = KColorSchemeManager::instance()->model()->index(i, 0);
        if ((theme.isEmpty() && index.data(KColorSchemeModel::PathRole).toString().isEmpty()) ||
            index.data(KColorSchemeModel::PathRole).toString().endsWith(theme)) {
            KColorSchemeManager::instance()->activateScheme(index);
            break;
        }
    }
#endif
    QStringList fileNames;
    QStringList fileDates;
    for (auto &s : urls) {
        QFileInfo info(s);
        fileNames.append(info.fileName());
        fileDates.append(locale.toString(info.lastModified(), "dd/MM/yy HH:mm:ss"));
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
        connect(m_rootObject, SIGNAL(switchPalette(bool)), this, SIGNAL(switchPalette(bool)));
        connect(m_rootObject, SIGNAL(clearHistory()), this, SIGNAL(clearHistory()));
        connect(m_rootObject, SIGNAL(clearProfiles()), this, SIGNAL(clearProfiles()));
        connect(m_rootObject, SIGNAL(forgetFile(QString)), this, SIGNAL(forgetFile(QString)));
        connect(m_rootObject, SIGNAL(forgetProfile(QString)), this, SIGNAL(forgetProfile(QString)));
        if (m_hasCrashRecovery) {
            // All signals should also trigger a release lock
            connect(m_rootObject, SIGNAL(resetConfig()), this, SIGNAL(resetConfig()));
            connect(m_rootObject, SIGNAL(openBlank()), this, SIGNAL(releaseLock()));
            connect(m_rootObject, SIGNAL(openOtherFile()), this, SIGNAL(releaseLock()));
            connect(m_rootObject, SIGNAL(openFile(QString)), this, SIGNAL(releaseLock()));
            connect(m_rootObject, SIGNAL(openTemplate(QString)), this, SIGNAL(releaseLock()));
            connect(m_rootObject, SIGNAL(firstStart(QString, QString, bool, int, int)), this, SIGNAL(releaseLock()));
        }
        connect(m_rootObject, SIGNAL(firstStart(QString, QString, bool, int, int)), this, SIGNAL(firstStart(QString, QString, bool, int, int)));
    } else {
        if (m_hasCrashRecovery || m_wasUpgraded) {
            connect(m_rootObject, SIGNAL(resetConfig()), this, SIGNAL(resetConfig()));
            connect(m_rootObject, SIGNAL(openLink(QString)), this, SIGNAL(openLink(QString)));
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
    return m_hasCrashRecovery || (!m_showWelcome && m_wasUpgraded);
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
    QMetaObject::invokeMethod(m_rootObject, "fade");
    // Plan deletion
    QTimer::singleShot(100, this, &Splash::deleteLater);
}

void Splash::fadeOut()
{
    QMetaObject::invokeMethod(m_rootObject, "fade");
}

void Splash::showProgressMessage(const QString &message, int)
{
    QMetaObject::invokeMethod(m_rootObject, "displayProgress", Qt::DirectConnection, Q_ARG(QVariant, message));
    qApp->processEvents();
}
