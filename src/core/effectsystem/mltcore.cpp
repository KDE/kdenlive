/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "mltcore.h"
#include "abstractparameterdescription.h"
#include <mlt++/Mlt.h>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QGLFormat>
#include <QDomElement>
#include <KStandardDirs>
#include <KUrl>
#include <KDebug>
#include <KServiceTypeTrader>
#include <KPluginInfo>


MltCore::MltCore()
{
    initRepository();
}

MltCore::~MltCore()
{
    delete m_repository;
}

Mlt::Repository *MltCore::repository()
{
    return m_repository;
}

void MltCore::checkConsumers()
{
  // Check what is available on the system
    Mlt::Properties *consumers = m_repository->consumers();
    int max = consumers->count();
    for (int i = 0; i < max; ++i) {
	QString cons = consumers->get_name(i);
        if (cons == "sdl_preview") {
	    m_availableDisplayModes << MLTSDL;
	    break;
	}
    }
    delete consumers;
    
    if (QGLFormat::hasOpenGL()) {
	m_availableDisplayModes << MLTOPENGL;
	m_availableDisplayModes << MLTSCENE;
	consumers = m_repository->filters();
	max = consumers->count();
	for (int i = 0; i < max; ++i) {
	    QString cons = consumers->get_name(i);
	    if (cons == "glsl.manager") {
		m_availableDisplayModes << MLTGLSL;
		break;
	    }
	}
	delete consumers;
    }
}

QList <DISPLAYMODE> MltCore::availableDisplayModes() const
{
    return m_availableDisplayModes;
}

// TODO: comment on repository->filters()->get_name() vs. metadata identifier
void MltCore::initRepository()
{
    loadParameterPlugins();
    m_repository = Mlt::Factory::init();
    if (!m_repository) {
        kWarning() << "MLT repository could not be loaded!!!";
        // TODO: error msg
        return;
    }
    checkConsumers();
}

AbstractParameterDescription* MltCore::newParameterDescription(const QString &type)
{
    if(m_parameterPlugins.contains(type)) {
        return m_parameterPlugins.value(type)->create<AbstractParameterDescription>();
    }
    return NULL;
}


void MltCore::getNamesFromProperties(Mlt::Properties* properties, QStringList& names)
{
    for (int i = 0; i < properties->count(); ++i) {
        names << QString(properties->get_name(i)).simplified();
    }
    // ?
    delete properties;
}

void MltCore::applyBlacklist(const QString &filename, QStringList& list)
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

void MltCore::loadParameterPlugins()
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

const QHash <QString, KPluginFactory*> MltCore::parameterPlugins() const
{
    return m_parameterPlugins;
}

