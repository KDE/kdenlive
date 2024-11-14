/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>

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
#include <QCryptographicHash>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDir>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent>

WhisperDownload::WhisperDownload(SpeechToText *engine, const QString &modelName, QWidget *parent)
    : QDialog(parent)
    , m_engine(engine)
    , m_modelToInstall(modelName)
{
    auto *l = new QVBoxLayout;
    setLayout(l);
    if (m_modelToInstall.isEmpty()) {
        l->addWidget(new QLabel(i18n("Select new models to download"), this));
    } else {
        l->addWidget(new QLabel(i18n("Installing model %1", m_modelToInstall), this));
    }
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
    m_bd = new QPushButton(i18n("Download Model"), this);
    l->addWidget(m_bd);
    m_bd->setEnabled(false);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    l->addWidget(buttonBox);
    connect(m_lw, &QListWidget::currentRowChanged, this, &WhisperDownload::updateDownloadButton);
    connect(buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &WhisperDownload::queryClose);
    // Fetch the model names
    connect(m_engine, &SpeechToText::scriptFeedback, this, &WhisperDownload::parseScriptFeedback);
    connect(m_engine, &SpeechToText::concurrentScriptFinished, this, &WhisperDownload::scriptFinished);
    connect(m_engine, &SpeechToText::installFeedback, this, &WhisperDownload::installFeedback);
    m_engine->runConcurrentScript(QStringLiteral("whisper/whisperquery.py"), {QStringLiteral("task=list")});
    connect(m_bd, &QPushButton::clicked, this, &WhisperDownload::downloadModel);
    if (!m_modelToInstall.isEmpty()) {
        m_lw->setEnabled(false);
    }
    connect(this, &WhisperDownload::itemMatch, [this](const QString hash, bool match) {
        for (int i = 0; i < m_lw->count(); i++) {
            auto *item = m_lw->item(i);
            if (item->data(WPUrlRole).toString().contains(hash)) {
                if (match) {
                    item->setData(WPInstalledRole, 1);
                    item->setIcon(QIcon::fromTheme(QStringLiteral("task-process-4")));
                    if (!m_modelToInstall.isEmpty() && item->data(WPModelNameRole).toString() == m_modelToInstall) {
                        // We are done, exit
                        accept();
                    }
                } else {
                    // Hash does not match, not ok, delete model
                    deleteModel(i);
                }
                break;
            }
        }
        updateDownloadButton(m_lw->currentRow());
    });
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

void WhisperDownload::updateDownloadButton(int ix)
{
    auto item = m_lw->item(ix);
    if (item == nullptr) {
        return;
    }
    int installStatus = item->data(WPInstalledRole).toInt();
    if (installStatus == -1) {
        // We are already downloading this one
        m_bd->setIcon(QIcon::fromTheme("dialog-cancel"));
        m_bd->setText(i18n("Abort downloads"));
    } else {
        m_bd->setEnabled(true);
        if (installStatus == 1) {
            m_bd->setIcon(QIcon::fromTheme("list-remove"));
            m_bd->setText(i18n("Uninstall model"));
        } else {
            m_bd->setIcon(QIcon::fromTheme("list-add"));
            m_bd->setText(i18n("Install model"));
        }
    }
}

void WhisperDownload::downloadModel()
{
    auto item = m_lw->currentItem();
    if (item == nullptr) {
        return;
    }
    if (item->data(WPInstalledRole).toInt() == 0) {
        // Start download
        m_mw->setVisible(false);
        item->setData(WPInstalledRole, -1);
        const QString modelName = item->data(WPModelNameRole).toString();
        const QString url = item->data(WPUrlRole).toString();
        QStringList args = {QStringLiteral("task=download"), QStringLiteral("model=%1").arg(modelName),
                            QStringLiteral("download_root=%1").arg(m_engine->modelFolder()), QStringLiteral("url=%1").arg(url)};
        m_downloadProgress = 0;
        item->setIcon(QIcon::fromTheme(QStringLiteral("task-process-1")));
        m_engine->runConcurrentScript(QStringLiteral("whisper/whisperquery.py"), args, true);
        m_downloadGroup->setVisible(true);
        updateDownloadButton(m_lw->row(item));
    } else if (item->data(WPInstalledRole).toInt() == 1) {
        // Remove model
        deleteModel(m_lw->row(item));
    } else if (item->data(WPInstalledRole).toInt() == -1) {
        // Abort download
        Q_EMIT m_engine->abortScript();
        // Remove model
        deleteModel(m_lw->row(item));
        m_downloadProgress = 0;
    }
}

void WhisperDownload::deleteModel(int row)
{
    QListWidgetItem *item = m_lw->item(row);
    if (item == nullptr) {
        return;
    }
    const QString modelPath = m_engine->modelFolder();
    if (modelPath.isEmpty()) {
        return;
    }
    QDir modelFolder(modelPath);
    const QString fileName = QUrl(item->data(WPUrlRole).toString()).fileName();
    const QString toRemove = modelFolder.absoluteFilePath(fileName);
    QFile::remove(toRemove);
    item->setData(WPInstalledRole, 0);
    item->setIcon(QIcon::fromTheme(QStringLiteral("task-process-0")));
    m_newModelInstalled = true;
    if (row == m_lw->currentRow()) {
        updateDownloadButton(row);
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
            return;
        }
        if (args.contains(QStringLiteral("task=download"))) {
            // check if model is now correcty downloaded
            return;
        }
        // Don't display installed or duplicated models
        const QStringList installedModels = m_engine->getInstalledModels();
        QStringList excludedModels = {QStringLiteral("large-v1"), QStringLiteral("large-v2"), QStringLiteral("large-v3"), QStringLiteral("large-v3-turbo")};
        QStringList updatedModelsList;
        QStringList hashList;
        QListWidgetItem *itemToInstall = nullptr;
        for (auto &s : jobData) {
            const QString name = s.section(QLatin1Char(':'), 0, 0).simplified();
            const QString url = s.section(QLatin1Char(':'), 1).simplified();
            const QString fileName = QUrl(url).fileName();
            const QString hash = url.section(QLatin1Char('/'), -2, -2);
            if (name.isEmpty()) {
                continue;
            }
            if (name != QLatin1String("root_folder")) {
                updatedModelsList << QString("%1=%2").arg(name, fileName);
                hashList << QString("%1=%2").arg(fileName, hash);
            }
            if (excludedModels.contains(name)) {
                // don't display duplicate models in list'
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
            if (!m_modelToInstall.isEmpty() && m_modelToInstall == name) {
                itemToInstall = item;
            }
            item->setData(WPUrlRole, url);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            if (installedModels.contains(name)) {
                item->setData(WPInstalledRole, 1);
                item->setIcon(QIcon::fromTheme(QStringLiteral("task-process-4")));
            } else {
                item->setData(WPInstalledRole, 0);
                item->setIcon(QIcon::fromTheme(QStringLiteral("task-process-0")));
            }
        }
        if (!updatedModelsList.isEmpty()) {
            KdenliveSettings::setWhisperAvailableModels(updatedModelsList);
            KdenliveSettings::setWhisperAvailableModelsHash(hashList);
        }
        if (itemToInstall) {
            m_lw->setCurrentItem(itemToInstall);
            downloadModel();
        }
        checkHashes(m_engine->getInstalledModels());
    }
}

void WhisperDownload::checkHashes(const QStringList modelsToCheck)
{
    // Check hashes
    const QString modelPath = m_engine->modelFolder();
    if (modelPath.isEmpty() || modelsToCheck.isEmpty()) {
        // return
        m_mw->animatedHide();
        return;
    }
    m_mw->setText(i18n("Checking models integrity…"));
    m_mw->show();
    QDir modelFolder(modelPath);
    QMap<QString, QString> hashesToCheck;
    for (int i = 0; i < m_lw->count(); i++) {
        QListWidgetItem *item = m_lw->item(i);
        if (modelsToCheck.contains(item->data(WPModelNameRole).toString())) {
            // Model is installed, check hash
            const QString url = item->data(WPUrlRole).toString();
            const QString fileName = QUrl(url).fileName();
            const QString hash = url.section(QLatin1Char('/'), -2, -2);
            hashesToCheck.insert(modelFolder.absoluteFilePath(fileName), hash);
        }
    }
    if (!hashesToCheck.isEmpty()) {
        (void)QtConcurrent::run(&WhisperDownload::processHashCheck, this, hashesToCheck);
    }
}

void WhisperDownload::processHashCheck(QMap<QString, QString> hashesToCheck)
{
    QMutexLocker lock(&m_closeMutex);
    QMapIterator<QString, QString> i(hashesToCheck);
    while (i.hasNext()) {
        i.next();
        QFile info(i.key());
        if (m_requestClose) {
            return;
        }
        bool hashMatch = false;
        if (info.open(QFile::ReadOnly)) {
            QCryptographicHash shahash(QCryptographicHash::Sha256);
            shahash.addData(&info);
            const QString calculatedHash(shahash.result().toHex());
            hashMatch = calculatedHash == i.value();
        }
        Q_EMIT itemMatch(i.value(), hashMatch);
    }
    QMetaObject::invokeMethod(m_mw, "hide", Qt::QueuedConnection);
}

void WhisperDownload::scriptFinished(const QString &scriptName, const QStringList &args)
{
    if (scriptName.contains("whisperquery")) {
        if (args.contains(QLatin1String("task=list"))) {
            // Nothing
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
            checkHashes({modelName});
        }
    }
}

bool WhisperDownload::newModelsInstalled()
{
    return m_newModelInstalled;
}

void WhisperDownload::queryClose()
{
    // Check if a download is in progress
    if (m_downloadGroup->isVisible()) {
        if (KMessageBox::questionTwoActions(this, i18n("A download is in progress, do you want to abort it ?"), {}, KGuiItem(i18n("Abort download")),
                                            KStandardGuiItem::cancel()) == KMessageBox::SecondaryAction) {
            return;
        }
        // Kill download job
        m_lw->setEnabled(false);
        Q_EMIT m_engine->abortScript();
        for (int i = 0; i < m_lw->count(); i++) {
            auto *item = m_lw->item(i);
            if (item->data(WPInstalledRole).toInt() == -1) {
                // Download was in progress, remove file
                deleteModel(i);
            }
        }
    }
    m_requestClose = true;
    m_closeMutex.lock();
    m_closeMutex.unlock();
    close();
}
