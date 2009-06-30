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


#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include "gentime.h"

#include <KLocale>

#include <QEvent>

const int FRAME_SIZE = 90;
const int MAXCLIPDURATION = 15000;

enum OPERATIONTYPE { NONE = 0, MOVE = 1, RESIZESTART = 2, RESIZEEND = 3, FADEIN = 4, FADEOUT = 5, TRANSITIONSTART = 6, TRANSITIONEND = 7, MOVEGUIDE = 8, KEYFRAME = 9, SEEK = 10, SPACER = 11, RUBBERSELECTION = 12};
enum CLIPTYPE { UNKNOWN = 0, AUDIO = 1, VIDEO = 2, AV = 3, COLOR = 4, IMAGE = 5, TEXT = 6, SLIDESHOW = 7, VIRTUAL = 8, PLAYLIST = 9, FOLDER = 10};
enum GRAPHICSRECTITEM { AVWIDGET = 70000 , LABELWIDGET , TRANSITIONWIDGET  , GROUPWIDGET};

enum PROJECTTOOL { SELECTTOOL = 0 , RAZORTOOL = 1 , SPACERTOOL = 2 };

enum TRANSITIONTYPE {
    /** TRANSITIONTYPE: between 0-99: video trans, 100-199: video+audio trans, 200-299: audio trans */
    LUMA_TRANSITION = 0,
    COMPOSITE_TRANSITION = 1,
    PIP_TRANSITION = 2,
    LUMAFILE_TRANSITION = 3,
    MIX_TRANSITION = 200
};

enum MessageType {
    DefaultMessage,
    OperationCompletedMessage,
    InformationMessage,
    ErrorMessage
};

enum TRACKTYPE { AUDIOTRACK = 0, VIDEOTRACK = 1 };

struct TrackInfo {
    TRACKTYPE type;
    bool isMute;
    bool isBlind;
    bool isLocked;
};

struct ItemInfo {
    GenTime startPos;
    GenTime endPos;
    GenTime cropStart;
    int track;
};

struct MltVideoProfile {
    QString path;
    QString description;
    int frame_rate_num;
    int frame_rate_den;
    int width;
    int height;
    bool progressive;
    int sample_aspect_num;
    int sample_aspect_den;
    int display_aspect_num;
    int display_aspect_den;
};


class EffectParameter
{
public:
    EffectParameter(const QString name, const QString value): m_name(name), m_value(value) {}
    QString name()   const          {
        return m_name;
    }
    QString value() const          {
        return m_value;
    }
    void setValue(const QString value) {
        m_value = value;
    }

private:
    QString m_name;
    QString m_value;
};

/** Use our own list for effect parameters so that they are not sorted in any ways, because
    some effects like sox need a precise order
*/
class EffectsParameterList: public QList < EffectParameter >
{
public:
    EffectsParameterList(): QList < EffectParameter >() {}
    bool hasParam(const QString name) const {
        for (int i = 0; i < size(); i++)
            if (at(i).name() == name) return true;
        return false;
    }
    QString paramValue(const QString name, QString defaultValue = QString()) const {
        for (int i = 0; i < size(); i++) {
            if (at(i).name() == name) return at(i).value();
        }
        return defaultValue;
    }
    void addParam(const QString &name, const QString &value) {
        if (name.isEmpty()) return;
        append(EffectParameter(name, value));
    }
    void removeParam(const QString name) {
        for (int i = 0; i < size(); i++)
            if (at(i).name() == name) {
                removeAt(i);
                break;
            }
    }
};

class CommentedTime
{
public:
    CommentedTime(): t(GenTime(0)) {}
    CommentedTime(const GenTime time, QString comment)
            : t(time), c(comment) { }

    QString comment()   const          {
        return (c.isEmpty() ? i18n("Marker") : c);
    }
    GenTime time() const          {
        return t;
    }
    void    setComment(QString comm) {
        c = comm;
    }

    /* Implementation of > operator; Works identically as with basic types. */
    bool operator>(CommentedTime op) const {
        return t > op.time();
    }
    /* Implementation of < operator; Works identically as with basic types. */
    bool operator<(CommentedTime op) const {
        return t < op.time();
    }
    /* Implementation of >= operator; Works identically as with basic types. */
    bool operator>=(CommentedTime op) const {
        return t >= op.time();
    }
    /* Implementation of <= operator; Works identically as with basic types. */
    bool operator<=(CommentedTime op) const {
        return t <= op.time();
    }
    /* Implementation of == operator; Works identically as with basic types. */
    bool operator==(CommentedTime op) const {
        return t == op.time();
    }
    /* Implementation of != operator; Works identically as with basic types. */
    bool operator!=(CommentedTime op) const {
        return t != op.time();
    }

private:
    GenTime t;
    QString c;


};

class MltErrorEvent : public QEvent
{
public:
    MltErrorEvent(QString message) : QEvent(QEvent::User), m_message(message) {}
    QString message() const {
        return m_message;
    }

private:
    QString m_message;
};



#endif
