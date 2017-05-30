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
#include "core.h"
#include "klocalizedstring.h"
#include "profiles/profilemodel.hpp"
#include <QDebug>
#include <QLocale>
#include <QString>

AssetParameterModel::AssetParameterModel(Mlt::Properties *asset, const QDomElement &assetXml, const QString &assetId, Kdenlive::MonitorId monitor,
                                         QObject *parent)
    : QAbstractListModel(parent)
    , m_xml(assetXml)
    , m_assetId(assetId)
    , monitorId(monitor)
    , m_asset(asset)
{
    Q_ASSERT(asset->is_valid());
    QDomNodeList nodeList = m_xml.elementsByTagName(QStringLiteral("parameter"));

    bool needsLocaleConversion = false;
    QChar separator, oldSeparator;
    // Check locale
    if (m_xml.hasAttribute(QStringLiteral("LC_NUMERIC"))) {
        QLocale locale = QLocale(m_xml.attribute(QStringLiteral("LC_NUMERIC")));
        if (locale.decimalPoint() != QLocale().decimalPoint()) {
            needsLocaleConversion = true;
            separator = QLocale().decimalPoint();
            oldSeparator = locale.decimalPoint();
        }
    }

    qDebug() << "XML parsing of " << assetId << ". found : " << nodeList.count();
    for (int i = 0; i < nodeList.count(); ++i) {
        QDomElement currentParameter = nodeList.item(i).toElement();

        // Convert parameters if we need to
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
        bool isFixed = (type == QLatin1String("fixed"));
        if (isFixed) {
            m_fixedParams[name] = value;
        }
        setParameter(name, value);
        if (isFixed) {
            // fixed parameters are not displayed so we don't store them.
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

void AssetParameterModel::setParameter(const QString &name, const QString &value)
{
    Q_ASSERT(m_asset->is_valid());
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);

    bool conversionSuccess;
    double doubleValue = locale.toDouble(value, &conversionSuccess);
    if (conversionSuccess) {
        m_asset->set(name.toLatin1().constData(), doubleValue);
        if (m_fixedParams.count(name) == 0) {
            m_params[name].value = doubleValue;
        } else {
            m_fixedParams[name] = doubleValue;
        }
    } else {
        m_asset->set(name.toLatin1().constData(), value.toUtf8().constData());
        if (m_fixedParams.count(name) == 0) {
            m_params[name].value = value;
        } else {
            m_fixedParams[name] = value;
        }
    }
    // refresh monitor after asset change
    QSize range(m_asset->get_int("in"), m_asset->get_int("out"));
    pCore->refreshProjectRange(range);
}

AssetParameterModel::~AssetParameterModel() = default;

QVariant AssetParameterModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_rows.size() || !index.isValid()) {
        return QVariant();
    }
    QString paramName = m_rows[index.row()];
    Q_ASSERT(m_params.count(paramName) > 0);
    const QDomElement &element = m_params.at(paramName).xml;
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
    case NameRole:
        return paramName;
    case TypeRole:
        return QVariant::fromValue<ParamType>(m_params.at(paramName).type);
    case CommentRole: {
        QDomElement commentElem = element.firstChildElement(QStringLiteral("comment"));
        QString comment;
        if (!commentElem.isNull()) {
            comment = i18n(commentElem.text().toUtf8().data());
        }
        return comment;
    }
    case InRole:
        return m_asset->get_int("in");
    case OutRole:
        return m_asset->get_int("out");
    case MinRole:
        return parseDoubleAttribute(QStringLiteral("min"), element);
    case MaxRole:
        return parseDoubleAttribute(QStringLiteral("max"), element);
    case FactorRole:
        return parseDoubleAttribute(QStringLiteral("factor"), element, 1);
    case DecimalsRole:
        return (int)parseDoubleAttribute(QStringLiteral("decimals"), element);
    case DefaultRole:
        return parseAttribute(QStringLiteral("default"), element);
    case SuffixRole:
        return element.attribute(QStringLiteral("suffix"));
    case OpacityRole:
        return element.attribute(QStringLiteral("opacity")) != QLatin1String("false");
    case ValueRole:
        return element.attribute(QStringLiteral("value")).isNull() ? parseAttribute(QStringLiteral("default"), element) : element.attribute(QStringLiteral("value"));
    case ListValuesRole:
        return element.attribute(QStringLiteral("paramlist")).split(QLatin1Char(';'));
    case ListNamesRole: {
        QDomElement namesElem = element.firstChildElement(QStringLiteral("paramlistdisplay"));
        return i18n(namesElem.text().toUtf8().data()).split(QLatin1Char(','));
    }
    }
    return QVariant();
}

int AssetParameterModel::rowCount(const QModelIndex &parent) const
{
    qDebug() << "===================================================== Requested rowCount" << parent << m_rows.size();
    if (parent.isValid()) return 0;
    return m_rows.size();
}

// static
ParamType AssetParameterModel::paramTypeFromStr(const QString &type)
{
    if (type == QLatin1String("double") || type == QLatin1String("constant")) {
        return ParamType::Double;
    }
    if (type == QLatin1String("list")) {
        return ParamType::List;
    }
    if (type == QLatin1String("bool")) {
        return ParamType::Bool;
    }
    if (type == QLatin1String("switch")) {
        return ParamType::Switch;
    } else if (type == QLatin1String("animated")) {
        return ParamType::Animated;
    } else if (type == QLatin1String("animatedrect")) {
        return ParamType::AnimatedRect;
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
    qDebug() << "WARNING: Unknown type :" << type;
    return ParamType::Double;
}


// static
QVariant AssetParameterModel::parseAttribute(const QString &attribute, const QDomElement &element, QVariant defaultValue)
{
    if (!element.hasAttribute(attribute)) {
        return defaultValue;
    }
    ParamType type = paramTypeFromStr(element.attribute(QStringLiteral("type")));
    QString content = element.attribute(attribute);
    if (content.contains(QLatin1Char('%'))) {
        std::unique_ptr<ProfileModel> &profile = pCore->getCurrentProfile();
        int width = profile->width();
        int height = profile->height();

        // replace symbols in the double parameter
        content.replace(QLatin1String("%maxWidth"), QString::number(width))
            .replace(QLatin1String("%maxHeight"), QString::number(height))
            .replace(QLatin1String("%width"), QString::number(width))
            .replace(QLatin1String("%height"), QString::number(height));

        if (type == ParamType::Double) {
            // Use a Mlt::Properties to parse mathematical operators
            Mlt::Properties p;
            p.set("eval", content.toLatin1().constData());
            return p.get_double("eval");
        }
    }
    qDebug()<<"__RESTLT VAL: "<<content;
    if (type == ParamType::Double) {
        QLocale locale;
        locale.setNumberOptions(QLocale::OmitGroupSeparator);
        return locale.toDouble(content);
    }
    return content;
}

// static
double AssetParameterModel::parseDoubleAttribute(const QString &attribute, const QDomElement &element, double defaultValue)
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    if (!element.hasAttribute(attribute)) {
        return defaultValue;
    }
    QString content = element.attribute(attribute);
    if (content.contains(QLatin1Char('%'))) {
        std::unique_ptr<ProfileModel> &profile = pCore->getCurrentProfile();
        int width = profile->width();
        int height = profile->height();

        // replace symbols in the double parameter
        content.replace(QLatin1String("%maxWidth"), QString::number(width))
            .replace(QLatin1String("%maxHeight"), QString::number(height))
            .replace(QLatin1String("%width"), QString::number(width))
            .replace(QLatin1String("%height"), QString::number(height));

        // Use a Mlt::Properties to parse mathematical operators
        Mlt::Properties p;
        p.set("eval", content.toLatin1().constData());
        return p.get_double("eval");
    }
    return locale.toDouble(content);
}

QString AssetParameterModel::getAssetId() const
{
    return m_assetId;
}

QVector<QPair<QString, QVariant>> AssetParameterModel::getAllParameters() const
{
    QVector<QPair<QString, QVariant>> res;
    res.reserve((int)m_fixedParams.size() + (int)m_params.size());
    for (const auto &fixed : m_fixedParams) {
        res.push_back(QPair<QString, QVariant>(fixed.first, fixed.second));
    }

    for (const auto &param : m_params) {
        res.push_back(QPair<QString, QVariant>(param.first, param.second.value));
    }
    return res;
}

void AssetParameterModel::setParameters(const QVector<QPair<QString, QVariant>> &params)
{
    for (const auto &param : params) {
        setParameter(param.first, param.second.toString());
    }
}

void AssetParameterModel::commitChanges(const QModelIndex &index, const QString &value)
{
    QString name = data(index, NameRole).toString();
    setParameter(name, value);
}
