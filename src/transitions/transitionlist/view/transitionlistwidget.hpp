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

#ifndef TRANSITIONLISTWIDGET_H
#define TRANSITIONLISTWIDGET_H

#include "assets/assetlist/view/assetlistwidget.hpp"

/* @brief This class is a widget that display the list of available effects
 */

class TransitionListWidget : public AssetListWidget
{
    Q_OBJECT

public:
    TransitionListWidget(QWidget *parent = Q_NULLPTR);

    /*@brief Return mime type used for drag and drop. It will be kdenlive/composition
     or kdenlive/transition*/
    Q_INVOKABLE QString getMimeType(const QString &assetId) const override;
};

#endif
