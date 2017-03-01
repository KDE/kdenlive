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
#include "effectslist/effectslist.h"

#include "kdenlive_debug.h"

#include <QTreeWidgetItem>
#include <QString>
#include <QHash>

const int MAXCLIPDURATION = 15000;

namespace Kdenlive
{

enum MonitorId {
    NoMonitor = 0x01,
    ClipMonitor = 0x02,
    ProjectMonitor = 0x04,
    RecordMonitor = 0x08,
    StopMotionMonitor = 0x10,
    DvdMonitor = 0x20
};

const int DefaultThumbHeight = 100;

}

enum OperationType {
    None = 0,
    WaitingForConfirm,
    MoveOperation,
    ResizeStart,
    ResizeEnd,
    RollingStart,
    RollingEnd,
    RippleStart,
    RippleEnd,
    FadeIn,
    FadeOut,
    TransitionStart,
    TransitionEnd,
    MoveGuide,
    KeyFrame,
    Seek,
    Spacer,
    RubberSelection,
    ScrollTimeline,
    ZoomTimeline
};

namespace PlaylistState
{

enum ClipState {
    Original = 0,
    VideoOnly = 1,
    AudioOnly = 2,
    Disabled = 3
};

}

namespace TimelineMode
{
enum EditMode {
    NormalEdit = 0,
    OverwriteEdit = 1,
    InsertEdit = 2
};
}

enum ClipType {
    Unknown = 0,
    Audio = 1,
    Video = 2,
    AV = 3,
    Color = 4,
    Image = 5,
    Text = 6,
    SlideShow = 7,
    Virtual = 8,
    Playlist = 9,
    WebVfx = 10,
    TextTemplate = 11,
    QText
};

enum ProjectItemType {
    ProjectClipType = QTreeWidgetItem::UserType,
    ProjectFoldeType,
    ProjectSubclipType
};

enum GraphicsRectItem {
    AVWidget = 70000,
    LabelWidget,
    TransitionWidget,
    GroupWidget
};

enum ProjectTool {
    SelectTool = 0,
    RazorTool = 1,
    SpacerTool = 2
};

enum MonitorSceneType {
    MonitorSceneNone = 0,
    MonitorSceneDefault,
    MonitorSceneGeometry,
    MonitorSceneCorners,
    MonitorSceneRoto,
    MonitorSceneSplit,
    MonitorSceneRipple
};

enum TransitionType {
    /** TRANSITIONTYPE: between 0-99: video trans, 100-199: video+audio trans, 200-299: audio trans */
    LumaTransition = 0,
    CompositeTransition = 1,
    PipTransition = 2,
    LumaFileTransition = 3,
    MixTransition = 200
};

enum MessageType {
    DefaultMessage,
    ProcessingJobMessage,
    OperationCompletedMessage,
    InformationMessage,
    ErrorMessage,
    MltError
};

enum TrackType {
    AudioTrack = 0,
    VideoTrack = 1
};

enum ClipJobStatus {
    NoJob = 0,
    JobWaiting = -1,
    JobWorking = -2,
    JobDone = -3,
    JobCrashed = -4,
    JobAborted = -5
};

enum CacheType {
    SystemCacheRoot = -1,
    CacheRoot = 0,
    CacheBase = 1,
    CachePreview = 2,
    CacheProxy = 3,
    CacheAudio = 4,
    CacheThumbs = 5
};

enum TrimMode {
    NormalTrim,
    RippleTrim,
    RollingTrim,
    SlipTrim,
    SlideTrim
};

class TrackInfo
{

public:
    TrackType type;
    QString trackName;
    bool isMute;
    bool isBlind;
    bool isLocked;
    int duration;
    EffectsList effectsList;
    TrackInfo() :
        type(VideoTrack),
        isMute(0),
        isBlind(0),
        isLocked(0),
        duration(0),
        effectsList(true) {}
};

struct ProfileInfo {
    QSize profileSize;
    double profileFps;
};

struct requestClipInfo {
    QDomElement xml;
    QString clipId;
    int imageHeight;
    bool replaceProducer;

    bool operator==(const requestClipInfo &a) const
    {
        return clipId == a.clipId;
    }
};

typedef QMap<QString, QString> stringMap;
typedef QMap<int, QMap<int, QByteArray> > audioByteArray;
typedef QVector<qint16> audioShortVector;

class ItemInfo
{
public:
    /** startPos is the position where the clip starts on the track */
    GenTime startPos;
    /** endPos is the duration where the clip ends on the track */
    GenTime endPos;
    /** cropStart is the position where the sub-clip starts, relative to the clip's 0 position */
    GenTime cropStart;
    /** cropDuration is the duration of the clip */
    GenTime cropDuration;
    /** Track number */
    int track;
    ItemInfo() : track(0) {}
    bool isValid() const
    {
        return startPos != endPos;
    }
    bool contains(GenTime pos) const
    {
        if (startPos == endPos) {
            return true;
        }
        return (pos <= endPos && pos >= startPos);
    }
    bool operator==(const ItemInfo &a) const
    {
        return startPos == a.startPos && endPos == a.endPos && track == a.track && cropStart == a.cropStart;
    }
};

class TransitionInfo
{
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
        forceTrack(0) {}
};

class MltVideoProfile
{
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
    // A profile's width should always be a multiple of 8
    void adjustWidth();
    MltVideoProfile();
    explicit MltVideoProfile(const QVariantList &params);
    bool operator==(const MltVideoProfile &point) const;
    bool operator!=(const MltVideoProfile &other) const;
    /** @brief Returns true if both profiles have same fps, and can be mixed with the xml producer */
    bool isCompatible(const MltVideoProfile &point) const;
    bool isValid() const;
    const QVariantList toList() const;
    const QString descriptiveString();
    const QString dialogDescriptiveString();
};

class CommentedTime
{
public:
    CommentedTime();
    CommentedTime(const GenTime &time, const QString &comment, int markerType = 0);
    CommentedTime(const QString &hash, const GenTime &time);

    QString comment() const;
    GenTime time() const;
    /** @brief Returns a string containing infos needed to store marker info. string equals marker type + QLatin1Char(':') + marker comment */
    QString hash() const;
    void setComment(const QString &comm);
    void setMarkerType(int t);
    int markerType() const;
    static QColor markerColor(int type);

    /* Implementation of > operator; Works identically as with basic types. */
    bool operator>(const CommentedTime &op) const;
    /* Implementation of < operator; Works identically as with basic types. */
    bool operator<(const CommentedTime &op) const;
    /* Implementation of >= operator; Works identically as with basic types. */
    bool operator>=(const CommentedTime &op) const;
    /* Implementation of <= operator; Works identically as with basic types. */
    bool operator<=(const CommentedTime &op) const;
    /* Implementation of == operator; Works identically as with basic types. */
    bool operator==(const CommentedTime &op) const;
    /* Implementation of != operator; Works identically as with basic types. */
    bool operator!=(const CommentedTime &op) const;

private:
    GenTime t;
    QString c;
    int type;
};

QDebug operator << (QDebug qd, const ItemInfo &info);
QDebug operator << (QDebug qd, const MltVideoProfile &profile);

//we provide hash function for qstring
namespace std {
    template <>
    struct hash<QString>
    {
        std::size_t operator()(const QString& k) const
        {
            return qHash(k);
        }
    };

}

#endif
