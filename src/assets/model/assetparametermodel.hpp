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

#ifndef ASSETPARAMETERMODEL_H
#define ASSETPARAMETERMODEL_H

#include <QAbstractListModel>
#include <QDomElement>
#include <unordered_map>
#include "klocalizedstring.h"
#include "core.h"
#include "definitions.h"

#include <mlt++/MltProperties.h>
#include <memory>

/* @brief This class is the model for a list of parameters.
   The behaviour of a transition or an effect is typically  controlled by several parameters. This class exposes this parameters as a list that can be rendered using the relevant widgets.

 */

enum class ParamType{
    Double,
    List,
    Bool,
    Switch,
    Animated,
    Geometry,
    Addedgeometry,
    Keyframe,
    Color,
    Position,
    Curve,
    Bezier_spline,
    Roto_spline,
    Wipe,
    Url,
    Keywords,
    Fontfamily,
    Filterjob,
    Readonly
};
class AssetParameterModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit AssetParameterModel(Mlt::Properties *asset, const QDomElement &assetXml, const QString &assetId, QObject *parent = nullptr);
    virtual ~AssetParameterModel();
    enum {
        NameRole = Qt::UserRole + 1,
        TypeRole,
        CommentRole,
        MinRole,
        MaxRole,
        DefaultRole,
        SuffixRole,
        DecimalsRole
    };

    /* @brief Returns the id of the asset represented by this object */
    QString getId() const;

    /* @brief Set the parameter with given name to the given value
       @param store: if this is true, then the value is also stored in m_params
     */
    void setParameter(const QString& name, const QString& value, bool store = true);


    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

protected:
    /* @brief Helper function to retrieve the type of a parameter given the string corresponding to it*/
    static ParamType paramTypeFromStr(const QString & type);

    /* @brief Helper function to get a double attribute from a dom element, given its name.
       The function additionally parses following keywords:
       - %width and %height that are replaced with profile's height and width.
       If keywords are found, mathematical operations are supported. For example "%width -1" is a valid value.
    */
    static double parseDoubleAttribute(const QString& attribute, const QDomElement& element);

    struct ParamRow{
        ParamType type;
        QDomElement xml;
        QVariant value;
    };

    QDomElement m_xml;
    QString m_assetId;
    std::unordered_map<QString, ParamRow > m_params; //Store all parameters by name
    QVector<QString> m_rows; // We store the params name in order of parsing

    std::unique_ptr<Mlt::Properties> m_asset;
};

#endif
