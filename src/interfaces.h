/***************************************************************************
 *   Copyright (C) 2009 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#ifndef INTERFACES_H
#define INTERFACES_H

#include <QStringList>

class ClipGenerator
{
public:
    virtual ~ClipGenerator() {}

    virtual QStringList generators(const QStringList &producers = QStringList()) const = 0;
    virtual QUrl generatedClip(const QString &renderer, const QString &generator, const QUrl &projectFolder, const QStringList &lumaNames,
                               const QStringList &lumaFiles, const double fps, const int width, const int height) = 0;
};

Q_DECLARE_INTERFACE(ClipGenerator, "com.kdenlive.ClipGenerator.ClipGeneratorInterface/1.0")

#endif
