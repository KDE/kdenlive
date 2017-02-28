/***************************************************************************
                          effectslist.cpp  -  description
                             -------------------
    begin                : Sat Aug 10 2002
    copyright            : (C) 2002 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "effectslist.h"
#include "kdenlivesettings.h"

#include <klocalizedstring.h>

EffectsList::EffectsList(bool indexRequired) : m_useIndex(indexRequired)
{
    m_baseElement = createElement(QStringLiteral("list"));
    appendChild(m_baseElement);
}

EffectsList::~EffectsList()
{
}

QDomElement EffectsList::getEffectByName(const QString &name) const
{
    QString effectName;
    QDomNodeList effects = m_baseElement.childNodes();
    for (int i = 0; i < effects.count(); ++i) {
        QDomElement effect =  effects.at(i).toElement();
        QDomElement namenode = effect.firstChildElement(QStringLiteral("name"));
        if (!namenode.isNull()) {
            effectName = i18n(namenode.text().toUtf8().data());
        }
        if (name == effectName) {
            QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
            for (int i = 0; i < params.count(); ++i) {
                QDomElement e = params.item(i).toElement();
                if (!e.hasAttribute(QStringLiteral("value")) && e.attribute(QStringLiteral("type")) != QLatin1String("animatedrect") && e.attribute(QStringLiteral("paramlist")) != QLatin1String("%lumaPaths")) {
                    e.setAttribute(QStringLiteral("value"), e.attribute(QStringLiteral("default")));
                }
            }
            return effect;
        }
    }

    return QDomElement();
}

QDomElement EffectsList::getEffectByTag(const QString &tag, const QString &id) const
{
    QDomNodeList effects = m_baseElement.childNodes();
    if (effects.isEmpty()) {
        return QDomElement();
    }
    for (int i = 0; i < effects.count(); ++i) {
        QDomElement effect =  effects.at(i).toElement();
        if (!id.isEmpty()) {
            if (effect.attribute(QStringLiteral("id")) == id) {
                if (effect.tagName() == QLatin1String("effectgroup")) {
                    // Effect group
                    QDomNodeList subeffects = effect.elementsByTagName(QStringLiteral("effect"));
                    for (int j = 0; j < subeffects.count(); ++j) {
                        QDomElement sub = subeffects.at(j).toElement();
                    }
                }
                return effect;
            }
        } else if (!tag.isEmpty()) {
            if (effect.attribute(QStringLiteral("tag")) == tag) {
                return effect;
            }
        }
    }
    return QDomElement();
}

QDomElement EffectsList::effectById(const QString &id) const
{
    QDomNodeList effects = m_baseElement.childNodes();
    for (int i = 0; i < effects.count(); ++i) {
        QDomElement effect =  effects.at(i).toElement();
        if (!id.isEmpty() && effect.attribute(QStringLiteral("id")) == id) {
            return effect;
        }
    }
    return QDomElement();
}

bool EffectsList::hasTransition(const QString &tag) const
{
    QDomNodeList trans = m_baseElement.childNodes();
    for (int i = 0; i < trans.count(); ++i) {
        QDomElement effect =  trans.at(i).toElement();
        if (effect.attribute(QStringLiteral("tag")) == tag) {
            return true;
        }
    }
    return false;
}

int EffectsList::hasEffect(const QString &tag, const QString &id) const
{
    QDomNodeList effects = m_baseElement.childNodes();
    for (int i = 0; i < effects.count(); ++i) {
        QDomElement effect =  effects.at(i).toElement();
        if (!id.isEmpty()) {
            if (effect.attribute(QStringLiteral("id")) == id) {
                return effect.attribute(QStringLiteral("kdenlive_ix")).toInt();
            }
        } else if (!tag.isEmpty() && effect.attribute(QStringLiteral("tag")) == tag) {
            return effect.attribute(QStringLiteral("kdenlive_ix")).toInt();
        }
    }
    return -1;
}

QStringList EffectsList::effectIdInfo(const int ix) const
{
    QDomElement effect = m_baseElement.childNodes().at(ix).toElement();
    return effectInfo(effect);
}

QStringList EffectsList::effectInfo(const QDomElement &effect) const
{
    QStringList info;
    if (effect.tagName() == QLatin1String("effectgroup")) {
        QString groupName = effect.attribute(QStringLiteral("name"));
        info << groupName << groupName << effect.attribute(QStringLiteral("id")) << QString::number(Kdenlive::groupEffect);
    } else {
        if (KdenliveSettings::gpu_accel()) {
            // Using Movit
            if (effect.attribute(QStringLiteral("context")) == QLatin1String("nomovit")) {
                // This effect has a Movit counterpart, so hide it when using Movit
                return info;
            }
        } else {
            // Not using Movit, don't display movit effects
            if (effect.attribute(QStringLiteral("tag")).startsWith(QLatin1String("movit."))) {
                return info;
            }
        }
        QDomElement namenode = effect.firstChildElement(QStringLiteral("name"));
        QString name = namenode.text();
        if (name.isEmpty()) {
            name = effect.attribute(QStringLiteral("tag"));
        }
        info << i18n(name.toUtf8().data()) << effect.attribute(QStringLiteral("tag")) << effect.attribute(QStringLiteral("id"));
    }
    return info;
}

QStringList EffectsList::effectNames() const
{
    QStringList list;
    QDomNodeList effects = m_baseElement.childNodes();
    for (int i = 0; i < effects.count(); ++i) {
        QDomElement effect =  effects.at(i).toElement();
        QDomElement namenode = effect.firstChildElement(QStringLiteral("name"));
        if (!namenode.isNull()) {
            list.append(i18n(namenode.text().toUtf8().data()));
        }
    }
    return list;
}

QString EffectsList::getInfo(const QString &tag, const QString &id) const
{
    return getEffectInfo(getEffectByTag(tag, id));
}

QString EffectsList::getInfoFromIndex(const int ix) const
{
    return getEffectInfo(m_baseElement.childNodes().at(ix).toElement());
}

QString EffectsList::getEffectInfo(const QDomElement &effect) const
{
    QString info;
    QDomElement namenode = effect.firstChildElement(QStringLiteral("description"));
    if (!namenode.isNull() && !namenode.firstChild().nodeValue().isEmpty()) {
        info = i18n(namenode.firstChild().nodeValue().simplified().toUtf8().data());
    }

    namenode = effect.firstChildElement(QStringLiteral("author"));
    if (!namenode.isNull() && !namenode.text().isEmpty()) {
        info.append(QStringLiteral("<br /><strong>") + i18n("Author:") + QStringLiteral(" </strong>") + i18n(namenode.text().toUtf8().data()));
    }

    namenode = effect.firstChildElement(QStringLiteral("version"));
    if (!namenode.isNull()) {
        info.append(QStringLiteral(" (v.%1)").arg(namenode.text()));
    }

    return info;
}

// static
bool EffectsList::hasKeyFrames(const QDomElement &effect)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("type")) == QLatin1String("keyframe")) {
            return true;
        }
    }
    return false;
}

void EffectsList::clone(const EffectsList &original)
{
    setContent(original.toString());
    m_baseElement = documentElement();
}

void EffectsList::clearList()
{
    while (!m_baseElement.firstChild().isNull()) {
        m_baseElement.removeChild(m_baseElement.firstChild());
    }
}

// static
void EffectsList::setParameter(QDomElement effect, const QString &name, const QString &value)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    bool found = false;
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("name")) == name) {
            e.setAttribute(QStringLiteral("value"), value);
            found = true;
            break;
        }
    }
    if (!found) {
        // create property
        QDomDocument doc = effect.ownerDocument();
        QDomElement e = doc.createElement(QStringLiteral("parameter"));
        e.setAttribute(QStringLiteral("name"), name);
        QDomText val = doc.createTextNode(value);
        e.appendChild(val);
        effect.appendChild(e);
    }
}

// static
QString EffectsList::parameter(const QDomElement &effect, const QString &name)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("name")) == name) {
            return e.attribute(QStringLiteral("value"));
        }
    }
    return QString();
}

// static
void EffectsList::setProperty(QDomElement effect, const QString &name, const QString &value)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("property"));
    // Update property if it already exists
    bool found = false;
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("name")) == name) {
            e.firstChild().setNodeValue(value);
            found = true;
            break;
        }
    }
    if (!found) {
        // create property
        QDomDocument doc = effect.ownerDocument();
        QDomElement e = doc.createElement(QStringLiteral("property"));
        e.setAttribute(QStringLiteral("name"), name);
        QDomText val = doc.createTextNode(value);
        e.appendChild(val);
        effect.appendChild(e);
    }
}

// static
void EffectsList::renameProperty(const QDomElement &effect, const QString &oldName, const QString &newName)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("property"));
    // Update property if it already exists
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("name")) == oldName) {
            e.setAttribute(QStringLiteral("name"), newName);
            break;
        }
    }
}

// static
QString EffectsList::property(const QDomElement &effect, const QString &name)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("property"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("name")) == name) {
            return e.firstChild().nodeValue();
        }
    }
    return QString();
}

// static
void EffectsList::removeProperty(QDomElement effect, const QString &name)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("property"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("name")) == name) {
            effect.removeChild(params.item(i));
            break;
        }
    }
}

// static
void EffectsList::removeMetaProperties(QDomElement producer)
{
    QDomNodeList params = producer.elementsByTagName(QStringLiteral("property"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("name")).startsWith(QLatin1String("meta"))) {
            producer.removeChild(params.item(i));
            --i;
        }
    }
}

QDomElement EffectsList::append(const QDomElement &e)
{
    QDomElement result;
    if (!e.isNull()) {
        result = m_baseElement.appendChild(importNode(e, true)).toElement();
        if (m_useIndex) {
            updateIndexes(m_baseElement.childNodes(), m_baseElement.childNodes().count() - 1);
        }
    }
    return result;
}

int EffectsList::count() const
{
    return m_baseElement.childNodes().count();
}

bool EffectsList::isEmpty() const
{
    return !m_baseElement.hasChildNodes();
}

const QDomElement EffectsList::at(int ix) const
{
    QDomNodeList effects = m_baseElement.childNodes();
    if (ix < 0 || ix >= effects.count()) {
        return QDomElement();
    }
    return effects.at(ix).toElement();
}

void EffectsList::removeAt(int ix)
{
    QDomNodeList effects = m_baseElement.childNodes();
    if (ix <= 0 || ix > effects.count()) {
        return;
    }
    m_baseElement.removeChild(effects.at(ix - 1));
    if (m_useIndex) {
        updateIndexes(effects, ix - 1);
    }
}

QDomElement EffectsList::itemFromIndex(int ix) const
{
    QDomNodeList effects = m_baseElement.childNodes();
    if (ix <= 0 || ix > effects.count()) {
        return QDomElement();
    }
    return effects.at(ix - 1).toElement();
}

QDomElement EffectsList::insert(const QDomElement &effect)
{
    QDomNodeList effects = m_baseElement.childNodes();
    int ix = effect.attribute(QStringLiteral("kdenlive_ix")).toInt();
    QDomElement result;
    if (ix <= 0 || ix > effects.count()) {
        ix = effects.count();
        result = m_baseElement.appendChild(importNode(effect, true)).toElement();
    } else {
        QDomElement listeffect =  effects.at(ix - 1).toElement();
        result = m_baseElement.insertBefore(importNode(effect, true), listeffect).toElement();
    }
    if (m_useIndex && ix > 0) {
        updateIndexes(effects, ix - 1);
    }
    return result;
}

void EffectsList::updateIndexes(const QDomNodeList &effects, int startIndex)
{
    for (int i = startIndex; i < effects.count(); ++i) {
        QDomElement listeffect =  effects.at(i).toElement();
        listeffect.setAttribute(QStringLiteral("kdenlive_ix"), i + 1);
    }
}

bool EffectsList::enableEffects(const QList<int> &indexes, bool disable)
{
    bool monitorRefresh = false;
    QDomNodeList effects = m_baseElement.childNodes();
    QDomElement effect;
    for (int i = 0; i < indexes.count(); ++i) {
        effect =  effectFromIndex(effects, indexes.at(i));
        effect.setAttribute(QStringLiteral("disable"), (int) disable);
        if (effect.attribute(QStringLiteral("type")) != QLatin1String("audio")) {
            monitorRefresh = true;
        }
    }
    return monitorRefresh;
}

QDomElement EffectsList::effectFromIndex(const QDomNodeList &effects, int ix)
{
    if (ix <= 0 || ix > effects.count()) {
        return QDomElement();
    }
    return effects.at(ix - 1).toElement();
}

void EffectsList::updateEffect(const QDomElement &effect)
{
    QDomNodeList effects = m_baseElement.childNodes();
    int ix = effect.attribute(QStringLiteral("kdenlive_ix")).toInt();
    QDomElement current = effectFromIndex(effects, ix);
    if (!current.isNull()) {
        m_baseElement.insertBefore(importNode(effect, true), current);
        m_baseElement.removeChild(current);
    } else {
        m_baseElement.appendChild(importNode(effect, true));
    }
}
