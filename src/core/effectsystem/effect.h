 /*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/
 
#ifndef EFFECT_H
#define EFFECT_H

#include "abstractparameterlist.h"

class EffectDescription;
class AbstractEffectList;
namespace Mlt
{
    class Filter;
}


/**
 * @class Effect
 * @brief Represents an effect in the effect system.
 * 
 * It handles the parameters and the connection to the MLT filter.
 */

class Effect : public AbstractParameterList
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an effect based on the given description.
     * @param effectDescription the description according to which the effect should be created
     * @param parent parent container of this effect
     * 
     * A new MLT filter gets created and handed over to the parent effect list to be attached to
     * the MLT DOM. Afterwards parameters are created.
     */
    Effect(EffectDescription *effectDescription, AbstractEffectList* parent);
    Effect(EffectDescription *effectDescription, Mlt::Filter *filter, AbstractEffectList* parent);
    virtual ~Effect();

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

private slots:
    void setDisabled(bool disabled);
    void setPropertiesViewCollapsed(bool collapsed);
    
private:
    Mlt::Filter *m_filter;
    AbstractEffectList* m_parent;
    EffectDescription *m_description;
};

#endif
