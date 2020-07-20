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

#include "effectsrepository.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "profiles/profilemodel.hpp"
#include "xml/xml.hpp"

#include <KLocalizedString>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>
#include <mlt++/Mlt.h>

std::unique_ptr<EffectsRepository> EffectsRepository::instance;
std::once_flag EffectsRepository::m_onceFlag;

EffectsRepository::EffectsRepository()
    : AbstractAssetsRepository<AssetListType::AssetType>()
{
    init();
    // Check that our favorite effects are valid
    QStringList invalidEffect;
    for (const QString &effect : KdenliveSettings::favorite_effects()) {
        if (!exists(effect)) {
            invalidEffect << effect;
        }
    }
    if (!invalidEffect.isEmpty()) {
        pCore->displayMessage(i18n("Some of your favorite effects are invalid and were removed: %1", invalidEffect.join(QLatin1Char(','))), ErrorMessage);
        QStringList newFavorites = KdenliveSettings::favorite_effects();
        for (const QString &effect : qAsConst(invalidEffect)) {
            newFavorites.removeAll(effect);
        }
        KdenliveSettings::setFavorite_effects(newFavorites);
    }
}

Mlt::Properties *EffectsRepository::retrieveListFromMlt() const
{
    return pCore->getMltRepository()->filters();
}

Mlt::Properties *EffectsRepository::getMetadata(const QString &effectId) const
{
    return pCore->getMltRepository()->metadata(filter_type, effectId.toLatin1().data());
}

void EffectsRepository::parseCustomAssetFile(const QString &file_name, std::unordered_map<QString, Info> &customAssets) const
{
    QFile file(file_name);
    QDomDocument doc;
    doc.setContent(&file, false);
    file.close();
    QDomElement base = doc.documentElement();
    if (base.tagName() == QLatin1String("effectgroup")) {
        QDomNodeList effects = base.elementsByTagName(QStringLiteral("effect"));
        if (effects.count() > 1) {
            // Effect group
            //qDebug() << "Error: found unsupported effect group" << base.attribute(QStringLiteral("name"))<<" : "<<file_name;
            Info result;
            result.xml = base;
            for (int i = 0; i < effects.count(); ++i) {
                QDomNode currentNode = effects.item(i);
                if (currentNode.isNull()) {
                    continue;
                }
                QDomElement currentEffect = currentNode.toElement();
                QString currentId = currentEffect.attribute(QStringLiteral("id"), QString());
                if (currentId.isEmpty()) {
                    currentId = currentEffect.attribute(QStringLiteral("tag"), QString());
                }
                if (!exists(currentId) && customAssets.count(currentId) == 0) {
                    qDebug() << "Error: found unsupported effect in group" << currentId<<" : "<<file_name;
                    return;
                }
            }
            QString type = base.attribute(QStringLiteral("type"), QString());
            if (type == QLatin1String("customAudio")) {
                result.type = AssetListType::AssetType::CustomAudio;
            } else {
                result.type = AssetListType::AssetType::Custom;
            }
            result.id = base.attribute(QStringLiteral("id"), QString());
            if (result.id.isEmpty()) {
                result.id = QFileInfo(file_name).baseName();
            }
            if (!result.id.isEmpty()) {
                result.name = result.id;
                customAssets[result.id] = result;
            }
            return;
        }
    }
    QDomNodeList effects = doc.elementsByTagName(QStringLiteral("effect"));

    int nbr_effect = effects.count();
    if (nbr_effect == 0) {
        qDebug() << "+++++++++++++\nEffect broken: " << file_name << "\n+++++++++++";
        return;
    }

    for (int i = 0; i < nbr_effect; ++i) {
        QDomNode currentNode = effects.item(i);
        if (currentNode.isNull()) {
            continue;
        }
        QDomElement currentEffect = currentNode.toElement();
        Info result;
        bool ok = parseInfoFromXml(currentEffect, result);
        if (!ok) {
            continue;
        }
        if (customAssets.count(result.id) > 0) {
            qDebug() << "Warning: duplicate custom definition of effect" << result.id << "found. Only last one will be considered. Duplicate found in"
                     << file_name;
        }

        result.xml = currentEffect;

        // Parse type information.
        // Video effect by default
        result.type = AssetListType::AssetType::Video;
        QString type = currentEffect.attribute(QStringLiteral("type"), QString());
        if (type == QLatin1String("audio")) {
            result.type = AssetListType::AssetType::Audio;
        } else if (type == QLatin1String("customVideo")) {
            result.type = AssetListType::AssetType::Custom;
        } else if (type == QLatin1String("customAudio")) {
            result.type = AssetListType::AssetType::CustomAudio;
        } else if (type == QLatin1String("hidden")) {
            result.type = AssetListType::AssetType::Hidden;
        } else if (type == QLatin1String("custom")) {
            // Old type effect, update to customVideo / customAudio
            const QString effectTag = currentEffect.attribute(QStringLiteral("tag"));
            QScopedPointer<Mlt::Properties> metadata(getMetadata(effectTag));
            if (metadata && metadata->is_valid()) {
                Mlt::Properties tags((mlt_properties)metadata->get_data("tags"));
                if (QString(tags.get(0)) == QLatin1String("Audio")) {
                    result.type = AssetListType::AssetType::CustomAudio;
                    currentEffect.setAttribute(QStringLiteral("type"), QStringLiteral("customAudio"));
                } else {
                    result.type = AssetListType::AssetType::Custom;
                    currentEffect.setAttribute(QStringLiteral("type"), QStringLiteral("customVideo"));
                }
                QFile effectFile(file_name);
                if (effectFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    effectFile.write(doc.toString().toUtf8());
                }
                file.close();
            }
        }
        customAssets[result.id] = result;
    }
}

std::unique_ptr<EffectsRepository> &EffectsRepository::get()
{
    std::call_once(m_onceFlag, [] { instance.reset(new EffectsRepository()); });
    return instance;
}

QStringList EffectsRepository::assetDirs() const
{
    return QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("effects"), QStandardPaths::LocateDirectory);
}

void EffectsRepository::parseType(QScopedPointer<Mlt::Properties> &metadata, Info &res)
{
    res.type = AssetListType::AssetType::Video;
    Mlt::Properties tags((mlt_properties)metadata->get_data("tags"));
    if (QString(tags.get(0)) == QLatin1String("Audio")) {
        res.type = AssetListType::AssetType::Audio;
    }
}

QString EffectsRepository::assetBlackListPath() const
{
    return QStringLiteral(":data/blacklisted_effects.txt");
}

QString EffectsRepository::assetPreferredListPath() const
{
    return QStringLiteral(":data/preferred_effects.txt");
}

bool EffectsRepository::isPreferred(const QString &effectId) const
{
    return m_preferred_list.contains(effectId);
}

std::unique_ptr<Mlt::Filter> EffectsRepository::getEffect(const QString &effectId) const
{
    Q_ASSERT(exists(effectId));
    QString service_name = m_assets.at(effectId).mltId;
    // We create the Mlt element from its name
    auto filter = std::make_unique<Mlt::Filter>(pCore->getCurrentProfile()->profile(), service_name.toLatin1().constData(), nullptr);
    return filter;
}

bool EffectsRepository::hasInternalEffect(const QString &effectId) const
{
    // Retrieve the list of MLT's available assets.
    QScopedPointer<Mlt::Properties> assets(retrieveListFromMlt());
    int max = assets->count();
    for (int i = 0; i < max; ++i) {
        if (assets->get_name(i) == effectId) {
            return true;
        }
    }
    return false;
}

QPair<QString, QString> EffectsRepository::reloadCustom(const QString &path)
{
    std::unordered_map<QString, Info> customAssets;
    parseCustomAssetFile(path, customAssets);
    QPair<QString, QString> result;
    // TODO: handle files with several effects
    for (const auto &custom : customAssets) {
        // Custom assets should override default ones
        m_assets[custom.first] = custom.second;
        result.first = custom.first;
        result.second = custom.second.mltId;
    }
    return result;
}

bool EffectsRepository::isGroup(const QString &assetId) const
{
    if (m_assets.count(assetId) > 0) {
        QDomElement xml = m_assets.at(assetId).xml;
        if (xml.tagName() == QLatin1String("effectgroup")) {
            return true;
        }
    }
    return false;
}


QPair <QStringList, QStringList> EffectsRepository::fixDeprecatedEffects()
{
    QString customAssetDir = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("effects"), QStandardPaths::LocateDirectory);
    QPair <QStringList, QStringList> results;
    QDir current_dir(customAssetDir);
    QStringList filter;
    filter << QStringLiteral("*.xml");
    QStringList fileList = current_dir.entryList(filter, QDir::Files);
    QStringList failed;
    for (const auto &file : qAsConst(fileList)) {
        QString path = current_dir.absoluteFilePath(file);
        QPair <QString, QString> fixResult = fixCustomAssetFile(path);
        if (!fixResult.first.isEmpty()) {
            results.first << fixResult.first;
        } else if (!fixResult.second.isEmpty()) {
            results.second << fixResult.second;
        }
    }
    return results;
}

QPair <QString, QString> EffectsRepository::fixCustomAssetFile(const QString &path)
{
    QPair <QString, QString> results;
    QFile file(path);
    QDomDocument doc;
    doc.setContent(&file, false);
    file.close();
    QDomElement base = doc.documentElement();
    if (base.tagName() == QLatin1String("effectgroup")) {
        // Groups not implemented
        return results;
    }
    QDomNodeList effects = doc.elementsByTagName(QStringLiteral("effect"));

    int nbr_effect = effects.count();
    if (nbr_effect == 0) {
        qDebug() << "+++++++++++++\nEffect broken: " << path << "\n+++++++++++";
        results.second = path;
        return results;
    }
    bool effectAdjusted = false;
    for (int i = 0; i < nbr_effect; ++i) {
        QDomNode currentNode = effects.item(i);
        if (currentNode.isNull()) {
            continue;
        }
        QDomElement currentEffect = currentNode.toElement();
        Info result;
        bool ok = parseInfoFromXml(currentEffect, result);
        if (!ok) {
            continue;
        }
        if (currentEffect.hasAttribute(QLatin1String("kdenlive_info"))) {
            // This is a pre 19.x custom effect, adjust param values
            // First backup effect in legacy folder
            QDir dir(QFileInfo(path).absoluteDir());
            if (!dir.mkpath(QStringLiteral("legacy"))) {
                // Cannot create the legacy folder, abort
                qDebug()<<" = = = Could not create legacy folder in : "<<dir.absolutePath();
                results.second = path;
                return results;
            }
            currentEffect.removeAttribute(QLatin1String("kdenlive_info"));
            effectAdjusted = true;
            QDomNodeList params = currentEffect.elementsByTagName(QLatin1String("parameter"));
            for (int j = 0; j < params.count(); ++j) {
                QDomNode node = params.item(j);
                if (node.isNull()) {
                    continue;
                }
                QDomElement param = node.toElement();
                if (param.hasAttribute(QLatin1String("factor")) && (param.attribute(QLatin1String("type")) == QLatin1String("simplekeyframe") || param.attribute(QLatin1String("type")) == QLatin1String("animated"))) {
                    // This is an old style effect, adjust current and default values
                    QString currentValue;
                    if (!param.hasAttribute(QLatin1String("value"))) {
                        currentValue = param.attribute(QLatin1String("keyframes"));
                    } else {
                        currentValue = param.attribute(QLatin1String("value"));
                    }
                    ok = false;
                    int factor = param.attribute(QLatin1String("factor")).toInt(&ok);
                    if (ok) {
                        double defaultVal = param.attribute(QLatin1String("default")).toDouble() / factor;
                        param.setAttribute(QLatin1String("default"), QString::number(defaultVal));
                        qDebug()<<" = = \nadjusting default to: "<<defaultVal;
                        if (currentValue.contains(QLatin1Char('='))) {
                            QStringList valueStr = currentValue.split(QLatin1Char(';'));
                            QStringList resultStr;
                            for (const QString &val : qAsConst(valueStr)) {
                                if (val.contains(QLatin1Char('='))) {
                                    QString frame = val.section(QLatin1Char('='), 0, 0);
                                    QString frameVal = val.section(QLatin1Char('='), 1);
                                    double v = frameVal.toDouble() / factor;
                                    resultStr << QString("%1=%2").arg(frame).arg(v);
                                } else {
                                    double v = val.toDouble() / factor;
                                    resultStr << QString::number(v);
                                }
                            }
                            param.setAttribute(QLatin1String("value"), resultStr.join(QLatin1Char(';')));
                            qDebug()<<"=== ADJUSTED VAL: "<<resultStr.join(QLatin1Char(';'));
                        }
                    }
                }
            }
        }
        result.xml = currentEffect;
    }
    if (effectAdjusted) {
        QDir dir(QFileInfo(path).absoluteDir());
        dir.cd(QStringLiteral("legacy"));
        if (!file.copy(dir.absoluteFilePath(QFileInfo(file).fileName()))) {
            // Cannot copy the backup file
            qDebug()<<" = = = Could not copy file in : "<<dir.absoluteFilePath(QFileInfo(file).fileName());
            results.second = path;
            return results;
        }
        if (file.open(QFile::WriteOnly | QFile::Truncate)) {
            QTextStream out(&file);
            out << doc.toString();
        }
        file.close();
        results.first = path;
    }
    return results;
}

void EffectsRepository::deleteEffect(const QString &id)
{
    if (!exists(id)) {
        return;
    }
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/"));
    QFile file(dir.absoluteFilePath(id + QStringLiteral(".xml")));
    if (file.exists()) {
        file.remove();
        m_assets.erase(id);
    }
}

bool EffectsRepository::isAudioEffect(const QString &assetId) const
{
    if (m_assets.count(assetId) > 0) {
        AssetListType::AssetType type = m_assets.at(assetId).type;
        return type == AssetListType::AssetType::Audio || type == AssetListType::AssetType::CustomAudio;
    }
    return false;
}
