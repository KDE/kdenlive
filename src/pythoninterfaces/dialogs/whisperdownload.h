/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "pythoninterfaces/speechtotext.h"

#include <QDialog>
#include <QMutex>
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
    WhisperDownload(SpeechToText *engine, const QString &modelName, QWidget *parent = nullptr);
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
    bool m_requestClose{false};
    QMutex m_closeMutex;
    int m_downloadProgress{0};
    bool m_newModelInstalled{false};
    void checkHashes(const QStringList modelsToCheck);
    void deleteModel(int row);

private Q_SLOTS:
    void parseScriptFeedback(const QString &scriptName, const QStringList args, const QStringList jobData);
    void scriptFinished(const QString &scriptName, const QStringList &args);
    void downloadModel();
    void installFeedback(const QString &feedback);
    void processHashCheck(QMap<QString, QString> hashesToCheck);
    void updateDownloadButton(int ix);
    void queryClose();

Q_SIGNALS:
    void itemMatch(const QString hash, bool match);
    // void subtitleProgressUpdate(int);
    // void subtitleFinished(int exitCode, QProcess::ExitStatus exitStatus);
};
