/***************************************************************************
 *   Copyright (C) 2020 by Jean-Baptiste Mardelle                          *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "textbasededit.h"
#include "monitor/monitor.h"
#include "bin/bin.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "mainwindow.h"
#include "kdenlivesettings.h"
#include "timecodedisplay.h"

#include "klocalizedstring.h"

#include <QEvent>
#include <QKeyEvent>
#include <QToolButton>

TextBasedEdit::TextBasedEdit(QWidget *parent)
    : QWidget(parent)
    , m_clipDuration(0.)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    vosk_config->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    vosk_config->setToolTip(i18n("Configure speech recognition"));
    connect(vosk_config, &QToolButton::clicked, [this]() {
        pCore->window()->slotPreferences(8);
    });
    connect(button_start, &QPushButton::clicked, this, &TextBasedEdit::startRecognition);
    listWidget->setWordWrap(true);
    frame_progress->setVisible(false);
    button_abort->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
    connect(button_abort, &QToolButton::clicked, [this]() {
        if (m_speechJob && m_speechJob->state() == QProcess::Running) {
            m_speechJob->kill();
        }
    });
    connect(pCore.get(), &Core::updateVoskAvailability, this, &TextBasedEdit::updateAvailability);
    connect(pCore.get(), &Core::voskModelUpdate, [&](QStringList models) {
        language_box->clear();
        language_box->addItems(models);
        updateAvailability();
        if (models.isEmpty()) {
            info_message->setMessageType(KMessageWidget::Information);
            info_message->setText(i18n("Please install speech recognition models"));
            info_message->animatedShow();
            vosk_config->setVisible(true);
        } else {
            if (KdenliveSettings::vosk_found()) {
                vosk_config->setVisible(false);
            }
            if (!KdenliveSettings::vosk_text_model().isEmpty() && models.contains(KdenliveSettings::vosk_text_model())) {
                int ix = language_box->findText(KdenliveSettings::vosk_text_model());
                if (ix > -1) {
                    language_box->setCurrentIndex(ix);
                }
            }
        }
    });
    connect(language_box, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this]() {
        KdenliveSettings::setVosk_text_model(language_box->currentText());
    });

    connect(listWidget, &QListWidget::itemDoubleClicked, [this] (QListWidgetItem *item) {
        if (item) {
            double startMs = item->data(Qt::UserRole).toDouble();
            double endMs = item->data(Qt::UserRole + 1).toDouble();
            pCore->getMonitor(Kdenlive::ClipMonitor)->requestSeek(GenTime(startMs).frames(pCore->getCurrentFps()));
            pCore->getMonitor(Kdenlive::ClipMonitor)->slotLoadClipZone(QPoint(GenTime(startMs).frames(pCore->getCurrentFps()), GenTime(endMs).frames(pCore->getCurrentFps())));
            pCore->getMonitor(Kdenlive::ClipMonitor)->slotPlayZone();
        }
    });
    connect(listWidget, &QListWidget::currentRowChanged, [this] (int ix) {
        if (ix > -1) {
            QListWidgetItem  *item = listWidget->item(ix);
            if (!item) {
                return;
            }
            double startMs = item->data(Qt::UserRole).toDouble();
            double endMs = item->data(Qt::UserRole + 1).toDouble();
            pCore->getMonitor(Kdenlive::ClipMonitor)->requestSeek(GenTime(startMs).frames(pCore->getCurrentFps()));
            pCore->getMonitor(Kdenlive::ClipMonitor)->slotLoadClipZone(QPoint(GenTime(startMs).frames(pCore->getCurrentFps()), GenTime(endMs).frames(pCore->getCurrentFps())));
        }
    });
    info_message->hide();
    
    // Search stuff
    search_frame->setVisible(false);
    button_search->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
    search_prev->setIcon(QIcon::fromTheme(QStringLiteral("go-up")));
    search_next->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));
    connect(button_search, &QToolButton::toggled, this, [&](bool toggled) {
        search_frame->setVisible(toggled);
    });
    connect(search_line, &QLineEdit::textChanged, [this](const QString &searchText) {
        if (searchText.length() > 2) {
            int ix = listWidget->currentRow();
            bool found = false;
            QListWidgetItem *item;
            while (!found && ix < listWidget->count()) {
                item = listWidget->item(ix);
                if (item) {
                    if (item->text().contains(searchText)) {
                        listWidget->setCurrentRow(ix);
                        found = true;
                        break;
                    }
                }
                ix++;
            }
        }
    });
    connect(search_next, &QToolButton::clicked, [this]() {
        const QString searchText = search_line->text();
        if (searchText.length() > 2) {
            int ix = listWidget->currentRow() + 1;
            bool found = false;
            QListWidgetItem *item;
            while (!found && ix < listWidget->count()) {
                item = listWidget->item(ix);
                if (item) {
                    if (item->text().contains(searchText)) {
                        listWidget->setCurrentRow(ix);
                        found = true;
                        break;
                    }
                }
                ix++;
            }
        }
    });
    connect(search_prev, &QToolButton::clicked, [this]() {
        const QString searchText = search_line->text();
        if (searchText.length() > 2) {
            int ix = listWidget->currentRow() - 1;
            bool found = false;
            QListWidgetItem *item;
            while (!found && ix > 0) {
                item = listWidget->item(ix);
                if (item) {
                    if (item->text().contains(searchText)) {
                        listWidget->setCurrentRow(ix);
                        found = true;
                        break;
                    }
                }
                ix--;
            }
        }
    });
    parseVoskDictionaries();
}

void TextBasedEdit::startRecognition()
{
    info_message->hide();
    QString pyExec = QStandardPaths::findExecutable(QStringLiteral("python3"));
    if (pyExec.isEmpty()) {
        info_message->setMessageType(KMessageWidget::Warning);
        info_message->setText(i18n("Cannot find python3, please install it on your system."));
        info_message->animatedShow();
        return;
    }
    // Start python script
    QString language = language_box->currentText();
    if (language.isEmpty()) {
        info_message->setMessageType(KMessageWidget::Warning);
        info_message->setText(i18n("Please install a language model."));
        info_message->animatedShow();
        return;
    }
    QString speechScript = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("scripts/speechtotext.py"));
    if (speechScript.isEmpty()) {
        info_message->setMessageType(KMessageWidget::Warning);
        info_message->setText(i18n("The speech script was not found, check your install."));
        info_message->animatedShow();
        return;
    }
    m_speechJob.reset(new QProcess(this));
    info_message->setMessageType(KMessageWidget::Information);
    info_message->setText(i18n("Starting speech recognition"));
    qApp->processEvents();
    QString modelDirectory = KdenliveSettings::vosk_folder_path();
    if (modelDirectory.isEmpty()) {
        modelDirectory = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("speechmodels"), QStandardPaths::LocateDirectory);
    }
    qDebug()<<"==== ANALYSIS SPEECH: "<<modelDirectory<<" - "<<language;
    
    m_sourceUrl.clear();
    QString clipName;
    const QString cid = pCore->getMonitor(Kdenlive::ClipMonitor)->activeClipId();
    std::shared_ptr<AbstractProjectItem> clip = pCore->projectItemModel()->getItemByBinId(cid);
    if (clip) {
        std::shared_ptr<ProjectClip> clipItem = std::static_pointer_cast<ProjectClip>(clip);
        if (clipItem) {
            m_sourceUrl = clipItem->url();
            clipName = clipItem->clipName();
            m_clipDuration = clipItem->duration().seconds();
        }
    }
    if (m_sourceUrl.isEmpty()) {
        info_message->setMessageType(KMessageWidget::Information);
        info_message->setText(i18n("Select a clip for speech recognition."));
        info_message->animatedShow();
        return;
    }
    info_message->setMessageType(KMessageWidget::Information);
    info_message->setText(i18n("Starting speech recognition on %1.", clipName));
    info_message->animatedShow();
    qApp->processEvents();
    //m_speechJob->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_speechJob.get(), &QProcess::readyReadStandardOutput, this, &TextBasedEdit::slotProcessSpeech);
    connect(m_speechJob.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, &TextBasedEdit::slotProcessSpeechStatus);
    listWidget->clear();
    qDebug()<<"=== STARTING RECO: "<<speechScript<<" / "<<modelDirectory<<" / "<<language<<" / "<<m_sourceUrl;
    m_speechJob->start(pyExec, {speechScript, modelDirectory, language, m_sourceUrl});
    speech_progress->setValue(0);
    frame_progress->setVisible(true);
}

void TextBasedEdit::updateAvailability()
{
    bool enabled = KdenliveSettings::vosk_found() && language_box->count() > 0;
    button_start->setEnabled(enabled);
    vosk_config->setVisible(!enabled);
}

void TextBasedEdit::slotProcessSpeechStatus(int, QProcess::ExitStatus status)
{
    if (status == QProcess::CrashExit) {
        info_message->setMessageType(KMessageWidget::Warning);
        info_message->setText(i18n("Speech recognition aborted."));
        info_message->animatedShow();
    } else {
        info_message->setMessageType(KMessageWidget::Positive);
        info_message->setText(i18n("Speech recognition finished."));
        info_message->animatedShow();
    }
    frame_progress->setVisible(false);
}

void TextBasedEdit::slotProcessSpeech()
{
    QString saveData = QString::fromUtf8(m_speechJob->readAllStandardOutput());
    //saveData.replace(QStringLiteral("\\\""), QStringLiteral("\""));
    qDebug()<<"=== GOT DATA:\n"<<saveData;
    int ix = 0;
    QVector <int> indexes;
    while (ix > -1) {
        ix = saveData.indexOf(QStringLiteral("{\n  \"result\""), ix);
        if (ix > -1) {
            indexes << ix;
            ix++;
        }
    }
    qDebug()<<"Found res: "<<indexes;
    QString chunk;
    while (!indexes.isEmpty()) {
        int first = indexes.takeFirst();
        if (indexes.isEmpty()) {
            chunk = saveData.mid(first);
        } else {
            chunk = saveData.mid(first, indexes.at(0) - first);
        }
        qDebug()<<"cut: "<<saveData;

        QJsonParseError error;
        auto loadDoc = QJsonDocument::fromJson(chunk.toUtf8(), &error);
        qDebug()<<"===JSON ERROR: "<<error.errorString();
    
        if (loadDoc.isArray()) {
            qDebug()<<"==== ITEM IS ARRAY";
            QJsonArray array = loadDoc.array();
            for (int i = 0; i < array.size(); i++) {
                QJsonValue val = array.at(i);
                qDebug()<<"==== FOUND KEYS: "<<val.toObject().keys();
                if (val.isObject() && val.toObject().keys().contains("text")) {
                    //textEdit->append(val.toObject().value("text").toString());
                }
            }
        } else if (loadDoc.isObject()) {
            QJsonObject obj = loadDoc.object();
            qDebug()<<"==== ITEM IS OBJECT";
            if (!obj.isEmpty()) {
                QString itemText = obj["text"].toString();
                QListWidgetItem *item = new QListWidgetItem(listWidget);
                if (obj["result"].isObject()) {
                    qDebug()<<"==== RESULT IS OBJECT";
                } else if (obj["result"].isArray()) {
                    qDebug()<<"==== RESULT IS ARRAY";
                    QJsonArray obj2 = obj["result"].toArray();
                    QJsonValue val = obj2.first();
                    if (val.isObject() && val.toObject().keys().contains("start")) {
                        double ms = val.toObject().value("start").toDouble();
                        itemText.prepend(QString("%1: ").arg(pCore->timecode().getDisplayTimecode(GenTime(ms), false)));
                        item->setData(Qt::UserRole, ms);
                    }
                    val = obj2.last();
                    if (val.isObject() && val.toObject().keys().contains("end")) {
                        double ms = val.toObject().value("end").toDouble();
                        item->setData(Qt::UserRole + 1, ms);
                        if (m_clipDuration > 0.) {
                            speech_progress->setValue(static_cast<int>(100 * ms / m_clipDuration));
                        }
                    }
                }
                item->setText(itemText);
            }
        } else if (loadDoc.isEmpty()) {
            qDebug()<<"==== EMPTY OBJEC DOC";
        }
    }
}

void TextBasedEdit::parseVoskDictionaries()
{
    QString modelDirectory = KdenliveSettings::vosk_folder_path();
    QDir dir;
    if (modelDirectory.isEmpty()) {
        modelDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        dir = QDir(modelDirectory);
        if (!dir.cd(QStringLiteral("speechmodels"))) {
            qDebug()<<"=== /// CANNOT ACCESS SPEECH DICTIONARIES FOLDER";
            pCore->voskModelUpdate({});
            return;
        }
    } else {
        dir = QDir(modelDirectory);
    }
    QStringList dicts = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList final;
    for (auto &d : dicts) {
        QDir sub(dir.absoluteFilePath(d));
        if (sub.exists(QStringLiteral("mfcc.conf")) || (sub.exists(QStringLiteral("conf/mfcc.conf")))) {
            final << d;
        }
    }
    pCore->voskModelUpdate(final);
}
