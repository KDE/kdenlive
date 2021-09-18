/*
SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle (jb@kdenlive.org)

This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "transcodeseek.h"
#include "kdenlivesettings.h"
#include "kxmlgui_version.h"

#include <QFontDatabase>
#include <QStandardPaths>
#include <KMessageBox>
#include <klocalizedstring.h>

TranscodeSeek::TranscodeSeek(QWidget *parent)
    : QDialog(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowTitle(i18n("Transcode Clip"));
    KConfig conf(QStringLiteral("kdenlivetranscodingrc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "intermediate");
    m_encodeParams = group.entryMap();
    encodingprofiles->addItems(group.keyList());
    int ix = encodingprofiles->findText(KdenliveSettings::transcodeFriendly());
    if (ix > -1) {
        encodingprofiles->setCurrentIndex(ix);
    }
}

TranscodeSeek::~TranscodeSeek()
{
}

void TranscodeSeek::addUrl(const QString &file, const QString &id)
{
    QListWidgetItem *it = new QListWidgetItem(file, listWidget);
    it->setData(Qt::UserRole, id);
}

std::vector<QString> TranscodeSeek::ids() const
{
    std::vector<QString> urls;
    for (int i = 0; i < listWidget->count(); i++) {
        urls.push_back(listWidget->item(i)->data(Qt::UserRole).toString());
    }
    return urls;
}

QString TranscodeSeek::params() const
{
    KdenliveSettings::setTranscodeFriendly(encodingprofiles->currentText());
    return m_encodeParams.value(encodingprofiles->currentText());
}
