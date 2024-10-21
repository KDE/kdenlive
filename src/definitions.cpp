/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "definitions.h"
#include <KLocalizedString>

#include <QColor>
#include <utility>

#ifdef CRASH_AUTO_TEST
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
#endif

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

void CommentedTime::setTime(const GenTime &t)
{
    m_time = t;
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
    Q_ASSERT(!av.first || !av.second);
    if (av.first) {
        return PlaylistState::VideoOnly;
    } else if (av.second) {
        return PlaylistState::AudioOnly;
    } else {
        return PlaylistState::Disabled;
    }
}

SubtitleEvent::SubtitleEvent(const QString &eventString, const double fps, const double factor, std::pair<int, GenTime> *start)
{
    m_isDialogue = (eventString.section(":", 0, 0) == "Dialogue");
    QStringList parts = eventString.section(":", 1).split(",");
    if (parts.size() < 10) {
        // Invalid subtitle event
        return;
    }

    // parse the parts
    int layer = parts[0].toInt();
    GenTime startTime = stringtoTime(parts[1], fps, factor);
    GenTime endTime = stringtoTime(parts[2], fps, factor);
    QString styleName = parts[3];
    QString name = parts[4];
    int marginL = parts[5].toInt();
    int marginR = parts[6].toInt();
    int marginV = parts[7].toInt();
    QString effect = parts[8];
    QString text = "";
    for (int i = 9; i < parts.size() - 1; i++) {
        text += parts.at(i) + ',';
    }
    text += parts.at(parts.size() - 1);
    text.replace(QLatin1String("\\n"), QLatin1String("\n"));

    // check the validity
    if (layer < 0 || endTime <= startTime) {
        // Invalid subtitle event
        return;
    }

    // m_layer = layer;
    // m_startTime = startTime;
    m_endTime = endTime;
    m_styleName = styleName;
    m_name = name;
    m_marginL = marginL;
    m_marginR = marginR;
    m_marginV = marginV;
    m_effect = effect;
    m_text = text;

    // Pass the layer and start time if needed
    if (start != nullptr) {
        start->first = layer;
        start->second = startTime;
    }
}

bool SubtitleEvent::operator==(const SubtitleEvent &op) const
{
    return m_isDialogue == op.m_isDialogue && m_endTime == op.m_endTime && m_styleName == op.m_styleName && m_name == op.m_name && m_marginL == op.m_marginL &&
           m_marginR == op.m_marginR && m_marginV == op.m_marginV && m_effect == op.m_effect && m_text == op.m_text;
}

bool SubtitleEvent::operator!=(const SubtitleEvent &op) const
{
    return m_isDialogue != op.m_isDialogue || m_endTime != op.m_endTime || m_styleName != op.m_styleName || m_name != op.m_name || m_marginL != op.m_marginL ||
           m_marginR != op.m_marginR || m_marginV != op.m_marginV || m_effect != op.m_effect || m_text != op.m_text;
}

QString SubtitleEvent::toString(int layer, GenTime start) const
{
    QString result = QStringLiteral("");

    if (this->isDialogue())
        result += QStringLiteral("Dialogue: ");
    else
        result += QStringLiteral("Comment: ");

    result += QStringLiteral("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10")
                  .arg(layer)
                  .arg(this->timeToString(start, 0))
                  .arg(this->timeToString(this->endTime(), 0))
                  .arg(this->styleName())
                  .arg(this->name())
                  .arg(this->marginL())
                  .arg(this->marginR())
                  .arg(this->marginV())
                  .arg(this->effect())
                  .arg(this->text());
    return result;
}

GenTime SubtitleEvent::stringtoTime(const QString &str, const double fps, const double factor)
{
    QStringList total, secs;
    double milliseconds = 0;
    GenTime pos;
    total = str.split(QLatin1Char(':'));
    if (total.count() == 3) {
        // There is an hour timestamp
        milliseconds += atoi(total.takeFirst().toStdString().c_str()) * 60 * 60 * 1000;
    }
    if (total.count() == 2) {
        // minutes
        milliseconds += atoi(total.at(0).toStdString().c_str()) * 60 * 1000;
        if (total.at(1).contains(QLatin1Char('.'))) {
            secs = total.at(1).split(QLatin1Char('.')); // ssa file, sbv file or vtt file
        } else {
            secs = total.at(1).split(QLatin1Char(',')); // srt file
        }
        if (secs.count() < 2) {
            // seconds
            milliseconds += atoi(total.at(1).toStdString().c_str()) * 1000;
        } else {
            milliseconds += atoi(secs.at(0).toStdString().c_str()) * 1000;
            // milliseconds or centiseconds
            if (total.at(1).contains(QLatin1Char('.'))) {
                if (secs.at(1).length() == 2) {
                    // ssa file
                    // milliseconds = centiseconds * 10
                    milliseconds += atoi(secs.at(1).toStdString().c_str()) * 10;
                } else {
                    // vtt file or sbv file
                    milliseconds += atoi(secs.at(1).toStdString().c_str());
                }
            } else {
                // srt file
                milliseconds += atoi(secs.at(1).toStdString().c_str());
            }
        }
    } else {
        // invalid time found
        Q_ASSERT(false);
        return GenTime();
    }

    pos = GenTime(milliseconds / 1000) / factor;
    // Ensure times are aligned with our project's frames
    int frames = pos.frames(fps);
    return GenTime(frames, fps);
}

QString SubtitleEvent::timeToString(const GenTime &time, const int format)
{
    int millisecond = (int)time.ms();
    int second = millisecond / 1000;
    millisecond %= 1000;
    int centisecond = (millisecond % 10 > 5) ? (millisecond / 10 + 1) : (millisecond / 10);
    int minute = second / 60;
    second %= 60;
    int hour = minute / 60;
    minute %= 60;

    switch (format) {
    // ASS Format
    case 0:
        return QStringLiteral("%1:%2:%3.%4")
            .arg(hour, 2, 10, QLatin1Char('0'))
            .arg(minute, 2, 10, QLatin1Char('0'))
            .arg(second, 2, 10, QLatin1Char('0'))
            .arg(centisecond, 2, 10, QLatin1Char('0'));
        break;
    // SRT Format
    case 1:
        return QStringLiteral("%1:%2:%3,%4")
            .arg(hour, 2, 10, QLatin1Char('0'))
            .arg(minute, 2, 10, QLatin1Char('0'))
            .arg(second, 2, 10, QLatin1Char('0'))
            .arg(millisecond, 3, 10, QLatin1Char('0'));
        break;
    default:
        return QString();
        break;
    }
}

SubtitleStyle::SubtitleStyle(QString styleString)
{
    QStringList parts = styleString.section(":", 1).trimmed().split(',');
    // m_name = parts.at(0);
    m_fontName = parts.at(1);
    m_fontSize = parts.at(2).toDouble();
    // Color : &HAABBGGRR
    QString originPrimary = parts.at(3);
    m_primaryColour.setAlpha(255 - originPrimary.mid(2, 2).toInt(nullptr, 16));
    m_primaryColour.setBlue(originPrimary.mid(4, 2).toInt(nullptr, 16));
    m_primaryColour.setGreen(originPrimary.mid(6, 2).toInt(nullptr, 16));
    m_primaryColour.setRed(originPrimary.mid(8, 2).toInt(nullptr, 16));

    QString originSecondary = parts.at(4);
    m_secondaryColour.setAlpha(255 - originSecondary.mid(2, 2).toInt(nullptr, 16));
    m_secondaryColour.setBlue(originSecondary.mid(4, 2).toInt(nullptr, 16));
    m_secondaryColour.setGreen(originSecondary.mid(6, 2).toInt(nullptr, 16));
    m_secondaryColour.setRed(originSecondary.mid(8, 2).toInt(nullptr, 16));

    QString originOutline = parts.at(5);
    m_outlineColour.setAlpha(255 - originOutline.mid(2, 2).toInt(nullptr, 16));
    m_outlineColour.setBlue(originOutline.mid(4, 2).toInt(nullptr, 16));
    m_outlineColour.setGreen(originOutline.mid(6, 2).toInt(nullptr, 16));
    m_outlineColour.setRed(originOutline.mid(8, 2).toInt(nullptr, 16));

    QString originBack = parts.at(6);
    m_backColour.setAlpha(255 - originBack.mid(2, 2).toInt(nullptr, 16));
    m_backColour.setBlue(originBack.mid(4, 2).toInt(nullptr, 16));
    m_backColour.setGreen(originBack.mid(6, 2).toInt(nullptr, 16));
    m_backColour.setRed(originBack.mid(8, 2).toInt(nullptr, 16));

    m_bold = (parts.at(7) == "0" ? false : true);
    m_italic = (parts.at(8) == "0" ? false : true);
    m_underline = (parts.at(9) == "0" ? false : true);
    m_strikeOut = (parts.at(10) == "0" ? false : true);
    m_scaleX = parts.at(11).toDouble();
    m_scaleY = parts.at(12).toDouble();
    m_spacing = parts.at(13).toDouble();
    m_angle = parts.at(14).toDouble();
    m_borderStyle = parts.at(15).toInt();
    m_outline = parts.at(16).toDouble();
    m_shadow = parts.at(17).toDouble();
    m_alignment = parts.at(18).toInt();
    m_marginL = parts.at(19).toInt();
    m_marginR = parts.at(20).toInt();
    m_marginV = parts.at(21).toInt();
    m_encoding = parts.at(22).toInt();
}

QString SubtitleStyle::toString(QString name) const
{
    // Color : &HAABBGGRR
    return QStringLiteral("Style: %1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12,%13,%14,%15,%16,%17,%18,%19,%20,%21,%22,%23")
        .arg(name)
        .arg(m_fontName)
        .arg(m_fontSize, 0, 'f', 2)
        .arg("&H" + QStringLiteral("%1").arg(QString::number(255 - m_primaryColour.alpha(), 16), 2, '0').toUpper() +
             QStringLiteral("%1").arg(QString::number(m_primaryColour.blue(), 16), 2, '0').toUpper() +
             QStringLiteral("%1").arg(QString::number(m_primaryColour.green(), 16), 2, '0').toUpper() +
             QStringLiteral("%1").arg(QString::number(m_primaryColour.red(), 16), 2, '0').toUpper())
        .arg("&H" + QStringLiteral("%1").arg(QString::number(255 - m_secondaryColour.alpha(), 16), 2, '0').toUpper() +
             QStringLiteral("%1").arg(QString::number(m_secondaryColour.blue(), 16), 2, '0').toUpper() +
             QStringLiteral("%1").arg(QString::number(m_secondaryColour.green(), 16), 2, '0').toUpper() +
             QStringLiteral("%1").arg(QString::number(m_secondaryColour.red(), 16), 2, '0').toUpper())
        .arg("&H" + QStringLiteral("%1").arg(QString::number(255 - m_outlineColour.alpha(), 16), 2, '0').toUpper() +
             QStringLiteral("%1").arg(QString::number(m_outlineColour.blue(), 16), 2, '0').toUpper() +
             QStringLiteral("%1").arg(QString::number(m_outlineColour.green(), 16), 2, '0').toUpper() +
             QStringLiteral("%1").arg(QString::number(m_outlineColour.red(), 16), 2, '0').toUpper())
        .arg("&H" + QStringLiteral("%1").arg(QString::number(255 - m_backColour.alpha(), 16), 2, '0').toUpper() +
             QStringLiteral("%1").arg(QString::number(m_backColour.blue(), 16), 2, '0').toUpper() +
             QStringLiteral("%1").arg(QString::number(m_backColour.green(), 16), 2, '0').toUpper() +
             QStringLiteral("%1").arg(QString::number(m_backColour.red(), 16), 2, '0').toUpper())
        .arg(m_bold ? -1 : 0)
        .arg(m_italic ? -1 : 0)
        .arg(m_underline ? -1 : 0)
        .arg(m_strikeOut ? -1 : 0)
        .arg(m_scaleX, 0, 'f', 2)
        .arg(m_scaleY, 0, 'f', 2)
        .arg(m_spacing, 0, 'f', 2)
        .arg(m_angle, 0, 'f', 2)
        .arg(m_borderStyle)
        .arg(m_outline, 0, 'f', 2)
        .arg(m_shadow, 0, 'f', 2)
        .arg(m_alignment)
        .arg(m_marginL)
        .arg(m_marginR)
        .arg(m_marginV)
        .arg(m_encoding);
}
