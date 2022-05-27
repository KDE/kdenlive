/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "transcodeseek.h"
#include "kdenlivesettings.h"

#include <KMessageBox>
#include <QFontDatabase>
#include <QPushButton>
#include <QStandardPaths>
#include <klocalizedstring.h>
#include <kxmlgui_version.h>

TranscodeSeek::TranscodeSeek(QWidget *parent)
    : QDialog(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowTitle(i18nc("@title:window", "Transcode Clip"));
    KConfig conf(QStringLiteral("kdenlivetranscodingrc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "intermediate");
    m_encodeParams = group.entryMap();
    encodingprofiles->addItems(group.keyList());
    connect(encodingprofiles, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [&](int ix) {
        const QString currentParams = m_encodeParams.value(encodingprofiles->itemText(ix));
        if (currentParams.endsWith(QLatin1String(";audio"))) {
            buttonBox->button(QDialogButtonBox::Ok)->setText(i18n("Audio transcode"));
        } else if (currentParams.endsWith(QLatin1String(";video"))) {
            buttonBox->button(QDialogButtonBox::Ok)->setText(i18n("Video transcode"));
        } else {
            buttonBox->button(QDialogButtonBox::Ok)->setText(i18n("Transcode"));
        }
    });
    int ix = encodingprofiles->findText(KdenliveSettings::transcodeFriendly());
    if (ix > -1) {
        encodingprofiles->setCurrentIndex(ix);
    }
    const QString currentParams = m_encodeParams.value(encodingprofiles->currentText());
    if (currentParams.endsWith(QLatin1String(";audio"))) {
        buttonBox->button(QDialogButtonBox::Ok)->setText(i18n("Audio transcode"));
    } else if (currentParams.endsWith(QLatin1String(";video"))) {
        buttonBox->button(QDialogButtonBox::Ok)->setText(i18n("Video transcode"));
    } else {
        buttonBox->button(QDialogButtonBox::Ok)->setText(i18n("Transcode"));
    }
    messagewidget->setVisible(false);
}

TranscodeSeek::~TranscodeSeek() {}

void TranscodeSeek::addUrl(const QString &file, const QString &id, const QString &suffix, ClipType::ProducerType type, const QString &message)
{
    QListWidgetItem *it = new QListWidgetItem(file, listWidget);
    it->setData(Qt::UserRole, id);
    it->setData(Qt::UserRole + 1, suffix);
    it->setData(Qt::UserRole + 2, QString::number(type));
    if (!message.isEmpty()) {
        if (messagewidget->text().isEmpty()) {
            messagewidget->setText(message);
            messagewidget->animatedShow();
        } else {
            QString mText = messagewidget->text();
            if (mText.length() < 120) {
                mText.append(QStringLiteral("<br/>"));
                mText.append(message);
                messagewidget->setText(mText);
                messagewidget->animatedShow();
            }
        }
    }
    const QString currentParams = m_encodeParams.value(encodingprofiles->currentText());
    if (listWidget->count() == 1) {
        QString currentParams = m_encodeParams.value(encodingprofiles->currentText());
        if (type == ClipType::Audio) {
            if (!currentParams.endsWith(QLatin1String(";audio"))) {
                // Switch to audio only profile
                QMapIterator<QString, QString> i(m_encodeParams);
                while (i.hasNext()) {
                    i.next();
                    if (i.value().endsWith(QLatin1String(";audio"))) {
                        int ix = encodingprofiles->findText(i.key());
                        if (ix > -1) {
                            encodingprofiles->setCurrentIndex(ix);
                            break;
                        }
                    }
                }
            }
        } else if (type == ClipType::Video) {
            if (!currentParams.endsWith(QLatin1String(";video"))) {
                // Switch to video only profile
                QMapIterator<QString, QString> i(m_encodeParams);
                while (i.hasNext()) {
                    i.next();
                    if (i.value().endsWith(QLatin1String(";video"))) {
                        int ix = encodingprofiles->findText(i.key());
                        if (ix > -1) {
                            encodingprofiles->setCurrentIndex(ix);
                            break;
                        }
                    }
                }
            }
        }
    } else {
        if ((type != ClipType::Video && currentParams.endsWith(QLatin1String(";video"))) ||
            (type != ClipType::Audio && currentParams.endsWith(QLatin1String(";audio")))) {
            // Switch back to an AV profile
            QMapIterator<QString, QString> i(m_encodeParams);
            while (i.hasNext()) {
                i.next();
                if (i.value().endsWith(QLatin1String(";av"))) {
                    int ix = encodingprofiles->findText(i.key());
                    if (ix > -1) {
                        encodingprofiles->setCurrentIndex(ix);
                        break;
                    }
                }
            }
        }
    }
}

QMap<QString, QStringList> TranscodeSeek::ids() const
{
    QMap<QString, QStringList> urls;
    for (int i = 0; i < listWidget->count(); i++) {
        QListWidgetItem *item = listWidget->item(i);
        urls.insert(item->data(Qt::UserRole).toString(), {item->data(Qt::UserRole + 1).toString(), item->data(Qt::UserRole + 2).toString()});
    }
    return urls;
}

QString TranscodeSeek::params(int clipType) const
{
    switch (clipType) {
    case ClipType::Audio: {
        if (!m_encodeParams.value(encodingprofiles->currentText()).endsWith(QLatin1String(";audio"))) {
            // Switch to audio only profile
            QMapIterator<QString, QString> i(m_encodeParams);
            while (i.hasNext()) {
                i.next();
                if (i.value().endsWith(QLatin1String(";audio"))) {
                    return i.value().section(QLatin1Char(';'), 0, -2);
                }
            }
        }
        break;
    }
    case ClipType::Video: {
        if (!m_encodeParams.value(encodingprofiles->currentText()).endsWith(QLatin1String(";video"))) {
            // Switch to video only profile
            QMapIterator<QString, QString> i(m_encodeParams);
            while (i.hasNext()) {
                i.next();
                if (i.value().endsWith(QLatin1String(";video"))) {
                    return i.value().section(QLatin1Char(';'), 0, -2);
                }
            }
        }
        break;
    }
    default:
        break;
    }
    if (m_encodeParams.value(encodingprofiles->currentText()).endsWith(QLatin1String(";av"))) {
        // Only store selected av preset
        KdenliveSettings::setTranscodeFriendly(encodingprofiles->currentText());
    }
    return m_encodeParams.value(encodingprofiles->currentText()).section(QLatin1Char(';'), 0, -2);
}

QString TranscodeSeek::preParams() const
{
    return QStringLiteral("-noautorotate");
}
