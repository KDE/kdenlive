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
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <effects/effectsrepository.hpp>
#define DEBUG_LOCALE false

AssetParameterModel::AssetParameterModel(std::unique_ptr<Mlt::Properties> asset, const QDomElement &assetXml, const QString &assetId, ObjectId ownerId,
                                         const QString& originalDecimalPoint, QObject *parent)
    : QAbstractListModel(parent)
    , monitorId(ownerId.first == ObjectType::BinClip ? Kdenlive::ClipMonitor : Kdenlive::ProjectMonitor)
    , m_assetId(assetId)
    , m_ownerId(ownerId)
    , m_asset(std::move(asset))
    , m_keyframes(nullptr)
{
    Q_ASSERT(m_asset->is_valid());
    QDomNodeList parameterNodes = assetXml.elementsByTagName(QStringLiteral("parameter"));
    m_hideKeyframesByDefault = assetXml.hasAttribute(QStringLiteral("hideKeyframes"));
    m_isAudio = assetXml.attribute(QStringLiteral("type")) == QLatin1String("audio");

    bool needsLocaleConversion = false;
    QChar separator, oldSeparator;
    // Check locale, default effects xml has no LC_NUMERIC defined and always uses the C locale
    if (assetXml.hasAttribute(QStringLiteral("LC_NUMERIC"))) {
        QLocale effectLocale = QLocale(assetXml.attribute(QStringLiteral("LC_NUMERIC"))); // Check if effect has a special locale → probably OK
        if (QLocale::c().decimalPoint() != effectLocale.decimalPoint()) {
            needsLocaleConversion = true;
            separator = QLocale::c().decimalPoint();
            oldSeparator = effectLocale.decimalPoint();
        }
    }

    if (EffectsRepository::get()->exists(assetId)) {
        qDebug() << "Asset " << assetId << " found in the repository. Description: " << EffectsRepository::get()->getDescription(assetId);
#if false
        QString str;
        QTextStream stream(&str);
        EffectsRepository::get()->getXml(assetId).save(stream, 4);
        qDebug() << "Asset XML: " << str;
#endif
    } else {
        qDebug() << "Asset not found in repo: " << assetId;
    }

    qDebug() << "XML parsing of " << assetId << ". found" << parameterNodes.count() << "parameters";

    if (DEBUG_LOCALE) {
        QString str;
        QTextStream stream(&str);
        assetXml.save(stream, 1);
        qDebug() << "XML to parse: " << str;
    }
    bool fixDecimalPoint = !originalDecimalPoint.isEmpty();
    if (fixDecimalPoint) {
        qDebug() << "Original decimal point was different:" << originalDecimalPoint << "Values will be converted if required.";
    }
    for (int i = 0; i < parameterNodes.count(); ++i) {
        QDomElement currentParameter = parameterNodes.item(i).toElement();

        // Convert parameters if we need to
        // Note: This is not directly related to the originalDecimalPoint parameter.
        // Is it still required? Does it work correctly for non-number values (e.g. lists which contain commas)?
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
        ParamRow currentRow;
        currentRow.type = paramTypeFromStr(type);
        currentRow.xml = currentParameter;
        if (value.isEmpty()) {
            QVariant defaultValue = parseAttribute(m_ownerId, QStringLiteral("default"), currentParameter);
            value = defaultValue.toString();
            qDebug() << "QLocale: Default value is" << defaultValue << "parsed:" << value;
        }
        bool isFixed = (type == QLatin1String("fixed"));
        if (isFixed) {
            m_fixedParams[name] = value;
        } else if (currentRow.type == ParamType::Position) {
            int val = value.toInt();
            if (val < 0) {
                int in = pCore->getItemIn(m_ownerId);
                int out = in + pCore->getItemDuration(m_ownerId) - 1;
                val += out;
                value = QString::number(val);
            }
        } else if (currentRow.type == ParamType::KeyframeParam || currentRow.type == ParamType::AnimatedRect) {
            if (!value.contains(QLatin1Char('='))) {
                value.prepend(QStringLiteral("%1=").arg(pCore->getItemIn(m_ownerId)));
            }
        }

        if (fixDecimalPoint) {
            bool converted = true;
            QString originalValue(value);
            switch (currentRow.type) {
                case ParamType::KeyframeParam:
                case ParamType::Position:
                    // Fix values like <position>=1,5
                    value.replace(QRegExp(R"((=\d+),(\d+))"), "\\1.\\2");
                    break;
                case ParamType::AnimatedRect:
                    // Fix values like <position>=50 20 1920 1080 0,75
                    value.replace(QRegExp(R"((=\d+ \d+ \d+ \d+ \d+),(\d+))"), "\\1.\\2");
                    break;
                case ParamType::ColorWheel:
                    // Colour wheel has 3 separate properties: prop_r, prop_g and prop_b, always numbers
                case ParamType::Double:
                case ParamType::Hidden:
                case ParamType::List:
                    // Despite its name, a list type parameter is a single value *chosen from* a list.
                    // If it contains a non-“.” decimal separator, it is very likely wrong.
                    // Fall-through, treat like Double
                case ParamType::Bezier_spline:
                    value.replace(originalDecimalPoint, ".");
                    break;
                case ParamType::Bool:
                case ParamType::Color:
                case ParamType::Fontfamily:
                case ParamType::Keywords:
                case ParamType::Readonly:
                case ParamType::RestrictedAnim: // Fine because unsupported
                case ParamType::Animated: // Fine because unsupported
                case ParamType::Addedgeometry: // Fine because unsupported
                case ParamType::Url:
                    // All fine
                    converted = false;
                    break;
                case ParamType::Curve:
                case ParamType::Geometry:
                case ParamType::Switch:
                case ParamType::Wipe:
                    // Pretty sure that those are fine
                    converted = false;
                    break;
                case ParamType::Roto_spline: // Not sure because cannot test
                case ParamType::Filterjob:
                    // Not sure if fine
                    converted = false;
                    break;
            }
            if (converted) {
                if (value != originalValue) {
                    qDebug() << "Decimal point conversion: " << name << "converted from" << originalValue << "to" << value;
                } else {
                    qDebug() << "Decimal point conversion: " << name << " is already ok: " << value;
                }
            } else {
                qDebug() << "No fixing needed for" << name << "=" << value;
            }
        }

        if (!name.isEmpty()) {
            internalSetParameter(name, value);
            // Keep track of param order
            m_paramOrder.push_back(name);
        }

        if (isFixed) {
            // fixed parameters are not displayed so we don't store them.
            continue;
        }
        currentRow.value = value;
        QString title = i18n(currentParameter.firstChildElement(QStringLiteral("name")).text().toUtf8().data());
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
    modelChanged();
}

void AssetParameterModel::prepareKeyframes()
{
    if (m_keyframes) return;
    int ix = 0;
    for (const auto &name : qAsConst(m_rows)) {
        if (m_params.at(name).type == ParamType::KeyframeParam || m_params.at(name).type == ParamType::AnimatedRect ||
            m_params.at(name).type == ParamType::Roto_spline) {
            addKeyframeParam(index(ix, 0));
        }
        ix++;
    }
    if (m_keyframes) {
        // Make sure we have keyframes at same position for all parameters
        m_keyframes->checkConsistency();
    }
}

QStringList AssetParameterModel::getKeyframableParameters() const
{
    QStringList paramNames;
    int ix = 0;
    for (const auto &name : m_rows) {
        if (m_params.at(name).type == ParamType::KeyframeParam || m_params.at(name).type == ParamType::AnimatedRect) {
            //addKeyframeParam(index(ix, 0));
            paramNames << name;
        }
        ix++;
    }
    return paramNames;
}

void AssetParameterModel::setParameter(const QString &name, int value, bool update)
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
        qDebug() << "// Warning, SOX effect, need unplug/replug";
        QStringList effectParam = {m_assetId.section(QLatin1Char('_'), 1)};
        for (const QString &pName : m_paramOrder) {
            effectParam << m_asset->get(pName.toUtf8().constData());
        }
        m_asset->set("effect", effectParam.join(QLatin1Char(' ')).toUtf8().constData());
        emit replugEffect(shared_from_this());
    } else if (m_assetId == QLatin1String("autotrack_rectangle") || m_assetId.startsWith(QStringLiteral("ladspa"))) {
        // these effects don't understand param change and need to be rebuild
        emit replugEffect(shared_from_this());
    }
    if (update) {
        emit modelChanged();
        emit dataChanged(index(0, 0), index(m_rows.count() - 1, 0), {});
        // Update fades in timeline
        pCore->updateItemModel(m_ownerId, m_assetId);
        if (!m_isAudio) {
            // Trigger monitor refresh
            pCore->refreshProjectItem(m_ownerId);
            // Invalidate timeline preview
            pCore->invalidateItem(m_ownerId);
        }
    }
}

void AssetParameterModel::internalSetParameter(const QString &name, const QString &paramValue, const QModelIndex &paramIndex)
{
    Q_ASSERT(m_asset->is_valid());
    // TODO: this does not really belong here, but I don't see another way to do it so that undo works
    if (data(paramIndex, AssetParameterModel::TypeRole).value<ParamType>() == ParamType::Curve) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        QStringList vals = paramValue.split(QLatin1Char(';'), QString::SkipEmptyParts);
#else
        QStringList vals = paramValue.split(QLatin1Char(';'), Qt::SkipEmptyParts);
#endif
        int points = vals.size();
        m_asset->set("3", points / 10.);
        m_params[QStringLiteral("3")].value = points / 10.;
        // for the curve, inpoints are numbered: 6, 8, 10, 12, 14
        // outpoints, 7, 9, 11, 13,15 so we need to deduce these enums
        for (int i = 0; i < points; i++) {
            const QString &pointVal = vals.at(i);
            int idx = 2 * i + 6;
            QString pName = QString::number(idx);
            double val = pointVal.section(QLatin1Char('/'), 0, 0).toDouble();
            m_asset->set(pName.toLatin1().constData(), val);
            m_params[pName].value = val;
            idx++;
            pName = QString::number(idx);
            val = pointVal.section(QLatin1Char('/'), 1, 1).toDouble();
            m_asset->set(pName.toLatin1().constData(), val);
            m_params[pName].value = val;
        }
    }
    bool conversionSuccess = true;
    double doubleValue = paramValue.toDouble(&conversionSuccess);
    if (conversionSuccess) {
        m_asset->set(name.toLatin1().constData(), doubleValue);
        if (m_fixedParams.count(name) == 0) {
            m_params[name].value = doubleValue;
        } else {
            m_fixedParams[name] = doubleValue;
        }
    } else {
        m_asset->set(name.toLatin1().constData(), paramValue.toUtf8().constData());
        if (m_fixedParams.count(name) == 0) {
            m_params[name].value = paramValue;
            if (m_keyframes) {
                KeyframeModel *km = m_keyframes->getKeyModel(paramIndex);
                if (km) {
                    km->refresh();
                }
                //m_keyframes->refresh();
            }
        } else {
            m_fixedParams[name] = paramValue;
        }
    }
    qDebug() << " = = SET EFFECT PARAM: " << name << " = " << m_asset->get(name.toLatin1().constData());
}

void AssetParameterModel::setParameter(const QString &name, const QString &paramValue, bool update, const QModelIndex &paramIndex)
{
    //qDebug() << "// PROCESSING PARAM CHANGE: " << name << ", UPDATE: " << update << ", VAL: " << paramValue;
    internalSetParameter(name, paramValue, paramIndex);
    bool updateChildRequired = true;
    if (m_assetId.startsWith(QStringLiteral("sox_"))) {
        // Warning, SOX effect, need unplug/replug
        QStringList effectParam = {m_assetId.section(QLatin1Char('_'), 1)};
        for (const QString &pName : m_paramOrder) {
            effectParam << m_asset->get(pName.toUtf8().constData());
        }
        m_asset->set("effect", effectParam.join(QLatin1Char(' ')).toUtf8().constData());
        emit replugEffect(shared_from_this());
        updateChildRequired = false;
    } else if (m_assetId == QLatin1String("autotrack_rectangle") || m_assetId.startsWith(QStringLiteral("ladspa"))) {
        // these effects don't understand param change and need to be rebuild
        emit replugEffect(shared_from_this());
        updateChildRequired = false;
    } else if (update) {
        qDebug() << "// SENDING DATA CHANGE....";
        if (paramIndex.isValid()) {
            emit dataChanged(paramIndex, paramIndex);
        } else {
            QModelIndex ix = index(m_rows.indexOf(name), 0);
            emit dataChanged(ix, ix);
        }
        emit modelChanged();
    }
    if (updateChildRequired) {
        emit updateChildren(name);
    }
    // Update timeline view if necessary
    if (m_ownerId.first == ObjectType::NoItem) {
        // Used for generator clips
        if (!update) emit modelChanged();
    } else {
        // Update fades in timeline
        pCore->updateItemModel(m_ownerId, m_assetId);
        if (!m_isAudio) {
            // Trigger monitor refresh
            pCore->refreshProjectItem(m_ownerId);
            // Invalidate timeline preview
            pCore->invalidateItem(m_ownerId);
        }
    }
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
        return pCore->getItemIn(m_ownerId);
    case ParentDurationRole:
        return pCore->getItemDuration(m_ownerId);
    case ParentPositionRole:
        return pCore->getItemPosition(m_ownerId);
    case HideKeyframesFirstRole:
        return m_hideKeyframesByDefault;
    case MinRole:
        return parseAttribute(m_ownerId, QStringLiteral("min"), element);
    case MaxRole:
        return parseAttribute(m_ownerId, QStringLiteral("max"), element);
    case FactorRole:
        return parseAttribute(m_ownerId, QStringLiteral("factor"), element, 1);
    case ScaleRole:
        return parseAttribute(m_ownerId, QStringLiteral("scale"), element, 0);
    case DecimalsRole:
        return parseAttribute(m_ownerId, QStringLiteral("decimals"), element);
    case OddRole:
        return element.attribute(QStringLiteral("odd")) == QLatin1String("1");
    case VisualMinRole:
        return parseAttribute(m_ownerId, QStringLiteral("visualmin"), element);
    case VisualMaxRole:
        return parseAttribute(m_ownerId, QStringLiteral("visualmax"), element);
    case DefaultRole:
        return parseAttribute(m_ownerId, QStringLiteral("default"), element);
    case FilterRole:
        return parseAttribute(m_ownerId, QStringLiteral("filter"), element);
    case FilterParamsRole:
        return parseAttribute(m_ownerId, QStringLiteral("filterparams"), element);
    case FilterConsumerParamsRole:
        return parseAttribute(m_ownerId, QStringLiteral("consumerparams"), element);
    case FilterJobParamsRole:
        return parseSubAttributes(QStringLiteral("jobparam"), element);
    case AlternateNameRole: {
        QDomNode child = element.firstChildElement(QStringLiteral("name"));
        if (child.toElement().hasAttribute(QStringLiteral("conditional"))) {
            return child.toElement().attribute(QStringLiteral("conditional"));
        }
        return m_params.at(paramName).name;
    }
    case SuffixRole:
        return element.attribute(QStringLiteral("suffix"));
    case OpacityRole:
        return element.attribute(QStringLiteral("opacity")) != QLatin1String("false");
    case RelativePosRole:
        return element.attribute(QStringLiteral("relative")) == QLatin1String("true");
    case ShowInTimelineRole:
        return !element.hasAttribute(QStringLiteral("notintimeline"));
    case AlphaRole:
        return element.attribute(QStringLiteral("alpha")) == QLatin1String("1");
    case ValueRole: {
        QString value(m_asset->get(paramName.toUtf8().constData()));
        return value.isEmpty() ? (element.attribute(QStringLiteral("value")).isNull() ? parseAttribute(m_ownerId, QStringLiteral("default"), element)
                                                                                      : element.attribute(QStringLiteral("value")))
                               : value;
    }
    case ListValuesRole:
        return element.attribute(QStringLiteral("paramlist")).split(QLatin1Char(';'));
    case ListNamesRole: {
        QDomElement namesElem = element.firstChildElement(QStringLiteral("paramlistdisplay"));
        return i18n(namesElem.text().toUtf8().data()).split(QLatin1Char(','));
    }
    case List1Role:
        return parseAttribute(m_ownerId, QStringLiteral("list1"), element);
    case List2Role:
        return parseAttribute(m_ownerId, QStringLiteral("list2"), element);
    case Enum1Role:
        return m_asset->get_double("1");
    case Enum2Role:
        return m_asset->get_double("2");
    case Enum3Role:
        return m_asset->get_double("3");
    case Enum4Role:
        return m_asset->get_double("4");
    case Enum5Role:
        return m_asset->get_double("5");
    case Enum6Role:
        return m_asset->get_double("6");
    case Enum7Role:
        return m_asset->get_double("7");
    case Enum8Role:
        return m_asset->get_double("8");
    case Enum9Role:
        return m_asset->get_double("9");
    case Enum10Role:
        return m_asset->get_double("10");
    case Enum11Role:
        return m_asset->get_double("11");
    case Enum12Role:
        return m_asset->get_double("12");
    case Enum13Role:
        return m_asset->get_double("13");
    case Enum14Role:
        return m_asset->get_double("14");
    case Enum15Role:
        return m_asset->get_double("15");
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
    } else if (type == QLatin1String("animatedrect") || type == QLatin1String("rect")) {
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
    } else if (type == QLatin1String("hidden")) {
        return ParamType::Hidden;
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
QVariant AssetParameterModel::parseAttribute(const ObjectId &owner, const QString &attribute, const QDomElement &element, QVariant defaultValue)
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
        int in = pCore->getItemIn(owner);
        int out = in + pCore->getItemDuration(owner) - 1;
        int frame_duration = pCore->getDurationFromString(KdenliveSettings::fade_duration());
        // replace symbols in the double parameter
        content.replace(QLatin1String("%maxWidth"), QString::number(width))
            .replace(QLatin1String("%maxHeight"), QString::number(height))
            .replace(QLatin1String("%width"), QString::number(width))
            .replace(QLatin1String("%height"), QString::number(height))
            .replace(QLatin1String("%out"), QString::number(out))
            .replace(QLatin1String("%fade"), QString::number(frame_duration));
        if (type == ParamType::AnimatedRect && attribute == QLatin1String("default")) {
            if (content.contains(QLatin1Char('%'))) {
                // This is a generic default like: "25% 0% 50% 100%". Parse values
                QStringList numbers = content.split(QLatin1Char(' '));
                content.clear();
                int ix = 0;
                for ( QString &val : numbers) {
                    if (val.endsWith(QLatin1Char('%'))) {
                        val.chop(1);
                        double n = val.toDouble()/100.;
                        if (ix %2 == 0) {
                            n *= width;
                        } else {
                            n *= height;
                        }
                        ix++;
                        content.append(QString("%1 ").arg(qRound(n)));
                    } else {
                        content.append(QString("%1 ").arg(val));
                    }
                }
            }
        }
        else if (type == ParamType::Double || type == ParamType::Hidden) {
            // Use a Mlt::Properties to parse mathematical operators
            Mlt::Properties p;
            p.set("eval", content.prepend(QLatin1Char('@')).toLatin1().constData());
            return p.get_double("eval");
        }
    } else if (type == ParamType::Double || type == ParamType::Hidden) {
        if (attribute == QLatin1String("default")) {
            return content.toDouble();
        }
        bool ok;
        double converted = content.toDouble(&ok);
        if (!ok) {
            qDebug() << "QLocale: Could not load double parameter" << content;
        }
        return converted;
    }
    if (attribute == QLatin1String("default")) {
        if (type == ParamType::RestrictedAnim) {
            content = getDefaultKeyframes(0, content, true);
        } else if (type == ParamType::KeyframeParam) {
            return content.toDouble();
        } else if (type == ParamType::List) {
            bool ok;
            double res = content.toDouble(&ok);
            if (ok) {
                return res;
            }
            return defaultValue.isNull() ? content : defaultValue;
        }
    }
    return content;
}

QVariant AssetParameterModel::parseSubAttributes(const QString &attribute, const QDomElement &element) const
{
    QDomNodeList nodeList = element.elementsByTagName(attribute);
    if (nodeList.isEmpty()) {
        return QVariant();
    }
    QVariantList jobDataList;
    for (int i = 0; i < nodeList.count(); ++i) {
        QDomElement currentParameter = nodeList.item(i).toElement();
        QStringList jobData {currentParameter.attribute(QStringLiteral("name")), currentParameter.text()};
        jobDataList << jobData;
    }
    return jobDataList;
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
        if (!param.first.isEmpty()) {
            res.push_back(QPair<QString, QVariant>(param.first, param.second.value));
        }
    }
    return res;
}

QJsonDocument AssetParameterModel::toJson(bool includeFixed) const
{
    QJsonArray list;
    if (includeFixed) {
        for (const auto &fixed : m_fixedParams) {
            QJsonObject currentParam;
            QModelIndex ix = index(m_rows.indexOf(fixed.first), 0);
            currentParam.insert(QLatin1String("name"), QJsonValue(fixed.first));
            currentParam.insert(QLatin1String("value"), fixed.second.type() == QVariant::Double ? QJsonValue(fixed.second.toDouble()) : QJsonValue(fixed.second.toString()));
            int type = data(ix, AssetParameterModel::TypeRole).toInt();
            double min = data(ix, AssetParameterModel::MinRole).toDouble();
            double max = data(ix, AssetParameterModel::MaxRole).toDouble();
            double factor = data(ix, AssetParameterModel::FactorRole).toDouble();
            int in = data(ix, AssetParameterModel::ParentInRole).toInt();
            int out = in + data(ix, AssetParameterModel::ParentDurationRole).toInt();
            if (factor > 0) {
                min /= factor;
                max /= factor;
            }
            currentParam.insert(QLatin1String("type"), QJsonValue(type));
            currentParam.insert(QLatin1String("min"), QJsonValue(min));
            currentParam.insert(QLatin1String("max"), QJsonValue(max));
            currentParam.insert(QLatin1String("in"), QJsonValue(in));
            currentParam.insert(QLatin1String("out"), QJsonValue(out));
            list.push_back(currentParam);
        }
    }

    for (const auto &param : m_params) {
        if (!includeFixed && param.second.type != ParamType::KeyframeParam && param.second.type != ParamType::AnimatedRect) {
            continue;
        }
        QJsonObject currentParam;
        QModelIndex ix = index(m_rows.indexOf(param.first), 0);
        currentParam.insert(QLatin1String("name"), QJsonValue(param.first));
        currentParam.insert(QLatin1String("value"), param.second.value.type() == QVariant::Double ? QJsonValue(param.second.value.toDouble()) : QJsonValue(param.second.value.toString()));
        int type = data(ix, AssetParameterModel::TypeRole).toInt();
        double min = data(ix, AssetParameterModel::MinRole).toDouble();
        double max = data(ix, AssetParameterModel::MaxRole).toDouble();
        double factor = data(ix, AssetParameterModel::FactorRole).toDouble();
        int in = data(ix, AssetParameterModel::ParentInRole).toInt();
        int out = in + data(ix, AssetParameterModel::ParentDurationRole).toInt();
        if (factor > 0) {
            min /= factor;
            max /= factor;
        }
        currentParam.insert(QLatin1String("type"), QJsonValue(type));
        currentParam.insert(QLatin1String("min"), QJsonValue(min));
        currentParam.insert(QLatin1String("max"), QJsonValue(max));
        currentParam.insert(QLatin1String("in"), QJsonValue(in));
        currentParam.insert(QLatin1String("out"), QJsonValue(out));
        list.push_back(currentParam);
    }
    return QJsonDocument(list);
}

void AssetParameterModel::deletePreset(const QString &presetFile, const QString &presetName)
{
    QJsonObject object;
    QJsonArray array;
    QFile loadFile(presetFile);
    if (loadFile.exists()) {
        if (loadFile.open(QIODevice::ReadOnly)) {
            QByteArray saveData = loadFile.readAll();
            QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));
            if (loadDoc.isArray()) {
                array = loadDoc.array();
                QList<int> toDelete;
                for (int i = 0; i < array.size(); i++) {
                    QJsonValue val = array.at(i);
                    if (val.isObject() && val.toObject().keys().contains(presetName)) {
                        toDelete << i;
                    }
                }
                for (int i : qAsConst(toDelete)) {
                    array.removeAt(i);
                }
            } else if (loadDoc.isObject()) {
                QJsonObject obj = loadDoc.object();
                qDebug() << " * * ** JSON IS AN OBJECT, DELETING: " << presetName;
                if (obj.keys().contains(presetName)) {
                    obj.remove(presetName);
                } else {
                    qDebug() << " * * ** JSON DOES NOT CONTAIN: " << obj.keys();
                }
                array.append(obj);
            }
            loadFile.close();
        }
    }
    if (!loadFile.open(QIODevice::WriteOnly)) {
        pCore->displayMessage(i18n("Cannot open preset file %1", presetFile), ErrorMessage);
        return;
    }
    if (array.isEmpty()) {
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
        if (dir.exists(presetFile)) {
            // Ensure we don't delete an unwanted file
            loadFile.remove();
        }
    } else {
        loadFile.write(QJsonDocument(array).toJson());
    }
}

void AssetParameterModel::savePreset(const QString &presetFile, const QString &presetName)
{
    QJsonObject object;
    QJsonArray array;
    QJsonDocument doc = toJson();
    QFile loadFile(presetFile);
    if (loadFile.exists()) {
        if (loadFile.open(QIODevice::ReadOnly)) {
            QByteArray saveData = loadFile.readAll();
            QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));
            if (loadDoc.isArray()) {
                array = loadDoc.array();
                QList<int> toDelete;
                for (int i = 0; i < array.size(); i++) {
                    QJsonValue val = array.at(i);
                    if (val.isObject() && val.toObject().keys().contains(presetName)) {
                        toDelete << i;
                    }
                }
                for (int i : qAsConst(toDelete)) {
                    array.removeAt(i);
                }
            } else if (loadDoc.isObject()) {
                QJsonObject obj = loadDoc.object();
                if (obj.keys().contains(presetName)) {
                    obj.remove(presetName);
                }
                array.append(obj);
            }
            loadFile.close();
        }
    }
    if (!loadFile.open(QIODevice::WriteOnly)) {
        pCore->displayMessage(i18n("Cannot open preset file %1", presetFile), ErrorMessage);
        return;
    }
    object[presetName] = doc.array();
    array.append(object);
    loadFile.write(QJsonDocument(array).toJson());
}

const QStringList AssetParameterModel::getPresetList(const QString &presetFile) const
{
    QFile loadFile(presetFile);
    if (loadFile.exists() && loadFile.open(QIODevice::ReadOnly)) {
        QByteArray saveData = loadFile.readAll();
        QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));
        if (loadDoc.isObject()) {
            qDebug() << "// PRESET LIST IS AN OBJECT!!!";
            return loadDoc.object().keys();
        } else if (loadDoc.isArray()) {
            qDebug() << "// PRESET LIST IS AN ARRAY!!!";
            QStringList result;
            QJsonArray array = loadDoc.array();
            for (auto &&i : array) {
                QJsonValue val = i;
                if (val.isObject()) {
                    result << val.toObject().keys();
                }
            }
            return result;
        }
    }
    return QStringList();
}

const QVector<QPair<QString, QVariant>> AssetParameterModel::loadPreset(const QString &presetFile, const QString &presetName)
{
    QFile loadFile(presetFile);
    QVector<QPair<QString, QVariant>> params;
    if (loadFile.exists() && loadFile.open(QIODevice::ReadOnly)) {
        QByteArray saveData = loadFile.readAll();
        QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));
        if (loadDoc.isObject() && loadDoc.object().contains(presetName)) {
            qDebug() << "..........\n..........\nLOADING OBJECT JSON";
            QJsonValue val = loadDoc.object().value(presetName);
            if (val.isObject()) {
                QVariantMap map = val.toObject().toVariantMap();
                QMap<QString, QVariant>::const_iterator i = map.constBegin();
                while (i != map.constEnd()) {
                    params.append({i.key(), i.value()});
                    ++i;
                }
            }
        } else if (loadDoc.isArray()) {
            QJsonArray array = loadDoc.array();
            for (auto &&i : array) {
                QJsonValue val = i;
                if (val.isObject() && val.toObject().contains(presetName)) {
                    QJsonValue preset = val.toObject().value(presetName);
                    if (preset.isArray()) {
                        QJsonArray paramArray = preset.toArray();
                        for (auto &&j : paramArray) {
                            QJsonValue v1 = j;
                            if (v1.isObject()) {
                                QJsonObject ob = v1.toObject();
                                params.append({ob.value("name").toString(), ob.value("value").toVariant()});
                            }
                        }
                    }
                    qDebug() << "// LOADED PRESET: " << presetName << "\n" << params;
                    break;
                }
            }
        }
    }
    return params;
}

void AssetParameterModel::setParameters(const QVector<QPair<QString, QVariant>> &params, bool update)
{
    ObjectType itemId;
    if (!update) {
        // Change itemId to NoItem to ensure we don't send any update like refreshProjectItem that would trigger monitor refreshes.
        itemId = m_ownerId.first;
        m_ownerId.first = ObjectType::NoItem;
    }
    for (const auto &param : params) {
        setParameter(param.first, param.second.toString(), false);
    }
    if (m_keyframes) {
        m_keyframes->refresh();
    }
    if (!update) {
        m_ownerId.first = itemId;
    }
    emit dataChanged(index(0), index(m_rows.count()), {});
}

ObjectId AssetParameterModel::getOwnerId() const
{
    return m_ownerId;
}

void AssetParameterModel::addKeyframeParam(const QModelIndex &index)
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

void AssetParameterModel::resetAsset(std::unique_ptr<Mlt::Properties> asset)
{
    m_asset = std::move(asset);
}

bool AssetParameterModel::hasMoreThanOneKeyframe() const
{
    if (m_keyframes) {
        return (!m_keyframes->isEmpty() && !m_keyframes->singleKeyframe());
    }
    return false;
}

int AssetParameterModel::time_to_frames(const QString &time)
{
    return m_asset->time_to_frames(time.toUtf8().constData());
}

void AssetParameterModel::passProperties(Mlt::Properties &target)
{
    target.set("_profile", pCore->getCurrentProfile()->get_profile(), 0);
    target.set_lcnumeric(m_asset->get_lcnumeric());
}
