/***************************************************************************
 *   Copyright (C) 2010 by Pascal Fleury (fleury@users.sourceforge.net)    *
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

#include "jogshuttleconfig.h"

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include <cstdlib>

using std::string;
using std::stringstream;
using std::vector;

// these 2 functions will convert the action maps to and from a string representation not unlike this:
// button1=rewind_one_frame;button2=forward_one_frame;button15=play

static const QChar DELIMITER = ';';
static const QChar KEY_VALUE_SEP = '=';
static const QLatin1String BUTTON_PREFIX("button");

QStringList JogShuttleConfig::actionMap(const QString &actionsConfig)
{
    QStringList actionMap;
    const QStringList mappings = actionsConfig.split(DELIMITER);

    for (const QString &mapping : mappings) {
        QStringList parts = mapping.split(KEY_VALUE_SEP);
        if (parts.size() != 2) {
            fprintf(stderr, "Invalid button configuration: %s", mapping.toLatin1().constData());
            continue;
        }
        // skip the 'button' prefix
        int button_id = parts[0].midRef(BUTTON_PREFIX.size()).toInt();
        // fprintf(stderr, " - Handling map key='%s' (ID=%d), value='%s'\n", parts[0].data().toLatin1(), button_id, parts[1].data().toLatin1()); // DBG
        while (actionMap.size() <= button_id) {
            actionMap << QString();
        }
        actionMap[button_id] = parts[1];
    }

    // for (int i = 0; i < actionMap.size(); ++i) fprintf(stderr, "button #%d -> action '%s'\n", i, actionMap[i].data().toLatin1());  //DBG
    return actionMap;
}

QString JogShuttleConfig::actionMap(const QStringList &actionMap)
{
    QStringList mappings;
    for (int i = 0; i < actionMap.size(); ++i) {
        if (actionMap[i].isEmpty()) {
            continue;
        }
        mappings << QStringLiteral("%1%2%3%4").arg(BUTTON_PREFIX).arg(i).arg(KEY_VALUE_SEP).arg(actionMap[i]);
    }

    return mappings.join(DELIMITER);
}
