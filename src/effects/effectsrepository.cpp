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
#include <QScopedPointer>
#include <QFile>
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
            res.version = metadata->get("version");

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
