/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   Copyright (C) 2011 by Marco Gittler (marco@gitma.de)                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "clipstabilize.h"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "widgets/doublewidget.h"
#include "widgets/positionwidget.h"
#include "assets/view/assetparameterview.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "effects/effectsrepository.hpp"

#include "kdenlivesettings.h"
#include <KMessageBox>
#include <QFontDatabase>
#include <mlt++/Mlt.h>

ClipStabilize::ClipStabilize(const std::vector<QString> &binIds, QString filterName, QWidget *parent)
    : QDialog(parent)
    , m_filtername(std::move(filterName))
    , m_binIds(binIds)
    , m_vbox(nullptr)
    , m_assetModel(nullptr)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setWindowTitle(i18n("Stabilize Clip"));
    auto_add->setText(i18np("Add clip to project", "Add clips to project", m_binIds.size()));
    auto_add->setChecked(KdenliveSettings::add_new_clip());

    // QString stylesheet = EffectStackView2::getStyleSheet();
    // setStyleSheet(stylesheet);

    Q_ASSERT(binIds.size() > 0);
    auto firstBinClip = pCore->projectItemModel()->getClipByBinID(m_binIds.front().section(QLatin1Char('/'), 0, 0));
    auto firstUrl = firstBinClip->url();
    if (m_binIds.size() == 1) {
        QString newFile = firstUrl;
        newFile.append(QStringLiteral(".mlt"));
        dest_url->setMode(KFile::File);
        dest_url->setUrl(QUrl(newFile));
    } else {
        label_dest->setText(i18n("Destination folder"));
        dest_url->setMode(KFile::Directory | KFile::ExistingOnly);
        dest_url->setUrl(QUrl(firstUrl).adjusted(QUrl::RemoveFilename));
    }
    m_vbox = new QVBoxLayout(optionsbox);
    if (m_filtername == QLatin1String("vidstab")) {
        m_view.reset(new AssetParameterView(this));
        qDebug()<<"// Fetching effect: "<<m_filtername;
        std::unique_ptr<Mlt::Filter> asset = EffectsRepository::get()->getEffect(m_filtername);
        auto prop = std::make_unique<Mlt::Properties>(asset->get_properties());
        QDomElement xml = EffectsRepository::get()->getXml(m_filtername);
        m_assetModel.reset(new AssetParameterModel(std::move(prop), xml, m_filtername, {ObjectType::NoItem, -1}));
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
        const QString presetFile = dir.absoluteFilePath(QString("%1.json").arg(m_assetModel->getAssetId()));
        const QVector<QPair<QString, QVariant>> params = m_assetModel->loadPreset(presetFile, i18n("Last setting"));
        if (!params.isEmpty()) {
            m_assetModel->setParameters(params);
        }
        m_view->setModel(m_assetModel, QSize(1920, 1080));
        m_vbox->addWidget(m_view.get());
        // Presets
        preset_button->setIcon(QIcon::fromTheme(QStringLiteral("adjustlevels")));
        preset_button->setMenu(m_view->presetMenu());
        preset_button->setToolTip(i18n("Presets"));
    }

    connect(buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &ClipStabilize::slotValidate);
    adjustSize();
}

ClipStabilize::~ClipStabilize()
{
    /*if (m_stabilizeProcess.state() != QProcess::NotRunning) {
        m_stabilizeProcess.close();
    }*/
    KdenliveSettings::setAdd_new_clip(auto_add->isChecked());
}

std::unordered_map<QString, QString> ClipStabilize::filterParams() const
{
    QVector<QPair<QString, QVariant>> result = m_assetModel->getAllParameters();
    std::unordered_map<QString, QString> params;

    for (const auto &it : qAsConst(result)) {
        params[it.first] = it.second.toString();
    }
    return params;
}

QString ClipStabilize::filterName() const
{
    return m_filtername;
}

QString ClipStabilize::destination() const
{
    QString path = dest_url->url().toLocalFile();
    if (m_binIds.size() > 1 && !path.endsWith(QDir::separator())) {
        path.append(QDir::separator());
    }
    return path;
}

QString ClipStabilize::desc() const
{
    return i18n("Stabilize clip");
}

bool ClipStabilize::autoAddClip() const
{
    return auto_add->isChecked();
}

void ClipStabilize::slotValidate()
{
    if (m_binIds.size() == 1) {
        if (QFile::exists(dest_url->url().toLocalFile())) {
            if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", dest_url->url().toLocalFile())) ==
                KMessageBox::No) {
                return;
            }
        }
    } else {
        QDir folder(dest_url->url().toLocalFile());
        QStringList existingFiles;
        for (const QString &binId : m_binIds) {
            auto binClip = pCore->projectItemModel()->getClipByBinID(binId.section(QLatin1Char('/'), 0, 0));
            auto url = binClip->url();
            if (folder.exists(url + QStringLiteral(".mlt"))) {
                existingFiles.append(folder.absoluteFilePath(url + QStringLiteral(".mlt")));
            }
        }
        if (!existingFiles.isEmpty()) {
            if (KMessageBox::warningContinueCancelList(this, i18n("The stabilize job will overwrite the following files:"), existingFiles) ==
                KMessageBox::Cancel) {
                return;
            }
        }
    }
    m_view->slotSavePreset(i18n("Last setting"));
    accept();
}

