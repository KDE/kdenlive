/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef PRODUCERDESCRIPTION_H
#define PRODUCERDESCRIPTION_H

#include <KService>
#include "../effectsystem/effectrepository.h"

class QString;
class AbstractParameterDescription;
class Producer;
class ProducerRepository;
class QDomElement;

namespace Mlt 
{
    class Repository;
}

/**
 * @class ProducerDescription
 * @brief Stores information to create producer from.
 * 
 * The description is created either from a Kdenlive XML producer description or from MLT metadata.
 * It stores all necessary information and provides them to the actual producer that are also created
 * through their description object.
 */

class ProducerDescription : protected QList<AbstractParameterDescription*>
{
public:
    /**
     * @brief Constructs a description from MLT metadata.
     * @param producerName name of the producer
     * @param mltRepository MLT repository containing the metadata
     * @param repository remove?
     */
    ProducerDescription(const QString &producerName, ProducerRepository *repository);

    /**
     * @brief Constructs a description from Kdenlive producer XML.
     * @param description "producer" element containing the description
     * @param version version of the producer
     * @param repository remove?
     */
    ProducerDescription(const QDomElement &description, double version, ProducerRepository *repository);
    virtual ~ProducerDescription();

    /**
     * @brief Returns the parameter descriptions.
     */
    QList <AbstractParameterDescription *> parameters();

    /**
     * @brief Returns the producers tag, which is the MLT producer name/identifier.
     */
    QString tag() const;

    /**
     * @brief Returns a localized user visible name to be used in UI.
     */
    QString displayName() const;

    /**
     * @brief Returns a brief description of the producer.
     */
    QString description() const;

    /**
     * @brief Returns the authors of the producer.
     */
    QString authors() const;

    /**
     * @brief Returns the used producer version.
     */
    double version() const;

    /**
     * @brief Returns true if this producer can be added only once to a producer device; otherwise false.
     */
    bool isUnique() const;
    
    bool isValid() const;
    QString getId() const;
    int paramCount() const;
    void convertMltParameterType(Mlt::Properties &properties);

private:
    QString getTextFromElement(const QDomElement &element);

    QString m_tag;
    QString m_name;
    QString m_id;
    QString m_nameOrig;
    QString m_description;
    QString m_descriptionOrig;
    QString m_authors;
    QString m_authorsOrig;
    double m_version;
    bool m_unique;
    bool m_valid;
    EffectTypes m_type;
};

#endif
