/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "transcodeseek.h"
#include "kdenlivesettings.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <QFontDatabase>
#include <QPushButton>
#include <QStandardPaths>

TranscodeSeek::TranscodeSeek(bool onUserRequest, bool forceReplace, QWidget *parent)
    : QDialog(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    if (onUserRequest) {
        label->setVisible(false);
    }
    if (KdenliveSettings::transcodeFriendly().isEmpty()) {
        // Use GPU accel by default if possible
        if (KdenliveSettings::supportedHWCodecs().contains(QLatin1String("h264_nvenc"))) {
            KdenliveSettings::setTranscodeFriendly(QStringLiteral("Lossy x264 I frame only (NVidia GPU)"));
        } else if (KdenliveSettings::supportedHWCodecs().contains(QLatin1String("h264_vaapi"))) {
            KdenliveSettings::setTranscodeFriendly(QStringLiteral("Lossy x264 I frame only (VAAPI GPU)"));
        } else {
            KdenliveSettings::setTranscodeFriendly(QStringLiteral("Lossy x264 I frame only"));
        }
    }
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowTitle(i18nc("@title:window", "Transcode Clip"));
    KConfig conf(QStringLiteral("kdenlivetranscodingrc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "intermediate");
    replace_original->setChecked(forceReplace || KdenliveSettings::transcodingReplace());
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

QString TranscodeSeek::params(int clipType, std::pair<int, int> fps_info) const
{
    QString params = m_encodeParams.value(encodingprofiles->currentText());
    params = params.section(QLatin1Char(';'), 0, -2);
    if (params.contains(QLatin1String(" -i "))) {
        params = params.section(QLatin1String(" -i "), 1);
    }
    QStringList splitParams = params.split(QLatin1Char(' '));
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
        if (m_encodeParams.value(encodingprofiles->currentText()).endsWith(QLatin1String(";av"))) {
            KdenliveSettings::setTranscodeFriendly(encodingprofiles->currentText());
            // Remove audio options
            int ix = splitParams.indexOf(QLatin1String("-codec:a"));
            if (ix == -1) {
                ix = splitParams.indexOf(QLatin1String("-c:a"));
            }
            if (ix > -1 && ix + 1 < splitParams.count()) {
                // Remove audio codec
                splitParams.removeAt(ix + 1);
                splitParams[ix] = QStringLiteral("-an");
            }
            // Now, remove audio bitrate info if any
            ix = splitParams.indexOf(QLatin1String("-ab"));
            if (ix > -1) {
                // Remove -ab
                splitParams.removeAt(ix);
                if (ix < splitParams.count()) {
                    // Remove bitrate
                    splitParams.removeAt(ix);
                }
            }
        }
        break;
    }
    default:
        if (m_encodeParams.value(encodingprofiles->currentText()).endsWith(QLatin1String(";av"))) {
            // Only store selected av preset
            KdenliveSettings::setTranscodeFriendly(encodingprofiles->currentText());
        }
        break;
    }
    if (clipType != ClipType::Audio && !m_encodeParams.value(encodingprofiles->currentText()).endsWith(QLatin1String(";audio"))) {
        // Enforce constant fps for clips with video
        int ix = splitParams.indexOf(QLatin1String("-vf"));
        if (ix == -1) {
            ix = splitParams.indexOf(QLatin1String("-filter:v"));
        }
        if (ix == -1) {
            ix = splitParams.indexOf(QLatin1String("-f:v"));
        }
        if (ix == -1) {
            splitParams.insert(splitParams.count() - 1, QStringLiteral("-filter:v"));
            splitParams.insert(splitParams.count() - 1, QStringLiteral("fps=fps=%1/%2").arg(fps_info.first).arg(fps_info.second));
        } else {
            // We already have a video filter, simply append the fps params
            ix++;
            if (ix < splitParams.count()) {
                QString filterParams = splitParams.at(ix);
                filterParams.append(QStringLiteral(",fps=fps=%1/%2").arg(fps_info.first).arg(fps_info.second));
                splitParams[ix] = filterParams;
            }
        }
    }
    params = splitParams.join(QLatin1Char(' '));
    return params;
}

QString TranscodeSeek::preParams() const
{
    QString params = m_encodeParams.value(encodingprofiles->currentText());
    params = params.section(QLatin1Char(';'), 0, -2);
    if (params.contains(QLatin1String(" -i "))) {
        QString p = params.section(QLatin1String(" -i "), 0, 0);
        p.append(QStringLiteral(" -noautorotate"));
        return p.simplified();
    } else {
        return QStringLiteral("-noautorotate");
    }
}
