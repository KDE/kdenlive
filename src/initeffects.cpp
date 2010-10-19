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
#include "kdenlivesettings.h"
#include "effectslist.h"
#include "effectstackedit.h"
#include "mainwindow.h"

#include <KDebug>
#include <kglobal.h>
#include <KStandardDirs>

#include <QFile>
#include <qregexp.h>
#include <QDir>
#include <QIcon>

initEffectsThumbnailer::initEffectsThumbnailer() :
    QThread()
{
}

void initEffectsThumbnailer::prepareThumbnailsCall(const QStringList& list)
{
    m_list = list;
    start();
    kDebug() << "done";
}

void initEffectsThumbnailer::run()
{
    foreach(const QString & entry, m_list) {
        kDebug() << entry;
        if (!entry.isEmpty() && (entry.endsWith(".png") || entry.endsWith(".pgm"))) {
            if (!EffectStackEdit::iconCache.contains(entry)) {
                QImage pix(entry);
                //if (!pix.isNull())
                EffectStackEdit::iconCache[entry] = pix.scaled(30, 30);
                kDebug() << "stored";
            }
        }
    }
}

initEffectsThumbnailer initEffects::thumbnailer;

// static
void initEffects::refreshLumas()
{
    // Check for Kdenlive installed luma files, add empty string at start for no luma
    QStringList imagenamelist = QStringList() << i18n("None");
    QStringList imagefiles = QStringList() << QString();
    QStringList filters;
    filters << "*.pgm" << "*.png";

    QStringList customLumas = KGlobal::dirs()->findDirs("appdata", "lumas");
    foreach(const QString & folder, customLumas) {
        QStringList filesnames = QDir(folder).entryList(filters, QDir::Files);
        foreach(const QString & fname, filesnames) {
            imagenamelist.append(fname);
            imagefiles.append(KUrl(folder).path(KUrl::AddTrailingSlash) + fname);
        }
    }

    // Check for MLT lumas
    KUrl folder(mlt_environment("MLT_DATA"));
    folder.addPath("lumas");
    folder.addPath(mlt_environment("MLT_NORMALISATION"));
    QDir lumafolder(folder.path());
    QStringList filesnames = lumafolder.entryList(filters, QDir::Files);
    foreach(const QString & fname, filesnames) {
        imagenamelist.append(fname);
        KUrl path(folder);
        path.addPath(fname);
        imagefiles.append(path.toLocalFile());
    }
    QDomElement lumaTransition = MainWindow::transitions.getEffectByTag("luma", "luma");
    QDomNodeList params = lumaTransition.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute("tag") == "resource") {
            e.setAttribute("paramlistdisplay", imagenamelist.join(","));
            e.setAttribute("paramlist", imagefiles.join(","));
            break;
        }
    }

    QDomElement compositeTransition = MainWindow::transitions.getEffectByTag("composite", "composite");
    params = compositeTransition.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute("tag") == "luma") {
            e.setAttribute("paramlistdisplay", imagenamelist.join(","));
            e.setAttribute("paramlist", imagefiles.join(","));
            break;
        }
    }
}

// static
QDomDocument initEffects::getUsedCustomEffects(QMap <QString, QString> effectids)
{
    QMapIterator<QString, QString> i(effectids);
    int ix;
    QDomDocument doc;
    QDomElement list = doc.createElement("customeffects");
    doc.appendChild(list);
    while (i.hasNext()) {
        i.next();
        ix = MainWindow::customEffects.hasEffect(i.value(), i.key());
        if (ix > -1) {
            QDomElement e = MainWindow::customEffects.at(ix);
            list.appendChild(doc.importNode(e, true));
        }
    }
    return doc;
}

//static
Mlt::Repository *initEffects::parseEffectFiles()
{
    QStringList::Iterator more;
    QStringList::Iterator it;
    QStringList fileList;
    QString itemName;

    Mlt::Repository *repository = Mlt::Factory::init();
    if (!repository) {
        kDebug() << "Repository didn't finish initialisation" ;
        return NULL;
    }

    // Retrieve the list of MLT's available effects.
    Mlt::Properties *filters = repository->filters();
    QStringList filtersList;
    for (int i = 0; i < filters->count(); ++i)
        filtersList << filters->get_name(i);
    delete filters;

    // Retrieve the list of available producers.
    Mlt::Properties *producers = repository->producers();
    QStringList producersList;
    for (int i = 0; i < producers->count(); ++i)
        producersList << producers->get_name(i);
    KdenliveSettings::setProducerslist(producersList);
    delete producers;

    // Retrieve the list of available transitions.
    Mlt::Properties *transitions = repository->transitions();
    QStringList transitionsItemList;
    for (int i = 0; i < transitions->count(); ++i)
        transitionsItemList << transitions->get_name(i);
    delete transitions;

    // Remove blacklisted transitions from the list.
    QFile file(KStandardDirs::locate("appdata", "blacklisted_transitions.txt"));
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

    // Set the directories to look into for ladspa plugins.
    KGlobal::dirs()->addResourceType("ladspa_plugin", 0, "lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/usr/lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/usr/local/lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/opt/lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/opt/local/lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/usr/lib64/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/usr/local/lib64/ladspa");

    // Set the directories to look into for effects.
    QStringList direc = KGlobal::dirs()->findDirs("appdata", "effects");

    // Iterate through effects directories to parse all XML files.
    for (more = direc.begin(); more != direc.end(); ++more) {
        QDir directory(*more);
        QStringList filter;
        filter << "*.xml";
        fileList = directory.entryList(filter, QDir::Files);
        for (it = fileList.begin(); it != fileList.end(); ++it) {
            itemName = KUrl(*more + *it).path();
            parseEffectFile(&MainWindow::customEffects,
                            &MainWindow::audioEffects,
                            &MainWindow::videoEffects,
                            itemName, filtersList, producersList);
        }
    }

    // Remove blacklisted effects from the filters list.
    QFile file2(KStandardDirs::locate("appdata", "blacklisted_effects.txt"));
    if (file2.open(QIODevice::ReadOnly)) {
        QTextStream in(&file2);
        while (!in.atEnd()) {
            QString black = in.readLine().simplified();
            if (!black.isEmpty() && !black.startsWith('#') &&
                    filtersList.contains(black))
                filtersList.removeAll(black);
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
    for (int i = 0; i < MainWindow::transitions.count(); ++i) {
        effectInfo = MainWindow::transitions.at(i);
        effectsMap.insert(effectInfo.elementsByTagName("name").item(0).toElement().text().toLower().toUtf8().data(), effectInfo);
    }
    MainWindow::transitions.clearList();
    foreach(const QDomElement & effect, effectsMap)
    MainWindow::transitions.append(effect);
    effectsMap.clear();
    for (int i = 0; i < MainWindow::customEffects.count(); ++i) {
        effectInfo = MainWindow::customEffects.at(i);
        effectsMap.insert(effectInfo.elementsByTagName("name").item(0).toElement().text().toLower().toUtf8().data(), effectInfo);
    }
    MainWindow::customEffects.clearList();
    foreach(const QDomElement & effect, effectsMap)
    MainWindow::customEffects.append(effect);
    effectsMap.clear();
    for (int i = 0; i < MainWindow::audioEffects.count(); ++i) {
        effectInfo = MainWindow::audioEffects.at(i);
        effectsMap.insert(effectInfo.elementsByTagName("name").item(0).toElement().text().toLower().toUtf8().data(), effectInfo);
    }
    MainWindow::audioEffects.clearList();
    foreach(const QDomElement & effect, effectsMap)
    MainWindow::audioEffects.append(effect);
    effectsMap.clear();
    for (int i = 0; i < MainWindow::videoEffects.count(); ++i) {
        effectInfo = MainWindow::videoEffects.at(i);
        effectsMap.insert(effectInfo.elementsByTagName("name").item(0).toElement().text().toLower().toUtf8().data(), effectInfo);
    }
    // Add remaining filters to the list of video effects.
    foreach(const QString & filtername, filtersList) {
        QDomDocument doc = createDescriptionFromMlt(repository, "filters", filtername);
        if (!doc.isNull())
            effectsMap.insert(doc.documentElement().elementsByTagName("name").item(0).toElement().text().toLower().toUtf8().data(), doc.documentElement());
    }
    MainWindow::videoEffects.clearList();
    foreach(const QDomElement & effect, effectsMap)
    MainWindow::videoEffects.append(effect);

    return repository;
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
    QString path = KStandardDirs::locateLocal("appdata", "effects/", true);
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
    foreach(const QString & filename, fileList) {
        QString itemName = KUrl(path + filename).path();
        QFile file(itemName);
        doc.setContent(&file, false);
        file.close();
        effects = doc.elementsByTagName("effect");
        if (effects.count() != 1) {
            kDebug() << "More than one effect in file " << itemName << ", not supported yet";
        } else {
            e = effects.item(0).toElement();
            effectsMap.insert(e.elementsByTagName("name").item(0).toElement().text().toLower().toUtf8().data(), e);
        }
    }
    foreach(const QDomElement & effect, effectsMap)
    MainWindow::customEffects.append(effect);
}

// static
void initEffects::parseEffectFile(EffectsList *customEffectList, EffectsList *audioEffectList, EffectsList *videoEffectList, QString name, QStringList filtersList, QStringList producersList)
{
    QDomDocument doc;
    QFile file(name);
    doc.setContent(&file, false);
    file.close();
    QDomElement documentElement = doc.documentElement();
    QDomNodeList effects = doc.elementsByTagName("effect");

    if (effects.count() == 0) {
        kDebug() << "Effect broken: " << name;
        return;
    }

    /*QString groupName;
    if (doc.elementsByTagName("effectgroup").item(0).toElement().tagName() == "effectgroup")
        groupName = documentElement.attribute("name", QString());*/

    for (int i = 0; !effects.item(i).isNull(); ++i) {
        documentElement = effects.item(i).toElement();
        QString tag = documentElement.attribute("tag", QString());
        bool ladspaOk = true;
        if (tag == "ladspa") {
            QString library = documentElement.attribute("library", QString());
            if (KStandardDirs::locate("ladspa_plugin", library).isEmpty()) ladspaOk = false;
        }

        // Parse effect information.
        if ((filtersList.contains(tag) || producersList.contains(tag)) && ladspaOk) {
            QString type = documentElement.attribute("type", QString());
            if (type == "audio")
                audioEffectList->append(documentElement);
            else if (type == "custom")
                customEffectList->append(documentElement);
            else
                videoEffectList->append(documentElement);
        }

        /*
             QDomNode n = documentElement.firstChild();
         QString id, effectName, effectTag, paramType;
         int paramCount = 0;
         EFFECTTYPE type;

                // Create Effect
                EffectParamDescFactory effectDescParamFactory;
                EffectDesc *effect = NULL;

         // parse effect file
         QDomNode namenode = documentElement.elementsByTagName("name").item(0);
         if (!namenode.isNull()) effectName = i18n(namenode.toElement().text());
         if (!groupName.isEmpty()) effectName.prepend("_" + groupName + "_");

         QDomNode propsnode = documentElement.elementsByTagName("properties").item(0);
         if (!propsnode.isNull()) {
             QDomElement propselement = propsnode.toElement();
             id = propselement.attribute("id", QString());
             effectTag = propselement.attribute("tag", QString());
             if (propselement.attribute("type", QString()) == "audio") type = AUDIOEFFECT;
             else if (propselement.attribute("type", QString()) == "custom") type = CUSTOMEFFECT;
             else type = VIDEOEFFECT;
         }

         QString effectDescription;
         QDomNode descnode = documentElement.elementsByTagName("description").item(0);
         if (!descnode.isNull()) effectDescription = descnode.toElement().text() + "<br />";

         QString effectAuthor;
         QDomNode authnode = documentElement.elementsByTagName("author").item(0);
         if (!authnode.isNull()) effectAuthor = authnode.toElement().text() + "<br />";

         if (effectName.isEmpty() || id.isEmpty() || effectTag.isEmpty()) return;

         effect = new EffectDesc(effectName, id, effectTag, effectDescription, effectAuthor, type);

         QDomNodeList paramList = documentElement.elementsByTagName("parameter");
         if (paramList.count() == 0) {
             QDomElement fixed = doc.createElement("parameter");
             fixed.setAttribute("type", "fixed");
             effect->addParameter(effectDescParamFactory.createParameter(fixed));
         }
         else for (int i = 0; i < paramList.count(); i++) {
             QDomElement e = paramList.item(i).toElement();
             if (!e.isNull()) {
          paramCount++;
           QDomNamedNodeMap attrs = e.attributes();
          int i = 0;
          QString value;
          while (!attrs.item(i).isNull()) {
              QDomNode n = attrs.item(i);
              value = n.nodeValue();
              if (value.find("MAX_WIDTH") != -1)
           value.replace("MAX_WIDTH", QString::number(KdenliveSettings::defaultwidth()));
              if (value.find("MID_WIDTH") != -1)
           value.replace("MID_WIDTH", QString::number(KdenliveSettings::defaultwidth() / 2));
              if (value.find("MAX_HEIGHT") != -1)
           value.replace("MAX_HEIGHT", QString::number(KdenliveSettings::defaultheight()));
              if (value.find("MID_HEIGHT") != -1)
           value.replace("MID_HEIGHT", QString::number(KdenliveSettings::defaultheight() / 2));
              n.setNodeValue(value);
              i++;
          }
          effect->addParameter(effectDescParamFactory.createParameter(e));
             }
         }
                effectList->append(effect);
         }*/
    }
}

//static
const char* initEffects::ladspaEffectString(int ladspaId, QStringList params)
{
    if (ladspaId == 1433)  //Pitch
        return ladspaPitchEffectString(params);
    else if (ladspaId == 1216)  //Room Reverb
        return ladspaRoomReverbEffectString(params);
    else if (ladspaId == 1423)  //Reverb
        return ladspaReverbEffectString(params);
    else if (ladspaId == 1901)  //Reverb
        return ladspaEqualizerEffectString(params);
    else {
        kDebug() << "++++++++++  ASKING FOR UNKNOWN LADSPA EFFECT: " << ladspaId << endl;
        return "<jackrack></jackrack>";
    }
}

//static
void initEffects::ladspaEffectFile(const QString & fname, int ladspaId, QStringList params)
{
    const char *filterString;
    switch (ladspaId) {
    case 1433: //Pitch
        filterString = ladspaPitchEffectString(params);
        break;
    case 1905: //Vinyl
        filterString = ladspaVinylEffectString(params);
        break;
    case 1216 : //Room Reverb
        filterString = ladspaRoomReverbEffectString(params);
        break;
    case 1423: //Reverb
        filterString = ladspaReverbEffectString(params);
        break;
    case 1195: //Declipper
        filterString = ladspaDeclipEffectString(params);
        break;
    case 1901:  //Reverb
        filterString = ladspaEqualizerEffectString(params);
        break;
    case 1913: // Limiter
        filterString = ladspaLimiterEffectString(params);
        break;
    case 1193: // Pitch Shifter
        filterString = ladspaPitchShifterEffectString(params);
        break;
    case 1417: // Rate Scaler
        filterString = ladspaRateScalerEffectString(params);
        break;
    case 1217: // Phaser
        filterString = ladspaPhaserEffectString(params);
        break;
    case 1197: // 15 Band Equalizer
        filterString = ladspaEqualizer15EffectString(params);
        break;
    default:
        kDebug() << "++++++++++  ASKING FOR UNKNOWN LADSPA EFFECT: " << ladspaId << endl;
        return;
        break;
    }

    QFile f(fname);
    if (f.open(QIODevice::WriteOnly)) {
        QTextStream stream(&f);
        stream << filterString;
        f.close();
    } else kDebug() << "++++++++++  ERROR CANNOT WRITE TO: " << KdenliveSettings::currenttmpfolder() +  fname << endl;
    delete [] filterString;
}

const QString jackString = "<?xml version=\"1.0\"?><!DOCTYPE jackrack SYSTEM \"http://purge.bash.sh/~rah/jack_rack_1.2.dtd\"><jackrack><channels>2</channels><samplerate>48000</samplerate><plugin><id>";


const char* initEffects::ladspaDeclipEffectString(QStringList)
{
    return qstrdup(QString(jackString + "1195</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall></plugin></jackrack>").toUtf8());
}

/*
const char* initEffects::ladspaVocoderEffectString(QStringList params)
{
 return qstrdup( QString(jackString + "1441</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>0.000000</value><value>0.000000</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).arg(params[3]));
}*/

const char* initEffects::ladspaVinylEffectString(QStringList params)
{
    return qstrdup(QString(jackString + "1905</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><controlrow><value>%1</value></controlrow><controlrow><value>%2</value></controlrow><controlrow><value>%3</value></controlrow><controlrow><value>%4</value></controlrow><controlrow><value>%5</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).arg(params[3]).arg(params[4]).toUtf8());
}

const char* initEffects::ladspaPitchEffectString(QStringList params)
{
    return qstrdup(QString(jackString + "1433</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.0</value><value>1.0</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>4.000000</value><value>4.000000</value></controlrow></plugin></jackrack>").arg(params[0]).toUtf8());
}

const char* initEffects::ladspaRoomReverbEffectString(QStringList params)
{
    return qstrdup(QString(jackString + "1216</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>0.750000</value><value>0.750000</value></controlrow><controlrow><lock>true</lock><value>-70.000000</value><value>-70.000000</value></controlrow><controlrow><lock>true</lock><value>0.000000</value><value>0.000000</value></controlrow><controlrow><lock>true</lock><value>-17.500000</value><value>-17.500000</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).toUtf8());
}

const char* initEffects::ladspaReverbEffectString(QStringList params)
{
    return qstrdup(QString(jackString + "1423</id><enabled>true</enabled>  <wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked>    <wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values>    <lockall>true</lockall><controlrow><lock>true</lock><value>%1</value>      <value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>0.250000</value><value>0.250000</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).toUtf8());
}

const char* initEffects::ladspaEqualizerEffectString(QStringList params)
{
    return qstrdup(QString(jackString + "1901</id><enabled>true</enabled>    <wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked>    <wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><controlrow><value>%1</value></controlrow><controlrow><value>%2</value></controlrow>    <controlrow><value>%3</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).toUtf8());
}

const char* initEffects::ladspaLimiterEffectString(QStringList params)
{
    return qstrdup(QString(jackString + "1913</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><controlrow><value>%1</value></controlrow><controlrow><value>%2</value></controlrow><controlrow><value>%3</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).toUtf8());
}

const char* initEffects::ladspaPitchShifterEffectString(QStringList params)
{
    return qstrdup(QString(jackString + "1193</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow></plugin></jackrack>").arg(params[0]).toUtf8());
}

const char* initEffects::ladspaRateScalerEffectString(QStringList params)
{
    return qstrdup(QString(jackString + "1417</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow></plugin></jackrack>").arg(params[0]).toUtf8());
}

const char* initEffects::ladspaPhaserEffectString(QStringList params)
{
    return qstrdup(QString(jackString + "1217</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).arg(params[3]).toUtf8());
}
const char* initEffects::ladspaEqualizer15EffectString(QStringList params)
{
    return qstrdup(QString(jackString + "1197</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow><controlrow><lock>true</lock><value>%5</value><value>%5</value></controlrow><controlrow><lock>true</lock><value>%6</value><value>%6</value></controlrow><controlrow><lock>true</lock><value>%7</value><value>%7</value></controlrow><controlrow><lock>true</lock><value>%8</value><value>%8</value></controlrow><controlrow><lock>true</lock><value>%9</value><value>%9</value></controlrow><controlrow><lock>true</lock><value>%10</value><value>%10</value></controlrow><controlrow><lock>true</lock><value>%11</value><value>%11</value></controlrow><controlrow><lock>true</lock><value>%12</value><value>%12</value></controlrow><controlrow><lock>true</lock><value>%13</value><value>%13</value></controlrow><controlrow><lock>true</lock><value>%14</value><value>%14</value></controlrow><controlrow><lock>true</lock><value>%15</value><value>%15</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).arg(params[3]).arg(params[4]).arg(params[5]).arg(params[6]).arg(params[7]).arg(params[8]).arg(params[9]).arg(params[10]).arg(params[11]).arg(params[12]).arg(params[13]).arg(params[14]).toUtf8());
}


QDomDocument initEffects::createDescriptionFromMlt(Mlt::Repository* repository, const QString& /*type*/, const QString& filtername)
{

    QDomDocument ret;
    Mlt::Properties *metadata = repository->metadata(filter_type, filtername.toAscii().data());
    //kDebug() << filtername;
    if (metadata && metadata->is_valid()) {
        if (metadata->get("title") && metadata->get("identifier")) {
            QDomElement eff = ret.createElement("effect");
            eff.setAttribute("tag", metadata->get("identifier"));
            eff.setAttribute("id", metadata->get("identifier"));

            QDomElement name = ret.createElement("name");
            name.appendChild(ret.createTextNode(metadata->get("title")));

            QDomElement desc = ret.createElement("description");
            desc.appendChild(ret.createTextNode(metadata->get("description")));

            QDomElement author = ret.createElement("author");
            author.appendChild(ret.createTextNode(metadata->get("creator")));

            eff.appendChild(name);
            eff.appendChild(author);
            eff.appendChild(desc);

            Mlt::Properties param_props((mlt_properties) metadata->get_data("parameters"));
            for (int j = 0; param_props.is_valid() && j < param_props.count(); j++) {
                QDomElement params = ret.createElement("parameter");

                Mlt::Properties paramdesc((mlt_properties) param_props.get_data(param_props.get_name(j)));

                params.setAttribute("name", paramdesc.get("identifier"));

                if (paramdesc.get("maximum")) params.setAttribute("max", paramdesc.get("maximum"));
                if (paramdesc.get("minimum")) params.setAttribute("min", paramdesc.get("minimum"));
                if (QString(paramdesc.get("type")) == "integer")
                    params.setAttribute("type", "constant");
                if (QString(paramdesc.get("type")) == "float") {
                    params.setAttribute("type", "constant");
                    params.setAttribute("factor", "1000");
                    if (paramdesc.get("maximum")) params.setAttribute("max", QString(paramdesc.get("maximum")).toFloat() * 1000.0);
                    if (paramdesc.get("minimum")) params.setAttribute("min", QString(paramdesc.get("minimum")).toFloat() * 1000.0);
                }
                if (QString(paramdesc.get("type")) == "boolean")
                    params.setAttribute("type", "bool");
                if (!QString(paramdesc.get("format")).isEmpty() && QString(paramdesc.get("type")) != "geometry") {
                    params.setAttribute("type", "geometry");
                    params.setAttribute("format", paramdesc.get("format"));
                }
                if (!QString(paramdesc.get("format")).isEmpty() && QString(paramdesc.get("type")) == "geometry") {
                    params.setAttribute("type", "geometry");
                    //params.setAttribute("format", paramdesc.get("format"));
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

                eff.appendChild(params);
            }
            ret.appendChild(eff);
        }
    }
    delete metadata;
    metadata = 0;
    /* QString outstr;
     QTextStream str(&outstr);
     ret.save(str, 2);
     kDebug() << outstr;*/
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
    QStringList customLumas = KGlobal::dirs()->findDirs("appdata", "lumas");
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
    KUrl folder(mlt_environment("MLT_DATA"));
    folder.addPath("lumas");
    folder.addPath(mlt_environment("MLT_NORMALISATION"));
    QDir lumafolder(folder.path());
    QStringList filesnames = lumafolder.entryList(filters, QDir::Files);
    foreach(const QString & fname, filesnames) {
        imagenamelist.append(fname);
        KUrl path(folder);
        path.addPath(fname);
        imagefiles.append(path.toLocalFile());
    }

    foreach(const QString & name, names) {
        QDomDocument ret;
        QDomElement ktrans = ret.createElement("ktransition");
        ret.appendChild(ktrans);
        ktrans.setAttribute("tag", name);

        QDomElement tname = ret.createElement("name");
        QDomElement desc = ret.createElement("description");
        ktrans.appendChild(tname);
        ktrans.appendChild(desc);
        Mlt::Properties *metadata = repository->metadata(transition_type, name.toUtf8().data());
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
                    params.setAttribute("type", "constant");
                    params.setAttribute("factor", "100");
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
            delete metadata;
            metadata = 0;
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
                paramList.append(quickParameterFill(ret, i18n("Image File"), "resource", "list", "", "", "", imagefiles.join(","), imagenamelist.join(",")));
                paramList.append(quickParameterFill(ret, i18n("Reverse Transition"), "reverse", "bool", "0", "0", "1"));
                //thumbnailer.prepareThumbnailsCall(imagelist);
            } else if (name == "composite") {
                ktrans.setAttribute("id", name);
                tname.appendChild(ret.createTextNode(i18n("Composite")));
                desc.appendChild(ret.createTextNode(i18n("A key-framable alpha-channel compositor for two frames.")));
                paramList.append(quickParameterFill(ret, i18n("Geometry"), "geometry", "geometry", "0%,0%:100%x100%:100", "-500;-500;-500;-500;0", "500;500;500;500;100"));
                paramList.append(quickParameterFill(ret, i18n("Alpha Channel Operation"), "operator", "list", "over", "", "", "over,and,or,xor", "over,and,or,xor"));
                paramList.append(quickParameterFill(ret, i18n("Align"), "aligned", "bool", "1", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Fill"), "fill", "bool", "1", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Distort"), "distort", "bool", "0", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Wipe File"), "luma", "list", "", "", "", imagefiles.join(","), imagenamelist.join(",")));
                paramList.append(quickParameterFill(ret, i18n("Wipe Softness"), "softness", "double", "0", "0", "100", "", "", "100"));
                paramList.append(quickParameterFill(ret, i18n("Wipe Invert"), "luma_invert", "bool", "0", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Force Progressive Rendering"), "progressive", "bool", "1", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Force Deinterlace Overlay"), "deinterlace", "bool", "0", "0", "1"));
            } else if (name == "affine") {
                tname.appendChild(ret.createTextNode(i18n("Affine")));
                paramList.append(quickParameterFill(ret, i18n("Rotate Y"), "rotate_y", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Rotate X"), "rotate_x", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Rotate Z"), "rotate_z", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Fix Rotate Y"), "fix_rotate_y", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Fix Rotate X"), "fix_rotate_x", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Fix Rotate Z"), "fix_rotate_z", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Shear Y"), "shear_y", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Shear X"), "shear_x", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Shear Z"), "shear_z", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Fix Shear Y"), "fix_shear_y", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Fix Shear X"), "fix_shear_x", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Fix Shear Z"), "fix_shear_z", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, i18n("Geometry"), "geometry", "geometry",  "0,0,100%,100%,100%", "0,0,100%,100%,100%", "0,0,100%,100%,100%", "", "", "", "", "", "false"));
            } else if (name == "mix") {
                tname.appendChild(ret.createTextNode(i18n("Mix")));
            } else if (name == "region") {
                ktrans.setAttribute("id", name);
                tname.appendChild(ret.createTextNode(i18n("Region")));
                desc.appendChild(ret.createTextNode(i18n("Use alpha channel of another clip to create a transition.")));
                paramList.append(quickParameterFill(ret, i18n("Transparency clip"), "resource", "url", "", "", "", "", "", ""));
                paramList.append(quickParameterFill(ret, i18n("Geometry"), "composite.geometry", "geometry", "0%,0%:100%x100%:100", "-500;-500;-500;-500;0", "500;500;500;500;100"));
                paramList.append(quickParameterFill(ret, i18n("Alpha Channel Operation"), "composite.operator", "list", "over", "", "", "over,and,or,xor", "over,and,or,xor"));
                paramList.append(quickParameterFill(ret, i18n("Align"), "composite.aligned", "bool", "1", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Fill"), "composite.fill", "bool", "1", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Distort"), "composite.distort", "bool", "0", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Wipe File"), "composite.luma", "list", "", "", "", imagefiles.join(","), imagenamelist.join(",")));
                paramList.append(quickParameterFill(ret, i18n("Wipe Softness"), "composite.softness", "double", "0", "0", "100", "", "", "100"));
                paramList.append(quickParameterFill(ret, i18n("Wipe Invert"), "composite.luma_invert", "bool", "0", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Force Progressive Rendering"), "composite.progressive", "bool", "1", "0", "1"));
                paramList.append(quickParameterFill(ret, i18n("Force Deinterlace Overlay"), "composite.deinterlace", "bool", "0", "0", "1"));
            }
            foreach(const QDomElement & e, paramList)
            ktrans.appendChild(e);
        }

        // Add the transition to the global list.
        transitions->append(ret.documentElement());
        //kDebug() << ret.toString();
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

QDomElement initEffects::quickParameterFill(QDomDocument & doc, QString name, QString tag, QString type, QString def, QString min, QString max, QString list, QString listdisplaynames, QString factor, QString namedesc, QString format, QString opacity)
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
