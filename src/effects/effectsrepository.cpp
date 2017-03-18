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
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>

#include <mlt++/Mlt.h>

std::unique_ptr<EffectsRepository> EffectsRepository::instance;
std::once_flag EffectsRepository::m_onceFlag;

EffectsRepository::EffectsRepository()
    : AbstractAssetsRepository<EffectType>()
{
    init();
}


Mlt::Properties* EffectsRepository::retrieveListFromMlt()
{
    return pCore->getMltRepository()->filters();
}

Mlt::Properties* EffectsRepository::getMetadata(const QString& effectId)
{
    return pCore->getMltRepository()->metadata(filter_type, effectId.toLatin1().data());
}


void EffectsRepository::parseCustomAssetFile(const QString& file_name)
{
    QFile file(file_name);
    QDomDocument doc;
    doc.setContent(&file, false);
    file.close();
    QDomElement base = doc.documentElement();
    if (base.tagName() == QLatin1String("effectgroup")) {
        //in that case we have a custom effect
        Info info;
        info.custom_xml_path = file_name;
        info.type = EffectType::Custom;
        QString tag = base.attribute(QStringLiteral("tag"), QString());
        QString id = base.hasAttribute(QStringLiteral("id")) ? base.attribute(QStringLiteral("id")) : tag;

        QString name = base.attribute(QStringLiteral("name"), QString());
        info.name = name;
        if (m_assets.count(id) > 0) {
            qDebug() << "Error: conflicting effect name"<<id;
        } else {
            m_assets[id] = info;
        }
        return;
    }
    QDomNodeList effects  = doc.elementsByTagName(QStringLiteral("effect"));

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
        QString id = parseInfoFromXml(currentEffect);
        if (id.isEmpty()) {
            continue;
        }

        m_assets[id].custom_xml_path = file_name;

        // Parse type information.
        QString type = currentEffect.attribute(QStringLiteral("type"), QString());
        if (type == QLatin1String("audio")) {
            m_assets[id].type = EffectType::Audio;
        } else if (type == QLatin1String("custom")) {
            m_assets[id].type = EffectType::Custom;
        } else {
            m_assets[id].type = EffectType::Video;
        }

    }
}


std::unique_ptr<EffectsRepository> & EffectsRepository::get()
{
    std::call_once(m_onceFlag, []{instance.reset(new EffectsRepository());});
    return instance;
}

QStringList EffectsRepository::assetDirs() const
{
    return QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("effects"), QStandardPaths::LocateDirectory);
}

void EffectsRepository::parseType(QScopedPointer<Mlt::Properties>& metadata, Info & res)
{
    res.type = EffectType::Video;

    Mlt::Properties tags((mlt_properties) metadata->get_data("tags"));
    if (QString(tags.get(0)) == QLatin1String("Audio")) {
        res.type = EffectType::Audio;
    }
}
