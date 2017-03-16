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
#include <QScopedPointer>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>

#include <mlt++/Mlt.h>
#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

std::unique_ptr<EffectsRepository> EffectsRepository::instance;
std::once_flag EffectsRepository::m_onceFlag;

EffectsRepository::EffectsRepository()
{
    // Warning: Mlt::Factory::init() resets the locale to the default system value, make sure we keep correct locale
#ifndef Q_OS_MAC
    setlocale(LC_NUMERIC, nullptr);
#else
    setlocale(LC_NUMERIC_MASK, nullptr);
#endif

    // Parse effects blacklist
    parseBlackList();

    // Retrieve the list of MLT's available effects.
    QScopedPointer<Mlt::Properties> filters(pCore->getMltRepository()->filters());
    int max = filters->count();
    for (int i = 0; i < max; ++i) {
        EffectInfo info;
        QString name = filters->get_name(i);
        if (!m_blacklist.contains(name) && parseEffectFromMlt(name, info)) {
            m_effects[name] = info;
        } else {
            if (m_blacklist.contains(name)) {
                qDebug() << name << "is blacklisted";
            } else {
                qDebug() << "WARNING : Fails to parse "<<name;
            }
        }
    }

    // We now parse custom effect xml
    // Set the directories to look into for effects.
    QStringList effect_dirs = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("effects"), QStandardPaths::LocateDirectory);

    for (const auto& dir : effect_dirs) {
        QDir current_dir(dir);
        QStringList filter;
        filter << QStringLiteral("*.xml");
        QStringList fileList = current_dir.entryList(filter, QDir::Files);
        for (const auto& file : fileList) {
            QString path = current_dir.absoluteFilePath(file);
            parseCustomEffectsFile(path);
        }
    }
}


void EffectsRepository::parseBlackList()
{
    QFile blacklist_file(QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("blacklisted_effects.txt")));
    if (blacklist_file.open(QIODevice::ReadOnly)) {
        QTextStream stream(&blacklist_file);
        QString line;
        while (stream.readLineInto(&line)) {
            line = line.simplified();
            if (!line.isEmpty() && !line.startsWith('#')) {
                m_blacklist.insert(line);
            }
        }
        blacklist_file.close();
    }
}

//static
bool EffectsRepository::parseEffectFromMlt(const QString& effectId, EffectInfo & res)
{
    Mlt::Properties *metadata = pCore->getMltRepository()->metadata(filter_type, effectId.toLatin1().data());
    if (metadata && metadata->is_valid()) {
        if (metadata->get("title") && metadata->get("identifier") && strlen(metadata->get("title")) > 0) {

            res.name = metadata->get("title");
            res.name[0] = res.name[0].toUpper();

            res.description = metadata->get("description");
            res.author = metadata->get("creator");
            res.version_str = metadata->get("version");
            res.version = metadata->get_double("version");

            res.type = EffectType::Video;

            Mlt::Properties tags((mlt_properties) metadata->get_data("tags"));
            if (QString(tags.get(0)) == QLatin1String("Audio")) {
                res.type = EffectType::Audio;
            }
            return true;
        }
    }
    return false;
}


void EffectsRepository::parseCustomEffectsFile(const QString& file_name)
{
    QFile file(file_name);
    QDomDocument doc;
    doc.setContent(&file, false);
    file.close();
    QDomElement base = doc.documentElement();
    if (base.tagName() == QLatin1String("effectgroup")) {
        //in that case we have a custom effect
        EffectInfo info;
        info.custom_xml_path = file_name;
        info.type = EffectType::Custom;
        QString tag = base.attribute(QStringLiteral("tag"), QString());
        QString id = base.hasAttribute(QStringLiteral("id")) ? base.attribute(QStringLiteral("id")) : tag;

        QString name = base.attribute(QStringLiteral("name"), QString());
        info.name = name;
        if (m_effects.count(id) > 0) {
            qDebug() << "Error: conflicting effect name"<<id;
        } else {
            m_effects[id] = info;
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
        QLocale locale;

        //We first deal with locale
        if (currentEffect.hasAttribute(QStringLiteral("LC_NUMERIC"))) {
            // set a locale for that effect
            locale = QLocale(currentEffect.attribute(QStringLiteral("LC_NUMERIC")));
        }
        locale.setNumberOptions(QLocale::OmitGroupSeparator);

        QString tag = currentEffect.attribute(QStringLiteral("tag"), QString());
        QString id = currentEffect.hasAttribute(QStringLiteral("id")) ? currentEffect.attribute(QStringLiteral("id")) : tag;

        if (!hasEffect(id)) {
            qDebug() << "++++++ Unknown effect : " << id;
            continue;
        }

        //Check if there is a maximal version set
        if (currentEffect.hasAttribute(QStringLiteral("version"))) {
            // a specific version of the filter is required
            if (locale.toDouble(currentEffect.attribute(QStringLiteral("version"))) > m_effects[id].version) {
                continue;
            }
        }

        m_effects[id].custom_xml_path = file_name;

        //Update description if the xml provide one
        QString description = Xml::getSubTagContent(currentEffect, QStringLiteral("description"));
        if (!description.isEmpty()) {
            m_effects[id].description = description;
        }

        //Update name if the xml provide one
        QString name = Xml::getSubTagContent(currentEffect, QStringLiteral("name"));
        if (!name.isEmpty()) {
            m_effects[id].name = name;
        }

        // Parse type information.
        QString type = currentEffect.attribute(QStringLiteral("type"), QString());
        if (type == QLatin1String("audio")) {
            m_effects[id].type = EffectType::Audio;
        } else if (type == QLatin1String("custom")) {
            m_effects[id].type = EffectType::Custom;
        } else {
            m_effects[id].type = EffectType::Video;
        }

    }
}


std::unique_ptr<EffectsRepository> & EffectsRepository::get()
{
    std::call_once(m_onceFlag, []{instance.reset(new EffectsRepository());});
    return instance;
}


bool EffectsRepository::hasEffect(const QString& effectId) const
{
    return m_effects.count(effectId) > 0;
}

QVector<QPair<QString,QString> > EffectsRepository::getEffectsNames() const
{
    QVector<QPair<QString,QString> > res;
    res.reserve((int)m_effects.size());
    for (const auto& effect : m_effects) {
        res.push_back({effect.first, effect.second.name});
    }
    std::sort(res.begin(), res.end(), [](const decltype(res.front())& a, const decltype(res.front())& b){
            return a.second < b.second;
        });
    return res;
}

EffectType EffectsRepository::getEffectType(const QString& effectId) const
{
    Q_ASSERT(m_effects.count(effectId) > 0);
    return m_effects.at(effectId).type;
}

QString EffectsRepository::getEffectName(const QString& effectId) const
{
    Q_ASSERT(m_effects.count(effectId) > 0);
    return m_effects.at(effectId).name;
}
bool EffectsRepository::isFavorite(const QString& effectId) const
{
    //TODO
    return true;
}
