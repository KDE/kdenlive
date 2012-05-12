/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "effectrepository.h"
#include "abstracteffectrepositoryitem.h"
#include "effectdescription.h"
#include <mlt++/Mlt.h>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDomElement>
#include <KStandardDirs>
#include <KUrl>
#include <KDebug>
#include <KServiceTypeTrader>

#include <KPluginInfo>

#include "core/effectsystem/abstractparameterdescription.h"


EffectRepository::EffectRepository()
{
    initRepository();
}

EffectRepository::~EffectRepository()
{
    qDeleteAll(m_effects);
}

// TODO: comment on repository->filters()->get_name() vs. metadata identifier
void EffectRepository::initRepository()
{
    loadParameterPlugins();

    Mlt::Repository *repository = Mlt::Factory::init();
    if (!repository) {
        // TODO: error msg
        return;
    }

    QStringList availableFilters;
    getNamesFromProperties(repository->filters(), availableFilters);
    applyBlacklist("blacklisted_effects.txt", availableFilters);

    QMultiHash<QString, QDomElement> availableEffectDescriptions;

    // load effect description XML from files
    QStringList effectDirectories = KGlobal::dirs()->findDirs("appdata", "effects");
    foreach(QString directoryName, effectDirectories) {
        QDir directory(directoryName);
        QStringList fileList = directory.entryList(QStringList() << "*.xml", QDir::Files);
        foreach (QString filename, fileList) {
            QFile file(KUrl(directoryName + filename).path());
            QDomDocument document;
            document.setContent(&file, false);
            file.close();
            QDomNodeList effectDescriptions = document.elementsByTagName("effect");
            for (int i = 0; i < effectDescriptions.count(); ++i) {
                QDomElement effectDescriptionElement = effectDescriptions.at(i).toElement();
                availableEffectDescriptions.insert(effectDescriptionElement.attribute("tag"), effectDescriptionElement);
            }
        }
    }

    bool filterHasDescription;
    EffectDescription *effectDescription;

    // TODO: do sth when there are multiple versions available
    double filterVersion = 0;
    QMultiHash<QString, QDomElement>::const_iterator effectDescriptionIterator;
    foreach (QString filterName, availableFilters) {
        filterHasDescription = false;

        // try to create the description from Kdenlive XML first
        if (availableEffectDescriptions.contains(filterName)) {
            effectDescriptionIterator = availableEffectDescriptions.constFind(filterName);

            // iterate over possibly multiple descriptions for the same mlt filter
            while (effectDescriptionIterator != availableEffectDescriptions.constEnd() && effectDescriptionIterator.key() == filterName) {
                effectDescription = new EffectDescription(effectDescriptionIterator.value(), filterVersion, this);
                if (effectDescription->isValid()) {
                    if (m_effects.contains(effectDescription->getId())) {
                        // simply remove exsiting descriptions with the same kdenlive internal id
                        // TODO: write about different versions (needs proper handling in effectdescription first!
                        delete m_effects.take(effectDescription->getId());
                    }
                    m_effects.insert(effectDescription->getId(), effectDescription);
                    filterHasDescription = true;
                } else {
                    delete effectDescription;
                }
                ++effectDescriptionIterator;
            }
        }

        // if no description for the filter could be created from the Kdenlive XML create it from MLT metadata
        if (!filterHasDescription) {
            effectDescription = new EffectDescription(filterName, repository, this);
            if (effectDescription->isValid()) {
                m_effects.insert(effectDescription->getId(), effectDescription);
            } else {
                delete effectDescription;
            }
        }
    }

    if (repository) {
//         delete repository;
    }
}

AbstractParameterDescription* EffectRepository::newParameterDescription(QString type)
{
    if(m_parameterPlugins.contains(type)) {
        return m_parameterPlugins.value(type)->create<AbstractParameterDescription>();
    }
    return NULL;
}

AbstractEffectRepositoryItem* EffectRepository::effectDescription(QString id)
{
    if (m_effects.contains(id)) {
        return m_effects.value(id);
    }
    return NULL;
}



void EffectRepository::getNamesFromProperties(Mlt::Properties* properties, QStringList& names) const
{
    for (int i = 0; i < properties->count(); ++i) {
        names << properties->get_name(i);
    }
    // ?
    delete properties;
}

void EffectRepository::applyBlacklist(QString filename, QStringList& list) const
{
    QFile file(KStandardDirs::locate("appdata", filename));
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().simplified();
            if (!line.isEmpty() && !line.startsWith('#')) {
                list.removeAll(line);
            }
        }
        file.close();
    }
}

void EffectRepository::loadParameterPlugins()
{
    KService::List availableParameterServices = KServiceTypeTrader::self()->query("Kdenlive/Parameter");
    for (int i = 0; i < availableParameterServices.count(); ++i) {
        KPluginFactory *factory = KPluginLoader(availableParameterServices.at(i)->library()).factory();
        KPluginInfo info = KPluginInfo(availableParameterServices.at(i));
        if (factory && info.isValid()) {
            QStringList types = info.property("X-Kdenlive-ParameterType").toStringList();
            foreach(QString type, types) {
                m_parameterPlugins.insert(type, factory);
            }
        }
    }
}
