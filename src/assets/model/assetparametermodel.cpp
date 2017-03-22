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

#include "assetparametermodel.hpp"
#include <QString>
#include <QLocale>
#include <QDebug>
#include <QScriptEngine>
#include "profiles/profilemodel.hpp"


AssetParameterModel::AssetParameterModel(Mlt::Properties *asset, const QDomElement &assetXml, QObject *parent)
    : QAbstractItemModel(parent)
    , m_xml(assetXml)
    , m_asset(asset)
{
    QDomNodeList nodeList = m_xml.elementsByTagName(QStringLiteral("parameter"));

    bool needsLocaleConversion = false;
    QChar separator, oldSeparator ;
    // Check locale
    if (m_xml.hasAttribute(QStringLiteral("LC_NUMERIC"))) {
        QLocale locale = QLocale(m_xml.attribute(QStringLiteral("LC_NUMERIC")));
        if (locale.decimalPoint() != QLocale().decimalPoint()) {
            needsLocaleConversion = true;
            separator = QLocale().decimalPoint();
            oldSeparator = locale.decimalPoint();
        }
    }

    for (int i = 0; i < nodeList.count(); ++i) {
        QDomElement currentParameter = nodeList.item(i).toElement();

        //Convert parameters if we need to
        if (needsLocaleConversion) {
            QDomNamedNodeMap attrs = currentParameter.attributes();
            for (int k = 0; k < attrs.count(); ++k) {
                QString nodeName = attrs.item(k).nodeName();
                if (nodeName != QLatin1String("type") && nodeName != QLatin1String("name")) {
                    QString val = attrs.item(k).nodeValue();
                    if (val.contains(oldSeparator)) {
                        QString newVal = val.replace(oldSeparator, separator);
                        attrs.item(k).setNodeValue(newVal);
                    }
                }
            }
        }

        // Parse the basic attributes of the parameter
        QString name = currentParameter.attribute(QStringLiteral("name"));
        QString type = currentParameter.attribute(QStringLiteral("type"));
        QString value = currentParameter.attribute(QStringLiteral("value"));
        if (value.isNull()) {
            value = currentParameter.attribute(QStringLiteral("default"));
        }
        setParameter(name, value);
        if (type == QLatin1String("fixed")) {
            //fixed parameters are not displayed so we don't store them.
            continue;
        }
        ParamRow currentRow;
        currentRow.type = paramTypeFromStr(type);
        currentRow.xml = currentParameter;
        currentRow.value = value;
        m_params[name] = currentRow;
        m_rows.push_back(name);
    }
}

void AssetParameterModel::setParameter(const QString& name, const QString& value)
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);

    bool conversionSuccess;
    double doubleValue = locale.toDouble(value, &conversionSuccess);
    if (conversionSuccess) {
        m_asset->set(name.toLatin1().constData(), doubleValue);
        m_params[name].value = doubleValue;
    } else {
        m_asset->set(name.toLatin1().constData(), value.toUtf8().constData());
        m_params[name].value = value;
    }
}

AssetParameterModel::~AssetParameterModel()
{
}

int AssetParameterModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant AssetParameterModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    QString paramName = m_rows[index.row()];
    const QDomElement &element = m_params.at(paramName).xml;
    switch (role) {
    case CommentRole:{
        QDomElement commentElem = element.firstChildElement(QStringLiteral("comment"));
        QString comment;
        if (!commentElem.isNull()) {
            comment = i18n(commentElem.text().toUtf8().data());
        }
        return comment;
    }
    case MinRole:
        return parseDoubleAttribute(QStringLiteral("min"), element);
    case MaxRole:
        return parseDoubleAttribute(QStringLiteral("max"), element);
    case DefaultRole:
        return parseDoubleAttribute(QStringLiteral("default"), element);
    case SuffixRole:
        return element.attribute(QStringLiteral("suffix"));
    case DecimalsRole:
        return element.attribute(QStringLiteral("decimals"));

    }
    return QVariant();
}

QModelIndex AssetParameterModel::index(int row, int column, const QModelIndex &parent)
            const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();
    return createIndex(row, column);
}

QModelIndex AssetParameterModel::parent(const QModelIndex &index) const
{
    Q_UNUSED(index);
    return QModelIndex();
}


int AssetParameterModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_rows.size();
}


//static
ParamType AssetParameterModel::paramTypeFromStr(const QString & type)
{
    if (type == QLatin1String("double") || type == QLatin1String("constant")) {
        return ParamType::Double;
    } else if (type == QLatin1String("list")) {
        return ParamType::List;
    } else if (type == QLatin1String("bool")) {
        return ParamType::Bool;
    } else if (type == QLatin1String("switch")) {
        return ParamType::Switch;
    } else if (type.startsWith(QLatin1String("animated"))) {
        return ParamType::Animated;
    } else if (type == QLatin1String("geometry")) {
        return ParamType::Geometry;
    } else if (type == QLatin1String("addedgeometry")) {
        return ParamType::Addedgeometry;
    } else if (type == QLatin1String("keyframe") || type == QLatin1String("simplekeyframe")) {
        return ParamType::Keyframe;
    } else if (type == QLatin1String("color")) {
        return ParamType::Color;
    } else if (type == QLatin1String("position")) {
        return ParamType::Position;
    } else if (type == QLatin1String("curve")) {
        return ParamType::Curve;
    } else if (type == QLatin1String("bezier_spline")) {
        return ParamType::Bezier_spline;
    } else if (type == QLatin1String("roto-spline")) {
        return ParamType::Roto_spline;
    } else if (type == QLatin1String("wipe")) {
        return ParamType::Wipe;
    } else if (type == QLatin1String("url")) {
        return ParamType::Url;
    } else if (type == QLatin1String("keywords")) {
        return ParamType::Keywords;
    } else if (type == QLatin1String("fontfamily")) {
        return ParamType::Fontfamily;
    } else if (type == QLatin1String("filterjob")) {
        return ParamType::Filterjob;
    } else if (type == QLatin1String("readonly")) {
        return ParamType::Readonly;
    }
    qDebug() << "WARNING: Unknown type :"<<type;
    return ParamType::Double;
}


// static
double AssetParameterModel::parseDoubleAttribute(const QString& attribute, const QDomElement& element)
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);

    QString content = element.attribute(attribute);
    if (content.contains(QLatin1Char('%'))) {
        QScriptEngine sEngine;
        std::unique_ptr<ProfileModel> &profile = pCore->getCurrentProfile();
        int width = profile->width();
        int height = profile->height();
        sEngine.globalObject().setProperty(QStringLiteral("maxWidth"), width);
        sEngine.globalObject().setProperty(QStringLiteral("maxHeight"), height);
        sEngine.globalObject().setProperty(QStringLiteral("width"), width);
        sEngine.globalObject().setProperty(QStringLiteral("height"), height);
        return sEngine.evaluate(content.remove('%')).toNumber();
    } else {
        return locale.toDouble(content);
    }
    return -1;
}
