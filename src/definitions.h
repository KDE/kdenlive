/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "utils/gentime.h"

#include "kdenlive_debug.h"

#include <QDomElement>
#include <QHash>
#include <QPersistentModelIndex>
#include <QString>
#include <QUuid>
#include <cassert>
#include <memory>

const int MAXCLIPDURATION = 15000;

namespace Kdenlive {

enum MonitorId { NoMonitor = 0x01, ClipMonitor = 0x02, ProjectMonitor = 0x04, RecordMonitor = 0x08, StopMotionMonitor = 0x10, RenderMonitor = 0x20 };
enum ConfigPage {
    PageMisc = 0,
    PageEnv,
    PageTimeline,
    PageTools,
    PageCapture,
    PageJogShuttle,
    PagePlayback,
    PageTranscode,
    PageProjectDefaults,
    PageColorsGuides,
    PageSpeech,
    NoPage
};

const int DefaultThumbHeight = 100;
} // namespace Kdenlive

enum class GroupType {
    Normal,
    Selection, // in that case, the group is used to emulate a selection
    AVSplit,   // in that case, the group links the audio and video of the same clip
    Leaf       // This is a leaf (clip or composition)
};

const QString groupTypeToStr(GroupType t);
GroupType groupTypeFromStr(const QString &s);

enum class ObjectType { TimelineClip, TimelineComposition, TimelineTrack, TimelineMix, TimelineSubtitle, BinClip, Master, NoItem };
// using ObjectId = std::pair<ObjectType, std::pair<int, QUuid>>;
struct ObjectId
{
    ObjectType type;
    int itemId;
    QUuid uuid;
    explicit constexpr ObjectId(const ObjectType tp = ObjectType::NoItem, int id = -1, const QUuid uid = QUuid())
        : type(tp)
        , itemId(id)
        , uuid(uid)
    {
    }
    /*inline ObjectId &operator=(const ObjectId &a)
    {
        type = a.type;
        itemId = a.itemId;
        uuid = a.uuid;
        return *this;
    }*/
    inline bool operator==(const ObjectId &a) const
    {
        if (a.type == type && a.itemId == itemId && a.uuid == uuid)
            return true;
        else
            return false;
    }
    inline bool operator!=(const ObjectId &a) const
    {
        if (a.type != type || a.itemId != itemId || a.uuid != uuid)
            return true;
        else
            return false;
    }
};

enum class MixAlignment { AlignNone, AlignLeft, AlignRight, AlignCenter };

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

namespace PlaylistState {
Q_NAMESPACE
enum ClipState { VideoOnly = 1, AudioOnly = 2, Disabled = 3, Unknown = 4 };
Q_ENUM_NS(ClipState)
} // namespace PlaylistState

namespace FileStatus {
Q_NAMESPACE
enum ClipStatus { StatusReady = 0, StatusProxy, StatusMissing, StatusWaiting, StatusDeleting, StatusProxyOnly };
Q_ENUM_NS(ClipStatus)
} // namespace PlaylistState

// returns a pair corresponding to (video, audio)
std::pair<bool, bool> stateToBool(PlaylistState::ClipState state);
PlaylistState::ClipState stateFromBool(std::pair<bool, bool> av);

namespace TimelineMode {
enum EditMode { NormalEdit = 0, OverwriteEdit = 1, InsertEdit = 2 };
}

namespace AssetListType {
Q_NAMESPACE
enum AssetType { Preferred, Video, Audio, Custom, CustomAudio, Template, TemplateAudio, Favorites, AudioComposition, VideoShortComposition, VideoComposition, AudioTransition, VideoTransition, Text, Hidden = -1 };
Q_ENUM_NS(AssetType)
}

namespace ClipType {
Q_NAMESPACE
enum ProducerType {
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
    QText = 12,
    Composition = 13,
    Track = 14,
    Qml = 15,
    Animation = 16,
    Timeline = 17
};
Q_ENUM_NS(ProducerType)
} // namespace ClipType

enum ProjectItemType { ProjectClipType = 0, ProjectFolderType, ProjectSubclipType };

enum GraphicsRectItem { AVWidget = 70000, LabelWidget, TransitionWidget, GroupWidget };

namespace ToolType {
Q_NAMESPACE
enum ProjectTool {
    SelectTool = 0,
    RazorTool = 1,
    SpacerTool = 2,
    RippleTool = 3,
    RollTool = 4,
    SlipTool = 5,
    SlideTool = 6,
    MulticamTool = 7
};
Q_ENUM_NS(ProjectTool)
}

enum MonitorSceneType {
    MonitorSceneNone = 0,
    MonitorSceneDefault,
    MonitorSceneGeometry,
    MonitorSceneCorners,
    MonitorSceneRoto,
    MonitorSceneSplit,
    MonitorSceneTrimming,
    MonitorSplitTrack
};

enum MessageType { DefaultMessage, ProcessingJobMessage, OperationCompletedMessage, InformationMessage, ErrorMessage, MltError, TooltipMessage };

namespace BinMessage {
enum BinCategory { NoMessage = 0, ProfileMessage, StreamsMessage, InformationMessage, UpdateMessage };
}

enum TrackType { AudioTrack = 0, VideoTrack = 1, AnyTrack = 2, SubtitleTrack = 3 };

enum CacheType {
    SystemCacheRoot = -1,
    CacheRoot = 0,
    CacheBase = 1,
    CachePreview = 2,
    CacheProxy = 3,
    CacheAudio = 4,
    CacheThumbs = 5,
    CacheSequence = 6,
    CacheTmpWorkFiles = 7
};

enum TrimMode { NormalTrim, RippleTrim, RollingTrim, SlipTrim, SlideTrim };

class TrackInfo
{

public:
    TrackType type;
    QString trackName;
    bool isMute;
    bool isBlind;
    bool isLocked;
    int duration;
    /*EffectsList effectsList;
    TrackInfo()
        : type(VideoTrack)
        , isMute(0)
        , isBlind(0)
        , isLocked(0)
        , duration(0)
        , effectsList(true)
    {
    }*/
};

struct requestClipInfo
{
    QDomElement xml;
    QString clipId;
    int imageHeight;
    bool replaceProducer;

    bool operator==(const requestClipInfo &a) const { return clipId == a.clipId; }
};

typedef QMap<QString, QString> stringMap;
typedef QMap<int, QMap<int, QByteArray>> audioByteArray;
using audioShortVector = QVector<qint16>;

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
    int track{0};
    ItemInfo() = default;
    bool isValid() const { return startPos != endPos; }
    bool contains(GenTime pos) const
    {
        if (startPos == endPos) {
            return true;
        }
        return (pos <= endPos && pos >= startPos);
    }
    bool operator==(const ItemInfo &a) const { return startPos == a.startPos && endPos == a.endPos && track == a.track && cropStart == a.cropStart; }
};

class TransitionInfo
{
public:
    /** startPos is the position where the clip starts on the track */
    GenTime startPos;
    /** endPos is the duration where the clip ends on the track */
    GenTime endPos;
    /** the track on which the transition is (b_track)*/
    int b_track{0};
    /** the track on which the transition is applied (a_track)*/
    int a_track{0};
    /** Does the user request for a special a_track */
    bool forceTrack{false};
    TransitionInfo() = default;
};

class CommentedTime
{
public:
    CommentedTime();
    CommentedTime(const GenTime &time, QString comment, int markerType = 0);
    CommentedTime(const QString &hash, const GenTime &time);

    QString comment() const;
    GenTime time() const;
    /** @brief Returns a string containing infos needed to store marker info. string equals marker type + QLatin1Char(':') + marker comment */
    QString hash() const;
    void setComment(const QString &comm);
    void setTime(const GenTime &t);
    void setMarkerType(int t);
    int markerType() const;

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
    GenTime m_time;
    QString m_comment;
    int m_type{0};
};

class SubtitledTime
{
public:
    SubtitledTime();
    SubtitledTime(const GenTime &start, QString sub, const GenTime &end);
    
    QString subtitle() const;
    GenTime start() const;
    GenTime end() const;
    
    void setSubtitle(const QString &sub);
    void setEndTime(const GenTime &end);
    
    /* Implementation of > operator; Works identically as with basic types. */
    bool operator>(const SubtitledTime &op) const;
    /* Implementation of < operator; Works identically as with basic types. */
    bool operator<(const SubtitledTime &op) const;
    /* Implementation of == operator; Works identically as with basic types. */
    bool operator==(const SubtitledTime &op) const;
    /* Implementation of != operator; Works identically as with basic types. */
    bool operator!=(const SubtitledTime &op) const;
    
private:
    GenTime m_starttime;
    QString m_subtitle;
    GenTime m_endtime;
};

QDebug operator<<(QDebug qd, const ItemInfo &info);

// we provide hash function for qstring and QPersistentModelIndex
namespace std {
template <> struct hash<QPersistentModelIndex>
{
    std::size_t operator()(const QPersistentModelIndex &k) const { return qHash(k); }
};
} // namespace std

// The following is a hack that allows one to use shared_from_this in the case of a multiple inheritance.
// Credit: https://stackoverflow.com/questions/14939190/boost-shared-from-this-and-multiple-inheritance
template <typename T> struct enable_shared_from_this_virtual;

class enable_shared_from_this_virtual_base : public std::enable_shared_from_this<enable_shared_from_this_virtual_base>
{
    using base_type = std::enable_shared_from_this<enable_shared_from_this_virtual_base>;
    template <typename T> friend struct enable_shared_from_this_virtual;

    std::shared_ptr<enable_shared_from_this_virtual_base> shared_from_this() { return base_type::shared_from_this(); }
    std::shared_ptr<enable_shared_from_this_virtual_base const> shared_from_this() const { return base_type::shared_from_this(); }
};

template <typename T> struct enable_shared_from_this_virtual : virtual enable_shared_from_this_virtual_base
{
    using base_type = enable_shared_from_this_virtual_base;

public:
    std::shared_ptr<T> shared_from_this()
    {
        std::shared_ptr<T> result(base_type::shared_from_this(), static_cast<T *>(this));
        return result;
    }

    std::shared_ptr<T const> shared_from_this() const
    {
        std::shared_ptr<T const> result(base_type::shared_from_this(), static_cast<T const *>(this));
        return result;
    }
};

// This is a small trick to have a QAbstractItemModel with shared_from_this enabled without multiple inheritance
// Be careful, if you use this class, you have to make sure to init weak_this_ when you construct a shared_ptr to your object
template <class T> class QAbstractItemModel_shared_from_this : public QAbstractItemModel
{
protected:
    QAbstractItemModel_shared_from_this()
        : QAbstractItemModel()
    {
    }

public:
    std::shared_ptr<T> shared_from_this()
    {
        std::shared_ptr<T> p(weak_this_);
        assert(p.get() == this);
        return p;
    }

    std::shared_ptr<T const> shared_from_this() const
    {
        std::shared_ptr<T const> p(weak_this_);
        assert(p.get() == this);
        return p;
    }

public: // actually private, but avoids compiler template friendship issues
    mutable std::weak_ptr<T> weak_this_;
};
