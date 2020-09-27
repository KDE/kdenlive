/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
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

#include "transitionsrepository.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "xml/xml.hpp"
#include <QFile>
#include <QStandardPaths>

#include "profiles/profilemodel.hpp"
#include <mlt++/Mlt.h>

std::unique_ptr<TransitionsRepository> TransitionsRepository::instance;
std::once_flag TransitionsRepository::m_onceFlag;

TransitionsRepository::TransitionsRepository()
    : AbstractAssetsRepository<AssetListType::AssetType>()
{
    init();
    QStringList invalidTransition;
    for (const QString &effect : KdenliveSettings::favorite_transitions()) {
        if (!exists(effect)) {
            invalidTransition << effect;
        }
    }
    if (!invalidTransition.isEmpty()) {
        pCore->displayMessage(i18n("Some of your favorite compositions are invalid and were removed: %1", invalidTransition.join(QLatin1Char(','))),
                              ErrorMessage);
        QStringList newFavorites = KdenliveSettings::favorite_transitions();
        for (const QString &effect : qAsConst(invalidTransition)) {
            newFavorites.removeAll(effect);
        }
        KdenliveSettings::setFavorite_transitions(newFavorites);
    }
}

Mlt::Properties *TransitionsRepository::retrieveListFromMlt() const
{
    return pCore->getMltRepository()->transitions();
}

Mlt::Properties *TransitionsRepository::getMetadata(const QString &assetId) const
{
    return pCore->getMltRepository()->metadata(transition_type, assetId.toLatin1().data());
}

void TransitionsRepository::parseCustomAssetFile(const QString &file_name, std::unordered_map<QString, Info> &customAssets) const
{
    QFile file(file_name);
    QDomDocument doc;
    doc.setContent(&file, false);
    file.close();

    QDomElement base = doc.documentElement();
    QDomNodeList transitions = doc.elementsByTagName(QStringLiteral("transition"));

    int nbr_transition = transitions.count();
    if (nbr_transition == 0) {
        qDebug() << "+++++++++++++\n Transition broken: " << file_name << "\n+++++++++++";
        return;
    }

    for (int i = 0; i < nbr_transition; ++i) {
        QDomNode currentNode = transitions.item(i);
        if (currentNode.isNull()) {
            continue;
        }
        Info result;
        bool ok = parseInfoFromXml(currentNode.toElement(), result);
        if (!ok) {
            continue;
        }

        if (customAssets.count(result.id) > 0) {
            qDebug() << "Warning: duplicate custom definition of transition" << result.id << "found. Only last one will be considered";
        }
        result.xml = currentNode.toElement();
        QString type = result.xml.attribute(QStringLiteral("type"), QString());
        if (type == QLatin1String("hidden")) {
            result.type = AssetListType::AssetType::Hidden;
        } else if (type == QLatin1String("short")) {
            result.type = AssetListType::AssetType::VideoShortComposition;
        }
        customAssets[result.id] = result;
    }
}

std::unique_ptr<TransitionsRepository> &TransitionsRepository::get()
{
    std::call_once(m_onceFlag, [] { instance.reset(new TransitionsRepository()); });
    return instance;
}

QStringList TransitionsRepository::assetDirs() const
{
    return QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("transitions"), QStandardPaths::LocateDirectory);
}

void TransitionsRepository::parseType(QScopedPointer<Mlt::Properties> &metadata, Info &res)
{
    Mlt::Properties tags((mlt_properties)metadata->get_data("tags"));
    bool audio = QString(tags.get(0)) == QLatin1String("Audio");

    if (getSingleTrackTransitions().contains(res.id)) {
        if (audio) {
            res.type = AssetListType::AssetType::AudioTransition;
        } else {
            res.type = AssetListType::AssetType::VideoTransition;
        }
    } else {
        if (audio) {
            res.type = AssetListType::AssetType::AudioComposition;
        } else {
            res.type = AssetListType::AssetType::VideoComposition;
        }
    }
}

QSet<QString> TransitionsRepository::getSingleTrackTransitions()
{
    // Disabled until same track transitions is implemented
    return {/*QStringLiteral("composite"), QStringLiteral("dissolve")*/};
}

QString TransitionsRepository::assetBlackListPath() const
{
    return QStringLiteral(":data/blacklisted_transitions.txt");
}

QString TransitionsRepository::assetPreferredListPath() const
{
    // Transitions do not have "Main" filter implemented, so we return an empty
    // string instead of path to a file with that list
    return QLatin1String("");
}

std::unique_ptr<Mlt::Transition> TransitionsRepository::getTransition(const QString &transitionId) const
{
    Q_ASSERT(exists(transitionId));
    QString service_name = m_assets.at(transitionId).mltId;
    // We create the Mlt element from its name
    auto transition = std::make_unique<Mlt::Transition>(pCore->getCurrentProfile()->profile(), service_name.toUtf8().constData());
    transition->set("kdenlive_id", transitionId.toUtf8().constData());
    return transition;
}

bool TransitionsRepository::isComposition(const QString &transitionId) const
{
    auto type = getType(transitionId);
    return type == AssetListType::AssetType::AudioComposition || type == AssetListType::AssetType::VideoComposition || type == AssetListType::AssetType::VideoShortComposition;
}

const QString TransitionsRepository::getCompositingTransition()
{
    if (KdenliveSettings::gpu_accel()) {
        return QStringLiteral("movit.overlay");
    }
    if (exists(QStringLiteral("qtblend"))) {
        return QStringLiteral("qtblend");
    }
    if (exists(QStringLiteral("frei0r.cairoblend"))) {
        return QStringLiteral("frei0r.cairoblend");
    }
    if (exists(QStringLiteral("composite"))) {
        return QStringLiteral("composite");
    }
    qDebug() << "Warning: no compositing found";
    return QString();
}
