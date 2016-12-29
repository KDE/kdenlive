/*
 * Kdenlive timeline clip handling MLT producer
 * Copyright 2015 Kdenlive team <kdenlive@kde.org>
 * Author: Vincent Pinon <vpinon@kde.org>
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

#ifndef CLIP_H
#define CLIP_H

#include <QObject>
#include <mlt++/MltProducer.h>
#include <mlt++/MltService.h>

class Clip : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Mlt::Producer producer READ producer WRITE setProducer)

public:
    explicit Clip(Mlt::Producer &producer);
    Clip(Clip &other);
    ~Clip();
    Clip &operator=(Clip &other);
    const QByteArray xml();
    /** @brief: Clone a producer (creates a completely independent copy). */
    Mlt::Producer *clone();
    /** @brief: Clone a producer without using xml-string producer.
     *          When Movit is used, we must use this because xml-string crashes (probably attaches some normalizers)
     */
    Mlt::Producer *softClone(const char *list);
    void deleteEffects();
    void addEffects(Mlt::Service &service, bool skipFades = false);
    void replaceEffects(Mlt::Service &service);
    void delEffect(int index);
    /** @brief: Dis/enable all kdenlive effects on a clip. */
    void disableEffects(bool disable);
    void adjustEffectsLength();
    Mlt::Producer &producer();

public Q_SLOTS:
    void setProducer(Mlt::Producer &producer);

private:
    Mlt::Producer m_producer;
};

#endif // CLIP_H
