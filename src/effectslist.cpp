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

#include <KDebug>
#include <KLocale>


EffectsList::EffectsList()
{
    m_baseElement = createElement("list");
    appendChild(m_baseElement);
}

EffectsList::~EffectsList()
{
}

QDomElement EffectsList::getEffectByName(const QString & name) const
{
    QString effectName;
    QDomNodeList effects = m_baseElement.childNodes();
    for (int i = 0; i < effects.count(); i++) {
        QDomElement effect =  effects.at(i).toElement();
        QDomNode namenode = effect.elementsByTagName("name").item(0);
        if (!namenode.isNull()) effectName = i18n(namenode.toElement().text().toUtf8().data());
        if (name == effectName) {
            QDomNodeList params = effect.elementsByTagName("parameter");
            for (int i = 0; i < params.count(); i++) {
                QDomElement e = params.item(i).toElement();
                if (!e.hasAttribute("value"))
                    e.setAttribute("value", e.attribute("default"));
            }
            return effect;
        }
    }

    return QDomElement();
}

QDomElement EffectsList::getEffectByTag(const QString & tag, const QString & id) const
{
    QDomNodeList effects = m_baseElement.childNodes();
    for (int i = 0; i < effects.count(); i++) {
        QDomElement effect =  effects.at(i).toElement();
        if (!id.isEmpty()) {
            if (effect.attribute("id") == id) {
                QDomNodeList params = effect.elementsByTagName("parameter");
                for (int i = 0; i < params.count(); i++) {
                    QDomElement e = params.item(i).toElement();
                    if (!e.hasAttribute("value"))
                        e.setAttribute("value", e.attribute("default"));
                }
                return effect;
            }
        } else if (!tag.isEmpty()) {
            if (effect.attribute("tag") == tag) {
                QDomNodeList params = effect.elementsByTagName("parameter");
                for (int i = 0; i < params.count(); i++) {
                    QDomElement e = params.item(i).toElement();
                    if (!e.hasAttribute("value"))
                        e.setAttribute("value", e.attribute("default"));
                }
                return effect;
            }
        }
    }
    return QDomElement();
}

int EffectsList::hasEffect(const QString & tag, const QString & id) const
{
    QDomNodeList effects = m_baseElement.childNodes();
    for (int i = 0; i < effects.count(); i++) {
        QDomElement effect =  effects.at(i).toElement();
        if (!id.isEmpty()) {
            if (effect.attribute("id") == id) return i;
        } else if (!tag.isEmpty() && effect.attribute("tag") == tag) return i;
    }
    return -1;
}

QStringList EffectsList::effectIdInfo(const int ix) const
{
    QStringList info;
    QDomElement effect = m_baseElement.childNodes().at(ix).toElement();
    QDomNode namenode = effect.elementsByTagName("name").item(0);
    info << i18n(namenode.toElement().text().toUtf8().data()) << effect.attribute("tag") << effect.attribute("id");
    return info;
}

QStringList EffectsList::effectNames()
{
    QStringList list;
    QDomNodeList effects = m_baseElement.childNodes();
    for (int i = 0; i < effects.count(); i++) {
        QDomElement effect =  effects.at(i).toElement();
        QDomNode namenode = effect.elementsByTagName("name").item(0);
        if (!namenode.isNull()) list.append(i18n(namenode.toElement().text().toUtf8().data()));
    }
    return list;
}

QString EffectsList::getInfo(const QString & tag, const QString & id) const
{
    QString info;
    QDomElement effect = getEffectByTag(tag, id);
    QDomNode namenode = effect.elementsByTagName("description").item(0);
    if (!namenode.isNull()) info = i18n(namenode.toElement().text().toUtf8().data());
    namenode = effect.elementsByTagName("author").item(0);
    if (!namenode.isNull()) info.append("<br /><strong>" + i18n("Author:") + " </strong>" + i18n(namenode.toElement().text().toUtf8().data()));
    return info;
}

QString EffectsList::getInfoFromIndex(const int ix) const
{
    QString info;
    QDomElement effect = m_baseElement.childNodes().at(ix).toElement();
    QDomNode namenode = effect.elementsByTagName("description").item(0);
    if (!namenode.isNull()) info = i18n(namenode.toElement().text().toUtf8().data());
    namenode = effect.elementsByTagName("author").item(0);
    if (!namenode.isNull()) info.append("<br /><strong>" + i18n("Author:") + " </strong>" + i18n(namenode.toElement().text().toUtf8().data()));
    return info;
}

// static
bool EffectsList::hasKeyFrames(QDomElement effect)
{
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute("type") == "keyframe") return true;
    }
    return false;
}

void EffectsList::clone(const EffectsList original)
{
    setContent(original.toString());
    m_baseElement = documentElement();
}

// static
void EffectsList::setParameter(QDomElement effect, const QString &name, const QString &value)
{
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute("name") == name) {
            e.setAttribute("value", value);
            break;
        }
    }
}

// static
QString EffectsList::parameter(QDomElement effect, const QString &name)
{
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute("name") == name) {
            return e.attribute("value");
        }
    }
    return QString();
}

void EffectsList::append(QDomElement e)
{
    m_baseElement.appendChild(importNode(e, true));
}

int EffectsList::count() const
{
    return m_baseElement.childNodes().count();
}

bool EffectsList::isEmpty() const
{
    return m_baseElement.childNodes().count() == 0;
}

const QDomElement EffectsList::at(int ix) const
{
    QDomNodeList effects = m_baseElement.childNodes();
    if (ix >= effects.count()) return QDomElement();
    return effects.at(ix).toElement();
}

void EffectsList::removeAt(int ix)
{
    QDomNodeList effects = m_baseElement.childNodes();
    if (ix >= effects.count()) return;
    m_baseElement.removeChild(effects.at(ix));
}

QDomElement EffectsList::item(int ix)
{
    QDomNodeList effects = m_baseElement.childNodes();
    if (ix >= effects.count()) return QDomElement();
    return effects.at(ix).toElement();
}

void EffectsList::insert(int ix, QDomElement effect)
{
    QDomNodeList effects = m_baseElement.childNodes();
    if (ix >= effects.count()) m_baseElement.appendChild(effect);
    else m_baseElement.insertBefore(effect, effects.at(ix));
}

