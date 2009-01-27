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


#include <QFile>
#include <qregexp.h>
#include <QDir>
#include <QIcon>

#include <KDebug>
#include <kglobal.h>
#include <KStandardDirs>

#include "initeffects.h"
#include "kdenlivesettings.h"
#include "effectslist.h"
#include "effectstackedit.h"
#include "mainwindow.h"

initEffectsThumbnailer::initEffectsThumbnailer() {

}

void initEffectsThumbnailer::prepareThumbnailsCall(const QStringList& list) {
    m_list = list;
    start();
    kDebug() << "done";
}

void initEffectsThumbnailer::run() {
    foreach(const QString &entry, m_list) {
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

initEffects::initEffects() {

}

initEffects::~initEffects() {
}


// static
void initEffects::refreshLumas() {

    // Check for Kdenlive installed luma files
    QStringList imagenamelist;
    QStringList imagefiles;
    QStringList filters;
    filters << "*.pgm" << "*.png";

    QStringList customLumas = KGlobal::dirs()->findDirs("appdata", "lumas");
    foreach(const QString &folder, customLumas) {
        QStringList filesnames = QDir(folder).entryList(filters, QDir::Files);
        foreach(const QString &fname, filesnames) {
            imagenamelist.append(fname);
            imagefiles.append(folder + '/' + fname);
        }
    }

    // Check for MLT lumas
    QString folder = mlt_environment("MLT_DATA");
    folder.append("/lumas/").append(mlt_environment("MLT_NORMALISATION"));
    QDir lumafolder(folder);
    QStringList filesnames = lumafolder.entryList(filters, QDir::Files);
    foreach(const QString &fname, filesnames) {
        imagenamelist.append(fname);
        imagefiles.append(folder + '/' + fname);
    }
    QDomElement lumaTransition = MainWindow::transitions.getEffectByTag("luma", QString());
    QDomNodeList params = lumaTransition.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute("tag") == "resource") {
            e.setAttribute("paramlistdisplay", imagenamelist.join(","));
            e.setAttribute("paramlist", imagefiles.join(","));
            break;
        }
    }


}

//static
Mlt::Repository *initEffects::parseEffectFiles() {
    QStringList::Iterator more;
    QStringList::Iterator it;
    QStringList fileList;
    QString itemName;


    // Build effects. Retrieve the list of MLT's available effects first.
    Mlt::Repository *repository = Mlt::Factory::init();
    if (!repository) {
        kDebug() << "Repository did not finish init " ;
        return NULL;
    }
    Mlt::Properties *filters = repository->filters();
    QStringList filtersList;

    // Check for blacklisted effects
    QString blacklist = KStandardDirs::locate("appdata", "blacklisted_effects.txt");

    QFile file(blacklist);
    QStringList blackListed;

    if (file.open(QIODevice::ReadOnly)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString black = in.readLine().simplified();
            if (!black.isEmpty() && !black.startsWith('#')) blackListed.append(black);
        }
    }
    file.close();

    // Check for blacklisted transitions
    blacklist = KStandardDirs::locate("appdata", "blacklisted_transitions.txt");

    QFile file2(blacklist);
    QStringList blackListedtransitions;

    if (file2.open(QIODevice::ReadOnly)) {
        QTextStream in(&file2);
        while (!in.atEnd()) {
            QString black = in.readLine().simplified();
            if (!black.isEmpty() && !black.startsWith('#')) blackListedtransitions.append(black);
        }
    }
    file2.close();

    for (int i = 0 ; i < filters->count() ; i++) {
        filtersList << filters->get_name(i);
    }

    // Build effects. check producers first.

    Mlt::Properties *producers = repository->producers();
    QStringList producersList;
    for (int i = 0 ; i < producers->count() ; i++) {
        producersList << producers->get_name(i);
    }
    delete filters;
    delete producers;

    Mlt::Properties *transitions = repository->transitions();
    QStringList transitionsItemList;
    for (int i = 0 ; i < transitions->count() ; i++) {
        transitionsItemList << transitions->get_name(i);
    }
    delete transitions;

    foreach(const QString &trans, blackListedtransitions) {
        if (transitionsItemList.contains(trans)) transitionsItemList.removeAll(trans);
    }
    fillTransitionsList(repository, &MainWindow::transitions, transitionsItemList);

    KGlobal::dirs()->addResourceType("ladspa_plugin", 0, "lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/usr/lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/usr/local/lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/opt/lib/ladspa");
    KGlobal::dirs()->addResourceDir("ladspa_plugin", "/opt/local/lib/ladspa");

    kDebug() << "//  INIT EFFECT SEARCH" << endl;

    QStringList direc = KGlobal::dirs()->findDirs("data", "kdenlive/effects");

    QDir directory;
    for (more = direc.begin() ; more != direc.end() ; ++more) {
        directory = QDir(*more);
        QStringList filter;
        filter << "*.xml";
        fileList = directory.entryList(filter, QDir::Files);
        for (it = fileList.begin() ; it != fileList.end() ; ++it) {
            itemName = KUrl(*more + *it).path();
            parseEffectFile(&MainWindow::customEffects, &MainWindow::audioEffects, &MainWindow::videoEffects, itemName, filtersList, producersList);
            // kDebug()<<"//  FOUND EFFECT FILE: "<<itemName<<endl;
        }
    }

    foreach(const QString &effect, blackListed) {
        if (filtersList.contains(effect)) filtersList.removeAll(effect);
    }

    foreach(const QString &filtername, filtersList) {
        QDomDocument doc = createDescriptionFromMlt(repository, "filters", filtername);
        if (!doc.isNull())
            MainWindow::videoEffects.append(doc.documentElement());
    }
    return repository;
}

// static
void initEffects::parseCustomEffectsFile() {
    MainWindow::customEffects.clear();
    QString path = KStandardDirs::locateLocal("appdata", "effects/", true);
    QDir directory = QDir(path);
    QStringList filter;
    filter << "*.xml";
    const QStringList fileList = directory.entryList(filter, QDir::Files);
    QString itemName;
    foreach(const QString filename, fileList) {
        itemName = KUrl(path + filename).path();
        QDomDocument doc;
        QFile file(itemName);
        doc.setContent(&file, false);
        file.close();
        QDomNodeList effects = doc.elementsByTagName("effect");
        if (effects.count() != 1) {
            kDebug() << "More than one effect in file " << itemName << ", NOT SUPPORTED YET";
        } else {
            QDomElement e = effects.item(0).toElement();
            MainWindow::customEffects.append(e);
        }
    }
}

// static
void initEffects::parseEffectFile(EffectsList *customEffectList, EffectsList *audioEffectList, EffectsList *videoEffectList, QString name, QStringList filtersList, QStringList producersList) {
    QDomDocument doc;
    QFile file(name);
    doc.setContent(&file, false);
    file.close();
    QDomElement documentElement = doc.documentElement();
    QDomNodeList effects = doc.elementsByTagName("effect");

    if (effects.count() == 0) {
        kDebug() << "// EFFECT FILET: " << name << " IS BROKEN" << endl;
        return;
    }
    QString groupName;
    if (doc.elementsByTagName("effectgroup").item(0).toElement().tagName() == "effectgroup") {
        groupName = documentElement.attribute("name", QString::null);
    }

    int i = 0;

    while (!effects.item(i).isNull()) {
        documentElement = effects.item(i).toElement();
        QString tag = documentElement.attribute("tag", QString::null);
        bool ladspaOk = true;
        if (tag == "ladspa") {
            QString library = documentElement.attribute("library", QString::null);
            if (KStandardDirs::locate("ladspa_plugin", library).isEmpty()) ladspaOk = false;
        }

        // Parse effect file
        if ((filtersList.contains(tag) || producersList.contains(tag)) && ladspaOk) {
            bool isAudioEffect = false;
            QString type = documentElement.attribute("type", QString::null);
            if (type == "audio") audioEffectList->append(documentElement);
            else if (type == "custom") customEffectList->append(documentElement);
            else videoEffectList->append(documentElement);
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
             id = propselement.attribute("id", QString::null);
             effectTag = propselement.attribute("tag", QString::null);
             if (propselement.attribute("type", QString::null) == "audio") type = AUDIOEFFECT;
             else if (propselement.attribute("type", QString::null) == "custom") type = CUSTOMEFFECT;
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
        i++;
    }
}

//static
char* initEffects::ladspaEffectString(int ladspaId, QStringList params) {
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
        return (char *) "<jackrack></jackrack>";
    }
}

//static
void initEffects::ladspaEffectFile(const QString & fname, int ladspaId, QStringList params) {
    char *filterString;
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
    delete filterString;
}

const QString jackString = "<?xml version=\"1.0\"?><!DOCTYPE jackrack SYSTEM \"http://purge.bash.sh/~rah/jack_rack_1.2.dtd\"><jackrack><channels>2</channels><samplerate>48000</samplerate><plugin><id>";


char* initEffects::ladspaDeclipEffectString(QStringList) {
    return qstrdup(QString(jackString + "1195</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall></plugin></jackrack>").toUtf8());
}

/*
char* initEffects::ladspaVocoderEffectString(QStringList params)
{
 return qstrdup( QString(jackString + "1441</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>0.000000</value><value>0.000000</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).arg(params[3]));
}*/

char* initEffects::ladspaVinylEffectString(QStringList params) {
    return qstrdup(QString(jackString + "1905</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><controlrow><value>%1</value></controlrow><controlrow><value>%2</value></controlrow><controlrow><value>%3</value></controlrow><controlrow><value>%4</value></controlrow><controlrow><value>%5</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).arg(params[3]).arg(params[4]).toUtf8());
}

char* initEffects::ladspaPitchEffectString(QStringList params) {
    return qstrdup(QString(jackString + "1433</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.0</value><value>1.0</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>4.000000</value><value>4.000000</value></controlrow></plugin></jackrack>").arg(params[0]).toUtf8());
}

char* initEffects::ladspaRoomReverbEffectString(QStringList params) {
    return qstrdup(QString(jackString + "1216</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>0.750000</value><value>0.750000</value></controlrow><controlrow><lock>true</lock><value>-70.000000</value><value>-70.000000</value></controlrow><controlrow><lock>true</lock><value>0.000000</value><value>0.000000</value></controlrow><controlrow><lock>true</lock><value>-17.500000</value><value>-17.500000</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).toUtf8());
}

char* initEffects::ladspaReverbEffectString(QStringList params) {
    return qstrdup(QString(jackString + "1423</id><enabled>true</enabled>  <wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked>    <wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values>    <lockall>true</lockall><controlrow><lock>true</lock><value>%1</value>      <value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>0.250000</value><value>0.250000</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).toUtf8());
}

char* initEffects::ladspaEqualizerEffectString(QStringList params) {
    return qstrdup(QString(jackString + "1901</id><enabled>true</enabled>    <wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked>    <wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><controlrow><value>%1</value></controlrow><controlrow><value>%2</value></controlrow>    <controlrow><value>%3</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).toUtf8());
}

char* initEffects::ladspaLimiterEffectString(QStringList params) {
    return qstrdup(QString(jackString + "1913</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><controlrow><value>%1</value></controlrow><controlrow><value>%2</value></controlrow><controlrow><value>%3</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).toUtf8());
}

char* initEffects::ladspaPitchShifterEffectString(QStringList params) {
    return qstrdup(QString(jackString + "1193</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow></plugin></jackrack>").arg(params[0]).toUtf8());
}

char* initEffects::ladspaRateScalerEffectString(QStringList params) {
    return qstrdup(QString(jackString + "1417</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow></plugin></jackrack>").arg(params[0]).toUtf8());
}

char* initEffects::ladspaPhaserEffectString(QStringList params) {
    return qstrdup(QString(jackString + "1217</id><enabled>true</enabled><wet_dry_enabled>false</wet_dry_enabled><wet_dry_locked>true</wet_dry_locked><wet_dry_values><value>1.000000</value><value>1.000000</value></wet_dry_values><lockall>true</lockall><controlrow><lock>true</lock><value>%1</value><value>%1</value></controlrow><controlrow><lock>true</lock><value>%2</value><value>%2</value></controlrow><controlrow><lock>true</lock><value>%3</value><value>%3</value></controlrow><controlrow><lock>true</lock><value>%4</value><value>%4</value></controlrow></plugin></jackrack>").arg(params[0]).arg(params[1]).arg(params[2]).arg(params[3]).toUtf8());
}


QDomDocument initEffects::createDescriptionFromMlt(Mlt::Repository* repository, const QString& type, const QString& filtername) {

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
            for (int j = 0; param_props.is_valid() && j < param_props.count();j++) {
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
                    if (paramdesc.get("maximum")) params.setAttribute("max", QString(paramdesc.get("maximum")).toFloat()*1000.0);
                    if (paramdesc.get("minimum")) params.setAttribute("min", QString(paramdesc.get("minimum")).toFloat()*1000.0);
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
    /* QString outstr;
     QTextStream str(&outstr);
     ret.save(str, 2);
     kDebug() << outstr;*/
    return ret;
}

void initEffects::fillTransitionsList(Mlt::Repository * repository, EffectsList* transitions, QStringList names) {
    // remove transitions that are not implemented
    int pos = names.indexOf("mix");
    if (pos != -1) names.takeAt(pos);
    pos = names.indexOf("region");
    if (pos != -1) names.takeAt(pos);
    foreach(const QString &name, names) {
        QDomDocument ret;
        QDomElement ktrans = ret.createElement("ktransition");
        ret.appendChild(ktrans);

        ktrans.setAttribute("tag", name);
        QDomElement tname = ret.createElement("name");

        QDomElement desc = ret.createElement("description");

        QList<QDomElement> paramList;
        Mlt::Properties *metadata = repository->metadata(transition_type, name.toAscii().data());
        //kDebug() << filtername;
        if (metadata && metadata->is_valid()) {

            desc.appendChild(ret.createTextNode(metadata->get("description")));

            Mlt::Properties param_props((mlt_properties) metadata->get_data("parameters"));
            for (int j = 0; param_props.is_valid() && j < param_props.count();j++) {
                QDomElement params = ret.createElement("parameter");

                Mlt::Properties paramdesc((mlt_properties) param_props.get_data(param_props.get_name(j)));

                params.setAttribute("name", paramdesc.get("identifier"));

                if (paramdesc.get("maximum")) params.setAttribute("max", paramdesc.get("maximum"));
                if (paramdesc.get("minimum")) params.setAttribute("min", paramdesc.get("minimum"));
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
                if (paramdesc.get("default")) params.setAttribute("default", paramdesc.get("default"));
                if (paramdesc.get("value")) {
                    params.setAttribute("value", paramdesc.get("value"));
                } else {
                    params.setAttribute("value", paramdesc.get("default"));
                }


                QDomElement pname = ret.createElement("name");
                pname.appendChild(ret.createTextNode(paramdesc.get("title")));
                params.appendChild(pname);

                ktrans.appendChild(params);
            }

            ret.appendChild(ktrans);
            if (metadata->get("title") && metadata->get("identifier")) {
                ktrans.setAttribute("tag", name);
                QDomElement tname = ret.createElement("name");
                tname.appendChild(ret.createTextNode(metadata->get("title")));
                ktrans.appendChild(tname);
            }

            //kDebug() << ret.toString();
        } else {
            if (name == "luma") {

                tname.appendChild(ret.createTextNode("Luma"));
                desc.appendChild(ret.createTextNode("Applies a luma transition between the current and next frames"));

                // Check for Kdenlive installed luma files
                QStringList imagenamelist;
                QStringList imagefiles;
                QStringList filters;
                filters << "*.pgm" << "*.png";

                QStringList customLumas = KGlobal::dirs()->findDirs("appdata", "lumas");
                foreach(const QString &folder, customLumas) {
                    QStringList filesnames = QDir(folder).entryList(filters, QDir::Files);
                    foreach(const QString &fname, filesnames) {
                        imagenamelist.append(fname);
                        imagefiles.append(folder + '/' + fname);
                    }
                }

                // Check for MLT lumas
                QString folder = mlt_environment("MLT_DATA");
                folder.append("/lumas/").append(mlt_environment("MLT_NORMALISATION"));
                QDir lumafolder(folder);
                QStringList filesnames = lumafolder.entryList(filters, QDir::Files);
                foreach(const QString &fname, filesnames) {
                    imagenamelist.append(fname);
                    imagefiles.append(folder + '/' + fname);
                }

                paramList.append(quickParameterFill(ret, "Softness", "softness", "double", "0", "0", "100", "", "", "100"));
                paramList.append(quickParameterFill(ret, "Invert", "invert", "bool", "0", "0", "1"));
                paramList.append(quickParameterFill(ret, "ImageFile", "resource", "list", "", "", "", imagefiles.join(","), imagenamelist.join(",")));
                paramList.append(quickParameterFill(ret, "Reverse Transition", "reverse", "bool", "0", "0", "1"));
                //thumbnailer.prepareThumbnailsCall(imagelist);

            } else if (name == "composite") {
                desc.appendChild(ret.createTextNode("A key-framable alpha-channel compositor for two frames."));
                paramList.append(quickParameterFill(ret, "Geometry", "geometry", "geometry", "0%,0%:100%x100%:100", "-500;-500;-500;-500;0", "500;500;500;500;100"));
                paramList.append(quickParameterFill(ret, "Distort", "distort", "bool", "1", "1", "1"));
                tname.appendChild(ret.createTextNode("Composite"));
                ktrans.setAttribute("id", "composite");
                /*QDomDocument ret1;
                QDomElement ktrans1 = ret1.createElement("ktransition");
                ret1.appendChild(ktrans1);
                ktrans1.setAttribute("tag", name);
                QDomElement tname1 = ret.createElement("name");
                tname1.appendChild(ret1.createTextNode("PIP"));*/

            } else if (name == "mix") {
                tname.appendChild(ret.createTextNode("Mix"));
            } else if (name == "affine") {
                tname.appendChild(ret.createTextNode("Affine"));
                paramList.append(quickParameterFill(ret, "Rotate Y", "rotate_y", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, "Rotate X", "rotate_x", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, "Rotate Z", "rotate_z", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, "Fix Rotate Y", "fix_rotate_y", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, "Fix Rotate X", "fix_rotate_x", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, "Fix Rotate Z", "fix_rotate_z", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, "Shear Y", "shear_y", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, "Shear X", "shear_x", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, "Shear Z", "shear_z", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, "Fix Shear Y", "fix_shear_y", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, "Fix Shear X", "fix_shear_x", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, "Fix Shear Z", "fix_shear_z", "double", "0", "0", "360"));
                paramList.append(quickParameterFill(ret, "Mirror", "mirror_off", "bool", "0", "0", "1"));
                paramList.append(quickParameterFill(ret, "Repeat", "repeat_off", "bool", "0", "0", "1"));
                paramList.append(quickParameterFill(ret, "Geometry", "geometry", "geometry",  "0;0;100;100;100", "0;0;100;100;100", "0;0;100;100;100"));
                tname.appendChild(ret.createTextNode("Composite"));
            } else if (name == "region") {
                tname.appendChild(ret.createTextNode("Region"));
            }


        }

        ktrans.appendChild(tname);
        ktrans.appendChild(desc);

        foreach(const QDomElement &e, paramList) {
            ktrans.appendChild(e);
        }


        transitions->append(ret.documentElement());
        //kDebug() << "//// ////  TRANSITON XML";
        // kDebug() << ret.toString();
        /*

         <transition fill="1" in="11" a_track="1" out="73" mlt_service="luma" b_track="2" softness="0" resource="/home/marco/Projekte/kdenlive/install_cmake/share/apps/kdenlive/pgm/PAL/square2.pgm" />
        */
    }

    QString wipetrans = "<ktransition tag=\"composite\" id=\"wipe\"><name>Wipe</name><description>Slide image from one side to another</description><parameter tag=\"geometry\" type=\"wipe\" default=\"-100%,0%:100%x100%;-1=0%,0%:100%x100%\" name=\"geometry\"><name>Direction</name>                                               </parameter><parameter tag=\"aligned\" default=\"0\" type=\"bool\" name=\"aligned\" ><name>Align</name></parameter></ktransition>";
    QDomDocument ret;
    ret.setContent(wipetrans);
    transitions->append(ret.documentElement());

    QString alphatrans = "<ktransition tag=\"composite\" id=\"alphatransparency\" ><name>Alpha transparency</name><description>Make alpha channel transparent</description><parameter tag=\"geometry\" type=\"fixed\" default=\"0%,0%:100%x100%\" name=\"geometry\"><name>Direction</name></parameter><parameter tag=\"fill\" default=\"0\" type=\"bool\" name=\"fill\" ><name>Rescale</name></parameter><parameter tag=\"aligned\" default=\"0\" type=\"bool\" name=\"aligned\" ><name>Align</name></parameter></ktransition>";
    QDomDocument ret2;
    ret2.setContent(alphatrans);
    transitions->append(ret2.documentElement());
}

QDomElement initEffects::quickParameterFill(QDomDocument & doc, QString name, QString tag, QString type, QString def, QString min, QString max, QString list, QString listdisplaynames, QString factor, QString namedesc, QString format) {
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
    QDomElement pname = doc.createElement("name");
    pname.appendChild(doc.createTextNode(name));
    parameter.appendChild(pname);

    return parameter;
}
