/*
 *  Copyright (c) 2015 Dmitry Kazakov <dimula73@gmail.com>
 *  Copyright (c) 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __KO_ICON_UTILS_H
#define __KO_ICON_UTILS_H

#include "kdenlivesettings.h"
#include "kxmlgui_version.h"
#include <QIcon>
#include <KIconEngine>
#include <KIconLoader>

namespace KoIconUtils
{
#if KXMLGUI_VERSION_MINOR > 22 || KXMLGUI_VERSION_MAJOR > 5
inline QIcon themedIcon(const QString &name)
{
    if (KdenliveSettings::force_breeze()) {
        return QIcon(new KIconEngine(name, KIconLoader::global()));
    }
    return QIcon::fromTheme(name);
}
#else
/**
 * Load a themed icon using its base name. Use it in
 * Krita instead of previous themedIcon()
 */
QIcon themedIcon(const QString &name);
#endif
}

#endif /* __KIS_ICON_UTILS_H */
