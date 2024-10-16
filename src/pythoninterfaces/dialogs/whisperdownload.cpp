/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "whisperdownload.h"
#include "core.h"
#include "kdenlivesettings.h"

#include <KIO/Global>
#include <KLocalizedString>
#include <KMessageBox>
#include <KMessageWidget>

#include <QApplication>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDir>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

WhisperDownload::WhisperDownload(SpeechToText *engine, QWidget *parent)
    : QDialog(parent)
    , m_engine(engine)
{
    auto *l = new QVBoxLayout;
    setLayout(l);
    l->addWidget(new QLabel(i18n("Select new models to download"), this));
    m_lw = new QListWidget(this);
    m_lw->setAlternatingRowColors(true);
    m_mw = new KMessageWidget(this);
    m_mw->setCloseButtonVisible(false);
    l->addWidget(m_lw);
    m_mw->setMessageType(KMessageWidget::Information);
    m_mw->setWordWrap(true);
    m_mw->setText(i18n("Checking available models"));
    l->addWidget(m_mw);
    m_downloadGroup = new QGroupBox(this);
    m_downloadLayout = new QHBoxLayout;
    QLabel *lab = new QLabel(i18n("Downloading"), this);
    m_downloadLayout->addWidget(lab);
    m_pb = new QProgressBar(this);
    m_downloadLayout->addWidget(m_pb);
    m_downloadGroup->setLayout(m_downloadLayout);
    l->addWidget(m_downloadGroup);
    m_downloadGroup->setVisible(false);
    m_bd = new QPushButton(i18n("Download selected models"), this);
    l->addWidget(m_bd);
    m_bd->setEnabled(false);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    l->addWidget(buttonBox);
    connect(m_lw, &QListWidget::itemChanged, this, &WhisperDownload::checkItemList);
    connect(buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &WhisperDownload::queryClose);
    // Fetch the model names
    connect(m_engine, &SpeechToText::scriptFeedback, this, &WhisperDownload::parseScriptFeedback);
    connect(m_engine, &SpeechToText::concurrentScriptFinished, this, &WhisperDownload::scriptFinished);
    connect(m_engine, &SpeechToText::installFeedback, this, &WhisperDownload::installFeedback);
    m_engine->runConcurrentScript(QStringLiteral("whisper/whisperquery.py"), {QStringLiteral("task=list")});
    connect(m_bd, &QPushButton::clicked, this, &WhisperDownload::downloadModels);
}

void WhisperDownload::installFeedback(const QString &feedback)
{
    qDebug() << "__________\nSCRIPT FB: " << feedback << "\n______________";
    if (feedback.contains(QLatin1Char('%'))) {
        bool ok;
        int progress = feedback.section(QLatin1Char('%'), 0, 0).simplified().toInt(&ok);
        if (ok && progress > m_downloadProgress) {
            // Display download progress
            m_downloadProgress = progress;
            m_pb->setValue(m_downloadProgress);
        }
    }
}

void WhisperDownload::downloadModels()
{
    for (int i = 0; i < m_lw->count(); i++) {
        auto item = m_lw->item(i);
        if (item->checkState() == Qt::Checked) {
            Qt::ItemFlags flags = m_lw->item(i)->flags();
            if (!(flags & Qt::ItemIsEnabled)) {
                continue;
            }
            m_mw->setVisible(false);
            item->setFlags(Qt::ItemIsUserCheckable);
            const QString modelName = item->data(WPModelNameRole).toString();
            QStringList args = {QStringLiteral("task=download"), QStringLiteral("model=%1").arg(modelName)};
            m_downloadProgress = 0;
            m_engine->runConcurrentScript(QStringLiteral("whisper/whisperquery.py"), args, true);
            m_downloadGroup->setVisible(true);
            break;
        }
    }
}

void WhisperDownload::checkItemList()
{
    bool found = false;
    for (int i = 0; i < m_lw->count(); i++) {
        QListWidgetItem *item = m_lw->item(i);
        if (item->checkState() == Qt::Checked) {
            found = true;
            m_bd->setEnabled(true);
            m_mw->setText(i18n("Model download can take several minutes depending on your connection speed"));
            m_mw->animatedShow();
            break;
        }
    }
    if (!found) {
        m_bd->setEnabled(false);
        m_mw->hide();
    }
}

void WhisperDownload::parseScriptFeedback(const QString &scriptName, const QStringList args, const QStringList jobData)
{
    // Check Whisper's model names
    if (scriptName.contains("whisperquery")) {
        qDebug() << "::: GOT WHISPER RESULT B: " << jobData << ", ARGS: " << args;
        if (args.contains(QStringLiteral("task=size"))) {
            // we just queried the download size for the selected model
            for (auto &s : jobData) {
                if (s.contains(QLatin1Char(':'))) {
                    const QString modelName = s.section(QLatin1Char(':'), 0, 0).simplified();
                    qulonglong size = s.section(QLatin1Char(':'), 1).simplified().toULongLong();
                    qulonglong totalSize = 0;
                    qDebug() << "////// GOT MODEL NAME AND SIZE: " << modelName << " = " << size;
                    for (int i = 0; i < m_lw->count(); i++) {
                        QListWidgetItem *item = m_lw->item(i);
                        qDebug() << "==== TRYIG TO MATCH: " << modelName << " = " << item->data(WPModelNameRole).toString();
                        if (size > 0 && item->data(WPModelNameRole).toString() == modelName) {
                            // Match
                            item->setData(WPSizeRole, size);
                        }
                        if (item->checkState() == Qt::Checked) {
                            totalSize += item->data(WPSizeRole).toULongLong();
                        }
                    }
                    if (totalSize > 0) {
                        m_mw->setText(i18n("Total download size: %1", KIO::convertSize(totalSize)));
                        m_mw->show();
                    } else {
                        m_mw->hide();
                    }
                }
            }
            // QMetaObject::invokeMethod(this, "checkWhisperModelSize", Qt::QueuedConnection);
            return;
        }
        if (args.contains(QStringLiteral("task=download"))) {
            // check if model is now correcty downloaded
            return;
        }
        // Don't display installed or duplicated models'
        m_mw->animatedHide();
        QStringList excludedModels = m_engine->getInstalledModels();
        excludedModels.append({QStringLiteral("large-v1"), QStringLiteral("large-v2"), QStringLiteral("large-v3"), QStringLiteral("large-v3-turbo")});
        QStringList updatedModelsList;
        for (auto &s : jobData) {
            const QString name = s.section(QLatin1Char(':'), 0, 0).simplified();
            const QString url = s.section(QLatin1Char(':'), 1).simplified();
            const QString fileName = QUrl(url).fileName();
            if (name.isEmpty()) {
                continue;
            }
            if (name != QLatin1String("root_folder")) {
                updatedModelsList << QString("%1=%2").arg(name, fileName);
            }
            if (excludedModels.contains(name)) {
                // don't display already installed models in list'
                continue;
            }
            if (name == QLatin1String("root_folder")) {
                KdenliveSettings::setWhisperModelFolder(s.section(QLatin1Char(':'), 1).simplified());
                qDebug() << "+++\nSET WHISPER FOLDER: " << KdenliveSettings::whisperModelFolder() << "\n\n+++++";
                continue;
            }
            QString displayName = name;
            displayName[0] = displayName[0].toUpper();
            QListWidgetItem *item = new QListWidgetItem(displayName, m_lw);
            item->setData(WPModelNameRole, name);
            item->setData(WPUrlRole, url);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);
        }
        if (!updatedModelsList.isEmpty()) {
            KdenliveSettings::setWhisperAvailableModels(updatedModelsList);
        }
    }
    // QMetaObject::invokeMethod(this, "checkWhisperModelFolder", Qt::QueuedConnection);
}

void WhisperDownload::scriptFinished(const QString &scriptName, const QStringList &args)
{
    if (scriptName.contains("whisperquery")) {
        if (args.contains(QLatin1String("task=list"))) {
            // Query model sizes
        } else if (args.contains(QLatin1String("task=download"))) {
            m_downloadGroup->setVisible(false);
            m_newModelInstalled = true;
            m_downloadProgress = 0;
            QString modelName;
            for (auto a : args) {
                if (a.startsWith(QStringLiteral("model="))) {
                    modelName = a.section(QLatin1Char('='), 1);
                    break;
                }
            }
            if (!modelName.isEmpty()) {
                for (int i = 0; i < m_lw->count(); i++) {
                    QListWidgetItem *item = m_lw->item(i);
                    if (item->data(WPModelNameRole).toString() == modelName) {
                        item->setCheckState(Qt::Unchecked);
                        item->setFlags(Qt::ItemIsSelectable);
                        break;
                    }
                }
            }
        }
    }
}

bool WhisperDownload::newModelsInstalled()
{
    return m_newModelInstalled;
}

void WhisperDownload::queryClose()
{
    qDebug() << "..... QUERYING CLOSE";
    // Check if a download is in progress
    if (m_downloadGroup->isVisible()) {
        if (KMessageBox::questionTwoActions(this, i18n("A download is in progress, do you want to abort it ?"), {}, KGuiItem(i18n("Abort download")),
                                            KStandardGuiItem::cancel()) == KMessageBox::SecondaryAction) {
            return;
        }
        // Kill download job
        m_lw->setEnabled(false);
        Q_EMIT m_engine->abortScript();
    }
    close();
}

// const QString message = i18n("The selected model %1 <b>must be downloaded (size of %2)</b>.<br/>Whisper processing on cpu is slow.",

/*m_configSpeech.combo_wr_model->currentText(), KIO::convertSize(size));
if (KMessageBox::questionTwoActions(QApplication::activeWindow(), message, {}, KGuiItem(i18n("Download model")),
KStandardGuiItem::cancel()) == KMessageBox::SecondaryAction) {
return;
}
const QString currentModel = m_configSpeech.combo_wr_model->currentData(WPModelNameRole).toString();
QStringList args = {QStringLiteral("task=download"), QStringLiteral("model=%1").arg(currentModel)};
m_sttWhisper->runConcurrentScript(QStringLiteral("whisper/whisperquery.py"), args, true);*/

/*
void KdenliveSettingsDialog::checkWhisperModelSize()
{
    QVariant status = m_configSpeech.combo_wr_model->currentData(WPSizeRole);
    qDebug() << "::: FOUND STATUS: " << status;
    if (status.toString() == QLatin1String("-")) {
        // Already checking for this model
        m_configSpeech.downloadButton->setVisible(true);
        return;
    }
    int size = status.toInt();
    if (size == 0) {
        m_configSpeech.downloadButton->setVisible(true);
        QStringList args = {QStringLiteral("task=size"), QStringLiteral("model=%1").arg(currentModel)};
        m_sttWhisper->runConcurrentScript(QStringLiteral("whisper/whisperquery.py"), args);
    } else if (size == 1) {
        // Model is already downloaded, nothing to do
        m_configSpeech.downloadButton->setVisible(false);
    } else if (size < 0) {
        // Model size not found, maybe offline
        m_configSpeech.downloadButton->setVisible(true);
    } else {
        m_configSpeech.downloadButton->setVisible(true);
    }
}

void KdenliveSettingsDialog::checkWhisperModelFolder()
{
    // Basic info about model folders
    QString folder = KdenliveSettings::whisperModelFolder();
    if (folder.isEmpty()) {
        folder = QStandardPaths::locate(QStandardPaths::GenericCacheLocation, QStringLiteral("whisper"), QStandardPaths::LocateDirectory);
    } else if (!QFileInfo::exists(folder)) {
        folder = QStandardPaths::locate(QStandardPaths::GenericCacheLocation, QStringLiteral("whisper"), QStandardPaths::LocateDirectory);
    }
    QDir modelsFolder(folder);
    if (!modelsFolder.exists()) {
        m_configSpeech.model_folder_label->setVisible(false);
        return;
    }
    m_configSpeech.model_folder_label->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(modelsFolder.absolutePath(), i18n("Models folder")));
    m_configSpeech.model_folder_label->setVisible(true);
    QStringList files = modelsFolder.entryList(QDir::Files);
    for (int i = 0; i < m_configSpeech.combo_wr_model->count(); i++) {

    }
    KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(modelsFolder.absolutePath()));
    connect(job, &KJob::result, this, [job, label = m_configSpeech.model_size]() {
        label->setText(KIO::convertSize(job->totalSize()));
        job->deleteLater();
    });
}*/

/*
void KdenliveSettingsDialog::checkWhisperModelSize()
{
    QVariant status = m_configSpeech.combo_wr_model->currentData(WPSizeRole);
    qDebug() << "::: FOUND STATUS: " << status;
    if (status.toString() == QLatin1String("-")) {
        // Already checking for this model
        m_configSpeech.downloadButton->setVisible(true);
        return;
    }
    int size = status.toInt();
    if (size == 0) {
        m_configSpeech.downloadButton->setVisible(true);
        QStringList args = {QStringLiteral("task=size"), QStringLiteral("model=%1").arg(currentModel)};
        m_sttWhisper->runConcurrentScript(QStringLiteral("whisper/whisperquery.py"), args);
    } else if (size == 1) {
        // Model is already downloaded, nothing to do
        m_configSpeech.downloadButton->setVisible(false);
    } else if (size < 0) {
        // Model size not found, maybe offline
        m_configSpeech.downloadButton->setVisible(true);
    } else {
        m_configSpeech.downloadButton->setVisible(true);
    }
}

void KdenliveSettingsDialog::checkWhisperModelFolder()
{
    // Basic info about model folders
    QString folder = KdenliveSettings::whisperModelFolder();
    if (folder.isEmpty()) {
        folder = QStandardPaths::locate(QStandardPaths::GenericCacheLocation, QStringLiteral("whisper"), QStandardPaths::LocateDirectory);
    } else if (!QFileInfo::exists(folder)) {
        folder = QStandardPaths::locate(QStandardPaths::GenericCacheLocation, QStringLiteral("whisper"), QStandardPaths::LocateDirectory);
    }
    QDir modelsFolder(folder);
    if (!modelsFolder.exists()) {
        m_configSpeech.model_folder_label->setVisible(false);
        return;
    }
    m_configSpeech.model_folder_label->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(modelsFolder.absolutePath(), i18n("Models folder")));
    m_configSpeech.model_folder_label->setVisible(true);
    QStringList files = modelsFolder.entryList(QDir::Files);
    for (int i = 0; i < m_configSpeech.combo_wr_model->count(); i++) {

    }
    KIO::DirectorySizeJob *job = KIO::directorySize(QUrl::fromLocalFile(modelsFolder.absolutePath()));
    connect(job, &KJob::result, this, [job, label = m_configSpeech.model_size]() {
        label->setText(KIO::convertSize(job->totalSize()));
        job->deleteLater();
    });
}
*/
