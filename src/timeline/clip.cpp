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

#include "clip.h"
#include <mlt++/Mlt.h>

Clip::Clip(Mlt::Producer &producer) :
    m_producer(producer)
{
}

Clip::Clip(Clip& other)
{
    m_producer = other.producer();
}

Clip::~Clip()
{
}

Clip& Clip::operator=(Clip& other)
{
    m_producer = other.producer();
    return *this;
}

Mlt::Producer & Clip::producer()
{
    return m_producer;
}

void Clip::setProducer(Mlt::Producer& producer)
{
    m_producer = producer;
}

void Clip::addEffects(Mlt::Service& service)
{
    for (int ix = 0; ix < service.filter_count(); ++ix) {
        Mlt::Filter *effect = service.filter(ix);
        // Only duplicate Kdenlive filters, and skip the fade in effects
        if (effect->is_valid()
                && effect->get("kdenlive_id")
                && strcmp(effect->get("kdenlive_id"), "fadein")
                && strcmp(effect->get("kdenlive_id"), "fade_from_black")) {
            // no easy filter copy: do it by hand!
            Mlt::Filter *copy = new Mlt::Filter(*effect->profile(), effect->get("mlt_service"));
            if (copy && copy->is_valid()) {
                for (int i = 0; i < effect->count(); ++i) {
                    copy->set(effect->get_name(i), effect->get(i));
                }
                m_producer.attach(*copy);
            }
        }
    }
}

QByteArray Clip::xml()
{
    Mlt::Consumer c(*m_producer.profile(), "xml", "string");
    Mlt::Service s(m_producer.get_service());
    int ignore = s.get_int("ignore_points");
    if (ignore) s.set("ignore_points", 0);
    if (s.get_int("ignore_points")) s.set("ignore_points", 0);
    c.connect(s);
    c.set("time_format", "frames");
    c.set("no_meta", 1);
    c.set("store", "kdenlive");
    c.start();
    if (ignore) s.set("ignore_points", ignore);
    return c.get("string");
}

Mlt::Producer *Clip::clone() {
    return new Mlt::Producer(*m_producer.profile(), "xml-string", xml().constData());
}