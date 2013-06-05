/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/


#include "producer.h"
#include "project/producerwrapper.h"
#include "producerdescription.h"
#include "producerpropertiesview.h"
#include "effectsystem/multiviewhandler.h"
#include <mlt++/Mlt.h>
#include <QWidget>
#include <QVBoxLayout>
#include <QFrame>
#include <QTimer>
#include <KDebug>
#include <KPushButton>


Producer::Producer(Mlt::Producer *producer, ProducerDescription *producerDescription) :
    AbstractParameterList()
    , m_description(producerDescription)
    , m_view(NULL)
{
    m_producer = producer;
    //new Mlt::Producer(*profile, producerDescription->tag().toUtf8().constData());
    /*setProperty("kdenlive_id", m_description->getId());

    // TODO: do this properly
    parent->appendProducer(m_producer);*/
    Mlt::Properties props(m_producer->get_properties());
    createParameters(producerDescription->parameters(), props);
    loadCurrentParameters();
}

Producer::~Producer()
{
    delete m_view;
    // ?
    //delete m_producer;
}

void Producer::loadCurrentParameters()
{
    // load properties from current producer
    Mlt::Properties props(m_producer->get_properties());
    Iterator childIterator = begin();
    while(childIterator != end()) {
	QString value = props.get((*childIterator)->name().toUtf8().constData());
	//kDebug()<<"/ / /CHKING PARAM: "<<(*childIterator)->name()<<" = "<<value;
	if (!value.isEmpty())
	    (*childIterator)->set(value.toUtf8().constData());
        ++childIterator;
    }
}

void Producer::setParameterValue(const QString &name, const QString &value)
{
    setProperty(name, value);
}

QString Producer::parameterValue(const QString &name) const
{
    return property(name);
}

void Producer::setProperty(const QString &name, const QString &value)
{
    m_producer->set(name.toUtf8().constData(), value.toUtf8().constData());
    //kDebug()<<"// Changed producer prop: "<<name<<" = "<<value;
    emit updateClip();
    //updatePreview();
}

QString Producer::property(const QString &name) const
{
    return m_producer->get(name.toUtf8().constData());
}


ProducerWrapper *Producer::getProducer()
{
    return new ProducerWrapper(m_producer);
}

void Producer::createView(const QString &id, QWidget *widget)
{
    widget->setProperty("clipId", id);
    //widget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    m_view = new ProducerPropertiesView(m_description, widget);
    m_viewHandler->setView(MultiViewHandler::propertiesView, m_view);
    kDebug()<<" / // PRODUCER: "<<m_description->displayName()<<" has params: "<<count();
    Iterator childIterator = begin();
    while(childIterator != end()) {
        (*childIterator)->checkPropertiesViewState();
        ++childIterator;
    }
    m_view->finishLayout();
    connect(m_view->buttonDone(), SIGNAL(clicked()), this, SIGNAL(editingDone()));
}


void Producer::checkPropertiesViewState()
{
  /*
    bool exists = m_viewHandler->hasView(MultiViewHandler::propertiesView);
    bool shouldExist = m_viewHandler->parentView(MultiViewHandler::propertiesView) ? true : false;
    if (shouldExist != exists) {
        if (shouldExist) {
            ProducerPropertiesView *view = new ProducerPropertiesView(m_description->displayName(),
                                                                  m_description->description(),
                                                                  static_cast<QWidget *>(m_viewHandler->parentView(MultiViewHandler::propertiesView))
                                                                 );
            connect(view, SIGNAL(reset()), this, SIGNAL(reset()));
            connect(view, SIGNAL(disabled(bool)), this, SLOT(setDisabled(bool)));
            connect(view, SIGNAL(collapsed(bool)), this, SLOT(setPropertiesViewCollapsed(bool)));
            m_viewHandler->setView(MultiViewHandler::propertiesView, view);
            orderedChildViewUpdate(MultiViewHandler::propertiesView, begin(), end());
        } else {
            QObject *view = m_viewHandler->popView(MultiViewHandler::propertiesView);
            orderedChildViewUpdate(MultiViewHandler::propertiesView, begin(), end());
            delete view;
        }
    }
    */
}

void Producer::checkTimelineViewState()
{
}

void Producer::checkMonitorViewState()
{
}

void Producer::setDisabled(bool disabled)
{
    setProperty("disable", QString::number(disabled));
}

void Producer::setPropertiesViewCollapsed(bool collapsed)
{
    // ...
}


#include "producer.moc"

