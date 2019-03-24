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

#include <QColor>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wfloat-equal"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <rttr/registration>
#pragma GCC diagnostic pop

RTTR_REGISTRATION
{
    using namespace rttr;
    // clang-format off
    registration::enumeration<GroupType>("GroupType")(
        value("Normal", GroupType::Normal),
        value("Selection", GroupType::Selection),
        value("AVSplit", GroupType::AVSplit),
        value("Leaf", GroupType::Leaf)
        );
    registration::enumeration<PlaylistState::ClipState>("PlaylistState")(
        value("VideoOnly", PlaylistState::VideoOnly),
        value("AudioOnly", PlaylistState::AudioOnly),
        value("Disabled", PlaylistState::Disabled)
        );
    // clang-format on
}

QDebug operator<<(QDebug qd, const ItemInfo &info)
{
    qd << "ItemInfo " << &info;
    qd << "\tTrack" << info.track;
    qd << "\tStart pos: " << info.startPos.toString();
    qd << "\tEnd pos: " << info.endPos.toString();
    qd << "\tCrop start: " << info.cropStart.toString();
    qd << "\tCrop duration: " << info.cropDuration.toString();
    return qd.maybeSpace();
}

CommentedTime::CommentedTime()
    : m_time(GenTime(0))

{
}

CommentedTime::CommentedTime(const GenTime &time, QString comment, int markerType)
    : m_time(time)
    , m_comment(std::move(comment))
    , m_type(markerType)
{
}

CommentedTime::CommentedTime(const QString &hash, const GenTime &time)
    : m_time(time)
    , m_comment(hash.section(QLatin1Char(':'), 1))
    , m_type(hash.section(QLatin1Char(':'), 0, 0).toInt())
{
}

QString CommentedTime::comment() const
{
    return (m_comment.isEmpty() ? i18n("Marker") : m_comment);
}

GenTime CommentedTime::time() const
{
    return m_time;
}

void CommentedTime::setComment(const QString &comm)
{
    m_comment = comm;
}

void CommentedTime::setMarkerType(int newtype)
{
    m_type = newtype;
}

QString CommentedTime::hash() const
{
    return QString::number(m_type) + QLatin1Char(':') + (m_comment.isEmpty() ? i18n("Marker") : m_comment);
}

int CommentedTime::markerType() const
{
    return m_type;
}

QColor CommentedTime::markerColor(int type)
{
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

bool CommentedTime::operator>(const CommentedTime &op) const
{
    return m_time > op.time();
}

bool CommentedTime::operator<(const CommentedTime &op) const
{
    return m_time < op.time();
}

bool CommentedTime::operator>=(const CommentedTime &op) const
{
    return m_time >= op.time();
}

bool CommentedTime::operator<=(const CommentedTime &op) const
{
    return m_time <= op.time();
}

bool CommentedTime::operator==(const CommentedTime &op) const
{
    return m_time == op.time();
}

bool CommentedTime::operator!=(const CommentedTime &op) const
{
    return m_time != op.time();
}

const QString groupTypeToStr(GroupType t)
{
    switch (t) {
    case GroupType::Normal:
        return QStringLiteral("Normal");
    case GroupType::Selection:
        return QStringLiteral("Selection");
    case GroupType::AVSplit:
        return QStringLiteral("AVSplit");
    case GroupType::Leaf:
        return QStringLiteral("Leaf");
    }
    Q_ASSERT(false);
    return QString();
}
GroupType groupTypeFromStr(const QString &s)
{
    std::vector<GroupType> types{GroupType::Selection, GroupType::Normal, GroupType::AVSplit, GroupType::Leaf};
    for (const auto &t : types) {
        if (s == groupTypeToStr(t)) {
            return t;
        }
    }
    Q_ASSERT(false);
    return GroupType::Normal;
}

std::pair<bool, bool> stateToBool(PlaylistState::ClipState state)
{
    return {state == PlaylistState::VideoOnly, state == PlaylistState::AudioOnly};
}
PlaylistState::ClipState stateFromBool(std::pair<bool, bool> av)
{
    assert(!av.first || !av.second);
    if (av.first) {
        return PlaylistState::VideoOnly;
    } else if (av.second) {
        return PlaylistState::AudioOnly;
    } else {
        return PlaylistState::Disabled;
    }
}
