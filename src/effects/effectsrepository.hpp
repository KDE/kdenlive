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

#ifndef EFFECTSREPOSITORY_H
#define EFFECTSREPOSITORY_H

#include <memory>
#include <mutex>
#include <QSet>
#include <unordered_map>
#include <QObject>
#include "definitions.h"

/** @brief This class stored all the effects that can be added by the user.
 * You can query any effect based on its name.
 * Note that this class is a Singleton
 */

enum class EffectType {
    Video,
    Audio,
    Custom
};
Q_DECLARE_METATYPE(EffectType)
class AbstractEffectModel;

class EffectsRepository
{

public:

    //Returns the instance of the Singleton
    static std::unique_ptr<EffectsRepository>& get();


    /* @brief Returns true if a given effect exists
     */
    bool hasEffect(const QString& effectId) const;

    /* @brief Returns a vector of pair (effect id, effect name)
     */
    QVector<QPair<QString, QString> > getEffectsNames() const;

    /* @brief Return type of effect */
    EffectType getEffectType(const QString& effectId) const;

    /* @brief Return name of effect */
    QString getEffectName(const QString& effectId) const;

    /* @brief Check whether a given effect is favorite */
    bool isFavorite(const QString& effectId) const;

protected:
    // Constructor is protected because class is a Singleton
    EffectsRepository();

    struct EffectInfo
    {
        QString name, description, author, version_str;
        double version;
        QString custom_xml_path;
        EffectType type;
    };

    // Reads the blacklist file and populate appropriate structure
    void parseBlackList();

    /* @brief Parse some info from a mlt effect (from the Mlt::Repository)
       @param res Datastructure to fill
       @return true on success
    */
    static bool parseEffectFromMlt(const QString& effectId, EffectInfo & res);

    /* @brief Retrieves additional info about effects from a custom XML file
     */
    void parseCustomEffectsFile(const QString& file_name);
    static std::unique_ptr<EffectsRepository> instance;
    static std::once_flag m_onceFlag; //flag to create the repository only once;

    std::unordered_map<QString, EffectInfo> m_effects;

    QSet<QString> m_blacklist;

};


#endif
