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
class QUndoCommand;
class Bin;
class ProjectClip;

/**
 * @namespace ClipCreationDialog
 * @brief This namespace contains a list of static methods displaying widgets
 *  allowing creation of all clip types.
 */

namespace ClipCreationDialog
{

QStringList getExtensions();
void createColorClip(KdenliveDoc *doc, const QStringList &groupInfo, Bin *bin);
void createQTextClip(KdenliveDoc *doc, const QStringList &groupInfo, Bin *bin, ProjectClip *clip = nullptr);
void createClipFromXml(KdenliveDoc *doc, QDomElement &xml, const QStringList &groupInfo, Bin *bin);
void createSlideshowClip(KdenliveDoc *doc, const QStringList &groupInfo, Bin *bin);
void createTitleClip(KdenliveDoc *doc, const QStringList &groupInfo, const QString &templatePath, Bin *bin);
void createTitleTemplateClip(KdenliveDoc *doc, const QStringList &groupInfo, Bin *bin);
void createClipsCommand(KdenliveDoc *doc, const QList<QUrl> &urls, const QStringList &groupInfo, Bin *bin, const QMap<QString, QString> &data = QMap<QString, QString>());
void createClipsCommand(Bin *bin, const QDomElement &producer, const QString &id, QUndoCommand *command);
void createClipsCommand(KdenliveDoc *doc, const QStringList &groupInfo, Bin *bin);
void addXmlProperties(QDomElement &producer, QMap<QString, QString> &properties);
}

#endif

