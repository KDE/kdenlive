/*
 *    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>
 *
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "pluginssettings.h"
#include "core.h"
#include "pythoninterfaces/dialogs/modeldownloadwidget.h"
#include "pythoninterfaces/saminterface.h"
#include "pythoninterfaces/speechtotextvosk.h"
#include "pythoninterfaces/speechtotextwhisper.h"

#include <KArchive>
#include <KArchiveDirectory>
#include <KIO/FileCopyJob>
#include <KMessageBox>
#include <KTar>
#include <KUrlRequesterDialog>
#include <KZip>
#include <kio/directorysizejob.h>

#include <QButtonGroup>
#include <QMimeData>
#include <QMimeDatabase>
#include <QTimer>

SpeechList::SpeechList(QWidget *parent)
    : QListWidget(parent)
{
    setAlternatingRowColors(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    viewport()->setAcceptDrops(true);
}

QStringList SpeechList::mimeTypes() const
{
    return QStringList() << QStringLiteral("text/uri-list");
}

void SpeechList::dropEvent(QDropEvent *event)
{
    const QMimeData *qMimeData = event->mimeData();
    if (qMimeData->hasUrls()) {
        QList<QUrl> urls = qMimeData->urls();
        if (!urls.isEmpty()) {
            Q_EMIT getDictionary(urls.takeFirst());
        }
    }
}

PluginsSettings::PluginsSettings(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
    m_stt = new SpeechToTextVosk(this);
    m_sttWhisper = new SpeechToTextWhisper(this);
    m_samInterface = new SamInterface(this);
    // Contextual help
    whisper_device_info->setContextualHelpText(i18n("CPU processing is very slow, GPU is recommended for faster processing"));
    seamless_device_info->setContextualHelpText(i18n("Text translation is performed by the SeamlessM4T model. This requires downloading "
                                                     "around 10Gb of data. Once installed, all processing will happen offline."));

    // Python env info label
    PythonDependencyMessage *pythonEnvLabel = new PythonDependencyMessage(this, m_sttWhisper, true);
    // Also show VOSK setup messages in the python env page
    connect(m_stt, &AbstractPythonInterface::setupMessage,
            [pythonEnvLabel](const QString message, int type) { pythonEnvLabel->doShowMessage(message, KMessageWidget::MessageType(type)); });
    noModelMessage->hide();
    m_sttWhisper->checkPython(true);
    m_downloadModelAction = new QAction(i18n("Download (1.4Gb)"), this);
    connect(m_downloadModelAction, &QAction::triggered, [this]() {
        disconnect(m_sttWhisper, &SpeechToText::installFeedback, this, &PluginsSettings::showSpeechLog);
        if (m_sttWhisper->installNewModel(QStringLiteral("turbo"))) {
            reloadWhisperModels();
        }
        connect(m_sttWhisper, &SpeechToText::installFeedback, this, &PluginsSettings::showSpeechLog, Qt::QueuedConnection);
    });
    // Fill models list
    reloadWhisperModels();
    const QString seamlessModelScript = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/whisper/seamlessquery.py"));
    ModelDownloadWidget *modelDownload = new ModelDownloadWidget(m_sttWhisper, seamlessModelScript, {QStringLiteral("task=download")}, this);
    connect(modelDownload, &ModelDownloadWidget::installFeedback, this, &PluginsSettings::showSpeechLog, Qt::QueuedConnection);
    connect(modelDownload, &ModelDownloadWidget::jobDone, this, &PluginsSettings::downloadJobDone, Qt::QueuedConnection);
    seamless_layout->addWidget(modelDownload);
    modelDownload->setVisible(false);
    connect(install_seamless, &QPushButton::clicked, this, [this]() {
        const QString scriptPath = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/whisper/requirements-seamless.txt"));
        if (!scriptPath.isEmpty()) {
            m_sttWhisper->addDependency(scriptPath, QString());
        }
        install_seamless->setEnabled(false);
        install_seamless->setText(i18n("Installing multilingual data…"));
        m_sttWhisper->installMissingDependencies();
    });

    QString voskModelFolder = KdenliveSettings::vosk_folder_path();
    if (voskModelFolder.isEmpty()) {
        voskModelFolder = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("speechmodels"), QStandardPaths::LocateDirectory);
    }
    if (!voskModelFolder.isEmpty()) {
        modelV_folder_label->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(voskModelFolder, i18n("Models folder")));
        modelV_folder_label->setVisible(true);
#if defined(Q_OS_WIN)
        // KIO::directorySize doesn't work on Windows
        KIO::filesize_t totalSize = 0;
        const auto flags = QDirListing::IteratorFlag::FilesOnly | QDirListing::IteratorFlag::Recursive;
        for (const auto &dirEntry : QDirListing(voskModelFolder, flags)) {
            totalSize += dirEntry.size();
        }
        modelV_size->setText(KIO::convertSize(totalSize));
#else
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(voskModelFolder));
        connect(job, &KJob::result, this, [job, label = modelV_size]() {
            label->setText(KIO::convertSize(job->totalSize()));
            job->deleteLater();
        });
#endif
    } else {
        modelV_folder_label->setVisible(false);
    }
    connect(modelV_folder_label, &QLabel::linkActivated, [](const QString &link) { pCore->highlightFileInExplorer({QUrl::fromLocalFile(link)}); });
    connect(whisper_folder_label, &QLabel::linkActivated, [](const QString &link) { pCore->highlightFileInExplorer({QUrl::fromLocalFile(link)}); });
    connect(seamless_folder_label, &QLabel::linkActivated, [](const QString &link) { pCore->highlightFileInExplorer({QUrl::fromLocalFile(link)}); });
    connect(sam_folder_label, &QLabel::linkActivated, [](const QString &link) { pCore->highlightFileInExplorer({QUrl::fromLocalFile(link)}); });
    connect(sam_venv_label, &QLabel::linkActivated, [](const QString &link) { pCore->highlightFileInExplorer({QUrl::fromLocalFile(link)}); });
    QButtonGroup *speechEngineSelection = new QButtonGroup(this);
    speechEngineSelection->addButton(engine_vosk);
    speechEngineSelection->addButton(engine_whisper);
    connect(speechEngineSelection, &QButtonGroup::buttonClicked, [this](QAbstractButton *button) {
        if (button == engine_vosk) {
            speech_stack->setCurrentIndex(0);
            m_stt->checkDependencies(false);
        } else if (button == engine_whisper) {
            speech_stack->setCurrentIndex(1);
            m_sttWhisper->checkDependencies(false);
        }
    });
    if (KdenliveSettings::speechEngine() == QLatin1String("whisper")) {
        engine_whisper->setChecked(true);
        speech_stack->setCurrentIndex(1);
    } else {
        engine_vosk->setChecked(true);
        speech_stack->setCurrentIndex(0);
    }
    // Python setup
    connect(m_sttWhisper, &SpeechToText::gotPythonSize, this, [this](const QString &label) {
        wr_venv_size->setText(label);
        deleteWrVenv->setEnabled(!label.isEmpty());
    });

    // Whisper
    if (KdenliveSettings::whisperInstalledModels().isEmpty()) {
        combo_wr_model->setPlaceholderText(i18n("Probing..."));
    }

    combo_wr_device->setPlaceholderText(i18n("Probing..."));
    combo_wr_model->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    combo_wr_device->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    PythonDependencyMessage *msgWhisper = new PythonDependencyMessage(this, m_sttWhisper);
    message_layout_wr->addWidget(msgWhisper);
    QMap<QString, QString> whisperLanguages = m_sttWhisper->speechLanguages();
    QMapIterator<QString, QString> j(whisperLanguages);
    while (j.hasNext()) {
        j.next();
        combo_wr_lang->addItem(j.key(), j.value());
    }
    int ix = combo_wr_lang->findData(KdenliveSettings::whisperLanguage());
    if (ix > -1) {
        combo_wr_lang->setCurrentIndex(ix);
    }
    script_log->hide();
    script_log->setCenterOnScroll(true);
    connect(m_sttWhisper, &SpeechToText::scriptStarted, [this]() { QMetaObject::invokeMethod(script_log, "clear"); });
    connect(m_sttWhisper, &SpeechToText::installFeedback, this, &PluginsSettings::showSpeechLog, Qt::QueuedConnection);
    connect(m_sttWhisper, &SpeechToText::scriptFinished, [this, modelDownload, msgWhisper](const QStringList &args) {
        if (args.join(QLatin1Char(' ')).contains("requirements-seamless.txt")) {
            install_seamless->setText(i18n("Downloading multilingual model…"));
            modelDownload->setVisible(true);
            modelDownload->startDownload();
        }
        QMetaObject::invokeMethod(msgWhisper, "checkAfterInstall", Qt::QueuedConnection);
    });
    connect(downloadButton, &QPushButton::clicked, [this]() {
        disconnect(m_sttWhisper, &SpeechToText::installFeedback, this, &PluginsSettings::showSpeechLog);
        if (m_sttWhisper->installNewModel()) {
            reloadWhisperModels();
        }
        connect(m_sttWhisper, &SpeechToText::installFeedback, this, &PluginsSettings::showSpeechLog, Qt::QueuedConnection);
    });

    connect(m_sttWhisper, &SpeechToText::scriptFeedback, [this](const QString &scriptName, const QStringList args, const QStringList jobData) {
        Q_UNUSED(args);
        if (scriptName.contains("checkgpu")) {
            combo_wr_device->clear();
            for (auto &s : jobData) {
                if (s.contains(QLatin1Char('#'))) {
                    combo_wr_device->addItem(s.section(QLatin1Char('#'), 1).simplified(), s.section(QLatin1Char('#'), 0, 0).simplified());
                } else {
                    combo_wr_device->addItem(s.simplified(), s.simplified());
                }
            }
        }
    });

    connect(deleteWrVenv, &QPushButton::clicked, this, &PluginsSettings::doDeleteWrVenv);

    connect(m_sttWhisper, &SpeechToText::concurrentScriptFinished, [this](const QString &scriptName, const QStringList &args) {
        qDebug() << "=========================\n\nCONCURRENT JOB FINISHED: " << scriptName << " / " << args << "\n\n================";
        if (scriptName.contains("checkgpu")) {
            if (!KdenliveSettings::whisperDevice().isEmpty()) {
                int ix = combo_wr_device->findData(KdenliveSettings::whisperDevice());
                if (ix > -1) {
                    combo_wr_device->setCurrentIndex(ix);
                }
            } else if (combo_wr_device->count() > 0) {
                combo_wr_device->setCurrentIndex(0);
            }
        }
    });
    connect(m_sttWhisper, &SpeechToText::dependenciesAvailable, this, [&]() {
        // Check if a GPU is available
        whispersettings->setEnabled(true);
        m_sttWhisper->runConcurrentScript(QStringLiteral("checkgpu.py"), {});
    });
    connect(m_sttWhisper, &SpeechToText::dependenciesMissing, this, [&](const QStringList &) { whispersettings->setEnabled(false); });

    // VOSK
    vosk_folder->setPlaceholderText(QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("speechmodels"), QStandardPaths::LocateDirectory));
    PythonDependencyMessage *msgVosk = new PythonDependencyMessage(this, m_stt);
    message_layout->addWidget(msgVosk);

    connect(m_stt, &SpeechToText::dependenciesAvailable, this, [&]() {
        if (m_speechListWidget->count() == 0) {
            doShowSpeechMessage(i18n("Please add a speech model."), KMessageWidget::Information);
        }
    });
    connect(m_stt, &SpeechToText::dependenciesMissing, this, [&](const QStringList &) { speech_info->animatedHide(); });
    connect(m_stt, &SpeechToText::scriptStarted, [this]() { QMetaObject::invokeMethod(script_log, "clear"); });
    connect(m_stt, &SpeechToText::installFeedback, this, &PluginsSettings::showSpeechLog, Qt::QueuedConnection);
    connect(m_stt, &SpeechToText::scriptFinished, [msgVosk]() { QMetaObject::invokeMethod(msgVosk, "checkAfterInstall", Qt::QueuedConnection); });

    m_speechListWidget = new SpeechList(this);
    connect(m_speechListWidget, &SpeechList::getDictionary, this, &PluginsSettings::getDictionary);
    QVBoxLayout *l = new QVBoxLayout(list_frame);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(m_speechListWidget);
    speech_info->setWordWrap(true);
    connect(check_config, &QPushButton::clicked, this, &PluginsSettings::slotCheckSttConfig);
    connect(check_config_sam, &QPushButton::clicked, this, &PluginsSettings::slotCheckSamConfig);

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(custom_vosk_folder, &QCheckBox::checkStateChanged, this, [this](Qt::CheckState state) {
#else
    connect(custom_vosk_folder, &QCheckBox::stateChanged, this, [this](int state) {
#endif
        vosk_folder->setEnabled(state != Qt::Unchecked);
        if (state == Qt::Unchecked) {
            // Clear custom folder
            vosk_folder->clear();
            KdenliveSettings::setVosk_folder_path(QString());
            slotParseVoskDictionaries();
        }
    });
    connect(vosk_folder, &KUrlRequester::urlSelected, this, [this](const QUrl &url) {
        KdenliveSettings::setVosk_folder_path(url.toLocalFile());
        slotParseVoskDictionaries();
    });
    models_url->setText(i18n("Download speech models from: <a href=\"https://alphacephei.com/vosk/models\">https://alphacephei.com/vosk/models</a>"));
    connect(models_url, &QLabel::linkActivated, this, &PluginsSettings::openBrowserUrl);
    connect(button_add, &QToolButton::clicked, this, [this]() { this->getDictionary(); });
    connect(button_delete, &QToolButton::clicked, this, &PluginsSettings::removeDictionary);
    connect(this, &PluginsSettings::parseDictionaries, this, &PluginsSettings::slotParseVoskDictionaries);
    slotParseVoskDictionaries();
    checkSpeechDependencies();

    // Sam
    PythonDependencyMessage *pythonSamLabel = new PythonDependencyMessage(this, m_samInterface, false);
    message_layout_sam->addWidget(pythonSamLabel);
    // Also show VOSK setup messages in the python env page
    connect(m_samInterface, &AbstractPythonInterface::setupMessage,
            [pythonSamLabel](const QString message, int type) { pythonSamLabel->doShowMessage(message, KMessageWidget::MessageType(type)); });
    connect(m_samInterface, &SpeechToText::gotPythonSize, this, [this](const QString &label) {
        sam_venv_size->setText(label);
        deleteSamVenv->setEnabled(!label.isEmpty());
    });
    m_samInterface->checkPython(true);
    connect(m_samInterface, &SpeechToText::installFeedback, this, &PluginsSettings::showSamLog, Qt::QueuedConnection);
    connect(m_samInterface, &SpeechToText::scriptFinished,
            [pythonSamLabel]() { QMetaObject::invokeMethod(pythonSamLabel, "checkAfterInstall", Qt::QueuedConnection); });
    combo_sam_model->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(downloadSamButton, &QPushButton::clicked, this, &PluginsSettings::downloadSamModels);
    connect(deleteSamVenv, &QPushButton::clicked, this, &PluginsSettings::doDeleteSamVenv);
    connect(deleteSamModels, &QPushButton::clicked, this, &PluginsSettings::doDeleteSamModels);
    connect(m_samInterface, &AbstractPythonInterface::venvSetupChanged, this,
            [this]() { QMetaObject::invokeMethod(this, "checkSamEnvironement", Qt::QueuedConnection); });
    QDir pluginDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    if (pluginDir.cd(m_samInterface->getVenvPath())) {
        sam_venv_label->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(pluginDir.absolutePath(), i18n("Virtual environment")));
    }
    connect(m_samInterface, &AbstractPythonInterface::dependenciesAvailable, this, [&]() {
        if (combo_sam_model->count() > 0) {
            Q_EMIT pCore->samConfigUpdated();
        }
    });
    checkSamEnvironement(false);
}

void PluginsSettings::checkSamEnvironement(bool afterInstall)
{
    std::pair<QString, QString> exes = m_samInterface->pythonExecs(true);
    if (exes.first.isEmpty() || exes.second.isEmpty()) {
        // Venv not setup
        modelBox->setEnabled(false);
        check_config_sam->setText(i18n("Install"));
        // Update env folder size
        m_samInterface->checkPython(true);
    } else {
        // Venv ready
        modelBox->setEnabled(true);
        check_config_sam->setText(i18n("Check config"));
        // Fill models list
        if (afterInstall) {
            m_samInterface->checkPython(true);
            installSamModelIfEmpty();
        } else {
            reloadSamModels();
        }
    }
}

void PluginsSettings::doDeleteSamModels()
{
    const QStringList models = m_samInterface->getInstalledModels();
    if (!models.isEmpty()) {
        if (KMessageBox::warningContinueCancel(this, i18n("This will delete the installed models\nYou can reinstall them later.")) != KMessageBox::Continue) {
            return;
        }
        for (auto &m : models) {
            if (!m.isEmpty() && m.endsWith(QLatin1String(".pt"))) {
                QFile::remove(m);
            }
        }
        reloadSamModels();
    }
}

void PluginsSettings::doDeleteSamVenv()
{
    QDir pluginDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    if (pluginDir.cd(m_samInterface->getVenvPath())) {
        if (KMessageBox::warningContinueCancel(this,
                                               i18n("This will delete the plugin python environment from:<br/><b>%1</b><br/>The environment will be recreated "
                                                    "and modules downloaded whenever you reenable the plugin.",
                                                    pluginDir.absolutePath())) != KMessageBox::Continue) {
            return;
        }
        m_samInterface->deleteVenv();
    }
}

void PluginsSettings::doDeleteWrVenv()
{
    QDir pluginDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    if (pluginDir.cd(m_sttWhisper->getVenvPath())) {
        if (KMessageBox::warningContinueCancel(this,
                                               i18n("This will delete the plugin python environment from:<br/><b>%1</b><br/>The environment will be recreated "
                                                    "and modules downloaded whenever you reenable the plugin.",
                                                    pluginDir.absolutePath())) != KMessageBox::Continue) {
            return;
        }
        m_sttWhisper->deleteVenv();
    }
}

void PluginsSettings::downloadJobDone(bool success)
{
    KdenliveSettings::setEnableSeamless(success);
    if (!success) {
        install_seamless->setEnabled(true);
    }
    install_seamless->setText(i18n("Install multilingual translation"));
}

void PluginsSettings::reloadWhisperModels()
{
    combo_wr_model->clear();
    const QStringList models = m_sttWhisper->getInstalledModels();
    if (models.isEmpty()) {
        // Show install button
        noModelMessage->setText(i18n("Install a speech model - we recommand <b>turbo</b>"));
        noModelMessage->addAction(m_downloadModelAction);
        noModelMessage->show();
        return;
    }
    noModelMessage->hide();
    for (auto &m : models) {
        if (m.isEmpty()) {
            continue;
        }
        QString modelName = m;
        modelName[0] = m.at(0).toUpper();
        combo_wr_model->addItem(modelName, m);
        qDebug() << ":::: LOADED MODEL: " << modelName << " = " << m;
    }
    int ix = combo_wr_model->findData(KdenliveSettings::whisperModel());
    qDebug() << "LOOKING FOR MODEL: " << KdenliveSettings::whisperModel() << " / IX: " << ix;

    if (ix == -1) {
        ix = 0;
    }
    combo_wr_model->setCurrentIndex(ix);
    checkWhisperFolderSize();
    Q_EMIT pCore->speechModelUpdate(SpeechToTextEngine::EngineWhisper, models);
}

void PluginsSettings::downloadSamModel(const QString &url)
{
    if (!url.startsWith(QLatin1String("https"))) {
        // TODO: error message
        return;
    }
    const QUrl srcUrl(url);
    QDir modelFolder(m_samInterface->modelFolder());
    QUrl saveUrl = QUrl::fromLocalFile(modelFolder.absoluteFilePath(srcUrl.fileName()));
    KIO::FileCopyJob *getJob = KIO::file_copy(srcUrl, saveUrl, -1, KIO::Overwrite);
    connect(getJob, &KJob::result, this, &PluginsSettings::reloadSamModels);
    getJob->start();
}

void PluginsSettings::downloadSamModels()
{
    KConfig conf(QStringLiteral("sammodelsinfo.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, QStringLiteral("models"));
    QStringList modelUrls = group.entryMap().values();
    const QStringList models = m_samInterface->getInstalledModels();
    QStringList installedModels;
    for (auto &m : models) {
        installedModels << QFileInfo(m).fileName();
    }
    QDialog d(this);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto *l = new QVBoxLayout;
    d.setLayout(l);
    QList<QCheckBox *> buttons;
    l->addWidget(new QLabel(i18n("Select model to download"), &d));
    for (auto &u : modelUrls) {
        QString modelName = QUrl(u).fileName();
        if (installedModels.contains(modelName)) {
            // Already installed, drop
            continue;
        }
        modelName[0] = modelName.at(0).toUpper();
        auto *cb = new QCheckBox(modelName, this);
        cb->setProperty("url", u);
        buttons << cb;
        l->addWidget(cb);
    }
    if (buttons.isEmpty()) {
        auto km = new KMessageWidget(&d);
        km->setMessageType(KMessageWidget::Information);
        km->setText(i18n("All models are already installed"));
        km->setCloseButtonVisible(false);
        l->addWidget(km);
    }
    l->addStretch(10);
    l->addWidget(buttonBox);
    d.connect(buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    d.connect(buttonBox, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    if (d.exec() != QDialog::Accepted) {
        return;
    }
    int downloadCount = 0;
    for (auto &b : buttons) {
        if (b->isChecked()) {
            const QString url = b->property("url").toString();
            downloadSamModel(url);
            downloadCount++;
        }
    }
    if (downloadCount > 0) {
        downloadSamButton->setText(i18n("Downloading…"));
        downloadSamButton->setEnabled(false);
    }
}

void PluginsSettings::installSamModelIfEmpty()
{
    const QStringList models = m_samInterface->getInstalledModels();
    if (models.isEmpty()) {
        // Download tiny model by default if none installed
        combo_sam_model->setPlaceholderText(i18n("Downloading smallest model…"));
        KConfig conf(QStringLiteral("sammodelsinfo.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
        KConfigGroup group(&conf, QStringLiteral("models"));
        QMap<QString, QString> values = group.entryMap();
        QMapIterator<QString, QString> i(values);
        while (i.hasNext()) {
            i.next();
            if (i.value().contains(QLatin1String("tiny"))) {
                downloadSamModel(i.value());
            }
        }
        return;
    }
    reloadSamModels();
}

void PluginsSettings::reloadSamModels()
{
    if (!downloadSamButton->isEnabled()) {
        downloadSamButton->setText(i18n("Download models"));
        downloadSamButton->setEnabled(true);
    }
    combo_sam_model->clear();
    const QStringList models = m_samInterface->getInstalledModels();
    for (auto &m : models) {
        if (m.isEmpty()) {
            continue;
        }
        QString modelName = QFileInfo(m).completeBaseName();
        QString listedModel;
        modelName[0] = modelName.at(0).toUpper();
        combo_sam_model->addItem(modelName, m);
    }
    int ix = combo_sam_model->findData(KdenliveSettings::samModelFile());
    qDebug() << "LOOKING FOR MODEL: " << KdenliveSettings::samModelFile() << " / IX: " << ix;

    if (ix == -1) {
        ix = 0;
    }
    combo_sam_model->setCurrentIndex(ix);
    checkSamFolderSize();
    Q_EMIT pCore->samConfigUpdated();
}

void PluginsSettings::checkSamFolderSize()
{
    const QString folder = m_samInterface->modelFolder();
    QDir modelsFolder(folder);
    if (folder.isEmpty() || !modelsFolder.exists()) {
        sam_folder_label->setVisible(false);
    } else {
        const QString path = modelsFolder.absolutePath();
        sam_folder_label->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(path, QStringLiteral("Models folder")));
        sam_folder_label->setVisible(true);
#if defined(Q_OS_WIN)
        // KIO::directorySize doesn't work on Windows
        KIO::filesize_t totalSize = 0;
        const auto flags = QDirListing::IteratorFlag::FilesOnly | QDirListing::IteratorFlag::Recursive;
        for (const auto &dirEntry : QDirListing(path, flags)) {
            totalSize += dirEntry.size();
        }
        sam_model_size->setText(KIO::convertSize(totalSize));
#else
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(path));
        connect(job, &KJob::result, this, [job, label = sam_model_size, button = downloadSamButton]() {
            label->setText(KIO::convertSize(job->totalSize()));
            job->deleteLater();
        });
#endif
    }
}

void PluginsSettings::checkWhisperFolderSize()
{
    const QString folder = m_sttWhisper->modelFolder();
    QDir modelsFolder(folder);
    if (folder.isEmpty() || !modelsFolder.exists()) {
        whisper_folder_label->setVisible(false);
        downloadButton->setText(i18n("Install a model"));
    } else {
        const QString path = modelsFolder.absolutePath();
        whisper_folder_label->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(path, QStringLiteral("Whisper")));
        whisper_folder_label->setVisible(true);
#if defined(Q_OS_WIN)
        // KIO::directorySize doesn't work on Windows
        KIO::filesize_t totalSize = 0;
        const auto flags = QDirListing::IteratorFlag::FilesOnly | QDirListing::IteratorFlag::Recursive;
        for (const auto &dirEntry : QDirListing(path, flags)) {
            totalSize += dirEntry.size();
        }
        whisper_model_size->setText(KIO::convertSize(totalSize));
        if (totalSize == 0) {
            downloadButton->setText(i18n("Install a model"));
        } else {
            downloadButton->setText(i18n("Manage models"));
        }
#else
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(path));
        connect(job, &KJob::result, this, [job, label = whisper_model_size, button = downloadButton]() {
            label->setText(KIO::convertSize(job->totalSize()));
            if (job->totalSize() == 0) {
                button->setText(i18n("Install a model"));
            } else {
                button->setText(i18n("Manage models"));
            }
            job->deleteLater();
        });
#endif
    }
    const QString folder2 = m_sttWhisper->modelFolder(false);
    QDir seamlessFolder(folder2);
    if (folder2.isEmpty() || !seamlessFolder.exists()) {
        seamless_folder_label->setVisible(false);
    } else {
        seamless_folder_label->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(seamlessFolder.absolutePath(), QStringLiteral("Seamless")));
        seamless_folder_label->setVisible(true);
#if defined(Q_OS_WIN)
        // KIO::directorySize doesn't work on Windows
        KIO::filesize_t totalSize = 0;
        const auto flags = QDirListing::IteratorFlag::FilesOnly | QDirListing::IteratorFlag::Recursive;
        for (const auto &dirEntry : QDirListing(seamlessFolder.absolutePath(), flags)) {
            totalSize += dirEntry.size();
        }
        seamless_folder_label->setText(KIO::convertSize(totalSize));
#else
        KIO::DirectorySizeJob *jobSeamless = KIO::directorySize(QUrl::fromLocalFile(seamlessFolder.absolutePath()));
        connect(jobSeamless, &KJob::result, this, [jobSeamless, label = seamless_folder_size]() {
            label->setText(KIO::convertSize(jobSeamless->totalSize()));
            jobSeamless->deleteLater();
        });
#endif
    }
}

void PluginsSettings::showSpeechLog(const QString &jobData)
{
    script_log->show();
    script_log->appendPlainText(jobData);
    script_log->ensureCursorVisible();
}

void PluginsSettings::showSamLog(const QString &jobData)
{
    script_sam_log->show();
    script_sam_log->appendPlainText(jobData);
    script_sam_log->ensureCursorVisible();
}

void PluginsSettings::slotCheckSttConfig()
{
    check_config->setEnabled(false);
    qApp->processEvents();
    if (engine_vosk->isChecked()) {
        m_stt->checkDependencies(true);
    } else {
        m_sttWhisper->checkDependencies(true);
    }
    // Leave button disabled for 3 seconds so that the user doesn't trigger it again while it is processing
    QTimer::singleShot(3000, this, [&]() { check_config->setEnabled(true); });
}

void PluginsSettings::slotCheckSamConfig()
{
    check_config_sam->setEnabled(false);
    qApp->processEvents();
    m_samInterface->checkDependencies(true);
    // Leave button disabled for 3 seconds so that the user doesn't trigger it again while it is processing
    QTimer::singleShot(3000, this, [&]() { check_config_sam->setEnabled(true); });
}

void PluginsSettings::doShowSpeechMessage(const QString &message, int messageType)
{
    speech_info->setMessageType(static_cast<KMessageWidget::MessageType>(messageType));
    speech_info->setText(message);
    speech_info->animatedShow();
}

void PluginsSettings::getDictionary(const QUrl &sourceUrl)
{
    QUrl url = KUrlRequesterDialog::getUrl(sourceUrl, this, i18n("Enter url for the new dictionary"));
    if (url.isEmpty()) {
        return;
    }
    if (!url.isLocalFile()) {
        KIO::FileCopyJob *copyjob = KIO::file_copy(url, QUrl::fromLocalFile(QDir::temp().absoluteFilePath(url.fileName())));
        doShowSpeechMessage(i18n("Downloading model…"), KMessageWidget::Information);
        connect(copyjob, &KIO::FileCopyJob::result, this, &PluginsSettings::downloadModelFinished);
    } else {
        processArchive(url.toLocalFile());
    }
}

void PluginsSettings::removeDictionary()
{
    if (!KdenliveSettings::vosk_folder_path().isEmpty()) {
        doShowSpeechMessage(i18n("We do not allow deleting custom folder models, please do it manually."), KMessageWidget::Warning);
        return;
    }
    QString modelDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(modelDirectory);
    if (!dir.cd(QStringLiteral("speechmodels"))) {
        doShowSpeechMessage(i18n("Cannot access dictionary folder."), KMessageWidget::Warning);
        return;
    }

    if (!m_speechListWidget->currentItem()) {
        return;
    }
    QString currentModel = m_speechListWidget->currentItem()->text();
    if (!currentModel.isEmpty() && dir.cd(currentModel)) {
        if (KMessageBox::questionTwoActions(this, i18n("Delete folder:\n%1", dir.absolutePath()), {}, KStandardGuiItem::del(), KStandardGuiItem::cancel()) ==
            KMessageBox::PrimaryAction) {
            // Make sure we don't accidentally delete a folder that is not ours
            if (dir.absolutePath().contains(QLatin1String("speechmodels"))) {
                dir.removeRecursively();
                slotParseVoskDictionaries();
            }
        }
    }
}

void PluginsSettings::downloadModelFinished(KJob *job)
{
    if (job->error() == 0 || job->error() == 112) {
        auto *jb = static_cast<KIO::FileCopyJob *>(job);
        if (jb) {
            QString archiveFile = jb->destUrl().toLocalFile();
            processArchive(archiveFile);
        } else {
            speech_info->setMessageType(KMessageWidget::Warning);
            speech_info->setText(i18n("Download error"));
        }
    } else {
        speech_info->setMessageType(KMessageWidget::Warning);
        speech_info->setText(i18n("Download error %1", job->errorString()));
    }
}

void PluginsSettings::processArchive(const QString &archiveFile)
{
    QMimeDatabase db;
    QMimeType type = db.mimeTypeForFile(archiveFile);
    std::unique_ptr<KArchive> archive;
    if (type.inherits(QStringLiteral("application/zip"))) {
        archive = std::make_unique<KZip>(archiveFile);
    } else {
        archive = std::make_unique<KTar>(archiveFile);
    }
    QString modelDirectory = KdenliveSettings::vosk_folder_path();
    QDir dir;
    if (modelDirectory.isEmpty()) {
        modelDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        dir = QDir(modelDirectory);
        dir.mkdir(QStringLiteral("speechmodels"));
        if (!dir.cd(QStringLiteral("speechmodels"))) {
            doShowSpeechMessage(i18n("Cannot access dictionary folder."), KMessageWidget::Warning);
            return;
        }
    } else {
        dir = QDir(modelDirectory);
    }
    if (archive->open(QIODevice::ReadOnly)) {
        doShowSpeechMessage(i18n("Extracting archive…"), KMessageWidget::Information);
        const KArchiveDirectory *archiveDir = archive->directory();
        if (!archiveDir->copyTo(dir.absolutePath())) {
            qDebug() << "=== Error extracting archive!!";
        } else {
            QFile::remove(archiveFile);
            Q_EMIT parseDictionaries();
            doShowSpeechMessage(i18n("New dictionary installed."), KMessageWidget::Positive);
        }
    } else {
        // Test if it is a folder
        QDir testDir(archiveFile);
        if (testDir.exists()) {
        }
        qDebug() << "=== CANNOT OPEN ARCHIVE!!";
    }
}

void PluginsSettings::slotParseVoskDictionaries()
{
    m_speechListWidget->clear();
    QStringList final = m_stt->getInstalledModels();
    m_speechListWidget->addItems(final);
    QString voskModelFolder;
    if (!KdenliveSettings::vosk_folder_path().isEmpty()) {
        custom_vosk_folder->setChecked(true);
        vosk_folder->setUrl(QUrl::fromLocalFile(KdenliveSettings::vosk_folder_path()));
        voskModelFolder = KdenliveSettings::vosk_folder_path();
    } else {
        voskModelFolder = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("speechmodels"), QStandardPaths::LocateDirectory);
    }
    if (!final.isEmpty() && m_stt->missingDependencies().isEmpty()) {
        speech_info->animatedHide();
    } else if (final.isEmpty()) {
        doShowSpeechMessage(i18n("Please add a speech model."), KMessageWidget::Information);
    }
    if (!voskModelFolder.isEmpty()) {
        modelV_folder_label->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(voskModelFolder, i18n("Models folder")));
        modelV_folder_label->setVisible(true);
#if defined(Q_OS_WIN)
        // KIO::directorySize doesn't work on Windows
        KIO::filesize_t totalSize = 0;
        const auto flags = QDirListing::IteratorFlag::FilesOnly | QDirListing::IteratorFlag::Recursive;
        for (const auto &dirEntry : QDirListing(voskModelFolder, flags)) {
            totalSize += dirEntry.size();
        }
        modelV_size->setText(KIO::convertSize(totalSize));
#else
        KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(voskModelFolder));
        connect(job, &KJob::result, this, [job, label = modelV_size]() {
            label->setText(KIO::convertSize(job->totalSize()));
            job->deleteLater();
        });
#endif
    } else {
        modelV_folder_label->setVisible(false);
    }
    Q_EMIT pCore->speechModelUpdate(SpeechToTextEngine::EngineVosk, final);
}

void PluginsSettings::checkSpeechDependencies()
{
    m_sttWhisper->checkDependenciesConcurrently();
    m_stt->checkDependenciesConcurrently();
}

void PluginsSettings::setActiveTab(int index)
{
    tabWidget->setCurrentIndex(index);
}

void PluginsSettings::applySettings()
{
    // Speech
    switch (speech_stack->currentIndex()) {
    case 1:
        if (KdenliveSettings::speechEngine() != QLatin1String("whisper")) {
            KdenliveSettings::setSpeechEngine(QStringLiteral("whisper"));
            Q_EMIT pCore->speechEngineChanged();
        }
        break;
    default:
        if (!KdenliveSettings::speechEngine().isEmpty()) {
            KdenliveSettings::setSpeechEngine(QString());
            Q_EMIT pCore->speechEngineChanged();
        }
        break;
    }

    if (combo_wr_lang->currentData().toString() != KdenliveSettings::whisperLanguage()) {
        KdenliveSettings::setWhisperLanguage(combo_wr_lang->currentData().toString());
    }
    if (combo_wr_model->currentData().toString() != KdenliveSettings::whisperModel()) {
        KdenliveSettings::setWhisperModel(combo_wr_model->currentData().toString());
    }
    if (combo_sam_model->currentData().toString() != KdenliveSettings::samModelFile()) {
        KdenliveSettings::setSamModelFile(combo_sam_model->currentData().toString());
        Q_EMIT pCore->samConfigUpdated();
    }
    if (combo_wr_device->currentData().toString() != KdenliveSettings::whisperDevice()) {
        KdenliveSettings::setWhisperDevice(combo_wr_device->currentData().toString());
    }
}
