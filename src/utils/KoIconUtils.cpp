/*
 *  Copyright (c) 2015 Dmitry Kazakov <dimula73@gmail.com>
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

#include "KoIconUtils.h"

#include <QIcon>
#include <QPalette>
#include <QApplication>
#include "kdenlive_debug.h"

#include <kiconloader.h>
#include <kiconengine.h>

namespace KoIconUtils
{

#if KXMLGUI_VERSION_MINOR > 22 || KXMLGUI_VERSION_MAJOR > 5
// Returning QIcon::fromTheme directly from header file
#else

QIcon themedIcon(const QString &name)
{
    // try load themed icon
    if (name.isEmpty()) {
        return QIcon();
    }
    QColor background = qApp->palette().window().color();
    bool useDarkIcons = background.value() < 100;
    QString realName;
    const char *const prefix = useDarkIcons ? "dk_" : "lt_";
    if (name.at(2) == '_') {
        realName = name.mid(3);
    } else {
        realName = name;
    }
    realName.prepend(QLatin1String(prefix));

    if (KIconLoader::global()->iconPath(realName, KIconLoader::User, true).isEmpty()) {
        realName = name;
    }
    //qCDebug(KDENLIVE_LOG)<<"-- LOADING ICON: "<<realName;
    QIcon icon = QIcon(new KIconEngine(realName, KIconLoader::global()));

    // fallback
    if (icon.isNull()) {
        icon = QIcon::fromTheme(name);
    }

    return icon;

}
#endif
}
