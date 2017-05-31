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

#include "definitions.h"
#include "klocalizedstring.h"
#include <QAbstractListModel>
#include <QDomElement>
#include <unordered_map>

#include <memory>
#include <mlt++/MltProperties.h>

/* @brief This class is the model for a list of parameters.
   The behaviour of a transition or an effect is typically  controlled by several parameters. This class exposes this parameters as a list that can be rendered
   using the relevant widgets.
   Note that internally parameters are not sorted in any ways, because some effects like sox need a precise order

 */

enum class ParamType {
    Double,
    List,
    Bool,
    Switch,
    Animated,
    AnimatedRect,
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
Q_DECLARE_METATYPE(ParamType)
class AssetParameterModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit AssetParameterModel(Mlt::Properties *asset, const QDomElement &assetXml, const QString &assetId,
                                 Kdenlive::MonitorId monitor = Kdenlive::ProjectMonitor, QObject *parent = nullptr);
    virtual ~AssetParameterModel();
    enum {
        NameRole = Qt::UserRole + 1,
        TypeRole,
        CommentRole,
        MinRole,
        MaxRole,
        DefaultRole,
        SuffixRole,
        DecimalsRole,
        ValueRole,
        ListValuesRole,
        ListNamesRole,
        FactorRole,
        OpacityRole,
        InRole,
        OutRole
    };

    /* @brief Returns the id of the asset represented by this object */
    QString getAssetId() const;

    /* @brief Set the parameter with given name to the given value
     */
    void setParameter(const QString &name, const QString &value);

    /* @brief Return all the parameters as pairs (parameter name, parameter value) */
    QVector<QPair<QString, QVariant>> getAllParameters() const;

    /* @brief Sets the value of a list of parameters
       @param params contains the pairs (parameter name, parameter value)
     */
    void setParameters(const QVector<QPair<QString, QVariant>> &params);

    /* @brief Apply a change of parameter sent by the view
       @param index is the index corresponding to the modified param
       @param value is the new value of the parameter
    */
    void commitChanges(const QModelIndex &index, const QString &value);

    /* Which monitor is attached to this asset (clip/project)
    */
    Kdenlive::MonitorId monitorId;

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

protected:
    /* @brief Helper function to retrieve the type of a parameter given the string corresponding to it*/
    static ParamType paramTypeFromStr(const QString &type);

    /* @brief Helper function to get an attribute from a dom element, given its name.
       The function additionally parses following keywords:
       - %width and %height that are replaced with profile's height and width.
       If keywords are found, mathematical operations are supported for double type params. For example "%width -1" is a valid value.
    */
    static QVariant parseAttribute(const QString &attribute, const QDomElement &element, QVariant defaultValue = QVariant());
    struct ParamRow
    {
        ParamType type;
        QDomElement xml;
        QVariant value;
        QString name;
    };

    QDomElement m_xml;
    QString m_assetId;
    std::unordered_map<QString, ParamRow> m_params;      // Store all parameters by name
    std::unordered_map<QString, QVariant> m_fixedParams; // We store values of fixed parameters aside
    QVector<QString> m_rows;                             // We store the params name in order of parsing. The order is important (cf some effects like sox)

    std::unique_ptr<Mlt::Properties> m_asset;
};

#endif
