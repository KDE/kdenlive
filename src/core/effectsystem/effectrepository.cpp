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
#include "abstractparameterdescription.h"
#include "effectdescription.h"
#include "mltcore.h"
#include <mlt++/Mlt.h>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QDomElement>
#include <KStandardDirs>
#include <KUrl>
#include <KDebug>



EffectRepository::EffectRepository(MltCore *core) :
    m_core(core)
{
    initRepository();
}

EffectRepository::~EffectRepository()
{
    qDeleteAll(m_effects);
    //delete m_repository;
}

Mlt::Repository *EffectRepository::repository()
{
    return m_core->repository();
}

// TODO: comment on repository->filters()->get_name() vs. metadata identifier
void EffectRepository::initRepository()
{
    if (!m_core->repository()) {
        kWarning() << "MLT repository could not be loaded!!!";
        // TODO: error msg
        return;
    }
    QStringList availableFilters;
    MltCore::getNamesFromProperties(m_core->repository()->filters(), availableFilters);
    MltCore::applyBlacklist("blacklisted_effects.txt", availableFilters);

    QMultiHash<QString, QDomElement> availableEffectDescriptions;

    // load effect description XML from files
    QStringList effectDirectories = KGlobal::dirs()->findDirs("appdata", "effects");
    foreach(QString directoryName, effectDirectories) {
        QDir directory(directoryName);
        const QStringList fileList = directory.entryList(QStringList() << "*.xml", QDir::Files);
        foreach (const QString &filename, fileList) {
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
            effectDescription = new EffectDescription(filterName, this);
            if (effectDescription->isValid()) {
                m_effects.insert(effectDescription->getId(), effectDescription);
            } else {
                delete effectDescription;
            }
        }
    }
}

AbstractEffectRepositoryItem* EffectRepository::effectDescription(const QString &id)
{
    if (m_effects.contains(id)) {
        return m_effects.value(id);
    }
    return NULL;
}

AbstractParameterDescription* EffectRepository::newParameterDescription(const QString &type)
{
    return m_core->newParameterDescription(type);
}

QMap<QString, QString> EffectRepository::getEffectsList()
{
    QMap<QString, QString> effects;
    QMap<QString, AbstractEffectRepositoryItem*>::const_iterator i = m_effects.constBegin();
    while (i != m_effects.constEnd()) {
        EffectDescription *desc = static_cast<EffectDescription *>(i.value());
        effects.insert(desc->displayName(), i.key());
        ++i;
    }
    return effects;
}


