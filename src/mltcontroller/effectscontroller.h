/*
Copyright (C) 2015  Jean-Baptiste Mardelle <jb@kdenlive.org>
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

#ifndef EFFECTSCONTROLLER_H
#define EFFECTSCONTROLLER_H

#include "definitions.h"

#include <mlt++/Mlt.h>
#include <QString>

/**)
 * @class EffectInfo
 * @brief A class holding some meta info for effects widgets, like state (collapsed or not, ...)
 * @author Jean-Baptiste Mardelle
 */

class EffectInfo
{
public:
    EffectInfo();
    bool isCollapsed;
    bool groupIsCollapsed;
    int groupIndex;
    QString groupName;
    QString toString() const;
    void fromString(const QString &value);
};

/**)
 * @class EffectParameter
 * @brief Base class holding a parameter name / value. Is this really useful? QMap ?
 * @author Jean-Baptiste Mardelle
 */

class EffectParameter
{
public:
    EffectParameter(const QString &name, const QString &value);
    QString name()   const;
    QString value() const;
    void setValue(const QString &value);

private:
    QString m_name;
    QString m_value;
};

/**)
 * @class EffectsParameterList
 * @brief Use our own list for effect parameters so that they are not sorted in any ways, because
 *               some effects like sox need a precise order
 * @author Jean-Baptiste Mardelle
 */

class EffectsParameterList: public QList< EffectParameter >
{
public:
    EffectsParameterList();
    bool hasParam(const QString &name) const;
    QString paramValue(const QString &name, const QString &defaultValue = QString()) const;
    void addParam(const QString &name, const QString &value);
    void removeParam(const QString &name);
};

/**
 * @namespace EffectsController
 * @brief Provides convenience methods to manage effects and convert between MLT's Filter format and Kdenlive's internal formats (xml or lists).
 */

namespace EffectsController
{
/** @brief Gets the effect parameters that will be passed to Mlt. */
EffectsParameterList getEffectArgs(const ProfileInfo &info, const QDomElement &effect);

/** @brief Get effect parameters ready for MLT*/
void adjustEffectParameters(EffectsParameterList &parameters, const QDomNodeList &params, const ProfileInfo &info, const QString &prefix = QString());

/** @brief Returns an value from a string by replacing "%width" and "%height" with given profile values:
 *  @param info The struct that gives width & height
 *  @param eval The string to be evaluated, for example: "%width / 2"
 *  @return the evaluated value */
double getStringEval(const ProfileInfo &info, QString eval, const QPoint &frameSize = QPoint());
QString getStringRectEval(const ProfileInfo &info, QString eval);

/** @brief Initialize some track effects parameters */
void initTrackEffect(ProfileInfo pInfo, const QDomElement &effect);
/** @brief Initialize some effects parameters: keyframes, fades, in / out points  */
void initEffect(const ItemInfo &info, ProfileInfo pInfo, const EffectsList &list, const QString &proxy, QDomElement effect, int diff = 0, int offset = 0);

/** @brief Adjust keyframes to the new clip. */
const QString adjustKeyframes(const QString &keyframes, int oldIn, int newIn, int newEnd, ProfileInfo pInfo);
EffectsParameterList addEffect(const ProfileInfo &info, const QDomElement &effect);

/** @brief Keyframe frame numbers are relative to clip's crop start. So when saving an effect, remove clip cropstart from keyframe numbers so that when we apply effect on another clip, frame numbers are applied relative to the new clip's crop start. */
void offsetKeyframes(int in, const QDomElement &effect);
}

#endif
