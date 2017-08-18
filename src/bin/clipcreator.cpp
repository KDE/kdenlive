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

#include "clipcreator.hpp"
#include "projectitemmodel.h"
#include "xml/xml.hpp"
#include "klocalizedstring.h"
#include <QDomDocument>

QString ClipCreator::createColorClip(const QString& color, int duration, const QString& name, const QString& parentFolder, std::shared_ptr<ProjectItemModel> model)
{
    QDomDocument xml;
    QDomElement prod = xml.createElement(QStringLiteral("producer"));
    xml.appendChild(prod);
    prod.setAttribute(QStringLiteral("type"), (int) Color);
    prod.setAttribute(QStringLiteral("in"), QStringLiteral("0"));
    prod.setAttribute(QStringLiteral("length"), duration);
    QMap<QString, QString> properties;
    properties.insert(QStringLiteral("resource"), color);
    properties.insert(QStringLiteral("kdenlive:clipname"), name);
    properties.insert(QStringLiteral("mlt_service"), QStringLiteral("color"));
    Xml::addXmlProperties(prod, properties);

    QString id;
    model->requestAddBinClip(id, xml.documentElement(), parentFolder, i18n("Create color clip"));
    return id;
}

