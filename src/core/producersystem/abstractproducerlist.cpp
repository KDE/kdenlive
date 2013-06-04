 /*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#include "abstractproducerlist.h"
#include "producerdescription.h"
#include "producerrepository.h"
#include "effectsystem/multiviewhandler.h"
#include "producer.h"


AbstractProducerList::AbstractProducerList(ProducerRepository *repository, AbstractProducerList */*parent*/) :
    m_repository(repository)
{
}

AbstractProducerList::~AbstractProducerList()
{
}

void AbstractProducerList::appendProducer(const QString& id)
{
    ProducerDescription *producer = m_repository->producerDescription(id);
    appendProducer(producer);
}

/*void AbstractProducerList::appendProducer(ProducerDescription* description)
{
    Producer *prod = description->createProducer(this);
    append(prod);
}*/

#include "abstractproducerlist.moc"
