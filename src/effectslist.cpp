/***************************************************************************
                          docclipbaseiterator.cpp  -  description
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

#include <KDebug>
#include <KLocale>

#include "effectslist.h"


EffectsList::EffectsList():
        QList < QDomElement > () {
}

EffectsList::~EffectsList() {
}

QMap <QString, QString> EffectsList::effect(const QString & name) {
    QMap <QString, QString> filter;
    QString effectName;
    QDomElement effect;
    for (int i = 0; i < this->size(); ++i) {
        effect =  this->at(i);
        QDomNode namenode = effect.elementsByTagName("name").item(0);
        if (!namenode.isNull()) {
            effectName = i18n(namenode.toElement().text().toUtf8().data());
            if (name == effectName) break;
        }
    }
    filter.insert("mlt_service", effect.attribute("tag"));
    filter.insert("name", name);
    return filter;
}


QDomElement EffectsList::getEffectByName(const QString & name) const {
    QString effectName;
    for (int i = 0; i < this->size(); ++i) {
        QDomElement effect =  this->at(i);
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

QDomElement EffectsList::getEffectByTag(const QString & tag, const QString & id) const {

    if (!id.isEmpty()) for (int i = 0; i < this->size(); ++i) {
            QDomElement effect =  this->at(i);
            kDebug() << "// SRCH EFFECT; " << id << ", LKING: " << effect.attribute("id");
            if (effect.attribute("id") == id) {
                QDomNodeList params = effect.elementsByTagName("parameter");
                for (int i = 0; i < params.count(); i++) {
                    QDomElement e = params.item(i).toElement();
                    if (!e.hasAttribute("value"))
                        e.setAttribute("value", e.attribute("default"));
                }
                return effect;
            }
        }

    if (!tag.isEmpty()) for (int i = 0; i < this->size(); ++i) {
            QDomElement effect =  this->at(i);
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

    return QDomElement();
}

QStringList EffectsList::effectNames() {
    QStringList list;
    for (int i = 0; i < this->size(); ++i) {
        QDomElement effect =  this->at(i);
        QDomNode namenode = effect.elementsByTagName("name").item(0);
        if (!namenode.isNull()) list.append(i18n(namenode.toElement().text().toUtf8().data()));
    }
    qSort(list);
    return list;
}

QString EffectsList::getInfo(QString effectName) {
    QString info;
    QDomElement effect = getEffectByName(effectName);
    QDomNode namenode = effect.elementsByTagName("description").item(0);
    if (!namenode.isNull()) info = i18n(namenode.toElement().text().toUtf8().data());
    namenode = effect.elementsByTagName("author").item(0);
    if (!namenode.isNull()) info.append(i18n("<br><b>Author:</b> ") + i18n(namenode.toElement().text().toUtf8().data()));
    return info;
}

bool EffectsList::hasKeyFrames(QDomElement effect) {
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute("type") == "keyframe") return true;
    }
    return false;
}

// static
void EffectsList::setParameter(QDomElement effect, QString name, QString value) {
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute("name") == name) {
            e.setAttribute("value", value);
            break;
        }
    }
}

