 /*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#ifndef PRODUCER_H
#define PRODUCER_H

#include "effectsystem/abstractparameterlist.h"

class ProducerDescription;
class ProducerPropertiesView;
class ProducerWrapper;

namespace Mlt
{
    class Producer;
}


/**
 * @class Producer
 * @brief Represents an producer in the producer system.
 * 
 * It handles the parameters and the connection to the MLT filter.
 */

class Producer : public AbstractParameterList
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an producer based on the given description.
     * @param producerDescription the description according to which the producer should be created
     * @param parent parent container of this producer
     * 
     * A new MLT filter gets created and handed over to the parent producer list to be attached to
     * the MLT DOM. Afterwards parameters are created.
     */
    Producer(Mlt::Producer *producer, ProducerDescription *producerDescription);
    virtual ~Producer();

    /**
     * @brief Sets the parameter value to the belonging MLT filter property.
     * @param name name/id of the parameter
     * @param value value to set
     * @see AbstractParameterList::setParameterValue
     */
    void setParameterValue(const QString &name, const QString &value);

    /**
     * @brief Receives and returns the parameter value from the belonging MLT filter property.
     * @param name name/id of the parameter
     * @see AbstractParameterList::parameterValue
     */
    QString parameterValue(const QString &name) const;

    /**
     * @brief Sets a MLT filter property.
     * @param name/id of the property
     * @param value value to set
     */
    virtual void setProperty(const QString &name, const QString &value);

    /**
     * @brief Returns a property value.
     * @param name name of the property
     */
    virtual QString property(const QString &name) const;

    /**
     * @brief Creates or destroys the properties view widget if necessary and informs the children.
     */
    void checkPropertiesViewState();

    /**
     * tbd
     */
    void checkTimelineViewState();

    /**
     * tbd
     */
    void checkMonitorViewState();
    
    void createView(const QString &id, QWidget *widget);
    
    ProducerWrapper *getProducer();

private slots:
    void setDisabled(bool disabled);
    void setPropertiesViewCollapsed(bool collapsed);

private:
    Mlt::Producer *m_producer;
    ProducerDescription *m_description;
    ProducerPropertiesView *m_view;
    void loadCurrentParameters();

signals:
    void updateClip();
    void editingDone();
};

#endif
