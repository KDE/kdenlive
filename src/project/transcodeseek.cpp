/*
Copyright (C) 2021 by Jean-Baptiste Mardelle (jb@kdenlive.org)        
                                                                         *
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
