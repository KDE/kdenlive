/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef EFFECTDESCRIPTION_H
#define EFFECTDESCRIPTION_H

#include "effectrepository.h"
#include "abstracteffectrepositoryitem.h"
#include <KService>

class QString;
class AbstractParameterDescription;
class Effect;
class AbstractEffectList;

/**
 * @class EffectDescription
 * @brief Stores information to create effect from.
 * 
 * The description is created either from a Kdenlive XML effect description or from MLT metadata.
 * It stores all necessary information and provides them to the actual effect that are also created
 * through their description object.
 */

class EffectDescription : public AbstractEffectRepositoryItem, protected QList<AbstractParameterDescription*>
{
public:
    /**
     * @brief Constructs a description from MLT metadata.
     * @param filterName name of the filter
     * @param mltRepository MLT repository containing the metadata
     * @param repository remove?
     */
    EffectDescription(const QString &filterName, EffectRepository *repository);

    /**
     * @brief Constructs a description from Kdenlive effect XML.
     * @param description "effect" element containing the description
     * @param version version of the effect
     * @param repository remove?
     */
    EffectDescription(QDomElement description, double version, EffectRepository *repository);
    virtual ~EffectDescription();

    /**
     * @brief Creates and returns a parameter based on this description.
     * @param parent the effect list that will contain the constructed effect
     */
    Effect *createEffect(AbstractEffectList *parent);

    /**
     * @brief Returns the parameter descriptions.
     */
    QList <AbstractParameterDescription *> parameters();

    /**
     * @brief Returns the effects tag, which is the MLT filter name/identifier.
     */
    QString tag() const;

    /**
     * @brief Returns a localized user visible name to be used in UI.
     */
    QString displayName() const;

    /**
     * @brief Returns a brief description of the effect.
     */
    QString description() const;

    /**
     * @brief Returns the authors of the effect.
     */
    QString authors() const;

    /**
     * @brief Returns the used filter version.
     */
    double version() const;

    /**
     * @brief Returns true if this effect can be added only once to a effect device; otherwise false.
     */
    bool isUnique() const;

private:
    QString getTextFromElement(QDomElement element);

    QString m_tag;
    QString m_name;
    QString m_nameOrig;
    QString m_description;
    QString m_descriptionOrig;
    QString m_authors;
    QString m_authorsOrig;
    double m_version;
    bool m_unique;
};

#endif
