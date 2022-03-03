/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "transcodeseek.h"
#include "kdenlivesettings.h"

#include <kxmlgui_version.h>
#include <KMessageBox>
#include <QFontDatabase>
#include <QStandardPaths>
#include <klocalizedstring.h>

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
    int ix = encodingprofiles->findText(KdenliveSettings::transcodeFriendly());
    if (ix > -1) {
        encodingprofiles->setCurrentIndex(ix);
    }
    autorotate->setChecked(KdenliveSettings::transcodeFriendlyRotate());
}

TranscodeSeek::~TranscodeSeek()
{
}

void TranscodeSeek::addUrl(const QString &file, const QString &id, const QString &suffix, ClipType::ProducerType type)
{
    QListWidgetItem *it = new QListWidgetItem(file, listWidget);
    it->setData(Qt::UserRole, id);
    it->setData(Qt::UserRole + 1, suffix);
    it->setData(Qt::UserRole + 2, QString::number(type));
    if (listWidget->count() == 1) {
        if (type == ClipType::Audio) {
            if (!m_encodeParams.value(encodingprofiles->currentText()).endsWith(QLatin1String(";audio"))) {
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
            if (!m_encodeParams.value(encodingprofiles->currentText()).endsWith(QLatin1String(";video"))) {
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
        if ((type != ClipType::Video && m_encodeParams.value(encodingprofiles->currentText()).endsWith(QLatin1String(";video"))) || (type != ClipType::Audio && m_encodeParams.value(encodingprofiles->currentText()).endsWith(QLatin1String(";audio")))) {
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

QMap<QString,QStringList> TranscodeSeek::ids() const
{
    QMap<QString,QStringList> urls;
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
    KdenliveSettings::setTranscodeFriendlyRotate(autorotate->isChecked());
    return autorotate->isChecked() ? QStringLiteral("-noautorotate") : QString();
}
