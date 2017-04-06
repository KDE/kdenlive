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

#ifndef EFFECTITEMMODEL_H
#define EFFECTITEMMODEL_H

#include "abstractmodel/treeitem.hpp"
#include "assets/model/assetparametermodel.hpp"
#include <mlt++/MltFilter.h>

/* @brief This represents an effect of the effectstack
 */
class EffectItemModel : public TreeItem, public AssetParameterModel
{

public:

    /* This construct an effect of the given id */
    static EffectItemModel* construct(const QString & effectId);

    /* @brief This function plants the effect into the given service in last position
     */
    void plant(std::weak_ptr<Mlt::Service> service);

    /* @brief This function change the individual enabled state of the effect */
    void setEnabled(bool enabled);

    /* @brief This function change the global (timeline-wise) enabled state of the effect */
    void setTimelineEffectsEnabled(bool enabled);

    /* @brief Returns whether the effect is enabled */
    bool isEnabled() const;

    Mlt::Filter &filter() const;

protected:
    EffectItemModel(const QList<QVariant> &data, Mlt::Properties *effect, const QDomElement &xml, const QString &effectId);

    /* @brief Toogles the mlt effect according to the current activation state*/
    void updateEnable();


    bool m_enabled;
    bool m_timelineEffectsEnabled;


};

#endif
