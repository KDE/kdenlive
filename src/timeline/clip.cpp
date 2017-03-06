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
#include <QDomDocument>
#include <QDebug>

Clip::Clip(Mlt::Producer &producer) : QObject(),
    m_producer(producer)
{
}

Clip::Clip(Clip &other) : QObject()
{
    m_producer = other.producer();
}

Clip::~Clip()
{
}

Clip &Clip::operator=(Clip &other)
{
    m_producer = other.producer();
    return *this;
}

Mlt::Producer &Clip::producer()
{
    return m_producer;
}

void Clip::setProducer(Mlt::Producer &producer)
{
    m_producer = producer;
}

void Clip::adjustEffectsLength()
{
    int ct = 0;
    Mlt::Filter *filter = m_producer.filter(ct);
    while (filter) {
        if (filter->get_int("kdenlive:sync_in_out") == 1) {
            filter->set_in_and_out(m_producer.get_in(), m_producer.get_out());
        }
        ct++;
        delete filter;
        filter = m_producer.filter(ct);
    }
}

void Clip::addEffects(Mlt::Service &service, bool skipFades)
{
    for (int ix = 0; ix < service.filter_count(); ++ix) {
        QScopedPointer<Mlt::Filter> effect(service.filter(ix));
        // Only duplicate Kdenlive filters, and skip the fade in effects
        if (effect->is_valid()) {
            QString effectId = QString::fromLatin1(effect->get("kdenlive_id"));
            if (effectId.isEmpty() || (skipFades && (effectId == QLatin1String("fadein") || effectId == QLatin1String("fade_from_black")))) {
                continue;
            }
            // no easy filter copy: do it by hand!
            Mlt::Filter *copy = new Mlt::Filter(*effect->profile(), effect->get("mlt_service"));
            if (copy && copy->is_valid()) {
                for (int i = 0; i < effect->count(); ++i) {
                    QString paramName = QString::fromLatin1(effect->get_name(i));
                    if (paramName == QLatin1String("kdenlive:sync_in_out") && QString::fromLatin1(effect->get(i)) == QLatin1String("1")) {
                        // Effect in/out must be synced with clip in/out
                        copy->set_in_and_out(m_producer.get_in(), m_producer.get_out());
                    }
                    if (paramName.at(0) != QLatin1Char('_')) {
                        copy->set(effect->get_name(i), effect->get(i));
                    }
                }
                m_producer.attach(*copy);
            }
            delete copy;
        }
    }
}

void Clip::replaceEffects(Mlt::Service &service)
{
    // remove effects first
    int ct = 0;
    Mlt::Filter *filter = m_producer.filter(ct);
    while (filter) {
        QString ix = QString::fromLatin1(filter->get("kdenlive_ix"));
        if (!ix.isEmpty()) {
            if (m_producer.detach(*filter) == 0) {
            } else {
                ct++;
            }
        } else {
            ct++;
        }
        delete filter;
        filter = m_producer.filter(ct);
    }
    addEffects(service);
}

void Clip::deleteEffects()
{
    // remove effects
    int ct = 0;
    Mlt::Filter *filter = m_producer.filter(ct);
    while (filter) {
        QString ix = QString::fromLatin1(filter->get("kdenlive_ix"));
        if (!ix.isEmpty()) {
            if (m_producer.detach(*filter) == 0) {
            } else {
                ct++;
            }
        } else {
            ct++;
        }
        delete filter;
        filter = m_producer.filter(ct);
    }
}

void Clip::disableEffects(bool disable)
{
    int ct = 0;
    Mlt::Filter *filter = m_producer.filter(ct);
    while (filter) {
        QString ix = QString::fromLatin1(filter->get("kdenlive_ix"));
        if (!ix.isEmpty()) {
            if (disable && filter->get_int("disable") == 0) {
                filter->set("disable", 1);
                filter->set("auto_disable", 1);
            } else if (!disable && filter->get_int("auto_disable") == 1) {
                filter->set("disable", (char *) nullptr);
                filter->set("auto_disable", (char *) nullptr);
            }
        }
        ct++;
        delete filter;
        filter = m_producer.filter(ct);
    }
}

const QByteArray Clip::xml()
{
    Mlt::Consumer c(*m_producer.profile(), "xml", "string");
    Mlt::Service s(m_producer.get_service());
    int ignore = s.get_int("ignore_points");
    if (ignore) {
        s.set("ignore_points", 0);
    }
    c.connect(s);
    c.set("time_format", "frames");
    c.set("no_meta", 1);
    c.set("no_root", 1);
    c.set("root", "/");
    c.set("store", "kdenlive");
    c.start();
    if (ignore) {
        s.set("ignore_points", ignore);
    }
    return c.get("string");
}

Mlt::Producer *Clip::clone()
{
    QByteArray prodXml = xml();
    //HACK: currently the MLT xml producer, when parsing a <profile>, does change the global profile accordingly.
    // causing crash on threaded calls. To avoid this, we discard the profile info from our xml
    QDomDocument doc;
    doc.setContent(prodXml, true);
    QDomNodeList profiles = doc.documentElement().elementsByTagName("profile");
    if (!profiles.isEmpty()) {
        QDomNode profile = profiles.item(0);
        doc.documentElement().removeChild(profile);
    }
    Mlt::Producer *clone = new Mlt::Producer(*m_producer.profile(), "xml-string", doc.toByteArray().constData());
    return clone;
}

Mlt::Producer *Clip::softClone(const char *list)
{
    QString service = QString::fromLatin1(m_producer.get("mlt_service"));
    QString resource = QString::fromLatin1(m_producer.get("resource"));
    Mlt::Producer *clone = new Mlt::Producer(*m_producer.profile(), service.toUtf8().constData(), resource.toUtf8().constData());
    Mlt::Properties original(m_producer.get_properties());
    Mlt::Properties cloneProps(clone->get_properties());
    cloneProps.pass_list(original, list);
    return clone;
}
