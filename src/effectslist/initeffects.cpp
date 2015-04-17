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

#include <QDebug>

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
    QStringList imagenamelist = QStringList() << i18n("None");
    QStringList imagefiles = QStringList() << QString();
    QStringList filters;
    filters << "*.pgm" << "*.png";

    QStringList customLumas = QStandardPaths::locateAll(QStandardPaths::DataLocation, "lumas");
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
    QDomElement lumaTransition = MainWindow::transitions.getEffectByTag("luma", "luma");
    QDomNodeList params = lumaTransition.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute("tag") == "resource") {
            e.setAttribute("paramlistdisplay", imagenamelist.join(","));
            e.setAttribute("paramlist", imagefiles.join(";"));
            break;
        }
    }

    QDomElement compositeTransition = MainWindow::transitions.getEffectByTag("composite", "composite");
    params = compositeTransition.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute("tag") == "luma") {
            e.setAttribute("paramlistdisplay", imagenamelist.join(","));
            e.setAttribute("paramlist", imagefiles.join(";"));
            break;
        }
    }
}

// static
QDomDocument initEffects::getUsedCustomEffects(const QMap <QString, QString>& effectids)
{
    QMapIterator<QString, QString> i(effectids);
    QDomDocument doc;
    QDomElement list = doc.createElement("customeffects");
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
void initEffects::parseEffectFiles(const QString &locale)
{
    QStringList::Iterator more;
    QStringList::Iterator it;
    QStringList fileList;
    QString itemName;

    Mlt::Repository *repository = Mlt::Factory::init();
    if (!repository) {
        //qDebug() << "Repository didn't finish initialisation" ;
        return;
    }

    // Warning: Mlt::Factory::init() resets the locale to the default system value, make sure we keep correct locale
    if (!locale.isEmpty()) setlocale(LC_NUMERIC, locale.toUtf8().constData());
    
    // Retrieve the list of MLT's available effects.
    Mlt::Properties *filters = repository->filters();
    QStringList filtersList;
    int max = filters->count();
    for (int i = 0; i < max; ++i)
        filtersList << filters->get_name(i);
    delete filters;

    // Retrieve the list of available producers.
    Mlt::Properties *producers = repository->producers();
    QStringList producersList;
    max = producers->count();
    for (int i = 0; i < max; ++i)
        producersList << producers->get_name(i);
    KdenliveSettings::setProducerslist(producersList);
    delete producers;

    // Retrieve the list of available transitions.
    Mlt::Properties *transitions = repository->transitions();
    QStringList transitionsItemList;
    max = transitions->count();
    for (int i = 0; i < max; ++i) {
        //qDebug()<<"TRANSITION "<<i<<" = "<<transitions->get_name(i);
        transitionsItemList << transitions->get_name(i);
    }
    delete transitions;

    // Remove blacklisted transitions from the list.
    QFile file(QStandardPaths::locate(QStandardPaths::DataLocation, "blacklisted_transitions.txt"));
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString black = in.readLine().simplified();
            if (!black.isEmpty() && !black.startsWith('#') &&
                    transitionsItemList.contains(black))
                transitionsItemList.removeAll(black);
        }
        file.close();
    }

    // Fill transitions list.
    fillTransitionsList(repository, &MainWindow::transitions, transitionsItemList);

    // Remove blacklisted effects from the filters list.
    QStringList mltFiltersList = filtersList;
    QStringList mltBlackList;
    QFile file2(QStandardPaths::locate(QStandardPaths::DataLocation, "blacklisted_effects.txt"));
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
        effectsMap.insert(effectInfo.firstChildElement("name").text().toLower().toUtf8().data(), effectInfo);
    }
    MainWindow::transitions.clearList();
    foreach(const QDomElement & effect, effectsMap)
        MainWindow::transitions.append(effect);
    effectsMap.clear();
    
    // Create structure holding all effects descriptions so that if an XML effect has no description, we take it from MLT
    QMap <QString, QString> effectDescriptions;
    foreach(const QString & filtername, mltBlackList) {
        QDomDocument doc = createDescriptionFromMlt(repository, "filters", filtername);
        if (!doc.isNull()) {
            if (doc.elementsByTagName("description").count() > 0) {
                QString desc = doc.documentElement().firstChildElement("description").text();
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
    foreach(const QString & filtername, mltFiltersList) {
        QDomDocument doc = createDescriptionFromMlt(repository, "filters", filtername);
        //WARNING: TEMPORARY FIX for empty MLT effects descriptions - disable effects without parameters - jbm 09-06-2011
        if (!doc.isNull() && doc.elementsByTagName("parameter").count() > 0) {
            if (doc.documentElement().attribute("type") == "audio") {
                if (doc.elementsByTagName("description").count() > 0) {
                    QString desc = doc.documentElement().firstChildElement("description").text();
                    //WARNING: TEMPORARY FIX for unusable MLT SOX parameters description
                    if (desc.startsWith(QLatin1String("Process audio using a SoX"))) {
                        // Remove MLT's SOX generated effects since the parameters properties are unusable for us
                    }
                    else {
			audioEffectsMap.insert(doc.documentElement().firstChildElement("name").text().toLower().toUtf8().data(), doc.documentElement());
		    }
                }
            }
            else
                videoEffectsMap.insert(doc.documentElement().firstChildElement("name").text().toLower().toUtf8().data(), doc.documentElement());
        }
    }

    // Set the directories to look into for effects.
    QStringList direc = QStandardPaths::locateAll(QStandardPaths::DataLocation, "effects", QStandardPaths::LocateDirectory);
    // Iterate through effects directories to parse all XML files.
    for (more = direc.begin(); more != direc.end(); ++more) {
        QDir directory(*more);
        QStringList filter;
        filter << "*.xml";
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
	if (effectInfo.tagName() == "effectgroup") {
	    effectsMap.insert(effectInfo.attribute("name").toUtf8().data(), effectInfo);
	}
        else effectsMap.insert(effectInfo.firstChildElement("name").text().toUtf8().data(), effectInfo);
    }
    MainWindow::customEffects.clearList();
    foreach(const QDomElement & effect, effectsMap)
        MainWindow::customEffects.append(effect);
    effectsMap.clear();

    // Create audio effects
    max = MainWindow::audioEffects.count();
    for (int i = 0; i < max; ++i) {
        effectInfo = MainWindow::audioEffects.at(i);
        audioEffectsMap.insert(effectInfo.firstChildElement("name").text().toLower().toUtf8().data(), effectInfo);
    }
    MainWindow::audioEffects.clearList();
    foreach(const QDomElement & effect, audioEffectsMap)
        MainWindow::audioEffects.append(effect);

    // Create video effects
    max = MainWindow::videoEffects.count();
    for (int i = 0; i < max; ++i) {
        effectInfo = MainWindow::videoEffects.at(i);
        videoEffectsMap.insert(effectInfo.firstChildElement("name").text().toLower().toUtf8().data(), effectInfo);
    }
    MainWindow::videoEffects.clearList();
    foreach(const QDomElement & effect, videoEffectsMap)
        MainWindow::videoEffects.append(effect);
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
    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/effects";
    QDir directory = QDir(path);
    QStringList filter;
    filter << "*.xml";
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
    foreach(const QString & filename, fileList) {
        QString itemName = directory.absoluteFilePath(filename);
        QFile file(itemName);
        doc.setContent(&file, false);
        file.close();
	QDomElement base = doc.documentElement();
        if (base.tagName() == "effectgroup") {
	    QString groupName = base.attribute("name");
	    if (groupName.isEmpty()) {
		groupName = i18n("Group %1", unknownGroupCount);
		base.setAttribute("name", groupName);
		unknownGroupCount++;
	    }
	    effectsMap.insert(groupName.toLower().toUtf8().data(), base);
        } else if (base.tagName() == "effect") {
            effectsMap.insert(base.firstChildElement("name").text().toLower().toUtf8().data(), base);
        }
        else qDebug() << "Unsupported effect file: " << itemName;
    }
    foreach(const QDomElement & effect, effectsMap)
        MainWindow::customEffects.append(effect);
}

// static
void initEffects::parseEffectFile(EffectsList *customEffectList, EffectsList *audioEffectList, EffectsList *videoEffectList, const QString &name, QStringList filtersList, QStringList producersList, Mlt::Repository *repository, QMap <QString, QString> effectDescriptions)
{
    QDomDocument doc;
    QFile file(name);
    doc.setContent(&file, false);
    file.close();
    QDomElement documentElement;
    QDomNodeList effects;
    QDomElement base = doc.documentElement();
    effects = doc.elementsByTagName("effect");

    if (effects.count() == 0) {
        qDebug() << "+++++++++++++\nEffect broken: " << name<<"\n+++++++++++";;
        return;
    }

    bool needsLocaleConversion = false;
    for (int i = 0; !effects.item(i).isNull(); ++i) {
        QLocale locale;
        documentElement = effects.item(i).toElement();
        QString tag = documentElement.attribute("tag", QString());
        
        //If XML has no description, take it fom MLT's descriptions
        if (effectDescriptions.contains(tag)) {
            QDomNodeList desc = documentElement.elementsByTagName("description");
            if (desc.isEmpty()) {
                QDomElement d = documentElement.ownerDocument().createElement("description");
                QDomText value = documentElement.ownerDocument().createTextNode(effectDescriptions.value(tag));
                d.appendChild(value);
                documentElement.appendChild(d);
            }
        }

        if (documentElement.hasAttribute("LC_NUMERIC")) {
            // set a locale for that file
            locale = QLocale(documentElement.attribute("LC_NUMERIC"));
            if (locale.decimalPoint() != QLocale().decimalPoint()) {
                needsLocaleConversion = true;
            }
        }
        locale.setNumberOptions(QLocale::OmitGroupSeparator);

        if (needsLocaleConversion) {
            // we need to convert all numbers to the system's locale (for example 0.5 -> 0,5)
            QChar separator = QLocale().decimalPoint();
            QChar oldSeparator = locale.decimalPoint();
            QDomNodeList params = documentElement.elementsByTagName("parameter");
            for (int j = 0; j < params.count(); ++j) {
                QDomNamedNodeMap attrs = params.at(j).attributes();
                for (int k = 0; k < attrs.count(); ++k) {
                    QString nodeName = attrs.item(k).nodeName();
                    if (nodeName != "type" && nodeName != "name") {
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
        if (metadata) delete metadata;
        if (documentElement.hasAttribute("version")) {
            // a specific version of the filter is required
            if (locale.toDouble(documentElement.attribute("version")) > version) {
                return;
            }
        }
        if (version > -1) {
            // Add version info to XML
            QDomNode versionNode = doc.createElement("version");
            versionNode.appendChild(doc.createTextNode(QLocale().toString(version)));
            documentElement.appendChild(versionNode);
        }

        // Parse effect information.
        if (base.tagName() != "effectgroup" && (filtersList.contains(tag) || producersList.contains(tag))) {
            QString type = documentElement.attribute("type", QString());
            if (type == "audio")
                audioEffectList->append(documentElement);
            else if (type == "custom") {
                customEffectList->append(documentElement);
            }
            else
                videoEffectList->append(documentElement);
        }
    }
    if (base.tagName() == "effectgroup") {
	QString type = base.attribute("type", QString());
        if (type == "audio")
            audioEffectList->append(base);
        else if (type == "custom")
	    customEffectList->append(base);
        else
	    videoEffectList->append(base);
    }
}

QDomDocument initEffects::createDescriptionFromMlt(Mlt::Repository* repository, const QString& /*type*/, const QString& filtername)
{

    QDomDocument ret;
    Mlt::Properties *metadata = repository->metadata(filter_type, filtername.toLatin1().data());
    ////qDebug() << filtername;
    if (metadata && metadata->is_valid()) {
        if (metadata->get("title") && metadata->get("identifier")) {
            QDomElement eff = ret.createElement("effect");
            QString id = metadata->get("identifier");
            eff.setAttribute("tag", id);
            eff.setAttribute("id", id);
            ////qDebug()<<"Effect: "<<id;

            QDomElement name = ret.createElement("name");
            name.appendChild(ret.createTextNode(metadata->get("title")));

            QDomElement desc = ret.createElement("description");
            desc.appendChild(ret.createTextNode(metadata->get("description")));

            QDomElement author = ret.createElement("author");
            author.appendChild(ret.createTextNode(metadata->get("creator")));

            QDomElement version = ret.createElement("version");
            version.appendChild(ret.createTextNode(metadata->get("version")));

            eff.appendChild(name);
            eff.appendChild(author);
            eff.appendChild(desc);
            eff.appendChild(version);

            Mlt::Properties tags((mlt_properties) metadata->get_data("tags"));
            if (QString(tags.get(0)) == "Audio") eff.setAttribute("type", "audio");
            /*for (int i = 0; i < tags.count(); ++i)
                //qDebug()<<tags.get_name(i)<<"="<<tags.get(i);*/

            Mlt::Properties param_props((mlt_properties) metadata->get_data("parameters"));
            for (int j = 0; param_props.is_valid() && j < param_props.count(); ++j) {
                QDomElement params = ret.createElement("parameter");

                Mlt::Properties paramdesc((mlt_properties) param_props.get_data(param_props.get_name(j)));
                params.setAttribute("name", paramdesc.get("identifier"));
		if (params.attribute("name") == "argument") {
		    // This parameter has to be given as attribute when using command line, do not show it in Kdenlive
		    continue;
		}

                if (paramdesc.get("maximum")) params.setAttribute("max", paramdesc.get("maximum"));
                if (paramdesc.get("minimum")) params.setAttribute("min", paramdesc.get("minimum"));

                QString paramType = paramdesc.get("type");
                
                if (paramType == "integer") {
		    if (params.attribute("min") == "0" && params.attribute("max") == "1")
			params.setAttribute("type", "bool");
                    else params.setAttribute("type", "constant");
		    
		}
                else if (paramType == "float") {
                    params.setAttribute("type", "constant");
                    // param type is float, set default decimals to 3
                    params.setAttribute("decimals", "3");
                }
                else if (paramType == "boolean")
                    params.setAttribute("type", "bool");
                else if (paramType == "geometry") {
                    params.setAttribute("type", "geometry");
                }
                else {
                    params.setAttribute("type", paramType);
                    if (!QString(paramdesc.get("format")).isEmpty()) params.setAttribute("format", paramdesc.get("format"));
                }
                if (paramdesc.get("default")) params.setAttribute("default", paramdesc.get("default"));
                if (paramdesc.get("value")) {
                    params.setAttribute("value", paramdesc.get("value"));
                } else {
                    params.setAttribute("value", paramdesc.get("default"));
                }

                QDomElement pname = ret.createElement("name");
                pname.appendChild(ret.createTextNode(paramdesc.get("title")));
                params.appendChild(pname);
		
		if (paramdesc.get("description")) {
		    QDomElement desc = ret.createElement("comment");
		    desc.appendChild(ret.createTextNode(paramdesc.get("description")));
		    params.appendChild(desc);
		}

                eff.appendChild(params);
            }
            ret.appendChild(eff);
        }
    }
    delete metadata;
    metadata = 0;
    /*QString outstr;
     QTextStream str(&outstr);
     ret.save(str, 2);
     //qDebug() << outstr;*/
    return ret;
}

void initEffects::fillTransitionsList(Mlt::Repository *repository, EffectsList *transitions, QStringList names)
{
    // Remove transitions that are not implemented.
    int pos = names.indexOf("mix");
    if (pos != -1)
        names.takeAt(pos);

    QStringList imagenamelist = QStringList() << i18n("None");
    QStringList imagefiles = QStringList() << QString();
    QStringList filters;
    filters << "*.pgm" << "*.png";
    QStringList customLumas = QStandardPaths::locateAll(QStandardPaths::DataLocation, "lumas");
    foreach(QString folder, customLumas) {
        if (!folder.endsWith('/'))
            folder.append('/');
        QStringList filesnames = QDir(folder).entryList(filters, QDir::Files);
        foreach(const QString & fname, filesnames) {
            imagenamelist.append(fname);
            imagefiles.append(folder + fname);
        }
    }

    // Check for MLT luma files.
    QUrl folder(QString(mlt_environment("MLT_DATA")) + QDir::separator() + "lumas" + QDir::separator() + QString(mlt_environment("MLT_NORMALISATION")));
    QDir lumafolder(folder.path());
    QStringList filesnames = lumafolder.entryList(filters, QDir::Files);
    foreach(const QString & fname, filesnames) {
        imagenamelist.append(fname);
        imagefiles.append(lumafolder.absoluteFilePath(fname));
    }
    
    //WARNING: this is a hack to get around temporary invalid metadata in MLT, 2nd of june 2011 JBM
    QStringList customTransitions;
    customTransitions << "composite" << "luma" << "affine" << "mix" << "region";

    foreach(const QString & name, names) {
        QDomDocument ret;
        QDomElement ktrans = ret.createElement("ktransition");
        ret.appendChild(ktrans);
        ktrans.setAttribute("tag", name);

        QDomElement tname = ret.createElement("name");
        QDomElement desc = ret.createElement("description");
        ktrans.appendChild(tname);
        ktrans.appendChild(desc);
        Mlt::Properties *metadata = NULL;
        if (!customTransitions.contains(name)) metadata = repository->metadata(transition_type, name.toUtf8().data());
        if (metadata && metadata->is_valid()) {
            // If possible, set name and description.
            if (metadata->get("title") && metadata->get("identifier"))
                tname.appendChild(ret.createTextNode(metadata->get("title")));
            desc.appendChild(ret.createTextNode(metadata->get("description")));

            Mlt::Properties param_props((mlt_properties) metadata->get_data("parameters"));
            for (int i = 0; param_props.is_valid() && i < param_props.count(); ++i) {
                QDomElement params = ret.createElement("parameter");

                Mlt::Properties paramdesc((mlt_properties) param_props.get_data(param_props.get_name(i)));

                params.setAttribute("name", paramdesc.get("identifier"));

                if (paramdesc.get("maximum"))
                    params.setAttribute("max", paramdesc.get("maximum"));
                if (paramdesc.get("minimum"))
                    params.setAttribute("min", paramdesc.get("minimum"));
                if (QString(paramdesc.get("type")) == "integer") {
                    if (params.attribute("min") == "0" && params.attribute("max") == "1")
                        params.setAttribute("type", "bool");
                    else {
                        params.setAttribute("type", "constant");
                        params.setAttribute("factor", "100");
                    }
                }
                if (QString(paramdesc.get("type")) == "boolean")
                    params.setAttribute("type", "bool");
                if (!QString(paramdesc.get("format")).isEmpty()) {
                    params.setAttribute("type", "complex");
                    params.setAttribute("format", paramdesc.get("format"));
                }
                if (paramdesc.get("default"))
                    params.setAttribute("default", paramdesc.get("default"));
                if (paramdesc.get("value"))
                    params.setAttribute("value", paramdesc.get("value"));
                else
                    params.setAttribute("value", paramdesc.get("default"));

                QDomElement pname = ret.createElement("name");
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
            QList<QDomElement> paramList;
            if (name == "luma") {
                ktrans.setAttribute("id", name);
                tname.appendChild(ret.createTextNode(i18n("Wipe")));
                desc.appendChild(ret.createTextNode(i18n("Applies a stationary transition between the current and next frames.")));

                paramList.append(quickParameterFill(ret, i18n("Softness"), "softness", "double", "0", "0", "100", "", "", "100"));
                paramList.append(quickParameterFill(ret, i18nc("@property: means that the image is inverted", "Invert"), "invert", "bool", "0", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Image File"), "resource", "list", "", "", "", imagefiles.join(";"), imagenamelist.join(",")));
                paramList.append(quickParameterFill(ret, i18n("Reverse Transition"), "reverse", "bool", "0", "0", "1"));
                //thumbnailer.prepareThumbnailsCall(imagelist);
            } else if (name == "composite") {
                ktrans.setAttribute("id", name);
                tname.appendChild(ret.createTextNode(i18n("Composite")));
                desc.appendChild(ret.createTextNode(i18n("A key-framable alpha-channel compositor for two frames.")));
                paramList.append(quickParameterFill(ret, i18n("Geometry"), "geometry", "geometry", "0%/0%:100%x100%:100", "-500;-500;-500;-500;0", "500;500;500;500;100"));
                paramList.append(quickParameterFill(ret, i18n("Alpha Channel Operation"), "operator", "list", "over", "", "", "over,and,or,xor", i18n("Over,And,Or,Xor")));
                paramList.append(quickParameterFill(ret, i18n("Align"), "aligned", "bool", "1", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Fill"), "fill", "bool", "1", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Distort"), "distort", "bool", "0", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Wipe File"), "luma", "list", "", "", "", imagefiles.join(";"), imagenamelist.join(",")));
                paramList.append(quickParameterFill(ret, i18n("Wipe Softness"), "softness", "double", "0", "0", "100", "", "", "100"));
                paramList.append(quickParameterFill(ret, i18n("Wipe Invert"), "luma_invert", "bool", "0", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Force Progressive Rendering"), "progressive", "bool", "1", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Force Deinterlace Overlay"), "deinterlace", "bool", "0", "0", "1"));
            } else if (name == "affine") {
                tname.appendChild(ret.createTextNode(i18n("Affine")));
                ret.documentElement().setAttribute("showrotation", "1");
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

                paramList.append(quickParameterFill(ret, "keyed", "keyed", "fixed", "1", "1", "1"));
                paramList.append(quickParameterFill(ret, i18n("Geometry"), "geometry", "geometry",  "0/0:100%x100%:100%", "0/0:100%x100%:100%", "0/0:100%x100%:100%", "", "", "", "", "", "true"));
                paramList.append(quickParameterFill(ret, i18n("Distort"), "distort", "bool", "0", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Rotate X"), "rotate_x", "addedgeometry", "0", "-1800", "1800", QString(), QString(), "10"));
                paramList.append(quickParameterFill(ret, i18n("Rotate Y"), "rotate_y", "addedgeometry", "0", "-1800", "1800", QString(), QString(), "10"));
                paramList.append(quickParameterFill(ret, i18n("Rotate Z"), "rotate_z", "addedgeometry", "0", "-1800", "1800", QString(), QString(), "10"));
                /*paramList.append(quickParameterFill(ret, i18n("Rotate Y"), "rotate_y", "simplekeyframe", "0", "-1800", "1800", QString(), QString(), "10"));
                paramList.append(quickParameterFill(ret, i18n("Rotate Z"), "rotate_z", "simplekeyframe", "0", "-1800", "1800", QString(), QString(), "10"));*/
                
                paramList.append(quickParameterFill(ret, i18n("Fix Shear Y"), "shear_y", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Fix Shear X"), "shear_x", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Fix Shear Z"), "shear_z", "double", "0", "0", "360"));
            } else if (name == "mix") {
                tname.appendChild(ret.createTextNode(i18n("Mix")));
            } else if (name == "region") {
                ktrans.setAttribute("id", name);
                tname.appendChild(ret.createTextNode(i18n("Region")));
                desc.appendChild(ret.createTextNode(i18n("Use alpha channel of another clip to create a transition.")));
                paramList.append(quickParameterFill(ret, i18n("Transparency clip"), "resource", "url", "", "", "", "", "", ""));
                paramList.append(quickParameterFill(ret, i18n("Geometry"), "composite.geometry", "geometry", "0%/0%:100%x100%:100", "-500;-500;-500;-500;0", "500;500;500;500;100"));
                paramList.append(quickParameterFill(ret, i18n("Alpha Channel Operation"), "composite.operator", "list", "over", "", "", "over,and,or,xor", i18n("Over,And,Or,Xor")));
                paramList.append(quickParameterFill(ret, i18n("Align"), "composite.aligned", "bool", "1", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Fill"), "composite.fill", "bool", "1", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Distort"), "composite.distort", "bool", "0", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Wipe File"), "composite.luma", "list", "", "", "", imagefiles.join(";"), imagenamelist.join(",")));
                paramList.append(quickParameterFill(ret, i18n("Wipe Softness"), "composite.softness", "double", "0", "0", "100", "", "", "100"));
                paramList.append(quickParameterFill(ret, i18n("Wipe Invert"), "composite.luma_invert", "bool", "0", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Force Progressive Rendering"), "composite.progressive", "bool", "1", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Force Deinterlace Overlay"), "composite.deinterlace", "bool", "0", "0", "1"));
            }
            foreach(const QDomElement & e, paramList)
            ktrans.appendChild(e);
        }
        delete metadata;
        metadata = 0;
        // Add the transition to the global list.
        transitions->append(ret.documentElement());
        ////qDebug() << ret.toString();
    }

    // Add some virtual transitions.
    QString slidetrans = "<ktransition tag=\"composite\" id=\"slide\"><name>" + i18n("Slide") + "</name><description>" + i18n("Slide image from one side to another.") + "</description><parameter tag=\"geometry\" type=\"wipe\" default=\"-100%,0%:100%x100%;-1=0%,0%:100%x100%\" name=\"geometry\"><name>" + i18n("Direction") + "</name>                                               </parameter><parameter tag=\"aligned\" default=\"0\" type=\"bool\" name=\"aligned\" ><name>" + i18n("Align") + "</name></parameter><parameter tag=\"progressive\" default=\"1\" type=\"bool\" name=\"progressive\" ><name>" + i18n("Force Progressive Rendering") + "</name></parameter><parameter tag=\"deinterlace\" default=\"0\" type=\"bool\" name=\"deinterlace\" ><name>" + i18n("Force Deinterlace Overlay") + "</name></parameter><parameter tag=\"invert\" default=\"0\" type=\"bool\" name=\"invert\" ><name>" + i18nc("@property: means that the image is inverted", "Invert") + "</name></parameter></ktransition>";
    QDomDocument ret;
    ret.setContent(slidetrans);
    transitions->append(ret.documentElement());

    QString dissolve = "<ktransition tag=\"luma\" id=\"dissolve\"><name>" + i18n("Dissolve") + "</name><description>" + i18n("Fade out one video while fading in the other video.") + "</description><parameter tag=\"reverse\" default=\"0\" type=\"bool\" name=\"reverse\" ><name>" + i18n("Reverse") + "</name></parameter></ktransition>";
    ret.setContent(dissolve);
    transitions->append(ret.documentElement());

    /*QString alphatrans = "<ktransition tag=\"composite\" id=\"alphatransparency\" ><name>" + i18n("Alpha Transparency") + "</name><description>" + i18n("Make alpha channel transparent.") + "</description><parameter tag=\"geometry\" type=\"fixed\" default=\"0%,0%:100%x100%\" name=\"geometry\"><name>" + i18n("Direction") + "</name></parameter><parameter tag=\"fill\" default=\"0\" type=\"bool\" name=\"fill\" ><name>" + i18n("Rescale") + "</name></parameter><parameter tag=\"aligned\" default=\"0\" type=\"bool\" name=\"aligned\" ><name>" + i18n("Align") + "</name></parameter></ktransition>";
    ret.setContent(alphatrans);
    transitions->append(ret.documentElement());*/
}

QDomElement initEffects::quickParameterFill(QDomDocument & doc, const QString &name, const QString &tag, const QString &type, const QString &def, const QString &min, const QString &max, const QString &list, const QString &listdisplaynames, const QString &factor, const QString &namedesc, const QString &format, const QString &opacity)
{
    QDomElement parameter = doc.createElement("parameter");
    parameter.setAttribute("tag", tag);
    parameter.setAttribute("default", def);
    parameter.setAttribute("type", type);
    parameter.setAttribute("name", tag);
    parameter.setAttribute("max", max);
    parameter.setAttribute("min", min);
    if (!list.isEmpty())
        parameter.setAttribute("paramlist", list);
    if (!listdisplaynames.isEmpty())
        parameter.setAttribute("paramlistdisplay", listdisplaynames);
    if (!factor.isEmpty())
        parameter.setAttribute("factor", factor);
    if (!namedesc.isEmpty())
        parameter.setAttribute("namedesc", namedesc);
    if (!format.isEmpty())
        parameter.setAttribute("format", format);
    if (!opacity.isEmpty())
        parameter.setAttribute("opacity", opacity);
    QDomElement pname = doc.createElement("name");
    pname.appendChild(doc.createTextNode(name));
    parameter.appendChild(pname);

    return parameter;
}



