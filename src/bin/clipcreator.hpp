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

#ifndef CLIPCREATOR_H
#define CLIPCREATOR_H

#include <QString>
#include <memory>

/** @brief This namespace provides convenienc function to create clips based on various parameters
 */

class ProjectItemModel;
namespace ClipCreator
{
    /* @brief Create and inserts a color clip
       @param color : a string of the form "0xff0000ff" (solid red in RGBA)
       @param duration : duration expressed in number of frames
       @param name: name of the clip
       @param parentFolder: the binId of the containing folder
       @param model: a shared pointer to the bin item model
       @return the binId of the created clip
    */
    QString createColorClip(const QString& color, int duration, const QString& name, const QString& parentFolder, std::shared_ptr<ProjectItemModel> model);
}

#endif
