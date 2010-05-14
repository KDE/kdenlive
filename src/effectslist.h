/***************************************************************************
                          effectslist.h  -  description
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

/**
 * @class EffectsList
 * @brief List for effects objects.
 * @author Jason Wood
 *
 * This is a list of DocClipBase objects, to be used instead of
 * QList<DocClipBase> to enable sorting lists correctly. It also contains the
 * ability to set a "master clip", which can be used by a number of operations
 * where there is the need of one clip to act as a reference for what happens to
 * all clips.
 */

#ifndef EFFECTSLIST_H
#define EFFECTSLIST_H

#include <QDomDocument>

class EffectsList: public QDomDocument
{
public:
    EffectsList();
    ~EffectsList();

    /** @brief Returns the XML element of an effect.
     * @param name name of the effect to be returned */
    QDomElement getEffectByName(const QString & name) const;
    QDomElement getEffectByTag(const QString & tag, const QString & id) const;

    /** @brief Checks the existance of an effect.
     * @param tag effect tag
     * @param id effect id
     * @return effect index if the effect exists, -1 otherwise */
    int hasEffect(const QString & tag, const QString & id) const;

    /** @brief Lists the core properties of an effect.
     * @param ix effect index
     * @return list of name, tag and id of an effect */
    QStringList effectIdInfo(const int ix) const;

    /** @brief Lists effects names. */
    QStringList effectNames();
    QString getInfo(const QString & tag, const QString & id) const;
    QString getInfoFromIndex(const int ix) const;
    void clone(const EffectsList original);
    void append(QDomElement e);
    bool isEmpty() const;
    int count() const;
    const QDomElement at(int ix) const;
    void removeAt(int ix);
    QDomElement item(int ix);
    void insert(int ix, QDomElement effect);
    void replace(int ix, QDomElement effect);
    static bool hasKeyFrames(QDomElement effect);
    static bool hasSimpleKeyFrames(QDomElement effect);
    static void setParameter(QDomElement effect, const QString &name, const QString &value);
    static QString parameter(QDomElement effect, const QString &name);
    static void setProperty(QDomElement effect, const QString &name, const QString &value);
    static QString property(QDomElement effect, const QString &name);
    void clearList();

private:
    QDomElement m_baseElement;

};

#endif
