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

#include "effectsrepository.hpp"
#include "core.h"
#include "xml/xml.hpp"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

#include "profiles/profilemodel.hpp"
#include <mlt++/Mlt.h>

std::unique_ptr<EffectsRepository> EffectsRepository::instance;
std::once_flag EffectsRepository::m_onceFlag;

EffectsRepository::EffectsRepository()
    : AbstractAssetsRepository<EffectType>()
{
    init();
}

Mlt::Properties *EffectsRepository::retrieveListFromMlt()
{
    return pCore->getMltRepository()->filters();
}

Mlt::Properties *EffectsRepository::getMetadata(const QString &effectId)
{
    return pCore->getMltRepository()->metadata(filter_type, effectId.toLatin1().data());
}

void EffectsRepository::parseCustomAssetFile(const QString &file_name, std::unordered_map<QString, Info> &customAssets) const
{
    QFile file(file_name);
    QDomDocument doc;
    doc.setContent(&file, false);
    file.close();
    QDomElement base = doc.documentElement();
    if (base.tagName() == QLatin1String("effectgroup")) {
        // in that case we have a custom effect
        Info info;
        info.xml = base;
        info.type = EffectType::Custom;
        QString tag = base.attribute(QStringLiteral("tag"), QString());
        QString id = base.hasAttribute(QStringLiteral("id")) ? base.attribute(QStringLiteral("id")) : tag;

        QString name = base.attribute(QStringLiteral("name"), QString());
        info.name = name;
        info.id = id;
        info.mltId = tag;
        if (customAssets.count(id) > 0) {
            qDebug() << "Error: conflicting effect name" << id;
        } else {
            customAssets[id] = info;
        }
        return;
    }
    QDomNodeList effects = doc.elementsByTagName(QStringLiteral("effect"));

    int nbr_effect = effects.count();
    if (nbr_effect == 0) {
        qDebug() << "+++++++++++++\nEffect broken: " << file_name << "\n+++++++++++";
        return;
    }

    for (int i = 0; i < nbr_effect; ++i) {
        QDomNode currentNode = effects.item(i);
        if (currentNode.isNull()) {
            continue;
        }
        QDomElement currentEffect = currentNode.toElement();
        Info result;
        bool ok = parseInfoFromXml(currentEffect, result);
        if (!ok) {
            continue;
        }

        if (customAssets.count(result.id) > 0) {
            qDebug() << "Warning: duplicate custom definition of effect" << result.id << "found. Only last one will be considered";
        }

        result.xml = currentEffect;

        // Parse type information.
        QString type = currentEffect.attribute(QStringLiteral("type"), QString());
        if (type == QLatin1String("audio")) {
            result.type = EffectType::Audio;
        } else if (type == QLatin1String("custom")) {
            result.type = EffectType::Custom;
        } else {
            result.type = EffectType::Video;
        }

        customAssets[result.id] = result;
    }
}

std::unique_ptr<EffectsRepository> &EffectsRepository::get()
{
    std::call_once(m_onceFlag, [] { instance.reset(new EffectsRepository()); });
    return instance;
}

QStringList EffectsRepository::assetDirs() const
{
    return QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("effects"), QStandardPaths::LocateDirectory);
}

void EffectsRepository::parseType(QScopedPointer<Mlt::Properties> &metadata, Info &res)
{
    res.type = EffectType::Video;

    Mlt::Properties tags((mlt_properties)metadata->get_data("tags"));
    if (QString(tags.get(0)) == QLatin1String("Audio")) {
        res.type = EffectType::Audio;
    }
}

QString EffectsRepository::assetBlackListPath() const
{
    return QStringLiteral(":data/blacklisted_effects.txt");
}

Mlt::Filter *EffectsRepository::getEffect(const QString &effectId) const
{
    Q_ASSERT(exists(effectId));
    QString service_name = m_assets.at(effectId).mltId;
    // We create the Mlt element from its name
    Mlt::Filter *filter = new Mlt::Filter(pCore->getCurrentProfile()->profile(), service_name.toLatin1().constData(), nullptr);
    return filter;
}
