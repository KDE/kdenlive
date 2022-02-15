/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "transcodeseek.h"
#include "kdenlivesettings.h"
#include "kxmlgui_version.h"

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

void TranscodeSeek::addUrl(const QString &file, const QString &id, const QString &suffix)
{
    QListWidgetItem *it = new QListWidgetItem(file, listWidget);
    it->setData(Qt::UserRole, id);
    it->setData(Qt::UserRole + 1, suffix);
}

QMap<QString,QString> TranscodeSeek::ids() const
{
    QMap<QString,QString> urls;
    for (int i = 0; i < listWidget->count(); i++) {
        urls.insert(listWidget->item(i)->data(Qt::UserRole).toString(), listWidget->item(i)->data(Qt::UserRole + 1).toString());
    }
    return urls;
}

QString TranscodeSeek::params() const
{
    KdenliveSettings::setTranscodeFriendly(encodingprofiles->currentText());
    return m_encodeParams.value(encodingprofiles->currentText());
}

QString TranscodeSeek::preParams() const
{
    KdenliveSettings::setTranscodeFriendlyRotate(autorotate->isChecked());
    return autorotate->isChecked() ? QStringLiteral("-noautorotate") : QString();
}
