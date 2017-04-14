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

#ifndef XML_H
#define XML_H

#include <QDomElement>
#include <QString>
#include <QVector>

/** @brief This static class provides helper functions to manipulate Dom objects easily
 */

class Xml
{

public:
    Xml() = delete;

    /* @brief Returns the content of a given tag within the current DomElement.
       For example, if your @param element looks like <html><title>foo</title><head>bar</head></html>, passing @tagName = "title" will return foo, and @tagName
       = "head" returns bar
       Returns empty string if tag is not found.
    */
    static QString getSubTagContent(const QDomElement &element, const QString &tagName);

    /* @brief Returns the direct children of given @element whose tag name matches given @paam tagName
       This is an alternative to QDomElement::elementsByTagName which returns also non-direct children
    */
    static QVector<QDomNode> getDirectChildrenByTagName(const QDomElement &element, const QString &tagName);
};

#endif
