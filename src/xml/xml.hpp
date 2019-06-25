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

#include "definitions.h"
#include <QDomElement>
#include <QString>
#include <QVector>
#include <unordered_map>

/** @brief This static class provides helper functions to manipulate Dom objects easily
 */

namespace Xml {

/* @brief Returns the content of a given tag within the current DomElement.
   For example, if your @param element looks like <html><title>foo</title><head>bar</head></html>, passing @tagName = "title" will return foo, and @tagName
   = "head" returns bar
   Returns empty string if tag is not found.
*/
QString getSubTagContent(const QDomElement &element, const QString &tagName);

/* @brief Returns the direct children of given @element whose tag name matches given @param tagName
   This is an alternative to QDomElement::elementsByTagName which returns also non-direct children
*/
QVector<QDomNode> getDirectChildrenByTagName(const QDomElement &element, const QString &tagName);

/* @brief Returns the content of a children tag of @param element, which respects the following conditions :
   - Its type is @param tagName
   - It as an attribute named @param attribute with value @param value
   For example, if your element is <html><param val="foo">bar</param></html>, you can retrieve "bar" with parameters: tagName="param", attribute="val", and
   value="foo" Returns @param defaultReturn when nothing is found. The methods returns the first match found, so make sure there can't be more than one. If
   @param directChildren is true, only immediate children of the node are considered
*/
QString getTagContentByAttribute(const QDomElement &element, const QString &tagName, const QString &attribute, const QString &value,
                                 const QString &defaultReturn = QString(), bool directChildren = true);

/* @brief This is a specialization of getTagContentByAttribute with tagName = "property" and attribute = "name".
   That is, to match something like <elem><property name="foo">bar</property></elem>, pass propertyName = foo, and this will return bar
*/
QString getXmlProperty(const QDomElement &element, const QString &propertyName, const QString &defaultReturn = QString());
QString getXmlParameter(const QDomElement &element, const QString &propertyName, const QString &defaultReturn = QString());

/* @brief Returns true if the element contains a named property
*/
bool hasXmlProperty(QDomElement element, const QString &propertyName);

/* @brief Add properties to the given xml element
   For each element (n, v) in the properties map, it creates a sub element of the form : <property name="n">v</property>
   @param producer is the xml element where to append properties
   @param properties is the map of properties
 */
void addXmlProperties(QDomElement &producer, const std::unordered_map<QString, QString> &properties);
void addXmlProperties(QDomElement &producer, const QMap<QString, QString> &properties);
/* @brief Edit or add a property
 */
void setXmlProperty(QDomElement element, const QString &propertyName, const QString &value);
/* @brief Remove a property
 */
void removeXmlProperty(QDomElement effect, const QString &name);
void removeMetaProperties(QDomElement producer);

void renameXmlProperty(const QDomElement &effect, const QString &oldName, const QString &newName);

QMap<QString, QString> getXmlPropertyByWildcard(QDomElement element, const QString &propertyName);

} // namespace Xml

#endif
