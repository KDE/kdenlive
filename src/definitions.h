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
#include "effectslist.h"

#include <KLocale>
#include <QDebug>

#include <QTreeWidgetItem>
 #include <QtCore/QString>

const int MAXCLIPDURATION = 15000;

namespace Kdenlive {
  enum MONITORID { noMonitor, clipMonitor, projectMonitor, recordMonitor, stopmotionMonitor, dvdMonitor };
  /*const QString clipMonitor("clipMonitor");
  const QString recordMonitor("recordMonitor");
  const QString projectMonitor("projectMonitor");
  const QString stopmotionMonitor("stopmotionMonitor");*/
}

enum OPERATIONTYPE { NONE = 0, MOVE = 1, RESIZESTART = 2, RESIZEEND = 3, FADEIN = 4, FADEOUT = 5, TRANSITIONSTART = 6, TRANSITIONEND = 7, MOVEGUIDE = 8, KEYFRAME = 9, SEEK = 10, SPACER = 11, RUBBERSELECTION = 12};
enum CLIPTYPE { UNKNOWN = 0, AUDIO = 1, VIDEO = 2, AV = 3, COLOR = 4, IMAGE = 5, TEXT = 6, SLIDESHOW = 7, VIRTUAL = 8, PLAYLIST = 9 };

enum PROJECTITEMTYPE { PROJECTCLIPTYPE = QTreeWidgetItem::UserType, PROJECTFOLDERTYPE, PROJECTSUBCLIPTYPE };

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
    ErrorMessage,
    MltError
};

enum TRACKTYPE { AUDIOTRACK = 0, VIDEOTRACK = 1 };

enum CLIPJOBSTATUS { NOJOB = 0, JOBWAITING = -1, JOBWORKING = -2, JOBDONE = -3, JOBCRASHED = -4, JOBABORTED = -5};

class TrackInfo {
public:
    TRACKTYPE type;
    QString trackName;
    bool isMute;
    bool isBlind;
    bool isLocked;
    EffectsList effectsList;
    int duration;
    TrackInfo() :
        type(VIDEOTRACK),
        isMute(0),
        isBlind(0),
        isLocked(0),
        duration(0) {};
};

typedef QMap<QString, QString> stringMap;
typedef QMap <int, QMap <int, QByteArray> > audioByteArray;

class ItemInfo {
public:
    /** startPos is the position where the clip starts on the track */
    GenTime startPos;
    /** endPos is the duration where the clip ends on the track */
    GenTime endPos;
    /** cropStart is the position where the sub-clip starts, relative to the clip's 0 position */
    GenTime cropStart;
    /** cropDuration is the duration of the clip */
    GenTime cropDuration;
    int track;
    ItemInfo() : track(0) {};
};

class TransitionInfo {
public:
/** startPos is the position where the clip starts on the track */
    GenTime startPos;
    /** endPos is the duration where the clip ends on the track */
    GenTime endPos;
    /** the track on which the transition is (b_track)*/
    int b_track;
    /** the track on which the transition is applied (a_track)*/
    int a_track;
    /** Does the user request for a special a_track */
    bool forceTrack;
    TransitionInfo() :
        b_track(0),
        a_track(0),
        forceTrack(0) {};
};

class MltVideoProfile {
public:
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
    int colorspace;
    MltVideoProfile() :
        frame_rate_num(0),
        frame_rate_den(0),
        width(0),
        height(0),
        progressive(0),
        sample_aspect_num(0),
        sample_aspect_den(0),
        display_aspect_num(0),
        display_aspect_den(0),
        colorspace(0) {};
    bool operator==(const MltVideoProfile& point) const
    {
        if (!description.isEmpty() && point.description  == description) return true;
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
    bool operator!=(const MltVideoProfile &other) const {
        return !(*this == other);
    }
};

/**)
 * @class EffectInfo
 * @brief A class holding some meta info for effects widgets, like state (collapsed or not, ...)
 * @author Jean-Baptiste Mardelle
 */

class EffectInfo
{
public:
    EffectInfo() {isCollapsed = false; groupIndex = -1; groupIsCollapsed = false;}
    bool isCollapsed;
    bool groupIsCollapsed;
    int groupIndex;
    QString groupName;
    QString toString() const {
        QStringList data;
	// effect collapsed state: 0 = effect not collapsed, 1 = effect collapsed, 
	// 2 = group collapsed - effect not, 3 = group and effect collapsed
	int collapsedState = (int) isCollapsed;
	if (groupIsCollapsed) collapsedState += 2;
	data << QString::number(collapsedState) << QString::number(groupIndex) << groupName;
	return data.join("/");
    }
    void fromString(QString value) {
	if (value.isEmpty()) return;
	QStringList data = value.split("/");
	isCollapsed = data.at(0).toInt() == 1 || data.at(0).toInt() == 3;
	groupIsCollapsed = data.at(0).toInt() >= 2;
	if (data.count() > 1) groupIndex = data.at(1).toInt();
	if (data.count() > 2) groupName = data.at(2);
    }
};

class EffectParameter
{
public:
    EffectParameter(const QString &name, const QString &value): m_name(name), m_value(value) {}
    QString name()   const          {
        return m_name;
    }
    QString value() const          {
        return m_value;
    }
    void setValue(const QString &value) {
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
    bool hasParam(const QString &name) const {
        for (int i = 0; i < size(); i++)
            if (at(i).name() == name) return true;
        return false;
    }
    void setParamValue(const QString &name, const QString &value) {
	bool found = false;
        for (int i = 0; i < size(); i++)
            if (at(i).name() == name) {
		// update value
		replace(i, EffectParameter(name, value));
		found = true;
	    }
	if (!found) addParam(name, value);
    }
        
    QString paramValue(const QString &name, QString defaultValue = QString()) const {
        for (int i = 0; i < size(); i++) {
            if (at(i).name() == name) return at(i).value();
        }
        return defaultValue;
    }
    void addParam(const QString &name, const QString &value) {
        if (name.isEmpty()) return;
        append(EffectParameter(name, value));
    }
    void removeParam(const QString &name) {
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
    CommentedTime(const GenTime &time, QString comment)
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

QDebug operator << (QDebug qd, const ItemInfo &info);


#endif
