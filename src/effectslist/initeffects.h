/***************************************************************************
                          initeffects.h  -  description
                             -------------------
    begin                :  Jul 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef InitEffects_H
#define InitEffects_H

#include <QDomDocument>
#include <QStringList>
#include <QMap>
#include <memory>
#include <mlt++/Mlt.h>

/**Init the MLT effects
  *@author Jean-Baptiste Mardelle
  */

class EffectsList;

class initEffects
{
public:

    /** @brief Fills the effects and transitions lists.
     * @ref fillTransitionsList
     * @ref parseEffectFile
     * @return true if Movit GPU effects are available
     *
     * It checks for all available effects and transitions, removes blacklisted
     * ones, calls fillTransitionsList() and parseEffectFile() to fill the lists
     * (with sorted, unique items) and then fills the global lists. */
    static bool parseEffectFiles(std::unique_ptr<Mlt::Repository> &repository, const QString &locale = QString());
    static void refreshLumas();
    static QDomDocument createDescriptionFromMlt(std::unique_ptr<Mlt::Repository> &repository, const QString &type, const QString &name);
    static QDomDocument getUsedCustomEffects(const QMap<QString, QString> &effectids);

    /** @brief Fills the transitions list.
     * @param repository MLT repository
     * @param transitions list to save the transitions data in
     * @param names list of transitions names
     *
     * It creates an element for each transition, asking to MLT for information
     * when possible, using default parameters otherwise. It also adds some
     * "virtual" transition, and removes those not implemented. */
    static void fillTransitionsList(std::unique_ptr<Mlt::Repository> &repository, EffectsList *transitions, QStringList names);

    /** @brief Creates an element describing a transition parameter.
     * @param doc document containing the transition element
     * @param name parameter name
     * @param tag parameter tag
     * @param type parameter type (string, double, bool, etc.)
     * @return element with the parameter information */
    static QDomElement quickParameterFill(QDomDocument &doc,
                                          const QString &name,
                                          const QString &tag,
                                          const QString &type,
                                          const QString &def = QString(),
                                          const QString &min = QString(),
                                          const QString &max = QString(),
                                          const QString &list = QString(),
                                          const QString &listdisplaynames = QString(),
                                          const QString &factor = QString(),
                                          const QString &opacity = QString());

    /** @brief Parses a file to record information about one or more effects.
     * @param customEffectList list of custom effect
     * @param audioEffectList list of audio effects
     * @param videoEffectList list of video effects
     * @param name file name
     * @param filtersList list of filters in the MLT repository
     * @param producersList list of producers in the MLT repository
     * @param repository MLT repository */
    static void parseEffectFile(EffectsList *customEffectList,
                                EffectsList *audioEffectList,
                                EffectsList *videoEffectList,
                                const QString &name, const QStringList &filtersList,
                                const QStringList &producersList,
                                std::unique_ptr<Mlt::Repository> &repository, const QMap<QString, QString> &effectDescriptions);
    static void parseTransitionFile(EffectsList *transitionList, const QString &name, std::unique_ptr<Mlt::Repository> &repository, const QStringList &installedTransitions, const QMap<QString, QString> &effectDescriptions);

    /** @brief Reloads information about custom effects. */
    static void parseCustomEffectsFile();

private:
    initEffects(); // disable the constructor
};

#endif
