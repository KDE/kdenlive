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
#include <QObject>
#include <QUrl>


namespace Mlt {
    class Profile;
}

/**
 * @namespace EffectsController
 * @brief Provides convenience methods to manage effects and convert between MLT's Filter format and Kdenlive's internal formats (xml or lists).
 */

namespace EffectsController
{
    /** @brief Gets the effect parameters that will be passed to Mlt. */
    EffectsParameterList getEffectArgs(Mlt::Profile *profile, const QDomElement &effect);

    /** @brief Get effect parameters ready for MLT*/
    void adjustEffectParameters(EffectsParameterList &parameters, QDomNodeList params, Mlt::Profile *profile, const QString &prefix = QString());

    /** @brief Returns an value from a string by replacing "%width" and "%height" with given profile values:
     *  @param profile The profile that gives width & height
     *  @param eval The string to be evaluated, for example: "%width / 2"
     *  @return the evaluated value */
    double getStringEval(Mlt::Profile *profile, QString eval, const QPoint& frameSize = QPoint());
    void initEffect(Mlt::Profile *profile, ItemInfo info, EffectsList list, const QString proxy, QDomElement effect, int diff = 0, int offset = 0);
        /** @brief Adjust keyframes to the new clip. */
    const QString adjustKeyframes(const QString &keyframes, int offset);
    EffectsParameterList addEffect(Mlt::Profile *profile, QDomElement effect);
};

#endif
