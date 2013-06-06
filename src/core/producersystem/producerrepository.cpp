/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "producerrepository.h"
#include "effectsystem/abstractparameterdescription.h"
#include "producerdescription.h"
#include "effectsystem/mltcore.h"
#include <mlt++/Mlt.h>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QDomElement>
#include <KStandardDirs>
#include <KUrl>
#include <KDebug>


ProducerRepository::ProducerRepository(MltCore *core) :
    m_core(core)
{
    initRepository();
}

ProducerRepository::~ProducerRepository()
{
    qDeleteAll(m_producers);
}

Mlt::Repository *ProducerRepository::repository()
{
    return m_core->repository();
}

// TODO: comment on repository->filters()->get_name() vs. metadata identifier
void ProducerRepository::initRepository()
{
    if (!m_core->repository()) {
        kWarning() << "MLT repository could not be loaded!!!";
        // TODO: error msg
        return;
    }
    
    // Build producers list
    
    // Retrieve the list of available producers.
    QStringList availableProducers;
    MltCore::getNamesFromProperties(m_core->repository()->producers(), availableProducers);
    MltCore::applyBlacklist("blacklisted_producers.txt", availableProducers);
    
    
    /*ProducerDescription *producerDescription;
    foreach (QString producerName, availableProducers) {
    producerDescription = new ProducerDescription(producerName, this);
        if (producerDescription->isValid()) {
        m_producers.insert(producerDescription->tag(), producerDescription);
        } else {
        delete producerDescription;
        }
    }*/
    QMultiHash<QString, QDomElement> availableProducerDescriptions;

    // load effect description XML from files
    QStringList producerDirectories = KGlobal::dirs()->findDirs("appdata", "producers");
    foreach(QString directoryName, producerDirectories) {
        QDir directory(directoryName);
        QStringList fileList = directory.entryList(QStringList() << "*.xml", QDir::Files);
        foreach (QString filename, fileList) {
            QFile file(KUrl(directoryName + filename).path());
            QDomDocument document;
            document.setContent(&file, false);
            file.close();
            QDomNodeList producerDescriptions = document.elementsByTagName("producer");
            for (int i = 0; i < producerDescriptions.count(); ++i) {
                QDomElement producerDescriptionElement = producerDescriptions.at(i).toElement();
                QString producerId = producerDescriptionElement.attribute("tag");
                if (producerId == "ladspa") producerId.append("." + producerDescriptionElement.attribute("ladspaid"));
                kDebug()<<" / // FOUND PROD DESC: "<<producerId;
                availableProducerDescriptions.insert(producerId, producerDescriptionElement);
            }
        }
    }

    bool producerHasDescription;
    ProducerDescription *producerDescription;

    // TODO: do sth when there are multiple versions available
    double producerVersion = 0;
    QMultiHash<QString, QDomElement>::const_iterator producerDescriptionIterator;
    foreach (QString producerName, availableProducers) {
        producerHasDescription = false;

        // try to create the description from Kdenlive XML first
        kDebug()<<" / // CHKING PROD DESC: "<<producerName;
        if (availableProducerDescriptions.contains(producerName)) {
            producerDescriptionIterator = availableProducerDescriptions.constFind(producerName);

            // iterate over possibly multiple descriptions for the same mlt filter
            while (producerDescriptionIterator != availableProducerDescriptions.constEnd() && producerDescriptionIterator.key() == producerName) {
                producerDescription = new ProducerDescription(producerDescriptionIterator.value(), producerVersion, this);
                if (producerDescription->isValid()) {
                    if (m_producers.contains(producerDescription->tag())) {
                        // simply remove existing descriptions with the same kdenlive internal id
                        // TODO: write about different versions (needs proper handling in effectdescription first!
                        delete m_producers.take(producerDescription->tag());
                    }
                    m_producers.insert(producerDescription->tag(), producerDescription);
                    producerHasDescription = true;
                } else {
                    delete producerDescription;
                }
                ++producerDescriptionIterator;
            }
        }

        // if no description for the producer could be created from the Kdenlive XML create it from MLT metadata
        if (!producerHasDescription) {
            producerDescription = new ProducerDescription(producerName, this);
            if (producerDescription->isValid()) {
                m_producers.insert(producerDescription->tag(), producerDescription);
            } else {
                delete producerDescription;
            }
        }
    }
    
    
}

QList <QAction *> ProducerRepository::producerActions(QWidget *parent) const
{
    QList <QAction *> actions;
    QList <ProducerDescription *>prods = m_producers.values();
    for (int i = 0; i < prods.count(); ++i) {
        QAction *a = new QAction(prods.at(i)->displayName(), parent);
        a->setData(prods.at(i)->tag());
        actions.append(a);
    }
    return actions;
}


ProducerDescription* ProducerRepository::producerDescription(const QString &id)
{
    if (m_producers.contains(id)) {
        return m_producers.value(id);
    }
    return NULL;
}

const QString ProducerRepository::getProducerDisplayName(const QString &id) const
{
    if (m_producers.contains(id)) {
        return m_producers.value(id)->displayName();
    }
    return QString();
}

QStringList ProducerRepository::producerProperties(const QString &id)
{
    QStringList values;
    if (m_producers.contains(id)) {
        QList< AbstractParameterDescription* > params = m_producers.value(id)->parameters();
        for (int i = 0; i < params.count(); ++i) {
            values.append(params.at(i)->name());
        }
    }
    return values;
}

AbstractParameterDescription* ProducerRepository::newParameterDescription(const QString &type)
{
    return m_core->newParameterDescription(type);
}



