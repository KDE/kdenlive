/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QFuture>
#include <QQmlApplicationEngine>

class QQmlComponent;

class Splash : public QObject
{
    Q_OBJECT

public:
    Splash(const QString version, const QStringList urls, const QStringList profileIds, const QStringList profileNames, bool showWelcome, bool firstRun,
           bool showCrashRecovery, bool wasUpgraded, QObject *parent = Q_NULLPTR);
    ~Splash() override;
    bool welcomeDisplayed() const;
    bool hasCrashRecovery() const;
    bool wasUpgraded() const;
    bool hasEventLoop() const;
    void setReady();

private:
    QQmlApplicationEngine *m_engine;
    QObject *m_rootObject{nullptr};
    bool m_showWelcome;
    bool m_hasCrashRecovery;
    bool m_wasUpgraded;
    QFuture<void> m_datesCheck;

public Q_SLOTS:
    void fadeOut();
    void hideSplash();
    void fadeOutAndDelete();
    void showProgressMessage(const QString &message, int max = -1);

private Q_SLOTS:
    void updateWelcomeDisplay(bool show);
    void switchPalette(bool dark);
    void clearHistory();
    void forgetFile(const QString &path);
    void clearProfiles();
    void forgetProfile(const QString &path);
    void calculateDates(QPromise<int> &promise, const QStringList filesList) const;

Q_SIGNALS:
    void openBlank();
    void openOtherFile();
    void closeApp();
    void openFile(QString);
    void openLink(QString);
    void openTemplate(QString);
    void firstStart(QString resolution, QString fps, bool interlaced, int vTracks, int aTracks);
    void resetConfig();
    void releaseLock();
};
