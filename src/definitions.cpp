/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "definitions.h"
#include <klocalizedstring.h>

QDebug operator << (QDebug qd, const ItemInfo &info)
{
    qd << "ItemInfo " << &info;
    qd << "\tTrack" << info.track;
    qd << "\tStart pos: " << info.startPos.toString();
    qd << "\tEnd pos: " << info.endPos.toString();
    qd << "\tCrop start: " << info.cropStart.toString();
    qd << "\tCrop duration: " << info.cropDuration.toString();
    return qd.maybeSpace();
}


MltVideoProfile::MltVideoProfile() :
    frame_rate_num(0),
    frame_rate_den(0),
    width(0),
    height(0),
    progressive(0),
    sample_aspect_num(0),
    sample_aspect_den(0),
    display_aspect_num(0),
    display_aspect_den(0),
    colorspace(0)
{
}

bool MltVideoProfile::operator==(const MltVideoProfile &point) const
{
    if (!description.isEmpty() && point.description  == description) {
        return true;
    }
    return      point.frame_rate_num == frame_rate_num &&
            point.frame_rate_den  == frame_rate_den  &&
            point.width == width &&
            point.height == height &&
            point.progressive == progressive &&
            point.sample_aspect_num == sample_aspect_num &&
            point.sample_aspect_den == sample_aspect_den &&
            point.display_aspect_den == display_aspect_den &&
            point.colorspace == colorspace;
}

bool MltVideoProfile::operator!=(const MltVideoProfile &other) const {
    return !(*this == other);
}


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


CommentedTime::CommentedTime(): t(GenTime(0)), type(0) {}


CommentedTime::CommentedTime(const GenTime &time, const QString &comment, int markerType)
    : t(time), c(comment), type(markerType) { }

QString CommentedTime::comment() const          {
    return (c.isEmpty() ? i18n("Marker") : c);
}

GenTime CommentedTime::time() const          {
    return t;
}

void CommentedTime::setComment(const QString &comm) {
    c = comm;
}

void CommentedTime::setMarkerType(int t) {
    type = t;
}

int CommentedTime::markerType() const {
    return type;
}

QColor CommentedTime::markerColor(int type) {
    switch (type) {
    case 0:
        return Qt::red;
        break;
    case 1:
        return Qt::blue;
        break;
    case 2:
        return Qt::green;
        break;
    case 3:
        return Qt::yellow;
        break;
    default:
        return Qt::cyan;
        break;
    }
}

bool CommentedTime::operator>(CommentedTime op) const {
    return t > op.time();
}

bool CommentedTime::operator<(CommentedTime op) const {
    return t < op.time();
}

bool CommentedTime::operator>=(CommentedTime op) const {
    return t >= op.time();
}

bool CommentedTime::operator<=(CommentedTime op) const {
    return t <= op.time();
}

bool CommentedTime::operator==(CommentedTime op) const {
    return t == op.time();
}

bool CommentedTime::operator!=(CommentedTime op) const {
    return t != op.time();
}
