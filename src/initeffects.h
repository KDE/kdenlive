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

#include <klocale.h>
#include <QDomDocument>
#include <QThread>
#include <mlt++/Mlt.h>


/**Init the MLT effects
  *@author Jean-Baptiste Mardelle
  */

class EffectsList;

class initEffectsThumbnailer : public QThread
{
    Q_OBJECT
public:
    initEffectsThumbnailer();
    void prepareThumbnailsCall(const QStringList&);
    void run();
private :
    QStringList m_list;

};

class initEffects
{
public:
    static Mlt::Repository *parseEffectFiles();
    static void refreshLumas();
    static QDomDocument createDescriptionFromMlt(Mlt::Repository* repository, const QString& type, const QString& name);
    static void fillTransitionsList(Mlt::Repository *, EffectsList* transitions, QStringList names);
    static QDomElement quickParameterFill(QDomDocument & doc, QString name, QString tag, QString type, QString def = QString(), QString min = QString(), QString max = QString(), QString list = QString(), QString listdisplaynames = QString(), QString factor = QString(), QString namedesc = QString(), QString format = QString(), QString opacity = QString());
    static void parseEffectFile(EffectsList *customEffectList, EffectsList *audioEffectList, EffectsList *videoEffectList, QString name, QStringList filtersList, QStringList producersList);
    static void parseCustomEffectsFile();
    static const char* ladspaEffectString(int ladspaId, QStringList params);
    static void ladspaEffectFile(const QString & fname, int ladspaId, QStringList params);

    static const char* ladspaPitchEffectString(QStringList params);
    static const char* ladspaReverbEffectString(QStringList params);
    static const char* ladspaRoomReverbEffectString(QStringList params);
    static const char* ladspaEqualizerEffectString(QStringList params);
    static const char* ladspaDeclipEffectString(QStringList);
    static const char* ladspaVinylEffectString(QStringList params);
    static const char* ladspaLimiterEffectString(QStringList params);
    static const char* ladspaPitchShifterEffectString(QStringList params);
    static const char* ladspaPhaserEffectString(QStringList params);
    static const char* ladspaRateScalerEffectString(QStringList params);

private:
    initEffects(); // disable the constructor
    static initEffectsThumbnailer thumbnailer;
};


#endif
