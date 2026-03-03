/*
 *    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
 *
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "pythoninterfaces/speechtotext.h"

#include "ui_configspeech_ui.h"
#include <QListWidget>
#include <QWidget>

class SamInterface;
class PythonDependencyMessage;
class KJob;

class SpeechList : public QListWidget
{
    Q_OBJECT

public:
    SpeechList(QWidget *parent = nullptr);

protected:
    QStringList mimeTypes() const override;
    void dropEvent(QDropEvent *event) override;

Q_SIGNALS:
    void getDictionary(const QUrl url);
};

class PluginsSettings : public QWidget, public Ui::ConfigSpeech_UI
{
    Q_OBJECT

public:
    PluginsSettings(QWidget *parent = nullptr);
    ~PluginsSettings() override;
    /** @brief Launch pytonh scripts to check speech engine dependencies */
    void checkSpeechDependencies();
    void applySettings();
    void setActiveTab(int index);

private:
    SpeechToText *m_sttVosk;
    SpeechToText *m_sttWhisper;
    PythonDependencyMessage *m_msgWhisper;
    PythonDependencyMessage *m_msgVosk;
    PythonDependencyMessage *m_pythonSamLabel;
    SamInterface *m_samInterface;
    SpeechList *m_speechListWidget;
    QAction *m_downloadModelAction;

    /** @brief Check folder size */
    void checkWhisperFolderSize();
    /** @brief Refresh the list of available models in combobox */
    void reloadWhisperModels();
    /** @brief Check folder size */
    void checkSamFolderSize();
    /** @brief Allow installing specific cuda version */
    void checkCuda(bool isSam);

private Q_SLOTS:
    void slotParseVoskDictionaries();
    void getDictionary(const QUrl &sourceUrl = QUrl());
    void removeDictionary();
    void downloadModelFinished(KJob *job);
    void processArchive(const QString &path);
    void doShowSpeechMessage(const QString &message, int messageType);
    /** @brief Check required python dependencies for speech engine */
    void slotCheckSttConfig();
    /** @brief Display the python job output */
    void showSpeechLog(const QString &jobData);
    void showSamLog(const QString &jobData);
    /** @brief A download job is finished  */
    void downloadJobDone(bool success);
    /** @brief Start downloading a model */
    void downloadSamModel(const QString &url);
    /** @brief Show a model download dialog */
    void downloadSamModels();
    /** @brief Install a model if none, and refresh the list of available SAM models in combobox */
    void installSamModelIfEmpty();
    /** @brief Refresh the list of available SAM models in combobox */
    void reloadSamModels();
    /** @brief Get ready to delete the venv */
    void doDeleteSamVenv();
    void doDeleteWrVenv();
    /** @brief Get ready to delete the models */
    void doDeleteSamModels();
    /** @brief Check if SAM is correctly setup */
    void checkSamEnvironement(bool afterInstall = true);
    void gotWhisperFeedback(const QString &scriptName, const QStringList args, const QStringList jobData);
    void whisperFinished(const QString &scriptName, const QStringList &args);
    void whisperAvailable();
    void whisperMissing();
    void gotSamFeedback(const QString &scriptName, const QStringList args, const QStringList jobData);
    void samFinished(const QString &scriptName, const QStringList &args);
    void samMissing(const QStringList &);
    void samDependenciesChecked();

Q_SIGNALS:
    void openBrowserUrl(const QString &url);
    /** @brief Trigger parsing of the speech models folder */
    void parseDictionaries();
};
