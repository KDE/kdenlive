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
QList < QDomElement > ()
{
}

EffectsList::~EffectsList()
{
}

QDomElement EffectsList::getEffectByName(const QString & name)
{
  QString effectName;
  for (int i = 0; i < this->size(); ++i) {
    QDomElement effect =  this->at(i);
    QDomNode namenode = effect.elementsByTagName("name").item(0);
    if (!namenode.isNull()) effectName = i18n(qstrdup(namenode.toElement().text().toUtf8()));
    if (name == effectName) return effect;
  }

  return QDomElement();
}

QStringList EffectsList::effectNames()
{
  QStringList list;
  for (int i = 0; i < this->size(); ++i) {
    QDomElement effect =  this->at(i);
    QDomNode namenode = effect.elementsByTagName("name").item(0);
    if (!namenode.isNull()) list.append(i18n(qstrdup(namenode.toElement().text().toUtf8())));
  }
  return list;
}


