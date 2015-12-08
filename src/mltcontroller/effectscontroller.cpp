/*
Copyright (C) 2012  Till Theato <root@ttill.de>
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy 
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "effectscontroller.h"
#include "dialogs/profilesdialog.h"

#include <QUrl>
#include <QDebug>
#include <QPixmap>
#include <QScriptEngine>
#include <KLocalizedString>

EffectInfo::EffectInfo() {isCollapsed = false; groupIndex = -1; groupIsCollapsed = false;}

QString EffectInfo::toString() const {
    QStringList data;
    // effect collapsed state: 0 = effect not collapsed, 1 = effect collapsed,
    // 2 = group collapsed - effect not, 3 = group and effect collapsed
    int collapsedState = (int) isCollapsed;
    if (groupIsCollapsed) collapsedState += 2;
    data << QString::number(collapsedState) << QString::number(groupIndex) << groupName;
    return data.join(QLatin1String("/"));
}

void EffectInfo::fromString(QString value) {
    if (value.isEmpty()) return;
    QStringList data = value.split(QLatin1String("/"));
    isCollapsed = data.at(0).toInt() == 1 || data.at(0).toInt() == 3;
    groupIsCollapsed = data.at(0).toInt() >= 2;
    if (data.count() > 1) groupIndex = data.at(1).toInt();
    if (data.count() > 2) groupName = data.at(2);
}


EffectParameter::EffectParameter(const QString &name, const QString &value): m_name(name), m_value(value) {}

QString EffectParameter::name() const          {
    return m_name;
}

QString EffectParameter::value() const          {
    return m_value;
}

void EffectParameter::setValue(const QString &value) {
    m_value = value;
}


EffectsParameterList::EffectsParameterList(): QList < EffectParameter >() {}

bool EffectsParameterList::hasParam(const QString &name) const {
    for (int i = 0; i < size(); ++i)
        if (at(i).name() == name) return true;
    return false;
}

QString EffectsParameterList::paramValue(const QString &name, const QString &defaultValue) const {
    for (int i = 0; i < size(); ++i) {
        if (at(i).name() == name) return at(i).value();
    }
    return defaultValue;
}

void EffectsParameterList::addParam(const QString &name, const QString &value) {
    if (name.isEmpty()) return;
    append(EffectParameter(name, value));
}

void EffectsParameterList::removeParam(const QString &name) {
    for (int i = 0; i < size(); ++i)
        if (at(i).name() == name) {
            removeAt(i);
            break;
        }
}

EffectsParameterList EffectsController::getEffectArgs(const ProfileInfo &info, const QDomElement &effect)
{
    EffectsParameterList parameters;
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    parameters.addParam("tag", effect.attribute("tag"));
    //if (effect.hasAttribute("region")) parameters.addParam("region", effect.attribute("region"));
    parameters.addParam("kdenlive_ix", effect.attribute("kdenlive_ix"));
    parameters.addParam("kdenlive_info", effect.attribute("kdenlive_info"));
    parameters.addParam("id", effect.attribute("id"));
    if (effect.hasAttribute("src")) parameters.addParam("src", effect.attribute("src"));
    if (effect.hasAttribute("disable")) parameters.addParam("disable", effect.attribute("disable"));
    if (effect.hasAttribute("in")) parameters.addParam("in", effect.attribute("in"));
    if (effect.hasAttribute("out")) parameters.addParam("out", effect.attribute("out"));
    if (effect.attribute("id") == "region") {
        QDomNodeList subeffects = effect.elementsByTagName("effect");
        for (int i = 0; i < subeffects.count(); ++i) {
            QDomElement subeffect = subeffects.at(i).toElement();
            int subeffectix = subeffect.attribute("region_ix").toInt();
            parameters.addParam(QString("filter%1").arg(subeffectix), subeffect.attribute("id"));
            parameters.addParam(QString("filter%1.tag").arg(subeffectix), subeffect.attribute("tag"));
            parameters.addParam(QString("filter%1.kdenlive_info").arg(subeffectix), subeffect.attribute("kdenlive_info"));
            QDomNodeList subparams = subeffect.elementsByTagName("parameter");
            adjustEffectParameters(parameters, subparams, info, QString("filter%1.").arg(subeffectix));
        }
    }

    QDomNodeList params = effect.elementsByTagName("parameter");
    adjustEffectParameters(parameters, params, info);
    return parameters;
}


void EffectsController::adjustEffectParameters(EffectsParameterList &parameters, QDomNodeList params, const ProfileInfo &info, const QString &prefix)
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        QString paramname = prefix + e.attribute("name");
        if (e.attribute("type") == "geometry" && !e.hasAttribute("fixed")) {
            // effects with geometry param need in / out synced with the clip, request it...
            parameters.addParam("_sync_in_out", "1");
        }
        if (e.attribute("type") == "simplekeyframe") {
            QStringList values = e.attribute("keyframes").split(';', QString::SkipEmptyParts);
            double factor = e.attribute("factor", "1").toDouble();
            double offset = e.attribute("offset", "0").toDouble();
            for (int j = 0; j < values.count(); ++j) {
                QString pos = values.at(j).section('=', 0, 0);
                double val = (values.at(j).section('=', 1, 1).toDouble() - offset) / factor;
                values[j] = pos + '=' + locale.toString(val);
            }
            // //qDebug() << "/ / / /SENDING KEYFR:" << values;
            parameters.addParam(paramname, values.join(";"));
            /*parameters.addParam(e.attribute("name"), e.attribute("keyframes").replace(":", "="));
            parameters.addParam("max", e.attribute("max"));
            parameters.addParam("min", e.attribute("min"));
            parameters.addParam("factor", e.attribute("factor", "1"));*/
        } else if (e.attribute("type") == "keyframe") {
            //qDebug() << "/ / / /SENDING KEYFR EFFECT TYPE";
            parameters.addParam("keyframes", e.attribute("keyframes"));
            parameters.addParam("max", e.attribute("max"));
            parameters.addParam("min", e.attribute("min"));
            parameters.addParam("factor", e.attribute("factor", "1"));
            parameters.addParam("offset", e.attribute("offset", "0"));
            parameters.addParam("starttag", e.attribute("starttag", "start"));
            parameters.addParam("endtag", e.attribute("endtag", "end"));
        } else if (e.attribute("namedesc").contains(';')) {
            QString format = e.attribute("format");
            QStringList separators = format.split("%d", QString::SkipEmptyParts);
            QStringList values = e.attribute("value").split(QRegExp("[,:;x]"));
            QString neu;
            QTextStream txtNeu(&neu);
            if (values.size() > 0)
                txtNeu << (int)values[0].toDouble();
            for (int i = 0; i < separators.size() && i + 1 < values.size(); ++i) {
                txtNeu << separators[i];
                txtNeu << (int)(values[i+1].toDouble());
            }
            parameters.addParam("start", neu);
        } else {
            if (e.attribute("factor", "1") != "1" || e.attribute("offset", "0") != "0") {
                double fact;
                if (e.attribute("factor").contains('%')) {
                    fact = getStringEval(info, e.attribute("factor"));
                } else {
                    fact = e.attribute("factor", "1").toDouble();
                }
                double offset = e.attribute("offset", "0").toDouble();
                parameters.addParam(paramname, locale.toString((e.attribute("value").toDouble() - offset) / fact));
            } else {
                parameters.addParam(paramname, e.attribute("value"));
            }
        }
    }
}


double EffectsController::getStringEval(const ProfileInfo &info, QString eval, const QPoint& frameSize)
{
    QScriptEngine sEngine;
    sEngine.globalObject().setProperty("maxWidth", info.profileSize.width() > frameSize.x() ? info.profileSize.width() : frameSize.x());
    sEngine.globalObject().setProperty("maxHeight", info.profileSize.height() > frameSize.y() ? info.profileSize.height() : frameSize.y());
    sEngine.globalObject().setProperty("width", info.profileSize.width());
    sEngine.globalObject().setProperty("height", info.profileSize.height());
    return sEngine.evaluate(eval.remove('%')).toNumber();
}

void EffectsController::initEffect(ItemInfo info, ProfileInfo pInfo, EffectsList list, const QString proxy, QDomElement effect, int diff, int offset)
{
    // the kdenlive_ix int is used to identify an effect in mlt's playlist, should
    // not be changed

    if (effect.attribute("id") == "freeze" && diff > 0) {
        EffectsList::setParameter(effect, "frame", QString::number(diff));
    }
    double fps = pInfo.profileFps;
    // Init parameter value & keyframes if required
    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();

        if (e.isNull())
            continue;

        // Check if this effect has a variable parameter
        if (e.attribute("default").contains('%')) {
            double evaluatedValue = EffectsController::getStringEval(pInfo, e.attribute("default"));
            e.setAttribute("default", evaluatedValue);
            if (e.hasAttribute("value") && e.attribute("value").startsWith('%')) {
                e.setAttribute("value", evaluatedValue);
            }
        }

        if (effect.attribute("id") == "crop") {
            // default use_profile to 1 for clips with proxies to avoid problems when rendering
            if (e.attribute("name") == "use_profile" && !(proxy.isEmpty() || proxy == "-"))
                e.setAttribute("value", "1");
        }

        if (e.attribute("type") == "keyframe" || e.attribute("type") == "simplekeyframe") {
            if (e.attribute("keyframes").isEmpty()) {
                // Effect has a keyframe type parameter, we need to set the values
                e.setAttribute("keyframes", QString::number((int) info.cropStart.frames(fps)) + '=' + e.attribute("default"));
            }
            else if (offset != 0) {
                // adjust keyframes to this clip
                QString adjusted = adjustKeyframes(e.attribute("keyframes"), offset - info.cropStart.frames(fps));
                e.setAttribute("keyframes", adjusted);
            }
        }

        if (e.attribute("type") == "geometry" && !e.hasAttribute("fixed")) {
            // Effects with a geometry parameter need to sync in / out with parent clip
            effect.setAttribute("in", QString::number((int) info.cropStart.frames(fps)));
            effect.setAttribute("out", QString::number((int) (info.cropStart + info.cropDuration).frames(fps) - 1));
            effect.setAttribute("_sync_in_out", "1");
        }
    }
    if (effect.attribute("tag") == "volume" || effect.attribute("tag") == "brightness") {
        if (effect.attribute("id") == "fadeout" || effect.attribute("id") == "fade_to_black") {
            int end = (info.cropDuration + info.cropStart).frames(fps) - 1;
            int start = end;
            if (effect.attribute("id") == "fadeout") {
                if (list.hasEffect(QString(), "fade_to_black") == -1) {
                    int effectDuration = EffectsList::parameter(effect, "out").toInt() - EffectsList::parameter(effect, "in").toInt();
                    if (effectDuration > info.cropDuration.frames(fps)) {
                        effectDuration = info.cropDuration.frames(fps) / 2;
                    }
                    start -= effectDuration;
                } else {
                    QDomElement fadeout = list.getEffectByTag(QString(), "fade_to_black");
                    start -= EffectsList::parameter(fadeout, "out").toInt() - EffectsList::parameter(fadeout, "in").toInt();
                }
            } else if (effect.attribute("id") == "fade_to_black") {
                if (list.hasEffect(QString(), "fadeout") == -1) {
                    int effectDuration = EffectsList::parameter(effect, "out").toInt() - EffectsList::parameter(effect, "in").toInt();
                    if (effectDuration > info.cropDuration.frames(fps)) {
                        effectDuration = info.cropDuration.frames(fps) / 2;
                    }
                    start -= effectDuration;
                } else {
                    QDomElement fadeout = list.getEffectByTag(QString(), "fadeout");
                    start -= EffectsList::parameter(fadeout, "out").toInt() - EffectsList::parameter(fadeout, "in").toInt();
                }
            }
            EffectsList::setParameter(effect, "in", QString::number(start));
            EffectsList::setParameter(effect, "out", QString::number(end));
        } else if (effect.attribute("id") == "fadein" || effect.attribute("id") == "fade_from_black") {
            int start = info.cropStart.frames(fps);
            int end = start;
            if (effect.attribute("id") == "fadein") {
                if (list.hasEffect(QString(), "fade_from_black") == -1) {
                    int effectDuration = EffectsList::parameter(effect, "out").toInt();
                    if (offset != 0) effectDuration -= offset;
                    if (effectDuration > info.cropDuration.frames(fps)) {
                        effectDuration = info.cropDuration.frames(fps) / 2;
                    }
                    end += effectDuration;
                } else {
		    QDomElement fadein = list.getEffectByTag(QString(), "fade_from_black");
                    end = EffectsList::parameter(fadein, "out").toInt() - offset;
		}
            } else if (effect.attribute("id") == "fade_from_black") {
                if (list.hasEffect(QString(), "fadein") == -1) {
                    int effectDuration = EffectsList::parameter(effect, "out").toInt();
                    if (offset != 0) effectDuration -= offset;
                    if (effectDuration > info.cropDuration.frames(fps)) {
                        effectDuration = info.cropDuration.frames(fps) / 2;
                    }
                    end += effectDuration;
                } else {
		    QDomElement fadein = list.getEffectByTag(QString(), "fadein");
                    end = EffectsList::parameter(fadein, "out").toInt() - offset;
		}
            }
            EffectsList::setParameter(effect, "in", QString::number(start));
            EffectsList::setParameter(effect, "out", QString::number(end));
        }
    }
}

const QString EffectsController::adjustKeyframes(const QString &keyframes, int offset)
{
    QStringList result;
    // Simple keyframes
    const QStringList list = keyframes.split(QLatin1Char(';'), QString::SkipEmptyParts);
    foreach(const QString &keyframe, list) {
        const int pos = keyframe.section('=', 0, 0).toInt() - offset;
        const QString newKey = QString::number(pos) + '=' + keyframe.section('=', 1);
        result.append(newKey);
    }
    return result.join(";");
}

EffectsParameterList EffectsController::addEffect(const ProfileInfo &info, QDomElement effect)
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    EffectsParameterList parameters;
    parameters.addParam("tag", effect.attribute("tag"));
    parameters.addParam("kdenlive_ix", effect.attribute("kdenlive_ix"));
    if (effect.hasAttribute("src")) parameters.addParam("src", effect.attribute("src"));
    if (effect.hasAttribute("disable")) parameters.addParam("disable", effect.attribute("disable"));

    QString effectId = effect.attribute("id");
    if (effectId.isEmpty()) effectId = effect.attribute("tag");
    parameters.addParam("id", effectId);

    QDomNodeList params = effect.elementsByTagName("parameter");
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull()) {
            if (e.attribute("type") == "simplekeyframe") {
                QStringList values = e.attribute("keyframes").split(';', QString::SkipEmptyParts);
                double factor = locale.toDouble(e.attribute("factor", "1"));
                double offset = e.attribute("offset", "0").toDouble();
                if (factor != 1 || offset != 0) {
                    for (int j = 0; j < values.count(); ++j) {
                        QString pos = values.at(j).section('=', 0, 0);
                        double val = (locale.toDouble(values.at(j).section('=', 1, 1)) - offset) / factor;
                        values[j] = pos + '=' + locale.toString(val);
                    }
                }
                parameters.addParam(e.attribute("name"), values.join(";"));
                /*parameters.addParam("max", e.attribute("max"));
                parameters.addParam("min", e.attribute("min"));
                parameters.addParam("factor", );*/
            } else if (e.attribute("type") == "keyframe") {
                parameters.addParam("keyframes", e.attribute("keyframes"));
                parameters.addParam("max", e.attribute("max"));
                parameters.addParam("min", e.attribute("min"));
                parameters.addParam("factor", e.attribute("factor", "1"));
                parameters.addParam("offset", e.attribute("offset", "0"));
                parameters.addParam("starttag", e.attribute("starttag", "start"));
                parameters.addParam("endtag", e.attribute("endtag", "end"));
            } else if (e.attribute("factor", "1") == "1" && e.attribute("offset", "0") == "0") {
                parameters.addParam(e.attribute("name"), e.attribute("value"));

            } else {
                double fact;
                if (e.attribute("factor").contains('%')) {
                    fact = EffectsController::getStringEval(info, e.attribute("factor"));
                } else {
                    fact = locale.toDouble(e.attribute("factor", "1"));
                }
                double offset = e.attribute("offset", "0").toDouble();
                parameters.addParam(e.attribute("name"), locale.toString((locale.toDouble(e.attribute("value")) - offset) / fact));
            }
        }
    }
    return parameters;
}
