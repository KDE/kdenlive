/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QQmlApplicationEngine>

class QQmlComponent;

class Splash : public QObject
{
    Q_OBJECT

public:
    Splash(const QString version, const QStringList urls, const QStringList profileIds, const QStringList profileNames, bool showWelcome, bool firstRun,
           bool showCrashRecovery, bool wasUpgraded, QObject *parent = Q_NULLPTR);
    bool welcomeDisplayed() const;
    bool hasCrashRecovery() const;
    bool wasUpgraded() const;
    bool hasEventLoop() const;

private:
    QQmlApplicationEngine *m_engine;
    QObject *m_rootObject{nullptr};
    bool m_showWelcome;
    bool m_hasCrashRecovery;
    bool m_wasUpgraded;

public Q_SLOTS:
    void fadeOut();
    void fadeOutAndDelete();
    void hideAndDelete();
    void showProgressMessage(const QString &message, int max = -1);

private Q_SLOTS:
    void updateWelcomeDisplay(bool show);

Q_SIGNALS:
    void openBlank();
    void openOtherFile();
    void closeApp();
    void openFile(QString);
    void openLink(QString);
    void openTemplate(QString);
    void switchPalette(bool);
    void firstStart(QString resolution, QString fps, bool interlaced, int vTracks, int aTracks);
    void resetConfig();
    void releaseLock();
    void clearHistory();
    void clearProfiles();
    void forgetFile(QString url);
    void forgetProfile(QString id);
};
