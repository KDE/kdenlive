/***************************************************************************
                          initeffects.cpp  -  description
                             -------------------
    begin                :  Jul 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
    copyright            : (C) 2008 Marco Gittler
    email                : g.marco@freenet.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "initeffects.h"
#include "effectslist.h"

#include "kdenlivesettings.h"
#include "mainwindow.h"

#include "kdenlive_debug.h"

#include <QFile>
#include <QDir>
#include <QStandardPaths>

#include <klocalizedstring.h>
#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

// static
void initEffects::refreshLumas()
{
    // Check for Kdenlive installed luma files, add empty string at start for no luma
    QStringList imagefiles;
    QStringList fileFilters;
    MainWindow::m_lumaFiles.clear();
    fileFilters << QStringLiteral("*.png") << QStringLiteral("*.pgm");
    QStringList customLumas = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("lumas"), QStandardPaths::LocateDirectory);
    customLumas.append(QString(mlt_environment("MLT_DATA")) + QStringLiteral("/lumas"));
    foreach (const QString &folder, customLumas) {
        QDir topDir(folder);
        QStringList folders = topDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
        foreach (const QString &f, folders) {
            QDir dir(topDir.absoluteFilePath(f));
            QStringList filesnames = dir.entryList(fileFilters, QDir::Files);
            if (MainWindow::m_lumaFiles.contains(f)) {
                imagefiles = MainWindow::m_lumaFiles.value(f);
            }
            foreach (const QString &fname, filesnames) {
                imagefiles.append(dir.absoluteFilePath(fname));
            }
            MainWindow::m_lumaFiles.insert(f, imagefiles);
        }
    }
    /*

    QStringList imagenamelist = QStringList() << i18n("None");
    QStringList imagefiles = QStringList() << QString();
    QStringList filters;
    filters << QStringLiteral("*.pgm") << QStringLiteral("*.png");

    QStringList customLumas = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("lumas"), QStandardPaths::LocateDirectory);
    foreach(const QString & folder, customLumas) {
        QDir directory(folder);
        QStringList filesnames = directory.entryList(filters, QDir::Files);
        foreach(const QString & fname, filesnames) {
            imagenamelist.append(fname);
            imagefiles.append(directory.absoluteFilePath(fname));
        }
    }

    // Check for MLT lumas
    QUrl folder(QString(mlt_environment("MLT_DATA")) + QDir::separator() + "lumas" + QDir::separator() + QString(mlt_environment("MLT_NORMALISATION")));
    QDir lumafolder(folder.path());
    QStringList filesnames = lumafolder.entryList(filters, QDir::Files);
    foreach(const QString & fname, filesnames) {
        imagenamelist.append(fname);
        imagefiles.append(lumafolder.absoluteFilePath(fname));
    }
    //TODO adapt to Wipe transition
    QDomElement lumaTransition = MainWindow::transitions.getEffectByTag(QStringLiteral("luma"), QStringLiteral("luma"));
    QDomNodeList params = lumaTransition.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("tag")) == QLatin1String("resource")) {
            e.setAttribute(QStringLiteral("paramlistdisplay"), imagenamelist.join(QLatin1Char(',')));
            e.setAttribute(QStringLiteral("paramlist"), imagefiles.join(QLatin1Char(';')));
            break;
        }
    }

    QDomElement compositeTransition = MainWindow::transitions.getEffectByTag(QStringLiteral("composite"), QStringLiteral("composite"));
    params = compositeTransition.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("tag")) == QLatin1String("luma")) {
            e.setAttribute(QStringLiteral("paramlistdisplay"), imagenamelist.join(QLatin1Char(',')));
            e.setAttribute(QStringLiteral("paramlist"), imagefiles.join(QLatin1Char(';')));
            break;
        }
    }*/
}

// static
QDomDocument initEffects::getUsedCustomEffects(const QMap<QString, QString> &effectids)
{
    QMapIterator<QString, QString> i(effectids);
    QDomDocument doc;
    QDomElement list = doc.createElement(QStringLiteral("customeffects"));
    doc.appendChild(list);
    while (i.hasNext()) {
        i.next();
        int ix = MainWindow::customEffects.hasEffect(i.value(), i.key());
        if (ix > -1) {
            QDomElement e = MainWindow::customEffects.at(ix);
            list.appendChild(doc.importNode(e, true));
        }
    }
    return doc;
}

//static
bool initEffects::parseEffectFiles(std::unique_ptr<Mlt::Repository> &repository, const QString &locale)
{
    bool movit = false;
    QStringList::Iterator more;
    QStringList::Iterator it;
    QStringList fileList;
    QString itemName;

    if (!repository) {
        //qCDebug(KDENLIVE_LOG) << "Repository didn't finish initialisation" ;
        return movit;
    }

    // Warning: Mlt::Factory::init() resets the locale to the default system value, make sure we keep correct locale
    if (!locale.isEmpty()) {
#ifndef Q_OS_MAC
        setlocale(LC_NUMERIC, locale.toUtf8().constData());
#else
        setlocale(LC_NUMERIC_MASK, locale.toUtf8().constData());
#endif
    }

    // Retrieve the list of MLT's available effects.
    Mlt::Properties *filters = repository->filters();
    QStringList filtersList;
    int max = filters->count();
    filtersList.reserve(max);
    for (int i = 0; i < max; ++i) {
        filtersList << filters->get_name(i);
    }
    delete filters;

    // Retrieve the list of available producers.
    Mlt::Properties *producers = repository->producers();
    QStringList producersList;
    max = producers->count();
    producersList.reserve(max);
    for (int i = 0; i < max; ++i) {
        producersList << producers->get_name(i);
    }
    KdenliveSettings::setProducerslist(producersList);
    delete producers;

    if (filtersList.contains(QStringLiteral("glsl.manager"))) {
        Mlt::Properties *consumers = repository->consumers();
        QStringList consumersList;
        max = consumers->count();
        consumersList.reserve(max);
        for (int i = 0; i < max; ++i) {
            consumersList << consumers->get_name(i);
        }
        delete consumers;
        movit = true;
    } else {
        KdenliveSettings::setGpu_accel(false);
    }

    // Retrieve the list of available transitions.
    Mlt::Properties *transitions = repository->transitions();
    QStringList transitionsItemList;
    max = transitions->count();
    for (int i = 0; i < max; ++i) {
        //qCDebug(KDENLIVE_LOG)<<"TRANSITION "<<i<<" = "<<transitions->get_name(i);
        transitionsItemList << transitions->get_name(i);
    }
    delete transitions;

    // Create structure holding all transitions descriptions so that if an XML file has no description, we take it from MLT
    QMap<QString, QString> transDescriptions;
    foreach (const QString &transname, transitionsItemList) {
        QDomDocument doc = createDescriptionFromMlt(repository, QStringLiteral("transitions"), transname);
        if (!doc.isNull()) {
            if (doc.elementsByTagName(QStringLiteral("description")).count() > 0) {
                QString desc = doc.documentElement().firstChildElement(QStringLiteral("description")).text();
                if (!desc.isEmpty()) {
                    transDescriptions.insert(transname, desc);
                }
            }
        }
    }
    transitionsItemList.sort();

    // Get list of installed luma files
    refreshLumas();

    // Parse xml transition files
    QStringList direc = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("transitions"), QStandardPaths::LocateDirectory);
    // Iterate through effects directories to parse all XML files.
    for (more = direc.begin(); more != direc.end(); ++more) {
        QDir directory(*more);
        QStringList filter;
        filter << QStringLiteral("*.xml");
        fileList = directory.entryList(filter, QDir::Files);
        for (it = fileList.begin(); it != fileList.end(); ++it) {
            itemName = directory.absoluteFilePath(*it);
            parseTransitionFile(&MainWindow::transitions, itemName, repository, transitionsItemList, transDescriptions);
        }
    }

    // Remove blacklisted transitions from the list.
    QFile file(QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("blacklisted_transitions.txt")));
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString black = in.readLine().simplified();
            if (!black.isEmpty() && !black.startsWith('#') &&
                    transitionsItemList.contains(black)) {
                transitionsItemList.removeAll(black);
            }
        }
        file.close();
    }

    // Fill transitions list.
    fillTransitionsList(repository, &MainWindow::transitions, transitionsItemList);

    // Remove blacklisted effects from the filters list.
    QStringList mltFiltersList = filtersList;
    QStringList mltBlackList;
    QFile file2(QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("blacklisted_effects.txt")));
    if (file2.open(QIODevice::ReadOnly)) {
        QTextStream in(&file2);
        while (!in.atEnd()) {
            QString black = in.readLine().simplified();
            if (!black.isEmpty() && !black.startsWith('#') &&
                    mltFiltersList.contains(black)) {
                mltFiltersList.removeAll(black);
                mltBlackList << black;
            }
        }
        file2.close();
    }

    /*
     * Cleanup the global lists. We use QMap because of its automatic sorting
     * (by key) and key uniqueness (using insert() instead of insertMulti()).
     * This introduces some more cycles (while removing them from other parts of
     * the code and centralising them), but due to the way this methods, QMap
     * and EffectsList are implemented, there's no easy way to make it
     * differently without reinplementing something (which should really be
     * done).
     */
    QDomElement effectInfo;
    QMap<QString, QDomElement> effectsMap;
    QMap<QString, QDomElement> videoEffectsMap;
    QMap<QString, QDomElement> audioEffectsMap;

    // Create transitions
    max = MainWindow::transitions.count();
    for (int i = 0; i < max; ++i) {
        effectInfo = MainWindow::transitions.at(i);
        effectsMap.insert(effectInfo.firstChildElement(QStringLiteral("name")).text().toLower().toUtf8().data(), effectInfo);
    }
    MainWindow::transitions.clearList();
    foreach (const QDomElement &effect, effectsMap) {
        MainWindow::transitions.append(effect);
    }
    effectsMap.clear();

    // Create structure holding all effects descriptions so that if an XML effect has no description, we take it from MLT
    QMap<QString, QString> effectDescriptions;
    foreach (const QString &filtername, mltBlackList) {
        QDomDocument doc = createDescriptionFromMlt(repository, QStringLiteral("filters"), filtername);
        if (!doc.isNull()) {
            if (doc.elementsByTagName(QStringLiteral("description")).count() > 0) {
                QString desc = doc.documentElement().firstChildElement(QStringLiteral("description")).text();
                //WARNING: TEMPORARY FIX for unusable MLT SOX parameters description
                if (desc.startsWith(QLatin1String("Process audio using a SoX"))) {
                    // Remove MLT's SOX generated effects since the parameters properties are unusable for us
                    continue;
                }
                if (!desc.isEmpty()) {
                    effectDescriptions.insert(filtername, desc);
                }
            }
        }
    }

    // Create effects from MLT
    foreach (const QString &filtername, mltFiltersList) {
        QDomDocument doc = createDescriptionFromMlt(repository, QStringLiteral("filters"), filtername);
        //WARNING: TEMPORARY FIX for empty MLT effects descriptions - disable effects without parameters - jbm 09-06-2011
        if (!doc.isNull() && doc.elementsByTagName(QStringLiteral("parameter")).count() > 0) {
            if (doc.documentElement().attribute(QStringLiteral("type")) == QLatin1String("audio")) {
                if (doc.elementsByTagName(QStringLiteral("description")).count() > 0) {
                    QString desc = doc.documentElement().firstChildElement(QStringLiteral("description")).text();
                    //WARNING: TEMPORARY FIX for unusable MLT SOX parameters description
                    if (desc.startsWith(QLatin1String("Process audio using a SoX"))) {
                        // Remove MLT's SOX generated effects since the parameters properties are unusable for us
                    } else {
                        audioEffectsMap.insert(doc.documentElement().firstChildElement(QStringLiteral("name")).text().toLower().toUtf8().data(), doc.documentElement());
                    }
                }
            } else {
                videoEffectsMap.insert(doc.documentElement().firstChildElement(QStringLiteral("name")).text().toLower().toUtf8().data(), doc.documentElement());
            }
        }
    }

    // Set the directories to look into for effects.
    direc = QStandardPaths::locateAll(QStandardPaths::AppDataLocation, QStringLiteral("effects"), QStandardPaths::LocateDirectory);
    // Iterate through effects directories to parse all XML files.
    for (more = direc.begin(); more != direc.end(); ++more) {
        QDir directory(*more);
        QStringList filter;
        filter << QStringLiteral("*.xml");
        fileList = directory.entryList(filter, QDir::Files);
        for (it = fileList.begin(); it != fileList.end(); ++it) {
            itemName = directory.absoluteFilePath(*it);
            parseEffectFile(&MainWindow::customEffects,
                            &MainWindow::audioEffects,
                            &MainWindow::videoEffects,
                            itemName, filtersList, producersList, repository, effectDescriptions);
        }
    }

    // Create custom effects
    max = MainWindow::customEffects.count();
    for (int i = 0; i < max; ++i) {
        effectInfo = MainWindow::customEffects.at(i);
        if (effectInfo.tagName() == QLatin1String("effectgroup")) {
            effectsMap.insert(effectInfo.attribute(QStringLiteral("name")).toUtf8().data(), effectInfo);
        } else {
            effectsMap.insert(effectInfo.firstChildElement(QStringLiteral("name")).text().toUtf8().data(), effectInfo);
        }
    }
    MainWindow::customEffects.clearList();
    foreach (const QDomElement &effect, effectsMap) {
        MainWindow::customEffects.append(effect);
    }
    effectsMap.clear();

    // Create audio effects
    max = MainWindow::audioEffects.count();
    for (int i = 0; i < max; ++i) {
        effectInfo = MainWindow::audioEffects.at(i);
        audioEffectsMap.insert(effectInfo.firstChildElement(QStringLiteral("name")).text().toLower().toUtf8().data(), effectInfo);
    }
    MainWindow::audioEffects.clearList();
    foreach (const QDomElement &effect, audioEffectsMap) {
        MainWindow::audioEffects.append(effect);
    }

    // Create video effects
    max = MainWindow::videoEffects.count();
    for (int i = 0; i < max; ++i) {
        effectInfo = MainWindow::videoEffects.at(i);
        videoEffectsMap.insert(effectInfo.firstChildElement(QStringLiteral("name")).text().toLower().toUtf8().data(), effectInfo);
    }
    MainWindow::videoEffects.clearList();
    foreach (const QDomElement &effect, videoEffectsMap) {
        MainWindow::videoEffects.append(effect);
    }

    return movit;
}

// static
void initEffects::parseCustomEffectsFile()
{
    MainWindow::customEffects.clearList();
    /*
     * Why a QMap? See parseEffectFiles(). It's probably useless here, but we
     * cannot be sure about it.
     */
    QMap<QString, QDomElement> effectsMap;
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects");
    QDir directory = QDir(path);
    QStringList filter;
    filter << QStringLiteral("*.xml");
    const QStringList fileList = directory.entryList(filter, QDir::Files);
    /*
     * We need to declare these variables outside the foreach, or the QMap will
     * refer to non existing variables (QMap::insert() takes references as
     * parameters).
     */
    QDomDocument doc;
    QDomNodeList effects;
    QDomElement e;
    int unknownGroupCount = 0;
    foreach (const QString &filename, fileList) {
        QString itemName = directory.absoluteFilePath(filename);
        QFile file(itemName);
        doc.setContent(&file, false);
        file.close();
        QDomElement base = doc.documentElement();
        if (base.tagName() == QLatin1String("effectgroup")) {
            QString groupName = base.attribute(QStringLiteral("name"));
            if (groupName.isEmpty()) {
                groupName = i18n("Group %1", unknownGroupCount);
                base.setAttribute(QStringLiteral("name"), groupName);
                unknownGroupCount++;
            }
            effectsMap.insert(groupName.toLower().toUtf8().data(), base);
        } else if (base.tagName() == QLatin1String("effect")) {
            effectsMap.insert(base.firstChildElement(QStringLiteral("name")).text().toLower().toUtf8().data(), base);
        } else {
            qCDebug(KDENLIVE_LOG) << "Unsupported effect file: " << itemName;
        }
    }
    foreach (const QDomElement &effect, effectsMap) {
        MainWindow::customEffects.append(effect);
    }
}

// static
void initEffects::parseEffectFile(EffectsList *customEffectList, EffectsList *audioEffectList, EffectsList *videoEffectList, const QString &name, const QStringList &filtersList, const QStringList &producersList, std::unique_ptr<Mlt::Repository> &repository, const QMap<QString, QString> &effectDescriptions)
{
    QDomDocument doc;
    QFile file(name);
    doc.setContent(&file, false);
    file.close();
    QDomElement documentElement;
    QDomNodeList effects;
    QDomElement base = doc.documentElement();
    QStringList addedTags;
    effects = doc.elementsByTagName(QStringLiteral("effect"));
    int i = effects.count();
    if (i == 0) {
        qCDebug(KDENLIVE_LOG) << "+++++++++++++\nEffect broken: " << name << "\n+++++++++++";
        return;
    }

    bool needsLocaleConversion = false;
    i--;
    for (; i >= 0; i--) {
        QLocale locale;
        QDomNode n = effects.item(i);
        if (n.isNull()) {
            continue;
        }
        documentElement = n.toElement();
        QString tag = documentElement.attribute(QStringLiteral("tag"), QString());
        QString id = documentElement.hasAttribute(QStringLiteral("id")) ? documentElement.attribute(QStringLiteral("id")) : tag;
        if (addedTags.contains(id)) {
            // We already processed a version of that filter
            continue;
        }
        //If XML has no description, take it fom MLT's descriptions
        if (effectDescriptions.contains(tag)) {
            QDomNodeList desc = documentElement.elementsByTagName(QStringLiteral("description"));
            if (desc.isEmpty()) {
                QDomElement d = documentElement.ownerDocument().createElement(QStringLiteral("description"));
                QDomText value = documentElement.ownerDocument().createTextNode(effectDescriptions.value(tag));
                d.appendChild(value);
                documentElement.appendChild(d);
            }
        }

        if (documentElement.hasAttribute(QStringLiteral("LC_NUMERIC"))) {
            // set a locale for that file
            locale = QLocale(documentElement.attribute(QStringLiteral("LC_NUMERIC")));
            if (locale.decimalPoint() != QLocale().decimalPoint()) {
                needsLocaleConversion = true;
            }
        }
        locale.setNumberOptions(QLocale::OmitGroupSeparator);

        if (needsLocaleConversion) {
            // we need to convert all numbers to the system's locale (for example 0.5 -> 0,5)
            QChar separator = QLocale().decimalPoint();
            QChar oldSeparator = locale.decimalPoint();
            QDomNodeList params = documentElement.elementsByTagName(QStringLiteral("parameter"));
            for (int j = 0; j < params.count(); ++j) {
                QDomNamedNodeMap attrs = params.at(j).attributes();
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
        }

        double version = -1;
        Mlt::Properties *metadata = repository->metadata(filter_type, tag.toUtf8().data());
        if (metadata && metadata->is_valid()) {
            version = metadata->get_double("version");
        }
        delete metadata;

        if (documentElement.hasAttribute(QStringLiteral("version"))) {
            // a specific version of the filter is required
            if (locale.toDouble(documentElement.attribute(QStringLiteral("version"))) > version) {
                continue;
            }
        }
        if (version > -1) {
            // Add version info to XML
            QDomNode versionNode = doc.createElement(QStringLiteral("version"));
            versionNode.appendChild(doc.createTextNode(QLocale().toString(version)));
            documentElement.appendChild(versionNode);
        }
        // Parse effect information.
        if (base.tagName() != QLatin1String("effectgroup") && (filtersList.contains(tag) || producersList.contains(tag))) {
            QString type = documentElement.attribute(QStringLiteral("type"), QString());
            if (type == QLatin1String("audio")) {
                audioEffectList->append(documentElement);
            } else if (type == QLatin1String("custom")) {
                customEffectList->append(documentElement);
            } else {
                videoEffectList->append(documentElement);
            }
            addedTags << id;
        }
    }
    if (base.tagName() == QLatin1String("effectgroup")) {
        QString type = base.attribute(QStringLiteral("type"), QString());
        if (type == QLatin1String("audio")) {
            audioEffectList->append(base);
        } else if (type == QLatin1String("custom")) {
            customEffectList->append(base);
        } else {
            videoEffectList->append(base);
        }
    }
}

QDomDocument initEffects::createDescriptionFromMlt(std::unique_ptr<Mlt::Repository> &repository, const QString & /*type*/, const QString &filtername)
{

    QDomDocument ret;
    Mlt::Properties *metadata = repository->metadata(filter_type, filtername.toLatin1().data());
    ////qCDebug(KDENLIVE_LOG) << filtername;
    if (metadata && metadata->is_valid()) {
        if (metadata->get("title") && metadata->get("identifier") && strlen(metadata->get("title")) > 0) {
            QDomElement eff = ret.createElement(QStringLiteral("effect"));
            QString id = metadata->get("identifier");
            eff.setAttribute(QStringLiteral("tag"), id);
            eff.setAttribute(QStringLiteral("id"), id);
            ////qCDebug(KDENLIVE_LOG)<<"Effect: "<<id;

            QDomElement name = ret.createElement(QStringLiteral("name"));
            QString name_str = metadata->get("title");
            name_str[0] = name_str[0].toUpper();
            name.appendChild(ret.createTextNode(name_str));

            QDomElement desc = ret.createElement(QStringLiteral("description"));
            desc.appendChild(ret.createTextNode(metadata->get("description")));

            QDomElement author = ret.createElement(QStringLiteral("author"));
            author.appendChild(ret.createTextNode(metadata->get("creator")));

            QDomElement version = ret.createElement(QStringLiteral("version"));
            version.appendChild(ret.createTextNode(metadata->get("version")));

            eff.appendChild(name);
            eff.appendChild(author);
            eff.appendChild(desc);
            eff.appendChild(version);

            Mlt::Properties tags((mlt_properties) metadata->get_data("tags"));
            if (QString(tags.get(0)) == QLatin1String("Audio")) {
                eff.setAttribute(QStringLiteral("type"), QStringLiteral("audio"));
            }
            /*for (int i = 0; i < tags.count(); ++i)
                //qCDebug(KDENLIVE_LOG)<<tags.get_name(i)<<"="<<tags.get(i);*/

            Mlt::Properties param_props((mlt_properties) metadata->get_data("parameters"));
            for (int j = 0; param_props.is_valid() && j < param_props.count(); ++j) {
                QDomElement params = ret.createElement(QStringLiteral("parameter"));

                Mlt::Properties paramdesc((mlt_properties) param_props.get_data(param_props.get_name(j)));
                params.setAttribute(QStringLiteral("name"), paramdesc.get("identifier"));
                if (params.attribute(QStringLiteral("name")) == QLatin1String("argument")) {
                    // This parameter has to be given as attribute when using command line, do not show it in Kdenlive
                    continue;
                }

                if (paramdesc.get("readonly") && !strcmp(paramdesc.get("readonly"), "yes")) {
                    // Do not expose readonly parameters
                    continue;
                }

                if (paramdesc.get("maximum")) {
                    params.setAttribute(QStringLiteral("max"), paramdesc.get("maximum"));
                }
                if (paramdesc.get("minimum")) {
                    params.setAttribute(QStringLiteral("min"), paramdesc.get("minimum"));
                }

                QString paramType = paramdesc.get("type");
                if (paramType == QLatin1String("integer")) {
                    if (params.attribute(QStringLiteral("min")) == QLatin1String("0") && params.attribute(QStringLiteral("max")) == QLatin1String("1")) {
                        params.setAttribute(QStringLiteral("type"), QStringLiteral("bool"));
                    } else {
                        params.setAttribute(QStringLiteral("type"), QStringLiteral("constant"));
                    }
                } else if (paramType == QLatin1String("float")) {
                    params.setAttribute(QStringLiteral("type"), QStringLiteral("constant"));
                    // param type is float, set default decimals to 3
                    params.setAttribute(QStringLiteral("decimals"), QStringLiteral("3"));
                } else if (paramType == QLatin1String("boolean")) {
                    params.setAttribute(QStringLiteral("type"), QStringLiteral("bool"));
                } else if (paramType == QLatin1String("geometry")) {
                    params.setAttribute(QStringLiteral("type"), QStringLiteral("geometry"));
                } else if (paramType == QLatin1String("string")) {
                    // string parameter are not really supported, so if we have a default value, enforce it
                    params.setAttribute(QStringLiteral("type"), QStringLiteral("fixed"));
                    if (paramdesc.get("default")) {
                        QString stringDefault = paramdesc.get("default");
                        stringDefault.remove(QLatin1Char('\''));
                        params.setAttribute(QStringLiteral("value"), stringDefault);
                    } else {
                        // String parameter without default, skip it completely
                        continue;
                    }
                } else {
                    params.setAttribute(QStringLiteral("type"), paramType);
                    if (!QString(paramdesc.get("format")).isEmpty()) {
                        params.setAttribute(QStringLiteral("format"), paramdesc.get("format"));
                    }
                }
                if (!params.hasAttribute(QStringLiteral("value"))) {
                    if (paramdesc.get("default")) {
                        params.setAttribute(QStringLiteral("default"), paramdesc.get("default"));
                    }
                    if (paramdesc.get("value")) {
                        params.setAttribute(QStringLiteral("value"), paramdesc.get("value"));
                    } else {
                        params.setAttribute(QStringLiteral("value"), paramdesc.get("default"));
                    }
                }

                QString paramName = paramdesc.get("title");
                if (!paramName.isEmpty()) {
                    QDomElement pname = ret.createElement(QStringLiteral("name"));
                    pname.appendChild(ret.createTextNode(paramName));
                    params.appendChild(pname);
                }

                if (paramdesc.get("description")) {
                    QDomElement desc = ret.createElement(QStringLiteral("comment"));
                    desc.appendChild(ret.createTextNode(paramdesc.get("description")));
                    params.appendChild(desc);
                }

                eff.appendChild(params);
            }
            ret.appendChild(eff);
        }
    }
    delete metadata;
    metadata = nullptr;
    /*QString outstr;
     QTextStream str(&outstr);
     ret.save(str, 2);
     //qCDebug(KDENLIVE_LOG) << outstr;*/
    return ret;
}

void initEffects::fillTransitionsList(std::unique_ptr<Mlt::Repository> &repository, EffectsList *transitions, QStringList names)
{
    // Remove transitions that are not implemented.
    int pos = names.indexOf(QStringLiteral("mix"));
    if (pos != -1) {
        names.takeAt(pos);
    }
    // Remove unused luma transition, replaced with a custom composite Wipe transition
    pos = names.indexOf(QStringLiteral("luma"));
    if (pos != -1) {
        names.takeAt(pos);
    }

    //WARNING: this is a hack to get around temporary invalid metadata in MLT, 2nd of june 2011 JBM
    QStringList customTransitions;
    customTransitions << QStringLiteral("composite") << QStringLiteral("affine") << QStringLiteral("mix") << QStringLiteral("region");

    foreach (const QString &name, names) {
        QDomDocument ret;
        QDomElement ktrans = ret.createElement(QStringLiteral("transition"));
        ret.appendChild(ktrans);
        ktrans.setAttribute(QStringLiteral("tag"), name);

        QDomElement tname = ret.createElement(QStringLiteral("name"));
        QDomElement desc = ret.createElement(QStringLiteral("description"));
        ktrans.appendChild(tname);
        ktrans.appendChild(desc);
        Mlt::Properties *metadata = nullptr;
        if (!customTransitions.contains(name)) {
            metadata = repository->metadata(transition_type, name.toUtf8().data());
        }
        if (metadata && metadata->is_valid()) {
            // If possible, set name and description.
            //qCDebug(KDENLIVE_LOG)<<" / / FOUND TRANS: "<<metadata->get("title");
            if (metadata->get("title") && metadata->get("identifier")) {
                tname.appendChild(ret.createTextNode(metadata->get("title")));
            }
            desc.appendChild(ret.createTextNode(metadata->get("description")));

            Mlt::Properties param_props((mlt_properties) metadata->get_data("parameters"));
            for (int i = 0; param_props.is_valid() && i < param_props.count(); ++i) {
                QDomElement params = ret.createElement(QStringLiteral("parameter"));

                Mlt::Properties paramdesc((mlt_properties) param_props.get_data(param_props.get_name(i)));

                params.setAttribute(QStringLiteral("name"), paramdesc.get("identifier"));

                if (paramdesc.get("maximum")) {
                    params.setAttribute(QStringLiteral("max"), paramdesc.get_double("maximum"));
                }
                if (paramdesc.get("minimum")) {
                    params.setAttribute(QStringLiteral("min"), paramdesc.get_double("minimum"));
                }
                if (paramdesc.get("default")) {
                    params.setAttribute(QStringLiteral("default"), paramdesc.get("default"));
                }
                if (QString(paramdesc.get("type")) == QLatin1String("integer")) {
                    if (params.attribute(QStringLiteral("min")) == QLatin1String("0") && params.attribute(QStringLiteral("max")) == QLatin1String("1")) {
                        params.setAttribute(QStringLiteral("type"), QStringLiteral("bool"));
                    } else {
                        params.setAttribute(QStringLiteral("type"), QStringLiteral("constant"));
                        params.setAttribute(QStringLiteral("factor"), QStringLiteral("100"));
                    }
                } else if (QString(paramdesc.get("type")) == QLatin1String("float")) {
                    params.setAttribute(QStringLiteral("type"), QStringLiteral("double"));
                    if (paramdesc.get_int("maximum") == 1) {
                        params.setAttribute(QStringLiteral("factor"), 100);
                        params.setAttribute(QStringLiteral("max"), 100);
                        params.setAttribute(QStringLiteral("default"), paramdesc.get_double("default") * 100);
                    }
                } else if (QString(paramdesc.get("type")) == QLatin1String("boolean")) {
                    params.setAttribute(QStringLiteral("type"), QStringLiteral("bool"));
                }
                if (!QString(paramdesc.get("format")).isEmpty()) {
                    params.setAttribute(QStringLiteral("type"), QStringLiteral("complex"));
                    params.setAttribute(QStringLiteral("format"), paramdesc.get("format"));
                }
                if (paramdesc.get("value")) {
                    params.setAttribute(QStringLiteral("value"), paramdesc.get("value"));
                } else {
                    params.setAttribute(QStringLiteral("value"), params.attribute(QStringLiteral("default")));
                }

                QDomElement pname = ret.createElement(QStringLiteral("name"));
                pname.appendChild(ret.createTextNode(paramdesc.get("title")));
                params.appendChild(pname);
                ktrans.appendChild(params);
            }
        } else {
            /*
             * Check for Kdenlive installed luma files, add empty string at
             * start for no luma file.
             */

            // Implement default transitions.
            //TODO: create xml files for transitions in data/transitions instead of hardcoding here
            QList<QDomElement> paramList;
            if (name == QLatin1String("composite")) {
                ktrans.setAttribute(QStringLiteral("id"), name);
                tname.appendChild(ret.createTextNode(i18n("Composite")));
                desc.appendChild(ret.createTextNode(i18n("A key-framable alpha-channel compositor for two frames.")));
                paramList.append(quickParameterFill(ret, i18n("Geometry"), QStringLiteral("geometry"), QStringLiteral("geometry"), QStringLiteral("0%/0%:100%x100%:100"), QStringLiteral("-500;-500;-500;-500;0"), QStringLiteral("500;500;500;500;100")));
                paramList.append(quickParameterFill(ret, i18n("Alpha Channel Operation"), QStringLiteral("operator"), QStringLiteral("list"), QStringLiteral("over"), QString(), QString(), QStringLiteral("over,and,or,xor"), i18n("Over,And,Or,Xor")));
                paramList.append(quickParameterFill(ret, i18n("Align"), QStringLiteral("aligned"), QStringLiteral("bool"), QStringLiteral("1"), QStringLiteral("0"), QStringLiteral("1")));
                paramList.append(quickParameterFill(ret, i18n("Align"), QStringLiteral("valign"), QStringLiteral("fixed"), QStringLiteral("middle"), QStringLiteral("middle"), QStringLiteral("middle")));
                paramList.append(quickParameterFill(ret, i18n("Align"), QStringLiteral("halign"), QStringLiteral("fixed"), QStringLiteral("centre"), QStringLiteral("centre"), QStringLiteral("centre")));
                paramList.append(quickParameterFill(ret, i18n("Fill"), QStringLiteral("fill"), QStringLiteral("bool"), QStringLiteral("1"), QStringLiteral("0"), QStringLiteral("1")));
                paramList.append(quickParameterFill(ret, i18n("Distort"), QStringLiteral("distort"), QStringLiteral("bool"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("1")));
                paramList.append(quickParameterFill(ret, i18n("Wipe Method"), QStringLiteral("luma"), QStringLiteral("list"), QString(), QString(), QString(), QStringLiteral("%lumaPaths"), QString()));
                paramList.append(quickParameterFill(ret, i18n("Wipe Softness"), QStringLiteral("softness"), QStringLiteral("double"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("100"), QString(), QString(), QStringLiteral("100")));
                paramList.append(quickParameterFill(ret, i18n("Wipe Invert"), QStringLiteral("luma_invert"), QStringLiteral("bool"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("1")));
                paramList.append(quickParameterFill(ret, i18n("Force Progressive Rendering"), QStringLiteral("progressive"), QStringLiteral("bool"), QStringLiteral("1"), QStringLiteral("0"), QStringLiteral("1")));
                paramList.append(quickParameterFill(ret, i18n("Force Deinterlace Overlay"), QStringLiteral("deinterlace"), QStringLiteral("bool"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("1")));
            } else if (name == QLatin1String("affine")) {
                tname.appendChild(ret.createTextNode(i18n("Affine")));
                ret.documentElement().setAttribute(QStringLiteral("showrotation"), QStringLiteral("1"));
                /*paramList.append(quickParameterFill(ret, i18n("Rotate Y"), "rotate_y", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Rotate X"), "rotate_x", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Rotate Z"), "rotate_z", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Fix Rotate Y"), "fix_rotate_y", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Fix Rotate X"), "fix_rotate_x", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Fix Rotate Z"), "fix_rotate_z", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Shear Y"), "shear_y", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Shear X"), "shear_x", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Shear Z"), "shear_z", "double", "0", "0", "360"));*/
                /*paramList.append(quickParameterFill(ret, i18n("Fix Shear Y"), "fix_shear_y", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Fix Shear X"), "fix_shear_x", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Fix Shear Z"), "fix_shear_z", "double", "0", "0", "360"));*/

                paramList.append(quickParameterFill(ret, QStringLiteral("keyed"), QStringLiteral("keyed"), QStringLiteral("fixed"), QStringLiteral("1"), QStringLiteral("1"), QStringLiteral("1")));
                paramList.append(quickParameterFill(ret, i18n("Geometry"), QStringLiteral("geometry"), QStringLiteral("geometry"),  QStringLiteral("0/0:100%x100%:100%"), QStringLiteral("0/0:100%x100%:100%"), QStringLiteral("0/0:100%x100%:100%"), QString(), QString(), QString(), QStringLiteral("true")));
                paramList.append(quickParameterFill(ret, i18n("Distort"), QStringLiteral("distort"), QStringLiteral("bool"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("1")));
                paramList.append(quickParameterFill(ret, i18n("Rotate X"), QStringLiteral("rotate_x"), QStringLiteral("addedgeometry"), QStringLiteral("0"), QStringLiteral("-1800"), QStringLiteral("1800"), QString(), QString(), QStringLiteral("10")));
                paramList.append(quickParameterFill(ret, i18n("Rotate Y"), QStringLiteral("rotate_y"), QStringLiteral("addedgeometry"), QStringLiteral("0"), QStringLiteral("-1800"), QStringLiteral("1800"), QString(), QString(), QStringLiteral("10")));
                paramList.append(quickParameterFill(ret, i18n("Rotate Z"), QStringLiteral("rotate_z"), QStringLiteral("addedgeometry"), QStringLiteral("0"), QStringLiteral("-1800"), QStringLiteral("1800"), QString(), QString(), QStringLiteral("10")));
                /*paramList.append(quickParameterFill(ret, i18n("Rotate Y"), "rotate_y", "simplekeyframe", "0", "-1800", "1800", QString(), QString(), "10"));
                paramList.append(quickParameterFill(ret, i18n("Rotate Z"), "rotate_z", "simplekeyframe", "0", "-1800", "1800", QString(), QString(), "10"));*/

                paramList.append(quickParameterFill(ret, i18n("Fix Shear Y"), QStringLiteral("shear_y"), QStringLiteral("double"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("360")));
                paramList.append(quickParameterFill(ret, i18n("Fix Shear X"), QStringLiteral("shear_x"), QStringLiteral("double"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("360")));
                paramList.append(quickParameterFill(ret, i18n("Fix Shear Z"), QStringLiteral("shear_z"), QStringLiteral("double"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("360")));
            } else if (name == QLatin1String("mix")) {
                tname.appendChild(ret.createTextNode(i18n("Mix")));
            } else if (name == QLatin1String("region")) {
                ktrans.setAttribute(QStringLiteral("id"), name);
                tname.appendChild(ret.createTextNode(i18n("Region")));
                desc.appendChild(ret.createTextNode(i18n("Use alpha channel of another clip to create a transition.")));
                paramList.append(quickParameterFill(ret, i18n("Transparency clip"), QStringLiteral("resource"), QStringLiteral("url"), QString(), QString(), QString(), QString(), QString(), QString()));
                paramList.append(quickParameterFill(ret, i18n("Geometry"), QStringLiteral("composite.geometry"), QStringLiteral("geometry"), QStringLiteral("0%/0%:100%x100%:100"), QStringLiteral("-500;-500;-500;-500;0"), QStringLiteral("500;500;500;500;100")));
                paramList.append(quickParameterFill(ret, i18n("Alpha Channel Operation"), QStringLiteral("composite.operator"), QStringLiteral("list"), QStringLiteral("over"), QString(), QString(), QStringLiteral("over,and,or,xor"), i18n("Over,And,Or,Xor")));
                paramList.append(quickParameterFill(ret, i18n("Align"), QStringLiteral("composite.aligned"), QStringLiteral("bool"), QStringLiteral("1"), QStringLiteral("0"), QStringLiteral("1")));
                paramList.append(quickParameterFill(ret, i18n("Fill"), QStringLiteral("composite.fill"), QStringLiteral("bool"), QStringLiteral("1"), QStringLiteral("0"), QStringLiteral("1")));
                paramList.append(quickParameterFill(ret, i18n("Distort"), QStringLiteral("composite.distort"), QStringLiteral("bool"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("1")));
                paramList.append(quickParameterFill(ret, i18n("Wipe File"), QStringLiteral("composite.luma"), QStringLiteral("list"), QString(), QString(), QString(), QStringLiteral("%lumaPaths"), QString()));
                paramList.append(quickParameterFill(ret, i18n("Wipe Softness"), QStringLiteral("composite.softness"), QStringLiteral("double"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("100"), QString(), QString(), QStringLiteral("100")));
                paramList.append(quickParameterFill(ret, i18n("Wipe Invert"), QStringLiteral("composite.luma_invert"), QStringLiteral("bool"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("1")));
                paramList.append(quickParameterFill(ret, i18n("Force Progressive Rendering"), QStringLiteral("composite.progressive"), QStringLiteral("bool"), QStringLiteral("1"), QStringLiteral("0"), QStringLiteral("1")));
                paramList.append(quickParameterFill(ret, i18n("Force Deinterlace Overlay"), QStringLiteral("composite.deinterlace"), QStringLiteral("bool"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("1")));
            }
            foreach (const QDomElement &e, paramList) {
                ktrans.appendChild(e);
            }
        }
        delete metadata;
        metadata = nullptr;
        // Add the transition to the global list.
        ////qCDebug(KDENLIVE_LOG) << ret.toString();
        transitions->append(ret.documentElement());
    }

    // Add some virtual transitions.
    QString slidetrans = QStringLiteral("<ktransition tag=\"composite\" id=\"slide\"><name>") + i18n("Slide") +
            QStringLiteral("</name><description>") + i18n("Slide image from one side to another.")
            + QStringLiteral("</description><parameter tag=\"geometry\" type=\"wipe\" default=\"-100%,0%:100%x100%;-1=0%,0%:100%x100%\" name=\"geometry\"><name>")
            + i18n("Direction") + QStringLiteral("</name>                                               </parameter><parameter tag=\"aligned\" default=\"0\" type=\"bool\" name=\"aligned\" ><name>")
            + i18n("Align") + QStringLiteral("</name></parameter><parameter tag=\"progressive\" default=\"1\" type=\"bool\" name=\"progressive\" ><name>")
            + i18n("Force Progressive Rendering") + QStringLiteral("</name></parameter><parameter tag=\"deinterlace\" default=\"0\" type=\"bool\" name=\"deinterlace\" ><name>")
            + i18n("Force Deinterlace Overlay") + QStringLiteral("</name></parameter><parameter tag=\"invert\" default=\"0\" type=\"bool\" name=\"invert\" ><name>")
            + i18nc("@property: means that the image is inverted", "Invert") + QStringLiteral("</name></parameter></ktransition>");
    QDomDocument ret;
    ret.setContent(slidetrans);
    transitions->append(ret.documentElement());

    QString dissolve = QStringLiteral("<ktransition tag=\"luma\" id=\"dissolve\"><name>")
            + i18n("Dissolve") + QStringLiteral("</name><description>") + i18n("Fade out one video while fading in the other video.")
            + QStringLiteral("</description><parameter tag=\"reverse\" default=\"0\" type=\"bool\" name=\"reverse\" ><name>")
            + i18n("Reverse") + QStringLiteral("</name></parameter></ktransition>");
    ret.setContent(dissolve);
    transitions->append(ret.documentElement());

    /*QString alphatrans = "<ktransition tag=\"composite\" id=\"alphatransparency\" ><name>" + i18n("Alpha Transparency") + QStringLiteral("</name><description>") + i18n("Make alpha channel transparent.") + "</description><parameter tag=\"geometry\" type=\"fixed\" default=\"0%,0%:100%x100%\" name=\"geometry\"><name>" + i18n("Direction") + "</name></parameter><parameter tag=\"fill\" default=\"0\" type=\"bool\" name=\"fill\" ><name>" + i18n("Rescale") + "</name></parameter><parameter tag=\"aligned\" default=\"0\" type=\"bool\" name=\"aligned\" ><name>" + i18n("Align") + "</name></parameter></ktransition>";
    ret.setContent(alphatrans);
    transitions->append(ret.documentElement());*/
}

QDomElement initEffects::quickParameterFill(QDomDocument &doc, const QString &name, const QString &tag, const QString &type, const QString &def, const QString &min, const QString &max, const QString &list, const QString &listdisplaynames, const QString &factor, const QString &opacity)
{
    QDomElement parameter = doc.createElement(QStringLiteral("parameter"));
    parameter.setAttribute(QStringLiteral("tag"), tag);
    parameter.setAttribute(QStringLiteral("default"), def);
    parameter.setAttribute(QStringLiteral("value"), def);
    parameter.setAttribute(QStringLiteral("type"), type);
    parameter.setAttribute(QStringLiteral("name"), tag);
    parameter.setAttribute(QStringLiteral("max"), max);
    parameter.setAttribute(QStringLiteral("min"), min);
    if (!list.isEmpty()) {
        parameter.setAttribute(QStringLiteral("paramlist"), list);
    }
    if (!listdisplaynames.isEmpty()) {
        parameter.setAttribute(QStringLiteral("paramlistdisplay"), listdisplaynames);
    }
    if (!factor.isEmpty()) {
        parameter.setAttribute(QStringLiteral("factor"), factor);
    }
    if (!opacity.isEmpty()) {
        parameter.setAttribute(QStringLiteral("opacity"), opacity);
    }
    QDomElement pname = doc.createElement(QStringLiteral("name"));
    pname.appendChild(doc.createTextNode(name));
    parameter.appendChild(pname);

    return parameter;
}

// static
void initEffects::parseTransitionFile(EffectsList *transitionList, const QString &name, std::unique_ptr<Mlt::Repository> &repository, const QStringList &installedTransitions, const QMap<QString, QString> &effectDescriptions)
{
    QFile file(name);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QDomDocument doc;
    QTextStream out(&file);
    QString fileContent = out.readAll();
    file.close();
    doc.setContent(fileContent, false);
    QDomElement documentElement;
    QDomNodeList effects;
    QDomElement base = doc.documentElement();
    QStringList addedTags;
    effects = doc.elementsByTagName(QStringLiteral("transition"));
    int i = effects.count();
    if (i == 0) {
        qCDebug(KDENLIVE_LOG) << "+++++++++++++Transition broken: " << name << "\n+++++++++++";
        return;
    }

    bool needsLocaleConversion = false;
    i--;
    for (; i >= 0; i--) {
        QLocale locale;
        QDomNode n = effects.item(i);
        if (n.isNull()) {
            continue;
        }
        documentElement = n.toElement();
        QString tag = documentElement.attribute(QStringLiteral("tag"));
        if (!installedTransitions.contains(tag)) {
            // This transition is not available
            return;
        }
        QString id = documentElement.attribute(QStringLiteral("id"));
        if (addedTags.contains(id)) {
            // We already processed a version of that filter
            continue;
        }
        //If XML has no description, take it fom MLT's descriptions
        if (effectDescriptions.contains(id)) {
            QDomNodeList desc = documentElement.elementsByTagName(QStringLiteral("description"));
            if (desc.isEmpty()) {
                QDomElement d = documentElement.ownerDocument().createElement(QStringLiteral("description"));
                QDomText value = documentElement.ownerDocument().createTextNode(effectDescriptions.value(id));
                d.appendChild(value);
                documentElement.appendChild(d);
            }
        }

        if (documentElement.hasAttribute(QStringLiteral("LC_NUMERIC"))) {
            // set a locale for that file
            locale = QLocale(documentElement.attribute(QStringLiteral("LC_NUMERIC")));
            if (locale.decimalPoint() != QLocale().decimalPoint()) {
                needsLocaleConversion = true;
            }
        }
        locale.setNumberOptions(QLocale::OmitGroupSeparator);

        if (needsLocaleConversion) {
            // we need to convert all numbers to the system's locale (for example 0.5 -> 0,5)
            QChar separator = QLocale().decimalPoint();
            QChar oldSeparator = locale.decimalPoint();
            QDomNodeList params = documentElement.elementsByTagName(QStringLiteral("parameter"));
            for (int j = 0; j < params.count(); ++j) {
                QDomNamedNodeMap attrs = params.at(j).attributes();
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
        }

        double version = -1;
        Mlt::Properties *metadata = repository->metadata(transition_type, id.toUtf8().data());
        if (metadata && metadata->is_valid()) {
            version = metadata->get_double("version");
        }
        delete metadata;

        if (documentElement.hasAttribute(QStringLiteral("version"))) {
            // a specific version of the filter is required
            if (locale.toDouble(documentElement.attribute(QStringLiteral("version"))) > version) {
                continue;
            }
        }
        if (version > -1) {
            // Add version info to XML
            QDomNode versionNode = doc.createElement(QStringLiteral("version"));
            versionNode.appendChild(doc.createTextNode(QLocale().toString(version)));
            documentElement.appendChild(versionNode);
        }
        addedTags << id;
    }
    transitionList->append(base);
}

