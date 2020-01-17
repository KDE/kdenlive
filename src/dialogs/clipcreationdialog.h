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

#ifndef CLIPCREATIONDIALOG_H
#define CLIPCREATIONDIALOG_H

#include "definitions.h"

class KdenliveDoc;
class Bin;
class ProjectClip;
class ProjectItemModel;

/**
 * @namespace ClipCreationDialog
 * @brief This namespace contains a list of static methods displaying widgets
 *  allowing creation of all clip types.
 */

namespace ClipCreationDialog {

QStringList getExtensions();
QString getExtensionsFilter(const QStringList& additionalFilters = QStringList());
void createColorClip(KdenliveDoc *doc, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model);
void createQTextClip(KdenliveDoc *doc, const QString &parentId, Bin *bin, ProjectClip *clip = nullptr);
void createSlideshowClip(KdenliveDoc *doc, const QString &parentId, std::shared_ptr<ProjectItemModel> model);
void createTitleClip(KdenliveDoc *doc, const QString &parentFolder, const QString &templatePath, std::shared_ptr<ProjectItemModel> model);
void createTitleTemplateClip(KdenliveDoc *doc, const QString &parentFolder, std::shared_ptr<ProjectItemModel> model);
void createClipsCommand(KdenliveDoc *doc, const QString &parentFolder, const std::shared_ptr<ProjectItemModel> &model);
} // namespace ClipCreationDialog

#endif
