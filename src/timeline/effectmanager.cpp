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

#include "effectmanager.h"
#include <mlt++/Mlt.h>

EffectManager::EffectManager(Mlt::Service &producer, QObject *parent)
    : QObject(parent),
      m_producer(producer)
{
}

EffectManager::EffectManager(EffectManager &other) : QObject()
{
    m_producer = other.producer();
}

EffectManager::~EffectManager()
{
}

EffectManager &EffectManager::operator=(EffectManager &other)
{
    m_producer = other.producer();
    return *this;
}

Mlt::Service &EffectManager::producer()
{
    return m_producer;
}

void EffectManager::setProducer(Mlt::Service &producer)
{
    m_producer = producer;
}

/*void Clip::adjustEffectsLength()
{
    int ct = 0;
    Mlt::Filter *filter = m_producer.filter(ct);
    while (filter) {
    if (filter->get_int("kdenlive:sync_in_out") == 1) {
            filter->set_in_and_out(m_producer.get_in(), m_producer.get_out());
        }
        ct++;
    filter = m_producer.filter(ct);
    }
}*/

bool EffectManager::addEffect(const EffectsParameterList &params, int duration)
{
    bool updateIndex = false;
    const int filter_ix = params.paramValue(QStringLiteral("kdenlive_ix")).toInt();
    int ct = 0;
    QList<Mlt::Filter *> filters;
    m_producer.lock();
    Mlt::Filter *filter = m_producer.filter(ct);
    while (filter) {
        filters << filter;
        if (filter->get_int("kdenlive_ix") == filter_ix) {
            // A filter at that position already existed, so we will increase all indexes later
            updateIndex = true;
            break;
        }
        ct++;
        filter = m_producer.filter(ct);
    }

    if (params.paramValue(QStringLiteral("id")) == QLatin1String("speed")) {
        // special case, speed effect is not really inserted, we just update the other effects index (kdenlive_ix)
        ct = 0;
        filter = m_producer.filter(ct);
        while (filter) {
            filters << filter;
            if (filter->get_int("kdenlive_ix") >= filter_ix) {
                if (updateIndex) {
                    filter->set("kdenlive_ix", filter->get_int("kdenlive_ix") + 1);
                }
            }
            ct++;
            filter = m_producer.filter(ct);
        }
        m_producer.unlock();
        qDeleteAll(filters);
        return true;
    }

    // temporarily remove all effects after insert point
    QList<Mlt::Filter *> filtersList;
    ct = 0;
    filter = m_producer.filter(ct);
    while (filter) {
        if (filter->get_int("kdenlive_ix") >= filter_ix) {
            filtersList.append(filter);
            m_producer.detach(*filter);
        } else {
            ct++;
        }
        filter = m_producer.filter(ct);
    }

    // Add new filter
    bool success = doAddFilter(params, duration);
    // re-add following filters
    for (int i = 0; i < filtersList.count(); ++i) {
        Mlt::Filter *cur_filter = filtersList.at(i);
        if (updateIndex) {
            cur_filter->set("kdenlive_ix", cur_filter->get_int("kdenlive_ix") + 1);
        }
        m_producer.attach(*cur_filter);
    }
    m_producer.unlock();
    qDeleteAll(filters);
    qDeleteAll(filtersList);
    return success;
}

bool EffectManager::doAddFilter(EffectsParameterList params, int duration)
{
    // create filter
    QString tag =  params.paramValue(QStringLiteral("tag"));
    QLocale locale;
    ////qCDebug(KDENLIVE_LOG) << " / / INSERTING EFFECT: " << tag << ", REGI: " << region;
    QString kfr = params.paramValue(QStringLiteral("keyframes"));
    if (!kfr.isEmpty()) {
        QStringList keyFrames = kfr.split(QLatin1Char(';'), QString::SkipEmptyParts);
        char *starttag = qstrdup(params.paramValue(QStringLiteral("starttag"), QStringLiteral("start")).toUtf8().constData());
        char *endtag = qstrdup(params.paramValue(QStringLiteral("endtag"), QStringLiteral("end")).toUtf8().constData());
        ////qCDebug(KDENLIVE_LOG) << "// ADDING KEYFRAME TAGS: " << starttag << ", " << endtag;
        //double max = params.paramValue("max").toDouble();
        double min = params.paramValue(QStringLiteral("min")).toDouble();
        double factor = params.paramValue(QStringLiteral("factor"), QStringLiteral("1")).toDouble();
        double paramOffset = params.paramValue(QStringLiteral("offset"), QStringLiteral("0")).toDouble();
        params.removeParam(QStringLiteral("starttag"));
        params.removeParam(QStringLiteral("endtag"));
        params.removeParam(QStringLiteral("keyframes"));
        params.removeParam(QStringLiteral("min"));
        params.removeParam(QStringLiteral("max"));
        params.removeParam(QStringLiteral("factor"));
        params.removeParam(QStringLiteral("offset"));
        // Special case, only one keyframe, means we want a constant value
        if (keyFrames.count() == 1) {
            Mlt::Filter *filter = new Mlt::Filter(*m_producer.profile(), qstrdup(tag.toUtf8().constData()));
            if (filter && filter->is_valid()) {
                filter->set("kdenlive_id", qstrdup(params.paramValue(QStringLiteral("id")).toUtf8().constData()));
                int x1 = keyFrames.at(0).section(QLatin1Char('='), 0, 0).toInt();
                double y1 = keyFrames.at(0).section(QLatin1Char('='), 1, 1).toDouble();
                for (int j = 0; j < params.count(); ++j) {
                    filter->set(params.at(j).name().toUtf8().constData(), params.at(j).value().toUtf8().constData());
                }
                filter->set("in", x1);
                ////qCDebug(KDENLIVE_LOG) << "// ADDING KEYFRAME vals: " << min<<" / "<<max<<", "<<y1<<", factor: "<<factor;
                filter->set(starttag, locale.toString(((min + y1) - paramOffset) / factor).toUtf8().data());
                m_producer.attach(*filter);
                delete filter;
            } else {
                delete[] starttag;
                delete[] endtag;
                //qCDebug(KDENLIVE_LOG) << "filter is nullptr";
                m_producer.unlock();
                return false;
            }
        } else for (int i = 0; i < keyFrames.size() - 1; ++i) {
                Mlt::Filter *filter = new Mlt::Filter(*m_producer.profile(), qstrdup(tag.toUtf8().constData()));
                if (filter && filter->is_valid()) {
                    filter->set("kdenlive_id", qstrdup(params.paramValue(QStringLiteral("id")).toUtf8().constData()));
                    int x1 = keyFrames.at(i).section(QLatin1Char('='), 0, 0).toInt();
                    double y1 = keyFrames.at(i).section(QLatin1Char('='), 1, 1).toDouble();
                    int x2 = keyFrames.at(i + 1).section(QLatin1Char('='), 0, 0).toInt();
                    double y2 = keyFrames.at(i + 1).section(QLatin1Char('='), 1, 1).toDouble();
                    if (x2 == -1) {
                        x2 = duration;
                    }
                    // non-overlapping sections
                    if (i > 0) {
                        y1 += (y2 - y1) / (x2 - x1);
                        ++x1;
                    }

                    for (int j = 0; j < params.count(); ++j) {
                        filter->set(params.at(j).name().toUtf8().constData(), params.at(j).value().toUtf8().constData());
                    }

                    filter->set("in", x1);
                    filter->set("out", x2);
                    ////qCDebug(KDENLIVE_LOG) << "// ADDING KEYFRAME vals: " << min<<" / "<<max<<", "<<y1<<", factor: "<<factor;
                    filter->set(starttag, locale.toString(((min + y1) - paramOffset) / factor).toUtf8().data());
                    filter->set(endtag, locale.toString(((min + y2) - paramOffset) / factor).toUtf8().data());
                    m_producer.attach(*filter);
                    delete filter;
                } else {
                    delete[] starttag;
                    delete[] endtag;
                    //qCDebug(KDENLIVE_LOG) << "filter is nullptr";
                    m_producer.unlock();
                    return false;
                }
            }
        delete[] starttag;
        delete[] endtag;
    } else {
        Mlt::Filter *filter;
        QString prefix;
        filter = new Mlt::Filter(*m_producer.profile(), qstrdup(tag.toUtf8().constData()));
        if (filter && filter->is_valid()) {
            filter->set("kdenlive_id", qstrdup(params.paramValue(QStringLiteral("id")).toUtf8().constData()));
        } else {
            //qCDebug(KDENLIVE_LOG) << "filter is nullptr";
            m_producer.unlock();
            return false;
        }
        params.removeParam(QStringLiteral("kdenlive_id"));
        if (params.paramValue(QStringLiteral("kdenlive:sync_in_out")) == QLatin1String("1")) {
            // This effect must sync in / out with parent clip
            //params.removeParam(QStringLiteral("_sync_in_out"));
            filter->set_in_and_out(m_producer.get_int("in"), m_producer.get_int("out"));
        }

        for (int j = 0; j < params.count(); ++j) {
            filter->set((prefix + params.at(j).name()).toUtf8().constData(), params.at(j).value().toUtf8().constData());
        }

        if (tag == QLatin1String("sox")) {
            QString effectArgs = params.paramValue(QStringLiteral("id")).section(QLatin1Char('_'), 1);

            params.removeParam(QStringLiteral("id"));
            params.removeParam(QStringLiteral("kdenlive_ix"));
            params.removeParam(QStringLiteral("tag"));
            params.removeParam(QStringLiteral("disable"));
            params.removeParam(QStringLiteral("region"));

            for (int j = 0; j < params.count(); ++j) {
                effectArgs.append(QLatin1Char(' ') + params.at(j).value());
            }
            ////qCDebug(KDENLIVE_LOG) << "SOX EFFECTS: " << effectArgs.simplified();
            filter->set("effect", effectArgs.simplified().toUtf8().constData());
        }
        // attach filter to the clip
        m_producer.attach(*filter);
        delete filter;
    }
    return true;
}

bool EffectManager::editEffect(const EffectsParameterList &params, int duration, bool replaceEffect)
{
    int index = params.paramValue(QStringLiteral("kdenlive_ix")).toInt();
    QString tag =  params.paramValue(QStringLiteral("tag"));

    if (!params.paramValue(QStringLiteral("keyframes")).isEmpty() || replaceEffect || tag.startsWith(QLatin1String("ladspa")) || tag == QLatin1String("sox") || tag == QLatin1String("autotrack_rectangle")) {
        // This is a keyframe effect, to edit it, we remove it and re-add it.
        if (removeEffect(index, false)) {
            return addEffect(params, duration);
        }
    }

    // find filter
    int ct = 0;
    Mlt::Filter *filter = m_producer.filter(ct);
    while (filter) {
        if (filter->get_int("kdenlive_ix") == index) {
            break;
        }
        delete filter;
        ct++;
        filter = m_producer.filter(ct);
    }

    if (!filter) {
        qCDebug(KDENLIVE_LOG) << "WARINIG, FILTER FOR EDITING NOT FOUND, ADDING IT! " << index << ", " << tag;
        // filter was not found, it was probably a disabled filter, so add it to the correct place...

        bool success = addEffect(params, duration);
        return success;
    }
    ct = 0;
    QString ser = QString::fromLatin1(filter->get("mlt_service"));
    QList<Mlt::Filter *> filtersList;
    m_producer.lock();
    if (ser != tag) {
        // Effect service changes, delete effect and re-add it
        m_producer.detach(*filter);
        delete filter;
        // Delete all effects after deleted one
        filter = m_producer.filter(ct);
        while (filter) {
            if (filter->get_int("kdenlive_ix") > index) {
                filtersList.append(filter);
                m_producer.detach(*filter);
            } else {
                ct++;
            }
            delete filter;
            filter = m_producer.filter(ct);
        }

        // re-add filter
        doAddFilter(params, duration);
        m_producer.unlock();
        return true;
    }
    if (params.hasParam(QStringLiteral("kdenlive:sync_in_out"))) {
        if (params.paramValue(QStringLiteral("kdenlive:sync_in_out")) == QLatin1String("1")) {
            // This effect must sync in / out with parent clip
            //params.removeParam(QStringLiteral("sync_in_out"));
            filter->set_in_and_out(m_producer.get_int("in"), m_producer.get_int("out"));
        } else {
            // Reset in/out properties
            filter->set("in", (char *)nullptr);
            filter->set("out", (char *)nullptr);
        }
    }

    for (int j = 0; j < params.count(); ++j) {
        filter->set(params.at(j).name().toUtf8().constData(), params.at(j).value().toUtf8().constData());
    }

    for (int j = 0; j < filtersList.count(); ++j) {
        m_producer.attach(*(filtersList.at(j)));
    }
    qDeleteAll(filtersList);
    m_producer.unlock();
    delete filter;
    return true;
}

bool EffectManager::removeEffect(int effectIndex, bool updateIndex)
{
    m_producer.lock();
    int ct = 0;
    bool success = false;
    Mlt::Filter *filter = m_producer.filter(ct);
    QList<Mlt::Filter *> filters;
    while (filter) {
        filters << filter;
        if ((effectIndex == -1 && strcmp(filter->get("kdenlive_id"), "")) || filter->get_int("kdenlive_ix") == effectIndex) {
            if (m_producer.detach(*filter) == 0) {
                success = true;
            }
        } else if (updateIndex) {
            // Adjust the other effects index
            if (filter->get_int("kdenlive_ix") > effectIndex) {
                filter->set("kdenlive_ix", filter->get_int("kdenlive_ix") - 1);
            }
            ct++;
        } else {
            ct++;
        }
        filter = m_producer.filter(ct);
    }
    m_producer.unlock();
    qDeleteAll(filters);
    return success;
}

bool EffectManager::enableEffects(const QList<int> &effectIndexes, bool disable, bool rememberState)
{
    int ct = 0;
    bool success = false;
    Mlt::Filter *filter = m_producer.filter(ct);
    while (filter) {
        if (effectIndexes.isEmpty() || effectIndexes.contains(filter->get_int("kdenlive_ix"))) {
            //m_producer.lock();
            if (rememberState) {
                if (disable && filter->get_int("disable") == 0) {
                    filter->set("auto_disable", 1);
                    filter->set("disable", (int) disable);
                } else if (!disable && filter->get_int("auto_disable") == 1) {
                    filter->set("disable", (char *) nullptr);
                    filter->set("auto_disable", (char *) nullptr);
                }
            } else {
                filter->set("disable", (int) disable);
            }
            success = true;
            //m_producer.unlock();
        }
        delete filter;
        ct++;
        filter = m_producer.filter(ct);
    }
    return success;
}

bool EffectManager::moveEffect(int oldPos, int newPos)
{
    int ct = 0;
    QList<Mlt::Filter *> filtersList;
    QList<Mlt::Filter *> toDelete;
    Mlt::Filter *filter = m_producer.filter(ct);
    if (newPos > oldPos) {
        bool found = false;
        while (filter) {
            toDelete << filter;
            if (!found && filter->get_int("kdenlive_ix") == oldPos) {
                filter->set("kdenlive_ix", newPos);
                filtersList.append(filter);
                m_producer.detach(*filter);
                filter = m_producer.filter(ct);
                while (filter && filter->get_int("kdenlive_ix") <= newPos) {
                    filter->set("kdenlive_ix", filter->get_int("kdenlive_ix") - 1);
                    ct++;
                    filter = m_producer.filter(ct);
                }
                found = true;
            }
            if (filter && filter->get_int("kdenlive_ix") > newPos) {
                filtersList.append(filter);
                m_producer.detach(*filter);
            } else {
                ct++;
            }
            filter = m_producer.filter(ct);
        }
    } else {
        while (filter) {
            toDelete << filter;
            if (filter->get_int("kdenlive_ix") == oldPos) {
                filter->set("kdenlive_ix", newPos);
                filtersList.append(filter);
                m_producer.detach(*filter);
            } else {
                ct++;
            }
            filter = m_producer.filter(ct);
        }

        ct = 0;
        filter = m_producer.filter(ct);
        while (filter) {
            toDelete << filter;
            int pos = filter->get_int("kdenlive_ix");
            if (pos >= newPos) {
                if (pos < oldPos) {
                    filter->set("kdenlive_ix", pos + 1);
                }
                filtersList.append(filter);
                m_producer.detach(*filter);
            } else {
                ct++;
            }
            filter = m_producer.filter(ct);
        }
    }

    for (int i = 0; i < filtersList.count(); ++i) {
        m_producer.attach(*(filtersList.at(i)));
    }
    qDeleteAll(toDelete);
    //TODO: check for success
    return true;
}
