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

QDebug operator << (QDebug qd, const MltVideoProfile &profile)
{
    qd << "Profile " << &profile;
    qd << "\tProfile fps num " << profile.frame_rate_num;
    qd << "\tProfile fps den " << profile.frame_rate_den;
    qd << "\tProfile width " << profile.width,
       qd << "\tProfile height " << profile.height;
    qd << "\tProfile progressive " << profile.progressive;
    qd << "\tProfile sar num " << profile.sample_aspect_num;
    qd << "\tProfile sar den " << profile.sample_aspect_den;
    qd << "\tProfile dar num " << profile.display_aspect_num;
    qd << "\tProfile dar den " << profile.display_aspect_den;
    qd << "\tProfile colorspace " << profile.colorspace;
    qd << "\tProfile description " << profile.description;
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

MltVideoProfile::MltVideoProfile(const QVariantList &params) :
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
    if (params.count() != 12) {
        qWarning("Trying to build a profile with incorrect param numbers");
        return;
    }
    frame_rate_num = params.at(0).toInt();
    frame_rate_den = params.at(1).toInt();
    width = params.at(2).toInt();
    height = params.at(3).toInt();
    progressive = params.at(4).toBool();
    sample_aspect_num = params.at(5).toInt();
    sample_aspect_den = params.at(6).toInt();
    display_aspect_num = params.at(7).toInt();
    display_aspect_den = params.at(8).toInt();
    colorspace = params.at(9).toInt();
    path = params.at(10).toString();
    description = params.at(11).toString();
}

bool MltVideoProfile::operator==(const MltVideoProfile &point) const
{
    if (!description.isEmpty() && point.description  == description) {
        return true;
    }
    int fps = frame_rate_num * 100 / frame_rate_den;
    int sar = sample_aspect_num * 100 / sample_aspect_den;
    int dar = display_aspect_num * 100 / display_aspect_den;
    return      point.frame_rate_num * 100 / point.frame_rate_den == fps &&
                point.width == width &&
                point.height == height &&
                point.progressive == progressive &&
                point.sample_aspect_num * 100 / point.sample_aspect_den == sar &&
                point.display_aspect_num * 100 / point.display_aspect_den == dar &&
                point.colorspace == colorspace;
}

bool MltVideoProfile::isCompatible(const MltVideoProfile &point) const
{
    return frame_rate_num * 100 / frame_rate_den == point.frame_rate_num * 100 / point.frame_rate_den;
}

bool MltVideoProfile::isValid() const
{
    return (frame_rate_den > 0 && sample_aspect_den > 0 && display_aspect_den > 0 && width > 0);
}

const QVariantList MltVideoProfile::toList() const
{
    QVariantList result;
    result << frame_rate_num << frame_rate_den << width << height << progressive << sample_aspect_num << sample_aspect_den << display_aspect_num << display_aspect_den << colorspace << path << description;
    return result;
}

void MltVideoProfile::adjustWidth()
{
    width = (width + 7) / 8 * 8;
}

const QString MltVideoProfile::descriptiveString()
{
    QString data = description;
    if (!data.isEmpty()) {
        data.append(QLatin1Char(' '));
    }
    QString fps;
    if (frame_rate_num % frame_rate_den == 0) {
        fps = QString::number(frame_rate_num / frame_rate_den);
    } else {
        fps = QString::number((double)frame_rate_num / frame_rate_den, 'f', 2);
    }
    data.append(QStringLiteral("(%1x%2, %3fps)").arg(width).arg(height).arg(fps));
    return data;
}

const QString MltVideoProfile::dialogDescriptiveString()
{
    QString text;
    if (frame_rate_num % frame_rate_den == 0) {
        text = QString::number(frame_rate_num / frame_rate_den);
    } else {
        text = QString::number((double)frame_rate_num / frame_rate_den, 'f', 2);
    }
    text.append(i18nc("frames per second", "fps"));
    if (!progressive) {
        text.append(i18n(" interlaced"));
    }
    return text;
}

bool MltVideoProfile::operator!=(const MltVideoProfile &other) const
{
    return !(*this == other);
}

CommentedTime::CommentedTime(): t(GenTime(0)), type(0) {}

CommentedTime::CommentedTime(const GenTime &time, const QString &comment, int markerType)
    : t(time), c(comment), type(markerType) { }

CommentedTime::CommentedTime(const QString &hash, const GenTime &time)
    : t(time), c(hash.section(QLatin1Char(':'), 1)), type(hash.section(QLatin1Char(':'), 0, 0).toInt()) { }

QString CommentedTime::comment() const
{
    return (c.isEmpty() ? i18n("Marker") : c);
}

GenTime CommentedTime::time() const
{
    return t;
}

void CommentedTime::setComment(const QString &comm)
{
    c = comm;
}

void CommentedTime::setMarkerType(int t)
{
    type = t;
}

QString CommentedTime::hash() const
{
    return QString::number(type) + QLatin1Char(':') + (c.isEmpty() ? i18n("Marker") : c);
}

int CommentedTime::markerType() const
{
    return type;
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
    return t > op.time();
}

bool CommentedTime::operator<(const CommentedTime &op) const
{
    return t < op.time();
}

bool CommentedTime::operator>=(const CommentedTime &op) const
{
    return t >= op.time();
}

bool CommentedTime::operator<=(const CommentedTime &op) const
{
    return t <= op.time();
}

bool CommentedTime::operator==(const CommentedTime &op) const
{
    return t == op.time();
}

bool CommentedTime::operator!=(const CommentedTime &op) const
{
    return t != op.time();
}
