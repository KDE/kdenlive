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


class EffectDescription : public AbstractEffectRepositoryItem, protected QList<AbstractParameterDescription*>
{
public:
    EffectDescription(const QString filterName, Mlt::Repository *mltRepository, EffectRepository *repository);
    EffectDescription(QDomElement description, double version, EffectRepository *repository);
    virtual ~EffectDescription();

    Effect *createEffect(AbstractEffectList *parent);

    QList <AbstractParameterDescription *> getParameters();
    QString getTag() const;
    QString getName() const;
    QString getDescription() const;
    QString getAuthors() const;
    double getVersion() const;
    bool getUniqueness() const;

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
