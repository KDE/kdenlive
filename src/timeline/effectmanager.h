/*
 * Kdenlive timeline clip handling MLT producer
 * Copyright 2016 Kdenlive team <kdenlive@kde.org>
 * Author: Jean-Baptiste Mardelle <jb@kdenlive.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef EFFECTMANAGER_H
#define EFFECTMANAGER_H

#include <QObject>
#include <mlt++/MltProducer.h>
#include <mlt++/MltService.h>

#include "mltcontroller/effectscontroller.h"

class EffectManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Mlt::Producer producer READ producer WRITE setProducer)

public:
    explicit EffectManager(Mlt::Service &producer, QObject *parent = nullptr);
    EffectManager(EffectManager &other);
    ~EffectManager();
    EffectManager &operator=(EffectManager &other);
    Mlt::Service &producer();
    void setProducer(Mlt::Service &producer);
    bool addEffect(const EffectsParameterList &params, int duration);
    bool doAddFilter(EffectsParameterList params, int duration);
    bool editEffect(const EffectsParameterList &params, int duration, bool replaceEffect);
    bool removeEffect(int effectIndex, bool updateIndex);
    bool enableEffects(const QList<int> &effectIndexes, bool disable, bool rememberState = false);
    bool moveEffect(int oldPos, int newPos);

public Q_SLOTS:

private:
    Mlt::Service m_producer;
};

#endif // CLIP_H

