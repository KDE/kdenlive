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
#include "effectstack/widgets/animationwidget.h"

#include "kdenlive_debug.h"

EffectInfo::EffectInfo()
{
    isCollapsed = false;
    groupIndex = -1;
    groupIsCollapsed = false;
}

QString EffectInfo::toString() const
{
    QStringList data;
    // effect collapsed state: 0 = effect not collapsed, 1 = effect collapsed,
    // 2 = group collapsed - effect not, 3 = group and effect collapsed
    int collapsedState = (int) isCollapsed;
    if (groupIsCollapsed) {
        collapsedState += 2;
    }
    data << QString::number(collapsedState) << QString::number(groupIndex) << groupName;
    return data.join(QLatin1Char('/'));
}

void EffectInfo::fromString(const QString &value)
{
    if (value.isEmpty()) {
        return;
    }
    QStringList data = value.split(QLatin1Char('/'));
    isCollapsed = data.at(0).toInt() == 1 || data.at(0).toInt() == 3;
    groupIsCollapsed = data.at(0).toInt() >= 2;
    if (data.count() > 1) {
        groupIndex = data.at(1).toInt();
    }
    if (data.count() > 2) {
        groupName = data.at(2);
    }
}

EffectParameter::EffectParameter(const QString &name, const QString &value): m_name(name), m_value(value) {}

QString EffectParameter::name() const
{
    return m_name;
}

QString EffectParameter::value() const
{
    return m_value;
}

void EffectParameter::setValue(const QString &value)
{
    m_value = value;
}

EffectsParameterList::EffectsParameterList(): QList< EffectParameter >() {}

bool EffectsParameterList::hasParam(const QString &name) const
{
    for (int i = 0; i < size(); ++i)
        if (at(i).name() == name) {
            return true;
        }
    return false;
}

QString EffectsParameterList::paramValue(const QString &name, const QString &defaultValue) const
{
    for (int i = 0; i < size(); ++i) {
        if (at(i).name() == name) {
            return at(i).value();
        }
    }
    return defaultValue;
}

void EffectsParameterList::addParam(const QString &name, const QString &value)
{
    if (name.isEmpty()) {
        return;
    }
    append(EffectParameter(name, value));
}

void EffectsParameterList::removeParam(const QString &name)
{
    for (int i = 0; i < size(); ++i)
        if (at(i).name() == name) {
            removeAt(i);
            break;
        }
}

EffectsParameterList EffectsController::getEffectArgs(const ProfileInfo &info, const QDomElement &effect)
{
    EffectsParameterList parameters;
    parameters.addParam(QStringLiteral("tag"), effect.attribute(QStringLiteral("tag")));
    //if (effect.hasAttribute("region")) parameters.addParam("region", effect.attribute("region"));
    parameters.addParam(QStringLiteral("kdenlive_ix"), effect.attribute(QStringLiteral("kdenlive_ix")));
    if (effect.hasAttribute(QStringLiteral("sync_in_out"))) {
        parameters.addParam(QStringLiteral("kdenlive:sync_in_out"), effect.attribute(QStringLiteral("sync_in_out")));
    }
    parameters.addParam(QStringLiteral("kdenlive_info"), effect.attribute(QStringLiteral("kdenlive_info")));
    parameters.addParam(QStringLiteral("id"), effect.attribute(QStringLiteral("id")));
    if (effect.hasAttribute(QStringLiteral("src"))) {
        parameters.addParam(QStringLiteral("src"), effect.attribute(QStringLiteral("src")));
    }
    if (effect.hasAttribute(QStringLiteral("disable"))) {
        parameters.addParam(QStringLiteral("disable"), effect.attribute(QStringLiteral("disable")));
    }
    /*if (effect.hasAttribute(QStringLiteral("in"))) parameters.addParam(QStringLiteral("in"), effect.attribute(QStringLiteral("in")));
    if (effect.hasAttribute(QStringLiteral("out"))) parameters.addParam(QStringLiteral("out"), effect.attribute(QStringLiteral("out")));*/
    if (effect.attribute(QStringLiteral("id")) == QLatin1String("region")) {
        QDomNodeList subeffects = effect.elementsByTagName(QStringLiteral("effect"));
        for (int i = 0; i < subeffects.count(); ++i) {
            QDomElement subeffect = subeffects.at(i).toElement();
            int subeffectix = subeffect.attribute(QStringLiteral("region_ix")).toInt();
            parameters.addParam(QStringLiteral("filter%1").arg(subeffectix), subeffect.attribute(QStringLiteral("id")));
            parameters.addParam(QStringLiteral("filter%1.tag").arg(subeffectix), subeffect.attribute(QStringLiteral("tag")));
            parameters.addParam(QStringLiteral("filter%1.kdenlive_info").arg(subeffectix), subeffect.attribute(QStringLiteral("kdenlive_info")));
            QDomNodeList subparams = subeffect.elementsByTagName(QStringLiteral("parameter"));
            adjustEffectParameters(parameters, subparams, info, QStringLiteral("filter%1.").arg(subeffectix));
        }
    }

    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    adjustEffectParameters(parameters, params, info);
    return parameters;
}

void EffectsController::adjustEffectParameters(EffectsParameterList &parameters, const QDomNodeList &params, const ProfileInfo &info, const QString &prefix)
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        QString paramname = prefix + e.attribute(QStringLiteral("name"));
        /*if (e.attribute(QStringLiteral("type")) == QLatin1String("animated") || (e.attribute(QStringLiteral("type")) == QLatin1String("geometry") && !e.hasAttribute(QStringLiteral("fixed")))) {
            // effects with geometry param need in / out synced with the clip, request it...
            parameters.addParam(QStringLiteral("kdenlive:sync_in_out"), QStringLiteral("1"));
            qCDebug(KDENLIVE_LOG)<<" ** * ADDIN EFFECT ANIM SYN TRUE";
        }*/
        if (e.attribute(QStringLiteral("type")) == QLatin1String("animated")) {
            parameters.addParam(paramname, e.attribute(QStringLiteral("value")));
        } else if (e.attribute(QStringLiteral("type")) == QLatin1String("simplekeyframe")) {
            QStringList values = e.attribute(QStringLiteral("keyframes")).split(QLatin1Char(';'), QString::SkipEmptyParts);
            double factor = e.attribute(QStringLiteral("factor"), QStringLiteral("1")).toDouble();
            double offset = e.attribute(QStringLiteral("offset"), QStringLiteral("0")).toDouble();
            for (int j = 0; j < values.count(); ++j) {
                QString pos = values.at(j).section(QLatin1Char('='), 0, 0);
                double val = (values.at(j).section(QLatin1Char('='), 1, 1).toDouble() - offset) / factor;
                values[j] = pos + QLatin1Char('=') + locale.toString(val);
            }
            // //qCDebug(KDENLIVE_LOG) << "/ / / /SENDING KEYFR:" << values;
            parameters.addParam(paramname, values.join(QLatin1Char(';')));
            /*parameters.addParam(e.attribute("name"), e.attribute("keyframes").replace(":", "="));
            parameters.addParam("max", e.attribute("max"));
            parameters.addParam("min", e.attribute("min"));
            parameters.addParam("factor", e.attribute("factor", "1"));*/
        } else if (e.attribute(QStringLiteral("type")) == QLatin1String("keyframe")) {
            //qCDebug(KDENLIVE_LOG) << "/ / / /SENDING KEYFR EFFECT TYPE";
            parameters.addParam(QStringLiteral("keyframes"), e.attribute(QStringLiteral("keyframes")));
            parameters.addParam(QStringLiteral("max"), e.attribute(QStringLiteral("max")));
            parameters.addParam(QStringLiteral("min"), e.attribute(QStringLiteral("min")));
            parameters.addParam(QStringLiteral("factor"), e.attribute(QStringLiteral("factor"), QStringLiteral("1")));
            parameters.addParam(QStringLiteral("offset"), e.attribute(QStringLiteral("offset"), QStringLiteral("0")));
            parameters.addParam(QStringLiteral("starttag"), e.attribute(QStringLiteral("starttag"), QStringLiteral("start")));
            parameters.addParam(QStringLiteral("endtag"), e.attribute(QStringLiteral("endtag"), QStringLiteral("end")));
        } else if (e.attribute(QStringLiteral("namedesc")).contains(QLatin1Char(';'))) {
            //TODO: Deprecated, does not seem used anywhere...
            QString format = e.attribute(QStringLiteral("format"));
            QStringList separators = format.split(QStringLiteral("%d"), QString::SkipEmptyParts);
            QStringList values = e.attribute(QStringLiteral("value")).split(QRegExp(QStringLiteral("[,:;x]")));
            QString neu;
            QTextStream txtNeu(&neu);
            if (!values.isEmpty()) {
                txtNeu << (int)values[0].toDouble();
            }
            for (int i = 0; i < separators.size() && i + 1 < values.size(); ++i) {
                txtNeu << separators[i];
                txtNeu << (int)(values[i + 1].toDouble());
            }
            parameters.addParam(QStringLiteral("start"), neu);
        } else {
            if (e.attribute(QStringLiteral("factor"), QStringLiteral("1")) != QLatin1String("1") || e.attribute(QStringLiteral("offset"), QStringLiteral("0")) != QLatin1String("0")) {
                double fact;
                if (e.attribute(QStringLiteral("factor")).contains(QLatin1Char('%'))) {
                    fact = getStringEval(info, e.attribute(QStringLiteral("factor")));
                } else {
                    fact = locale.toDouble(e.attribute(QStringLiteral("factor"), QStringLiteral("1")));
                }
                double offset = e.attribute(QStringLiteral("offset"), QStringLiteral("0")).toDouble();
                parameters.addParam(paramname, locale.toString((locale.toDouble(e.attribute(QStringLiteral("value"))) - offset) / fact));
            } else {
                parameters.addParam(paramname, e.attribute(QStringLiteral("value")));
            }
        }
    }
}

double EffectsController::getStringEval(const ProfileInfo &info, QString eval, const QPoint &frameSize)
{
    eval.replace(QLatin1String("%maxWidth"),  QString::number(info.profileSize.width() > frameSize.x() ? info.profileSize.width() : frameSize.x()))
        .replace(QLatin1String("%maxHeight"), QString::number(info.profileSize.height() > frameSize.y() ? info.profileSize.height() : frameSize.y()))
        .replace(QLatin1String("%width"),     QString::number(info.profileSize.width()))
        .replace(QLatin1String("%height"),    QString::number(info.profileSize.height()));
    Mlt::Properties p;
    p.set("eval", eval.toLatin1().constData());
    return p.get_double("eval");
}

QString EffectsController::getStringRectEval(const ProfileInfo &info, QString eval)
{
    eval.replace(QStringLiteral("%width"), QString::number(info.profileSize.width()));
    eval.replace(QStringLiteral("%height"), QString::number(info.profileSize.height()));
    return eval;
}

void EffectsController::initTrackEffect(ProfileInfo pInfo, const QDomElement &effect)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        const QString type = e.attribute(QStringLiteral("type"));

        if (e.isNull()) {
            continue;
        }

        bool hasValue = e.hasAttribute(QStringLiteral("value"));
        // Check if this effect has a variable parameter, init effects default value
        if ((type == QLatin1String("animatedrect") || type == QLatin1String("geometry")) && !hasValue) {
            QString kfr = AnimationWidget::getDefaultKeyframes(0, e.attribute(QStringLiteral("default")), type == QLatin1String("geometry"));
            if (kfr.contains(QLatin1String("%"))) {
                kfr = EffectsController::getStringRectEval(pInfo, kfr);
            }
            e.setAttribute(QStringLiteral("value"), kfr);
        } else if (e.attribute(QStringLiteral("default")).contains(QLatin1Char('%'))) {
            double evaluatedValue = EffectsController::getStringEval(pInfo, e.attribute(QStringLiteral("default")));
            e.setAttribute(QStringLiteral("default"), evaluatedValue);
            if (e.hasAttribute(QStringLiteral("value"))) {
                if (e.attribute(QStringLiteral("value")).startsWith(QLatin1Char('%'))) {
                    e.setAttribute(QStringLiteral("value"), evaluatedValue);
                }
            } else {
                e.setAttribute(QStringLiteral("value"), evaluatedValue);
            }
        } else  if (!hasValue) {
            if (type == QLatin1String("animated")) {
                e.setAttribute(QStringLiteral("value"), AnimationWidget::getDefaultKeyframes(0, e.attribute(QStringLiteral("default"))));
            } else if (type == QLatin1String("keyframe") || type == QLatin1String("simplekeyframe")) {
                // Effect has a keyframe type parameter, we need to set the values
                e.setAttribute(QStringLiteral("keyframes"), QStringLiteral("0=") + e.attribute(QStringLiteral("default")));
            } else {
                e.setAttribute(QStringLiteral("value"), e.attribute(QStringLiteral("default")));
            }
        }
    }
}

void EffectsController::initEffect(const ItemInfo &info, ProfileInfo pInfo, const EffectsList &list, const QString &proxy, QDomElement effect, int diff, int offset)
{
    // the kdenlive_ix int is used to identify an effect in mlt's playlist, should
    // not be changed
    if (effect.attribute(QStringLiteral("id")) == QLatin1String("freeze") && diff > 0) {
        EffectsList::setParameter(effect, QStringLiteral("frame"), QString::number(diff));
    }
    double fps = pInfo.profileFps;
    // Init parameter value & keyframes if required
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        const QString type = e.attribute(QStringLiteral("type"));

        if (e.isNull()) {
            continue;
        }

        bool hasValue = e.hasAttribute(QStringLiteral("value"));
        // Check if this effect has a variable parameter, init effects default value
        if ((type == QLatin1String("animatedrect") || type == QLatin1String("geometry")) && !hasValue) {
            int pos = type == QLatin1String("geometry") ? 0 : info.cropStart.frames(fps);
            QString kfr = AnimationWidget::getDefaultKeyframes(pos, e.attribute(QStringLiteral("default")), type == QLatin1String("geometry"));
            if (kfr.contains(QLatin1String("%"))) {
                kfr = EffectsController::getStringRectEval(pInfo, kfr);
            }
            e.setAttribute(QStringLiteral("value"), kfr);
        } else if (e.attribute(QStringLiteral("default")).contains(QLatin1Char('%'))) {
            double evaluatedValue = EffectsController::getStringEval(pInfo, e.attribute(QStringLiteral("default")));
            e.setAttribute(QStringLiteral("default"), evaluatedValue);
            if (e.hasAttribute(QStringLiteral("value"))) {
                if (e.attribute(QStringLiteral("value")).startsWith(QLatin1Char('%'))) {
                    e.setAttribute(QStringLiteral("value"), evaluatedValue);
                }
            } else {
                e.setAttribute(QStringLiteral("value"), evaluatedValue);
            }
        } else {
            if (type == QLatin1String("animated") && !hasValue) {
                e.setAttribute(QStringLiteral("value"), AnimationWidget::getDefaultKeyframes(info.cropStart.frames(fps), e.attribute(QStringLiteral("default"))));
            } else if (!hasValue) {
                e.setAttribute(QStringLiteral("value"), e.attribute(QStringLiteral("default")));
            }
        }

        if (effect.attribute(QStringLiteral("id")) == QLatin1String("crop")) {
            // default use_profile to 1 for clips with proxies to avoid problems when rendering
            if (e.attribute(QStringLiteral("name")) == QLatin1String("use_profile") && !(proxy.isEmpty() || proxy == QLatin1String("-"))) {
                e.setAttribute(QStringLiteral("value"), QStringLiteral("1"));
            }
        }
        if (type == QLatin1String("keyframe") || type == QLatin1String("simplekeyframe")) {
            if (e.attribute(QStringLiteral("keyframes")).isEmpty()) {
                // Effect has a keyframe type parameter, we need to set the values
                e.setAttribute(QStringLiteral("keyframes"), QString::number((int) info.cropStart.frames(fps)) + QLatin1Char('=') + e.attribute(QStringLiteral("default")));
            } else {
                // adjust keyframes to this clip
                QString adjusted = adjustKeyframes(e.attribute(QStringLiteral("keyframes")), offset, info.cropStart.frames(fps), (info.cropStart + info.cropDuration).frames(fps) - 1, pInfo);
                e.setAttribute(QStringLiteral("keyframes"), adjusted);
            }
        }

        if (EffectsList::parameter(effect, QStringLiteral("kdenlive:sync_in_out")) == QLatin1String("1")  || (type == QLatin1String("geometry") && !e.hasAttribute(QStringLiteral("fixed")))) {
            // Effects with a geometry parameter need to sync in / out with parent clip
            effect.setAttribute(QStringLiteral("in"), QString::number((int) info.cropStart.frames(fps)));
            effect.setAttribute(QStringLiteral("out"), QString::number((int)(info.cropStart + info.cropDuration).frames(fps) - 1));
            effect.setAttribute(QStringLiteral("sync_in_out"), QStringLiteral("1"));
        }
    }
    if (effect.attribute(QStringLiteral("tag")) == QLatin1String("volume") || effect.attribute(QStringLiteral("tag")) == QLatin1String("brightness")) {
        if (effect.attribute(QStringLiteral("id")) == QLatin1String("fadeout") || effect.attribute(QStringLiteral("id")) == QLatin1String("fade_to_black")) {
            int end = (info.cropDuration + info.cropStart).frames(fps) - 1;
            int start = end;
            if (effect.attribute(QStringLiteral("id")) == QLatin1String("fadeout")) {
                if (list.hasEffect(QString(), QStringLiteral("fade_to_black")) == -1) {
                    int effectDuration = EffectsList::parameter(effect, QStringLiteral("out")).toInt() - EffectsList::parameter(effect, QStringLiteral("in")).toInt();
                    if (effectDuration > info.cropDuration.frames(fps)) {
                        effectDuration = info.cropDuration.frames(fps) / 2;
                    }
                    start -= effectDuration;
                } else {
                    QDomElement fadeout = list.getEffectByTag(QString(), QStringLiteral("fade_to_black"));
                    start -= EffectsList::parameter(fadeout, QStringLiteral("out")).toInt() - EffectsList::parameter(fadeout, QStringLiteral("in")).toInt();
                }
            } else if (effect.attribute(QStringLiteral("id")) == QLatin1String("fade_to_black")) {
                if (list.hasEffect(QString(), QStringLiteral("fadeout")) == -1) {
                    int effectDuration = EffectsList::parameter(effect, QStringLiteral("out")).toInt() - EffectsList::parameter(effect, QStringLiteral("in")).toInt();
                    if (effectDuration > info.cropDuration.frames(fps)) {
                        effectDuration = info.cropDuration.frames(fps) / 2;
                    }
                    start -= effectDuration;
                } else {
                    QDomElement fadeout = list.getEffectByTag(QString(), QStringLiteral("fadeout"));
                    start -= EffectsList::parameter(fadeout, QStringLiteral("out")).toInt() - EffectsList::parameter(fadeout, QStringLiteral("in")).toInt();
                }
            }
            EffectsList::setParameter(effect, QStringLiteral("in"), QString::number(start));
            EffectsList::setParameter(effect, QStringLiteral("out"), QString::number(end));
        } else if (effect.attribute(QStringLiteral("id")) == QLatin1String("fadein") || effect.attribute(QStringLiteral("id")) == QLatin1String("fade_from_black")) {
            int start = info.cropStart.frames(fps);
            int end = start;
            if (effect.attribute(QStringLiteral("id")) == QLatin1String("fadein")) {
                if (list.hasEffect(QString(), QStringLiteral("fade_from_black")) == -1) {
                    int effectDuration = EffectsList::parameter(effect, QStringLiteral("out")).toInt();
                    if (offset != 0) {
                        effectDuration -= offset;
                    }
                    if (effectDuration > info.cropDuration.frames(fps)) {
                        effectDuration = info.cropDuration.frames(fps) / 2;
                    }
                    end += effectDuration;
                } else {
                    QDomElement fadein = list.getEffectByTag(QString(), QStringLiteral("fade_from_black"));
                    end = EffectsList::parameter(fadein, QStringLiteral("out")).toInt() - offset;
                }
            } else if (effect.attribute(QStringLiteral("id")) == QStringLiteral("fade_from_black")) {
                if (list.hasEffect(QString(), QStringLiteral("fadein")) == -1) {
                    int effectDuration = EffectsList::parameter(effect, QStringLiteral("out")).toInt();
                    if (offset != 0) {
                        effectDuration -= offset;
                    }
                    if (effectDuration > info.cropDuration.frames(fps)) {
                        effectDuration = info.cropDuration.frames(fps) / 2;
                    }
                    end += effectDuration;
                } else {
                    QDomElement fadein = list.getEffectByTag(QString(), QStringLiteral("fadein"));
                    end = EffectsList::parameter(fadein, QStringLiteral("out")).toInt() - offset;
                }
            }
            EffectsList::setParameter(effect, QStringLiteral("in"), QString::number(start));
            EffectsList::setParameter(effect, QStringLiteral("out"), QString::number(end));
        }
    }
}

const QString EffectsController::adjustKeyframes(const QString &keyframes, int oldIn, int newIn, int newOut, ProfileInfo pInfo)
{
    QStringList result;
    // Simple keyframes
    int offset = newIn - oldIn;
    int max = newOut - newIn;
    QLocale locale;
    const QStringList list = keyframes.split(QLatin1Char(';'), QString::SkipEmptyParts);
    for (const QString &keyframe : list) {
        const int pos = keyframe.section(QLatin1Char('='), 0, 0).toInt() + offset;
        const QString newKey = QString::number(pos) + QLatin1Char('=') + keyframe.section(QLatin1Char('='), 1);
        if (pos >= newOut) {
            if (pos == newOut) {
                result.append(newKey);
            } else {
                // Find correct interpolated value
                Mlt::Geometry geom;
                int framePos = oldIn + max;
                geom.parse(keyframes.toUtf8().data(), newOut, pInfo.profileSize.rwidth(), pInfo.profileSize.rheight());
                Mlt::GeometryItem item;
                geom.fetch(&item, framePos);
                const QString lastKey = QString::number(newIn + max) + QLatin1Char('=') + locale.toString(item.x());
                result.append(lastKey);
            }
            break;
        }
        result.append(newKey);
    }
    return result.join(QLatin1Char(';'));
}

EffectsParameterList EffectsController::addEffect(const ProfileInfo &info, const QDomElement &effect)
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    EffectsParameterList parameters;
    parameters.addParam(QStringLiteral("tag"), effect.attribute(QStringLiteral("tag")));
    parameters.addParam(QStringLiteral("kdenlive_ix"), effect.attribute(QStringLiteral("kdenlive_ix")));
    if (effect.hasAttribute(QStringLiteral("src"))) {
        parameters.addParam(QStringLiteral("src"), effect.attribute(QStringLiteral("src")));
    }
    if (effect.hasAttribute(QStringLiteral("disable"))) {
        parameters.addParam(QStringLiteral("disable"), effect.attribute(QStringLiteral("disable")));
    }

    QString effectId = effect.attribute(QStringLiteral("id"));
    if (effectId.isEmpty()) {
        effectId = effect.attribute(QStringLiteral("tag"));
    }
    parameters.addParam(QStringLiteral("id"), effectId);

    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull()) {
            if (e.attribute(QStringLiteral("type")) == QLatin1String("simplekeyframe")) {
                QStringList values = e.attribute(QStringLiteral("keyframes")).split(QLatin1Char(';'), QString::SkipEmptyParts);
                double factor = locale.toDouble(e.attribute(QStringLiteral("factor"), QStringLiteral("1")));
                double offset = e.attribute(QStringLiteral("offset"), QStringLiteral("0")).toDouble();
                if (factor != 1 || offset != 0) {
                    for (int j = 0; j < values.count(); ++j) {
                        QString pos = values.at(j).section(QLatin1Char('='), 0, 0);
                        double val = (locale.toDouble(values.at(j).section(QLatin1Char('='), 1, 1)) - offset) / factor;
                        values[j] = pos + QLatin1Char('=') + locale.toString(val);
                    }
                }
                parameters.addParam(e.attribute(QStringLiteral("name")), values.join(QLatin1Char(';')));
                /*parameters.addParam("max", e.attribute("max"));
                parameters.addParam("min", e.attribute("min"));
                parameters.addParam("factor", );*/
            } else if (e.attribute(QStringLiteral("type")) == QLatin1String("keyframe")) {
                parameters.addParam(QStringLiteral("keyframes"), e.attribute(QStringLiteral("keyframes")));
                parameters.addParam(QStringLiteral("max"), e.attribute(QStringLiteral("max")));
                parameters.addParam(QStringLiteral("min"), e.attribute(QStringLiteral("min")));
                parameters.addParam(QStringLiteral("factor"), e.attribute(QStringLiteral("factor"), QStringLiteral("1")));
                parameters.addParam(QStringLiteral("offset"), e.attribute(QStringLiteral("offset"), QStringLiteral("0")));
                parameters.addParam(QStringLiteral("starttag"), e.attribute(QStringLiteral("starttag"), QStringLiteral("start")));
                parameters.addParam(QStringLiteral("endtag"), e.attribute(QStringLiteral("endtag"), QStringLiteral("end")));
            } else if (e.attribute(QStringLiteral("factor"), QStringLiteral("1")) == QLatin1String("1") && e.attribute(QStringLiteral("offset"), QStringLiteral("0")) == QLatin1String("0")) {
                parameters.addParam(e.attribute(QStringLiteral("name")), e.attribute(QStringLiteral("value")));

            } else {
                double fact;
                if (e.attribute(QStringLiteral("factor")).contains(QLatin1Char('%'))) {
                    fact = EffectsController::getStringEval(info, e.attribute(QStringLiteral("factor")));
                } else {
                    fact = locale.toDouble(e.attribute(QStringLiteral("factor"), QStringLiteral("1")));
                }
                double offset = e.attribute(QStringLiteral("offset"), QStringLiteral("0")).toDouble();
                parameters.addParam(e.attribute(QStringLiteral("name")), locale.toString((locale.toDouble(e.attribute(QStringLiteral("value"))) - offset) / fact));
            }
        }
    }
    return parameters;
}

void EffectsController::offsetKeyframes(int in, const QDomElement &effect)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.attribute(QStringLiteral("type")) == QLatin1String("simplekeyframe")) {
            QStringList values = e.attribute(QStringLiteral("keyframes")).split(QLatin1Char(';'), QString::SkipEmptyParts);
            for (int j = 0; j < values.count(); ++j) {
                int pos = values.at(j).section(QLatin1Char('='), 0, 0).toInt() - in;
                values[j] = QString::number(pos) + QLatin1Char('=') + values.at(j).section(QLatin1Char('='), 1);
            }
            e.setAttribute(QStringLiteral("keyframes"), values.join(QLatin1Char(';')));
        }
    }
}

