/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "pythoninterfaces/speechtotext.h"

#include <QDialog>
#include <QFutureSynchronizer>
#include <QProcess>

class QListWidget;
class QListWidgetItem;
class KMessageWidget;
class QPushButton;
class QHBoxLayout;
class QProgressBar;
class QGroupBox;

class WhisperDownload : public QDialog
{
    Q_OBJECT
public:
    enum WhisperRole { WPModelNameRole = Qt::UserRole, WPUrlRole, WPSizeRole, WPInstalledRole };
    enum InstallStatus { NotInstalled = 0, Installed = 1, Downloading = 2 };
    WhisperDownload(SpeechToText *engine, const QString &modelName, QWidget *parent = nullptr);
    ~WhisperDownload() override;
    static const QString getWhisperModelsFolder();
    bool newModelsInstalled();

protected:
private:
    SpeechToText *m_engine;
    QListWidget *m_lw;
    KMessageWidget *m_mw;
    QPushButton *m_bd;
    QHBoxLayout *m_downloadLayout;
    QProgressBar *m_pb;
    QGroupBox *m_downloadGroup;
    QString m_modelToInstall;
    /** @brief The hash check workers. They use this object, so it must not be destroyed before they have returned */
    QFutureSynchronizer<void> m_hashChecks;
    int m_downloadProgress{0};
    bool m_newModelInstalled{false};
    void checkHashes(const QStringList modelsToCheck);
    void deleteModel(int row);
    /** @brief Wait until the running hash checks have finished, then handle their results */
    void waitForHashChecks();

private Q_SLOTS:
    void parseScriptFeedback(const QString &scriptName, const QStringList args, const QStringList jobData);
    void scriptFinished(const QString &scriptName, const QStringList &args);
    void downloadModel();
    void installFeedback(const QString &feedback);
    void processHashCheck(QMap<QString, QString> hashesToCheck);
    void updateDownloadButton(int ix);
    void queryClose();
    void checkDownloads();

Q_SIGNALS:
    void itemMatch(const QString hash, bool match);
    // void subtitleProgressUpdate(int);
    // void subtitleFinished(int exitCode, QProcess::ExitStatus exitStatus);
};
