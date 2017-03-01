/***************************************************************************
 *   Copyright (C) 2015 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "transitionhandler.h"
#include "mltcontroller/effectscontroller.h"
#include "mainwindow.h"
#include "kdenlivesettings.h"

TransitionHandler::TransitionHandler(Mlt::Tractor *tractor) : QObject()
    , m_tractor(tractor)
{
}

bool TransitionHandler::addTransition(const QString &tag, int a_track, int b_track, GenTime in, GenTime out, const QDomElement &xml)
{
    if (in >= out) {
        return false;
    }
    double fps = m_tractor->get_fps();
    QMap<QString, QString> args = getTransitionParamsFromXml(xml);
    QScopedPointer<Mlt::Field> field(m_tractor->field());

    Mlt::Transition transition(*m_tractor->profile(), tag.toUtf8().constData());
    if (!transition.is_valid()) {
        return false;
    }
    if (out != GenTime()) {
        transition.set_in_and_out((int) in.frames(fps), (int) out.frames(fps) - 1);
    }

    QMap<QString, QString>::Iterator it;
    QString key;
    if (xml.attribute(QStringLiteral("automatic")) == QLatin1String("1")) {
        transition.set("automatic", 1);
    }
    ////qCDebug(KDENLIVE_LOG) << " ------  ADDING TRANSITION PARAMs: " << args.count();
    if (xml.hasAttribute(QStringLiteral("id"))) {
        transition.set("kdenlive_id", xml.attribute(QStringLiteral("id")).toUtf8().constData());
    }
    if (xml.hasAttribute(QStringLiteral("force_track"))) {
        transition.set("force_track", xml.attribute(QStringLiteral("force_track")).toInt());
    }

    for (it = args.begin(); it != args.end(); ++it) {
        key = it.key();
        if (!it.value().isEmpty()) {
            transition.set(key.toUtf8().constData(), it.value().toUtf8().constData());
        }
        ////qCDebug(KDENLIVE_LOG) << " ------  ADDING TRANS PARAM: " << key << ": " << it.value();
    }
    // attach transition
    m_tractor->lock();
    plantTransition(field.data(), transition, a_track, b_track);
    // field->plant_transition(*transition, a_track, b_track);
    m_tractor->unlock();
    return true;
}

void TransitionHandler::initTransition(const QDomElement &xml)
{
    QMap<QString, QString> params = getTransitionParamsFromXml(xml);
    QMapIterator<QString, QString> i(params);
    while (i.hasNext()) {
        i.next();
        EffectsList::setParameter(xml, i.key(), i.value());
    }
}

QMap<QString, QString> TransitionHandler::getTransitionParamsFromXml(const QDomElement &xml)
{
    QDomNodeList attribs = xml.elementsByTagName(QStringLiteral("parameter"));
    QMap<QString, QString> map;
    QLocale locale;
    for (int i = 0; i < attribs.count(); ++i) {
        QDomElement e = attribs.item(i).toElement();
        QString name = e.attribute(QStringLiteral("name"));
        if (e.hasAttribute(QStringLiteral("value"))) {
            map[name] = e.attribute(QStringLiteral("value"));
        } else {
            QString defaultValue = e.attribute(QStringLiteral("default"));
            // special case: luma file, we need to find full path for the default luma
            if (!defaultValue.isEmpty() && e.attribute(QStringLiteral("paramlist")) == QLatin1String("%lumaPaths")) {
                QString lumaFolder;
                if (m_tractor->profile()->width() > 1000) {
                    lumaFolder = QStringLiteral("HD");
                } else {
                    lumaFolder = QStringLiteral("PAL");
                }
                QStringList lumas = MainWindow::m_lumaFiles.value(lumaFolder);
                bool found = false;
                foreach (const QString &luma, lumas) {
                    if (QUrl::fromLocalFile(luma).fileName() == defaultValue) {
                        map[name] = luma;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (lumas.isEmpty()) {
                        // Something is wrong
                        map[name] = QString();
                    } else {
                        map[name] = lumas.first();
                    }
                }
            } else {
                if (defaultValue.contains(QLatin1Char('%'))) {
                    ProfileInfo pInfo;
                    pInfo.profileSize = QSize(m_tractor->profile()->width(), m_tractor->profile()->height());
                    pInfo.profileFps = m_tractor->profile()->fps();
                    defaultValue = EffectsController::getStringRectEval(pInfo, defaultValue);
                }
                map[name] = defaultValue;
            }
        }
        double factor = e.attribute(QStringLiteral("factor"), QStringLiteral("1")).toDouble();
        double offset = e.attribute(QStringLiteral("offset"), QStringLiteral("0")).toDouble();
        if (factor != 1 || offset != 0) {
            if (e.attribute(QStringLiteral("type")) == QLatin1String("simplekeyframe")) {
                QStringList values = map.value(name).split(QLatin1Char(';'), QString::SkipEmptyParts);
                for (int j = 0; j < values.count(); ++j) {
                    QString pos = values.at(j).section(QLatin1Char('='), 0, 0);
                    double val = (values.at(j).section(QLatin1Char('='), 1, 1).toDouble() - offset) / factor;
                    values[j] = pos + QLatin1Char('=') + locale.toString(val);
                }
                map[name] = values.join(QLatin1Char(';'));
            } else if (e.attribute(QStringLiteral("type")) != QLatin1String("addedgeometry")) {
                map[name] = locale.toString((locale.toDouble(map.value(name)) - offset) / factor);
                //map[name]=map[name].replace(".",","); //FIXME how to solve locale conversion of . ,
            }
        }
        if (e.attribute(QStringLiteral("namedesc")).contains(QLatin1Char(';'))) {
            //TODO: Deprecated, does not seem used anywhere...
            QString format = e.attribute(QStringLiteral("format"));
            QStringList separators = format.split(QStringLiteral("%d"), QString::SkipEmptyParts);
            QStringList values = map.value(name).split(QRegExp(QStringLiteral("[,:;x]")));
            QString neu;
            QTextStream txtNeu(&neu);
            if (!values.isEmpty()) {
                txtNeu << (int)values[0].toDouble();
            }
            int j = 0;
            for (j = 0; j < separators.size() && j + 1 < values.size(); ++j) {
                txtNeu << separators[j];
                txtNeu << (int)(values[j + 1].toDouble());
            }
            if (j < separators.size()) {
                txtNeu << separators[j];
            }
            map[name] = neu;
        }

    }
    return map;
}

// adds the transition by keeping the instance order from topmost track down to background
void TransitionHandler::plantTransition(Mlt::Transition &tr, int a_track, int b_track)
{
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    plantTransition(field.data(), tr, a_track, b_track);
}

// adds the transition by keeping the instance order from topmost track down to background
void TransitionHandler::plantTransition(Mlt::Field *field, Mlt::Transition &tr, int a_track, int b_track)
{
    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString resource = mlt_properties_get(properties, "mlt_service");
    QList<Mlt::Transition *> trList;
    mlt_properties insertproperties = tr.get_properties();
    QString insertresource = mlt_properties_get(insertproperties, "mlt_service");
    bool isMixTransition = insertresource == QLatin1String("mix");

    mlt_service_type mlt_type = mlt_service_identify(nextservice);
    while (mlt_type == transition_type) {
        Mlt::Transition transition((mlt_transition) nextservice);
        nextservice = mlt_service_producer(nextservice);
        int aTrack = transition.get_a_track();
        int bTrack = transition.get_b_track();
        int internal = transition.get_int("internal_added");
        if ((isMixTransition || resource != QLatin1String("mix")) && (internal > 0 || aTrack < a_track || (aTrack == a_track && bTrack > b_track))) {
            Mlt::Properties trans_props(transition.get_properties());
            Mlt::Transition *cp = new Mlt::Transition(*m_tractor->profile(), transition.get("mlt_service"));
            Mlt::Properties new_trans_props(cp->get_properties());
            //new_trans_props.inherit(trans_props);
            cloneProperties(new_trans_props, trans_props);
            trList.append(cp);
            field->disconnect_service(transition);
        }
        //else qCDebug(KDENLIVE_LOG) << "// FOUND TRANS OK, "<<resource<< ", A_: " << aTrack << ", B_ "<<bTrack;

        if (nextservice == nullptr) {
            break;
        }
        properties = MLT_SERVICE_PROPERTIES(nextservice);
        mlt_type = mlt_service_identify(nextservice);
        resource = mlt_properties_get(properties, "mlt_service");
    }
    field->plant_transition(tr, a_track, b_track);

    // re-add upper transitions
    for (int i = trList.count() - 1; i >= 0; --i) {
        ////qCDebug(KDENLIVE_LOG)<< "REPLANT ON TK: "<<trList.at(i)->get_a_track()<<", "<<trList.at(i)->get_b_track();
        field->plant_transition(*trList.at(i), trList.at(i)->get_a_track(), trList.at(i)->get_b_track());
    }
    qDeleteAll(trList);
}

void TransitionHandler::cloneProperties(Mlt::Properties &dest, Mlt::Properties &source)
{
    int count = source.count();
    int i = 0;
    for (i = 0; i < count; i ++) {
        char *value = source.get(i);
        if (value != nullptr) {
            char *name = source.get_name(i);
            if (name != nullptr && name[0] != QLatin1Char('_')) {
                dest.set(name, value);
            }
        }
    }
}

void TransitionHandler::updateTransition(const QString &oldTag, const QString &tag, int a_track, int b_track, GenTime in, GenTime out, const QDomElement &xml, bool force)
{
    if (oldTag == tag && !force) {
        updateTransitionParams(tag, a_track, b_track, in, out, xml);
    } else {
        ////qCDebug(KDENLIVE_LOG)<<"// DELETING TRANS: "<<a_track<<"-"<<b_track;
        deleteTransition(oldTag, a_track, b_track, in, out, xml, false);
        addTransition(tag, a_track, b_track, in, out, xml);
    }
}

void TransitionHandler::updateTransitionParams(const QString &type, int a_track, int b_track, GenTime in, GenTime out, const QDomElement &xml)
{
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();
    double fps = m_tractor->get_fps();
    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString resource = mlt_properties_get(properties, "mlt_service");
    int in_pos = (int) in.frames(fps);
    int out_pos = (int) out.frames(fps) - 1;

    mlt_service_type mlt_type = mlt_service_identify(nextservice);
    while (mlt_type == transition_type) {
        mlt_transition tr = (mlt_transition) nextservice;
        int currentTrack = mlt_transition_get_b_track(tr);
        int currentBTrack = mlt_transition_get_a_track(tr);
        int currentIn = (int) mlt_transition_get_in(tr);
        int currentOut = (int) mlt_transition_get_out(tr);

        // //qCDebug(KDENLIVE_LOG)<<"Looking for transition : " << currentIn <<'x'<<currentOut<< ", OLD oNE: "<<in_pos<<'x'<<out_pos;
        if (resource == type && b_track == currentTrack && currentIn == in_pos && currentOut == out_pos) {
            QMap<QString, QString> map = getTransitionParamsFromXml(xml);
            QMap<QString, QString>::Iterator it;
            QString key;
            mlt_properties transproperties = MLT_TRANSITION_PROPERTIES(tr);

            QString currentId = mlt_properties_get(transproperties, "kdenlive_id");
            if (currentId != xml.attribute(QStringLiteral("id"))) {
                // The transition ID is not the same, so reset all properties
                mlt_properties_set(transproperties, "kdenlive_id", xml.attribute(QStringLiteral("id")).toUtf8().constData());
                // Cleanup previous properties
                QStringList permanentProps;
                permanentProps << QStringLiteral("factory") << QStringLiteral("kdenlive_id") << QStringLiteral("mlt_service") << QStringLiteral("mlt_type") << QStringLiteral("in");
                permanentProps << QStringLiteral("out") << QStringLiteral("a_track") << QStringLiteral("b_track");
                for (int i = 0; i < mlt_properties_count(transproperties); ++i) {
                    QString propName = mlt_properties_get_name(transproperties, i);
                    if (!propName.startsWith('_') && ! permanentProps.contains(propName)) {
                        mlt_properties_set(transproperties, propName.toUtf8().constData(), "");
                    }
                }
            }

            mlt_properties_set_int(transproperties, "force_track", xml.attribute(QStringLiteral("force_track")).toInt());
            mlt_properties_set_int(transproperties, "automatic", xml.attribute(QStringLiteral("automatic"), QStringLiteral("0")).toInt());

            if (currentBTrack != a_track) {
                mlt_properties_set_int(transproperties, "a_track", a_track);
            }
            for (it = map.begin(); it != map.end(); ++it) {
                key = it.key();
                mlt_properties_set(transproperties, key.toUtf8().constData(), it.value().toUtf8().constData());
                //qCDebug(KDENLIVE_LOG) << " ------  UPDATING TRANS PARAM: " << key.toUtf8().constData() << ": " << it.value().toUtf8().constData();
                //filter->set("kdenlive_id", id);
            }
            break;
        }
        nextservice = mlt_service_producer(nextservice);
        if (nextservice == nullptr) {
            break;
        }
        properties = MLT_SERVICE_PROPERTIES(nextservice);
        mlt_type = mlt_service_identify(nextservice);
        resource = mlt_properties_get(properties, "mlt_service");
    }
    field->unlock();
    //askForRefresh();
    //if (m_isBlocked == 0) m_mltConsumer->set("refresh", 1);
}

bool TransitionHandler::deleteTransition(const QString &tag, int /*a_track*/, int b_track, GenTime in, GenTime out, const QDomElement &/*xml*/, bool /*do_refresh*/)
{
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();
    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString resource = mlt_properties_get(properties, "mlt_service");
    double fps = m_tractor->get_fps();
    const int old_pos = (int)((in + out).frames(fps) / 2);
    bool found = false;
    ////qCDebug(KDENLIVE_LOG) << " del trans pos: " << in.frames(25) << '-' << out.frames(25);

    mlt_service_type mlt_type = mlt_service_identify(nextservice);
    while (mlt_type == transition_type) {
        mlt_transition tr = (mlt_transition) nextservice;
        int currentTrack = mlt_transition_get_b_track(tr);
        int currentIn = (int) mlt_transition_get_in(tr);
        int currentOut = (int) mlt_transition_get_out(tr);
        ////qCDebug(KDENLIVE_LOG) << "// FOUND EXISTING TRANS, IN: " << currentIn << ", OUT: " << currentOut << ", TRACK: " << currentTrack;

        if (resource == tag && b_track == currentTrack && currentIn <= old_pos && currentOut >= old_pos) {
            found = true;
            mlt_field_disconnect_service(field->get_field(), nextservice);
            break;
        }
        nextservice = mlt_service_producer(nextservice);
        if (nextservice == nullptr) {
            break;
        }
        properties = MLT_SERVICE_PROPERTIES(nextservice);
        mlt_type = mlt_service_identify(nextservice);
        resource = mlt_properties_get(properties, "mlt_service");
    }
    field->unlock();
    //askForRefresh();
    //if (m_isBlocked == 0) m_mltConsumer->set("refresh", 1);
    return found;
}

void TransitionHandler::deleteTrackTransitions(int ix)
{
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_service_type type = mlt_service_identify(nextservice);
    while (type == transition_type) {
        Mlt::Transition transition((mlt_transition) nextservice);
        nextservice = mlt_service_producer(nextservice);
        int currentTrack = transition.get_b_track();
        if (ix == currentTrack) {
            field->disconnect_service(transition);
        }
        if (nextservice == nullptr) {
            break;
        }
        type = mlt_service_identify(nextservice);
    }
}

bool TransitionHandler::moveTransition(const QString &type, int startTrack, int newTrack, int newTransitionTrack, GenTime oldIn, GenTime oldOut, GenTime newIn, GenTime newOut)
{
    double fps = m_tractor->get_fps();
    int new_in = (int)newIn.frames(fps);
    int new_out = (int)newOut.frames(fps) - 1;
    if (new_in >= new_out) {
        return false;
    }
    int old_in = (int)oldIn.frames(fps);
    int old_out = (int)oldOut.frames(fps) - 1;

    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();
    mlt_service nextservice = mlt_service_get_producer(field->get_service());
    mlt_properties properties = MLT_SERVICE_PROPERTIES(nextservice);
    QString resource = mlt_properties_get(properties, "mlt_service");
    int old_pos = (int)(old_in + old_out) / 2;
    bool found = false;
    mlt_service_type mlt_type = mlt_service_identify(nextservice);
    while (mlt_type == transition_type) {
        Mlt::Transition transition((mlt_transition) nextservice);
        nextservice = mlt_service_producer(nextservice);
        int currentTrack = transition.get_b_track();
        int currentIn = (int) transition.get_in();
        int currentOut = (int) transition.get_out();

        if (resource == type && startTrack == currentTrack && currentIn <= old_pos && currentOut >= old_pos) {
            found = true;
            if (newTrack - startTrack != 0) {
                Mlt::Properties trans_props(transition.get_properties());
                Mlt::Transition new_transition(*m_tractor->profile(), transition.get("mlt_service"));
                Mlt::Properties new_trans_props(new_transition.get_properties());
                // We cannot use MLT's property inherit because it also clones internal values like _unique_id which messes up the playlist
                cloneProperties(new_trans_props, trans_props);
                new_transition.set_in_and_out(new_in, new_out);
                field->disconnect_service(transition);
                plantTransition(field.data(), new_transition, newTransitionTrack, newTrack);
            } else {
                transition.set_in_and_out(new_in, new_out);
            }
            break;
        }
        if (nextservice == nullptr) {
            break;
        }
        properties = MLT_SERVICE_PROPERTIES(nextservice);
        mlt_type = mlt_service_identify(nextservice);
        resource = mlt_properties_get(properties, "mlt_service");
    }
    field->unlock();
    //if (m_isBlocked == 0) m_mltConsumer->set("refresh", 1);
    return found;
}

Mlt::Transition *TransitionHandler::getTransition(const QString &name, int b_track, int a_track, bool internalTransition) const
{
    QScopedPointer<Mlt::Service> service(m_tractor->field());
    while (service && service->is_valid()) {
        if (service->type() == transition_type) {
            Mlt::Transition t((mlt_transition) service->get_service());
            if (name == t.get("mlt_service") && t.get_b_track() == b_track) {
                if (a_track == -1 || t.get_a_track() == a_track) {
                    int internal = t.get_int("internal_added");
                    if (internal == 0) {
                        if (!internalTransition) {
                            return new Mlt::Transition(t);
                        }
                    } else if (internalTransition) {
                        return new Mlt::Transition(t);
                    }
                }
            }
        }
        service.reset(service->producer());
    }
    return nullptr;
}

Mlt::Transition *TransitionHandler::getTrackTransition(const QStringList &names, int b_track, int a_track) const
{
    QScopedPointer<Mlt::Service> service(m_tractor->field());
    while (service && service->is_valid()) {
        if (service->type() == transition_type) {
            Mlt::Transition t((mlt_transition) service->get_service());
            int internal = t.get_int("internal_added");
            if (internal >= 200) {
                if (names.contains(t.get("mlt_service")) && t.get_b_track() == b_track && (a_track == -1 || t.get_a_track() == a_track)) {
                    return new Mlt::Transition(t);
                }
            }
        }
        service.reset(service->producer());
    }
    return nullptr;
}

void TransitionHandler::duplicateTransitionOnPlaylist(int in, int out, const QString &tag, const QDomElement &xml, int a_track, int b_track, Mlt::Field *field)
{
    QMap<QString, QString> args = getTransitionParamsFromXml(xml);
    Mlt::Transition transition(*m_tractor->profile(), tag.toUtf8().constData());
    if (!transition.is_valid()) {
        return;
    }
    if (out != 0) {
        transition.set_in_and_out(in, out);
    }

    QMap<QString, QString>::Iterator it;
    QString key;
    if (xml.attribute(QStringLiteral("automatic")) == QLatin1String("1")) {
        transition.set("automatic", 1);
    }
    ////qCDebug(KDENLIVE_LOG) << " ------  ADDING TRANSITION PARAMs: " << args.count();
    if (xml.hasAttribute(QStringLiteral("id"))) {
        transition.set("kdenlive_id", xml.attribute(QStringLiteral("id")).toUtf8().constData());
    }
    if (xml.hasAttribute(QStringLiteral("force_track"))) {
        transition.set("force_track", xml.attribute(QStringLiteral("force_track")).toInt());
    }

    for (it = args.begin(); it != args.end(); ++it) {
        key = it.key();
        if (!it.value().isEmpty()) {
            transition.set(key.toUtf8().constData(), it.value().toUtf8().constData());
        }
        ////qCDebug(KDENLIVE_LOG) << " ------  ADDING TRANS PARAM: " << key << ": " << it.value();
    }
    // attach transition
    field->plant_transition(transition, a_track, b_track);
}

void TransitionHandler::enableMultiTrack(bool enable)
{
    int tracks = m_tractor->count();
    if (tracks < 3) {
        // we need at leas 3 tracks (black bg track + 2 tracks to use this)
        return;
    }
    QStringList compositeService { QStringLiteral("qtblend"), QStringLiteral("frei0r.cairoblend"),  QStringLiteral("movit.overlay") };
    QScopedPointer<Mlt::Service> service(m_tractor->field());
    QScopedPointer<Mlt::Field> field(m_tractor->field());
    field->lock();
    if (enable) {
        // disable track composition (frei0r.cairoblend)
        mlt_service nextservice = mlt_service_get_producer(field->get_service());
        mlt_service_type type = mlt_service_identify(nextservice);
        while (type == transition_type) {
            Mlt::Transition transition((mlt_transition) nextservice);
            nextservice = mlt_service_producer(nextservice);
            int added = transition.get_int("internal_added");
            if (added == 237) {
                if (compositeService.contains(transition.get("mlt_service")) && transition.get_int("disable") == 0) {
                    transition.set("disable", 1);
                    transition.set("split_disable", 1);
                }
            }
            if (nextservice == nullptr) {
                break;
            }
            type = mlt_service_identify(nextservice);
        }
        for (int i = 1, screen = 0; i < tracks && screen < 4; ++i) {
            Mlt::Producer trackProducer(m_tractor->track(i));
            if (QString(trackProducer.get("hide")).toInt() != 1) {
                Mlt::Transition transition(*m_tractor->profile(), "composite");
                transition.set("mlt_service", "composite");
                transition.set("a_track", 0);
                transition.set("b_track", i);
                transition.set("distort", 0);
                transition.set("aligned", 0);
                // 200 is an arbitrary number so we can easily remove these transition later
                transition.set("internal_added", 200);
                QString geometry;
                switch (screen) {
                case 0:
                    geometry = QStringLiteral("0/0:50%x50%");
                    break;
                case 1:
                    geometry = QStringLiteral("50%/0:50%x50%");
                    break;
                case 2:
                    geometry = QStringLiteral("0/50%:50%x50%");
                    break;
                case 3:
                default:
                    geometry = QStringLiteral("50%/50%:50%x50%");
                    break;
                }
                transition.set("geometry", geometry.toUtf8().constData());
                transition.set("always_active", 1);
                field->plant_transition(transition, 0, i);
                screen++;
            }
        }
    } else {
        mlt_service nextservice = mlt_service_get_producer(field->get_service());
        mlt_service_type type = mlt_service_identify(nextservice);
        while (type == transition_type) {
            Mlt::Transition transition((mlt_transition) nextservice);
            nextservice = mlt_service_producer(nextservice);
            int added = transition.get_int("internal_added");
            if (added == 200) {
                field->disconnect_service(transition);
            } else if (added == 237) {
                // re-enable track compositing
                if (compositeService.contains(transition.get("mlt_service")) && transition.get_int("split_disable") == 1) {
                    transition.set("disable", 0);
                    transition.set("split_disable", (char *) nullptr);
                }
            }
            if (nextservice == nullptr) {
                break;
            }
            type = mlt_service_identify(nextservice);
        }
    }
    field->unlock();
    emit refresh();
}

// static
const QString TransitionHandler::compositeTransition()
{
    if (KdenliveSettings::gpu_accel()) {
        return QStringLiteral("movit.overlay");
    }
    if (MainWindow::transitions.hasTransition(QStringLiteral("qtblend"))) {
        return QStringLiteral("qtblend");
    }
    if (MainWindow::transitions.hasTransition(QStringLiteral("frei0r.cairoblend"))) {
        return QStringLiteral("frei0r.cairoblend");
    }
    return QStringLiteral("composite");
}

void TransitionHandler::rebuildTransitions(int mode, const QList<int> &videoTracks, int maxTrack)
{
    QStringList compositeService { QStringLiteral("qtblend"), QStringLiteral("composite"), QStringLiteral("frei0r.cairoblend"),  QStringLiteral("movit.overlay") };
    QList<int> disabled;
    QScopedPointer<Mlt::Service> service(m_tractor->field());
    Mlt::Field *field = m_tractor->field();
    field->lock();
    // Get the list of composite transitions
    while (service && service->is_valid()) {
        if (service->type() == transition_type) {
            Mlt::Transition t((mlt_transition) service->get_service());
            int internal = t.get_int("internal_added");
            if (internal == 237) {
                QString serviceName = t.get("mlt_service");
                if (serviceName == QLatin1String("mix")) {
                    field->disconnect_service(t);
                } else if (compositeService.contains(serviceName)) {
                    if (mode < 0) {
                        mode = serviceName == QLatin1String("composite") ? 1 : 2;
                    }
                    field->disconnect_service(t);
                }
            }
        }
        service.reset(service->producer());
    }
    // Rebuild audio mix
    for (int i = 1; i < maxTrack; i++) {
        Mlt::Transition transition(*m_tractor->profile(), "mix");
        transition.set("always_active", 1);
        transition.set("combine", 1);
        transition.set("a_track", 0);
        transition.set("b_track", i);
        transition.set("internal_added", 237);
        field->plant_transition(transition, 0, i);
    }

    if (mode <= 0) {
        // no compositing wanted, return
        field->unlock();
        delete field;
        return;
    }
    // Re-add correct composite transitions
    QString composite;
    QString compositeGeometry;
    if (mode == 1) {
        composite = QStringLiteral("composite");
        compositeGeometry = QStringLiteral("0=0/0:%1x%2").arg(m_tractor->profile()->width()).arg(m_tractor->profile()->height());
    } else {
        composite = compositeTransition();
    }
    foreach (int track,  videoTracks) {
        Mlt::Transition transition(*m_tractor->profile(), composite.toUtf8().constData());
        transition.set("always_active", 1);
        transition.set("a_track", 0);
        transition.set("b_track", track);
        if (mode == 1) {
            transition.set("valign", "middle");
            transition.set("halign", "centre");
            transition.set("fill", 1);
            transition.set("geometry", compositeGeometry.toUtf8().constData());
        }
        transition.set("internal_added", 237);
        field->plant_transition(transition, 0, track);
    }
    field->unlock();
    delete field;
}
