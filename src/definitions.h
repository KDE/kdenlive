/*
    SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "utils/gentime.h"

#include "kdenlive_debug.h"
#include "mlt++/Mlt.h"
#include <QDomElement>
#include <QHash>
#include <QPersistentModelIndex>
#include <QString>
#include <QUuid>
#include <cassert>
#include <memory>
#include <qcolor.h>

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

enum LinuxPackageType { AppImage, Flatpak, Snap, Unknown };

enum class GroupType {
    Normal,
    Selection, // in that case, the group is used to emulate a selection
    AVSplit,   // in that case, the group links the audio and video of the same clip
    Leaf       // This is a leaf (clip or composition)
};

const QString groupTypeToStr(GroupType t);
GroupType groupTypeFromStr(const QString &s);

// We can not use just ObjectType as name because that causes conflicts with Xcode on macOS
enum class KdenliveObjectType { TimelineClip, TimelineComposition, TimelineTrack, TimelineMix, TimelineSubtitle, BinClip, Master, NoItem };
// using ObjectId = std::pair<ObjectType, std::pair<int, QUuid>>;
struct ObjectId
{
    KdenliveObjectType type;
    int itemId;
    QUuid uuid;
    explicit constexpr ObjectId(const KdenliveObjectType tp = KdenliveObjectType::NoItem, int id = -1, const QUuid uid = QUuid())
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
    NoOperation = 0,
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
enum AssetType {
    Preferred,
    Video,
    Audio,
    Custom,
    CustomAudio,
    Template,
    TemplateAudio,
    TemplateCustom,
    TemplateCustomAudio,
    Favorites,
    AudioComposition,
    VideoShortComposition,
    VideoComposition,
    AudioTransition,
    VideoTransition,
    Text,
    Hidden = -1
};
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

namespace KeyframeType {
Q_NAMESPACE
enum KeyframeEnum {
    Linear = mlt_keyframe_linear,
    Discrete = mlt_keyframe_discrete,
    Curve = mlt_keyframe_smooth,
    CurveSmooth = mlt_keyframe_smooth_natural,
    BounceIn = mlt_keyframe_bounce_in,
    BounceOut = mlt_keyframe_bounce_out,
    CubicIn = mlt_keyframe_cubic_in,
    CubicOut = mlt_keyframe_cubic_out,
    ExponentialIn = mlt_keyframe_exponential_in,
    ExponentialOut = mlt_keyframe_exponential_out,
    CircularIn = mlt_keyframe_circular_in,
    CircularOut = mlt_keyframe_circular_out,
    ElasticIn = mlt_keyframe_elastic_in,
    ElasticOut = mlt_keyframe_elastic_out
};
Q_ENUM_NS(KeyframeEnum)
} // namespace KeyframeType

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

struct SequenceInfo
{
    QString sequenceName;
    QString sequenceId;
    QString sequenceDuration;
    int sequenceFrameDuration{0};
};

typedef QMap<QString, QString> stringMap;
typedef QMap<int, QMap<int, QByteArray>> audioByteArray;
typedef QMap<QUuid, SequenceInfo> sequenceMap;
using audioShortVector = QVector<qint16>;

class ItemInfo
{
public:
    /** startPos is the position where the clip starts on the track */
    int position{-1};
    /** endPos is the duration where the clip ends on the track */
    int endPos{-1};
    /** cropStart is the position where the sub-clip starts, relative to the clip's 0 position */
    int cropStart{0};
    /** playtime is the duration of the clip */
    int playTime;
    /** maxDuration is the maximum length of the clip, -1 if no limit */
    int maxDuration{-1};
    /** Track id */
    int trackId{-1};
    /** Clip id */
    int itemId{-1};
    ItemInfo() = default;
    bool isValid() const { return position != endPos; }
    bool contains(int pos) const
    {
        if (position == endPos) {
            return true;
        }
        return (pos <= endPos && pos >= position);
    }
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

class SubtitleEvent
{
public:
    SubtitleEvent(){};
    // Create a subtitle event from a string, and pass the layer & start time through the pointer if needed
    SubtitleEvent(const QString &eventString, const double fps, const double factor = 1., std::pair<int, GenTime> *start = nullptr);
    SubtitleEvent(bool isDialogue, const GenTime &end, const QString &styleName, const QString &name, int marginL, int marginR, int marginV,
                  const QString &effect, const QString &text)
        : m_isDialogue(isDialogue)
        , m_endTime(end)
        , m_styleName(styleName)
        , m_name(name)
        , m_marginL(marginL)
        , m_marginR(marginR)
        , m_marginV(marginV)
        , m_effect(effect)
        , m_text(text)
    {
    }

    bool isDialogue() const { return m_isDialogue; }
    const GenTime endTime() const { return m_endTime; }
    const QString styleName() const { return m_styleName; }
    const QString name() const { return m_name; }
    int marginL() const { return m_marginL; }
    int marginR() const { return m_marginR; }
    int marginV() const { return m_marginV; }
    const QString effect() const { return m_effect; }
    const QString text() const { return m_text; }

    void setIsDialogue(bool isDialogue) { m_isDialogue = isDialogue; }
    void setEndTime(const GenTime &time) { m_endTime = time; }
    void setStyleName(const QString &style) { m_styleName = style; }
    void setName(const QString &name) { m_name = name; }
    void setMarginL(int margin) { m_marginL = margin; }
    void setMarginR(int margin) { m_marginR = margin; }
    void setMarginV(int margin) { m_marginV = margin; }
    void setEffect(const QString &effect)
    {
        m_effect = effect;
        m_effect.remove('\n');
    }
    void setText(const QString &text) { m_text = text; }

    // convert ASS format time string to GenTime
    static GenTime stringtoTime(const QString &str, const double fps, const double factor = 1.);

    /* convert GenTime to string
     *
     * 0 - ASS Format
     *
     * 1 - SRT Format
     */
    static QString timeToString(const GenTime &time, const int format);

    QString toString(int layer, GenTime start) const;

    bool operator==(const SubtitleEvent &op) const;
    bool operator!=(const SubtitleEvent &op) const;

private:
    bool m_isDialogue = false;
    GenTime m_endTime = GenTime();
    QString m_styleName;
    QString m_name;
    int m_marginL;
    int m_marginR;
    int m_marginV;
    QString m_effect;
    QString m_text;
};

class SubtitleStyle
{
public:
    SubtitleStyle(){};
    SubtitleStyle(QString styleString);
    SubtitleStyle(QString fontName, double fontSize, QColor primaryColour, QColor secondaryColour, QColor outlineColour, QColor backColour, bool bold,
                  bool italic, bool underline, bool strikeOut, double scaleX, double scaleY, double spacing, double angle, int borderStyle, double outline,
                  double shadow, int alignment, int marginL, int marginR, int marginV, int encoding)
        : m_fontName(fontName)
        , m_fontSize(fontSize)
        , m_primaryColour(primaryColour)
        , m_secondaryColour(secondaryColour)
        , m_outlineColour(outlineColour)
        , m_backColour(backColour)
        , m_bold(bold)
        , m_italic(italic)
        , m_underline(underline)
        , m_strikeOut(strikeOut)
        , m_scaleX(scaleX)
        , m_scaleY(scaleY)
        , m_spacing(spacing)
        , m_angle(angle)
        , m_borderStyle(borderStyle)
        , m_outline(outline)
        , m_shadow(shadow)
        , m_alignment(alignment)
        , m_marginL(marginL)
        , m_marginR(marginR)
        , m_marginV(marginV)
        , m_encoding(encoding)
    {
    }

    QString toString(QString name) const;

    QString fontName() const { return m_fontName; }
    double fontSize() const { return m_fontSize; }
    QColor primaryColour() const { return m_primaryColour; }
    QColor secondaryColour() const { return m_secondaryColour; }
    QColor outlineColour() const { return m_outlineColour; }
    QColor backColour() const { return m_backColour; }
    bool bold() const { return m_bold; }
    bool italic() const { return m_italic; }
    bool underline() const { return m_underline; }
    bool strikeOut() const { return m_strikeOut; }
    double scaleX() const { return m_scaleX; }
    double scaleY() const { return m_scaleY; }
    double spacing() const { return m_spacing; }
    double angle() const { return m_angle; }
    int borderStyle() const { return m_borderStyle; }
    double outline() const { return m_outline; }
    double shadow() const { return m_shadow; }
    int alignment() const { return m_alignment; }
    int marginL() const { return m_marginL; }
    int marginR() const { return m_marginR; }
    int marginV() const { return m_marginV; }
    int encoding() const { return m_encoding; }

    void setFontName(QString fontName) { m_fontName = fontName; }
    void setFontSize(double fontSize) { m_fontSize = fontSize; }
    void setPrimaryColour(QColor primaryColour) { m_primaryColour = primaryColour; }
    void setSecondaryColour(QColor secondaryColour) { m_secondaryColour = secondaryColour; }
    void setOutlineColour(QColor outlineColour) { m_outlineColour = outlineColour; }
    void setBackColour(QColor backColour) { m_backColour = backColour; }
    void setBold(bool bold) { m_bold = bold; }
    void setItalic(bool italic) { m_italic = italic; }
    void setUnderline(bool underline) { m_underline = underline; }
    void setStrikeOut(bool strikeOut) { m_strikeOut = strikeOut; }
    void setScaleX(double scaleX) { m_scaleX = scaleX; }
    void setScaleY(double scaleY) { m_scaleY = scaleY; }
    void setSpacing(double spacing) { m_spacing = spacing; }
    void setAngle(double angle) { m_angle = angle; }
    void setBorderStyle(int borderStyle) { m_borderStyle = borderStyle; }
    void setOutline(double outline) { m_outline = outline; }
    void setShadow(double shadow) { m_shadow = shadow; }
    void setAlignment(int alignment) { m_alignment = alignment; }
    void setMarginL(int marginL) { m_marginL = marginL; }
    void setMarginR(int marginR) { m_marginR = marginR; }
    void setMarginV(int marginV) { m_marginV = marginV; }
    void setEncoding(int encoding) { m_encoding = encoding; }

private:
    QString m_fontName = "Arial";
    double m_fontSize = 60;
    QColor m_primaryColour = QColor(255, 255, 255, 255);
    QColor m_secondaryColour = QColor(255, 0, 0, 255);
    QColor m_outlineColour = QColor(0, 0, 0, 255);
    QColor m_backColour = QColor(0, 0, 0, 255);
    bool m_bold = false;
    bool m_italic = false;
    bool m_underline = false;
    bool m_strikeOut = false;
    double m_scaleX = 100;
    double m_scaleY = 100;
    double m_spacing = 0;
    double m_angle = 0;
    int m_borderStyle = 1;
    double m_outline = 1;
    double m_shadow = 0;
    int m_alignment = 2;
    int m_marginL = 40;
    int m_marginR = 40;
    int m_marginV = 40;
    int m_encoding = 1;
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
