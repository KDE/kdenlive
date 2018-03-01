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
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "klocalizedstring.h"
#include "profiles/profilemodel.hpp"
#include <QDebug>
#include <QLocale>
#include <QString>

AssetParameterModel::AssetParameterModel(Mlt::Properties *asset, const QDomElement &assetXml, const QString &assetId, ObjectId ownerId, QObject *parent)
    : QAbstractListModel(parent)
    , monitorId(ownerId.first == ObjectType::BinClip ? Kdenlive::ClipMonitor : Kdenlive::ProjectMonitor)
    , m_xml(assetXml)
    , m_assetId(assetId)
    , m_ownerId(ownerId)
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
            value = parseAttribute(QStringLiteral("default"), currentParameter).toString();
        }
        bool isFixed = (type == QLatin1String("fixed"));
        if (isFixed) {
            m_fixedParams[name] = value;
        }
        if (!name.isEmpty()) {
            setParameter(name, value, false);
            // Keep track of param order
            m_paramOrder.push_back(name);
        }
        if (isFixed) {
            // fixed parameters are not displayed so we don't store them.
            continue;
        }
        ParamRow currentRow;
        currentRow.type = paramTypeFromStr(type);
        currentRow.xml = currentParameter;
        currentRow.value = value;
        QString title = currentParameter.firstChildElement(QStringLiteral("name")).text();
        currentRow.name = title.isEmpty() ? name : title;
        m_params[name] = currentRow;
        m_rows.push_back(name);
    }
    if (m_assetId.startsWith(QStringLiteral("sox_"))) {
        // Sox effects need to have a special "Effect" value set
        QStringList effectParam = {m_assetId.section(QLatin1Char('_'), 1)};
        for (const QString &pName : m_paramOrder) {
            effectParam << m_asset->get(pName.toUtf8().constData());
        }
        m_asset->set("effect", effectParam.join(QLatin1Char(' ')).toUtf8().constData());
    }
    qDebug() << "END parsing of " << assetId << ". Number of found parameters" << m_rows.size();
    emit modelChanged();
}

void AssetParameterModel::prepareKeyframes()
{
    if (m_keyframes) return;
    int ix = 0;
    for (const auto &name : m_rows) {
        if (m_params[name].type == ParamType::KeyframeParam || m_params[name].type == ParamType::AnimatedRect) {
            addKeyframeParam(index(ix, 0));
        }
        ix++;
    }
}

void AssetParameterModel::setParameter(const QString &name, const int value, bool update)
{
    Q_ASSERT(m_asset->is_valid());
    m_asset->set(name.toLatin1().constData(), value);
    if (m_fixedParams.count(name) == 0) {
        m_params[name].value = value;
    } else {
        m_fixedParams[name] = value;
    }
    if (update) {
        if (m_assetId.startsWith(QStringLiteral("sox_"))) {
            // Warning, SOX effect, need unplug/replug
            qDebug()<<"// Warning, SOX effect, need unplug/replug";
            QStringList effectParam = {m_assetId.section(QLatin1Char('_'), 1)};
            for (const QString &pName : m_paramOrder) {
                effectParam << m_asset->get(pName.toUtf8().constData());
            }
            m_asset->set("effect", effectParam.join(QLatin1Char(' ')).toUtf8().constData());
            emit replugEffect(shared_from_this());
        } else if (m_assetId == QLatin1String("autotrack_rectangle") || m_assetId.startsWith(QStringLiteral("ladspa"))) {
            // these effects don't understand param change and need to be rebuild
            emit replugEffect(shared_from_this());
        } else {
            emit modelChanged();
        }
        // Update timeline view if necessary
        pCore->updateItemModel(m_ownerId, m_assetId);
        pCore->refreshProjectItem(m_ownerId);
        pCore->invalidateItem(m_ownerId);
    }
}

void AssetParameterModel::setParameter(const QString &name, const QString &value, bool update)
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
    if (update) {
        if (m_assetId.startsWith(QStringLiteral("sox_"))) {
            // Warning, SOX effect, need unplug/replug
            qDebug()<<"// Warning, SOX effect, need unplug/replug";
            QStringList effectParam = {m_assetId.section(QLatin1Char('_'), 1)};
            for (const QString &pName : m_paramOrder) {
                effectParam << m_asset->get(pName.toUtf8().constData());
            }
            m_asset->set("effect", effectParam.join(QLatin1Char(' ')).toUtf8().constData());
            emit replugEffect(shared_from_this());
        } else if (m_assetId == QLatin1String("autotrack_rectangle") || m_assetId.startsWith(QStringLiteral("ladspa"))) {
            // these effects don't understand param change and need to be rebuild
            emit replugEffect(shared_from_this());
        } else {
            emit modelChanged();
        }
        // Update timeline view if necessary
        pCore->updateItemModel(m_ownerId, m_assetId);
        pCore->refreshProjectItem(m_ownerId);
        pCore->invalidateItem(m_ownerId);
    }
}

void AssetParameterModel::setParameter(const QString &name, double &value)
{
    Q_ASSERT(m_asset->is_valid());
    m_asset->set(name.toLatin1().constData(), value);
    if (m_fixedParams.count(name) == 0) {
        m_params[name].value = value;
    } else {
        m_fixedParams[name] = value;
    }
    if (m_assetId.startsWith(QStringLiteral("sox_"))) {
        // Warning, SOX effect, need unplug/replug
        qDebug()<<"// Warning, SOX effect, need unplug/replug";
        QStringList effectParam = {m_assetId.section(QLatin1Char('_'), 1)};
        for (const QString &pName : m_paramOrder) {
            effectParam << m_asset->get(pName.toUtf8().constData());
        }
        m_asset->set("effect", effectParam.join(QLatin1Char(' ')).toUtf8().constData());
        emit replugEffect(shared_from_this());
    } else if (m_assetId == QLatin1String("autotrack_rectangle") || m_assetId.startsWith(QStringLiteral("ladspa"))) {
            // these effects don't understand param change and need to be rebuild
            emit replugEffect(shared_from_this());
        } else {
        emit modelChanged();
    }
    pCore->refreshProjectItem(m_ownerId);
    pCore->invalidateItem(m_ownerId);
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
        return m_params.at(paramName).name;
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
    case ParentInRole:
        return pCore->getItemPosition(m_ownerId);
    case ParentDurationRole:
        return pCore->getItemDuration(m_ownerId);
    case MinRole:
        return parseAttribute(QStringLiteral("min"), element);
    case MaxRole:
        return parseAttribute(QStringLiteral("max"), element);
    case FactorRole:
        return parseAttribute(QStringLiteral("factor"), element, 1);
    case ScaleRole:
        return parseAttribute(QStringLiteral("scale"), element, 0);
    case DecimalsRole:
        return parseAttribute(QStringLiteral("decimals"), element);
    case DefaultRole:
        return parseAttribute(QStringLiteral("default"), element);
    case SuffixRole:
        return element.attribute(QStringLiteral("suffix"));
    case OpacityRole:
        return element.attribute(QStringLiteral("opacity")) != QLatin1String("false");
    case AlphaRole:
        return element.attribute(QStringLiteral("alpha")) == QLatin1String("1");
    case ValueRole: {
        QString value(m_asset->get(paramName.toUtf8().constData()));
        return value.isEmpty() ? (element.attribute(QStringLiteral("value")).isNull() ? parseAttribute(QStringLiteral("default"), element)
                                                                                      : element.attribute(QStringLiteral("value")))
                               : value;
    }
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
    if (type == QLatin1String("double") || type == QLatin1String("float") || type == QLatin1String("constant")) {
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
    } else if (type == QLatin1String("simplekeyframe")) {
        return ParamType::KeyframeParam;
    } else if (type == QLatin1String("animatedrect")) {
        return ParamType::AnimatedRect;
    } else if (type == QLatin1String("geometry")) {
        return ParamType::Geometry;
    } else if (type == QLatin1String("addedgeometry")) {
        return ParamType::Addedgeometry;
    } else if (type == QLatin1String("keyframe") || type == QLatin1String("animated")) {
        return ParamType::KeyframeParam;
    } else if (type == QLatin1String("color")) {
        return ParamType::Color;
    } else if (type == QLatin1String("colorwheel")) {
        return ParamType::ColorWheel;
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
QString AssetParameterModel::getDefaultKeyframes(int start, const QString &defaultValue, bool linearOnly)
{
    QString keyframes = QString::number(start);
    if (linearOnly) {
        keyframes.append(QLatin1Char('='));
    } else {
        switch (KdenliveSettings::defaultkeyframeinterp()) {
        case mlt_keyframe_discrete:
            keyframes.append(QStringLiteral("|="));
            break;
        case mlt_keyframe_smooth:
            keyframes.append(QStringLiteral("~="));
            break;
        default:
            keyframes.append(QLatin1Char('='));
            break;
        }
    }
    keyframes.append(defaultValue);
    return keyframes;
}

// static
QVariant AssetParameterModel::parseAttribute(const QString &attribute, const QDomElement &element, QVariant defaultValue)
{
    if (!element.hasAttribute(attribute) && !defaultValue.isNull()) {
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
    } else if (type == ParamType::Double) {
        QLocale locale;
        locale.setNumberOptions(QLocale::OmitGroupSeparator);
        return locale.toDouble(content);
    }
    if (attribute == QLatin1String("default")) {
        if (type == ParamType::RestrictedAnim) {
            content = getDefaultKeyframes(0, content, true);
        } else {
            if (element.hasAttribute(QStringLiteral("factor"))) {
                QLocale locale;
                locale.setNumberOptions(QLocale::OmitGroupSeparator);
                return QVariant(locale.toDouble(content) / locale.toDouble(element.attribute(QStringLiteral("factor"))));
            }
        }
    }
    return content;
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

ObjectId AssetParameterModel::getOwnerId() const
{
    return m_ownerId;
}

void AssetParameterModel::addKeyframeParam(const QModelIndex index)
{
    if (m_keyframes) {
        m_keyframes->addParameter(index);
    } else {
        m_keyframes.reset(new KeyframeModelList(shared_from_this(), index, pCore->undoStack()));
    }
}

std::shared_ptr<KeyframeModelList> AssetParameterModel::getKeyframeModel()
{
    return m_keyframes;
}

void AssetParameterModel::resetAsset(Mlt::Properties *asset)
{
    m_asset.reset(asset);
}
