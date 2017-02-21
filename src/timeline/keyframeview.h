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

#ifndef KEYFRAMEVIEW_H
#define KEYFRAMEVIEW_H

#include "definitions.h"
#include "gentime.h"

#include "mlt++/MltProperties.h"
#include "mlt++/MltAnimation.h"

class QAction;

/**
 * @class KeyframeView
 * @brief Provides functionality for displaying and managing animation keyframes in timeline.
 *
 */

class KeyframeView : public QObject
{
    Q_OBJECT

public:

    enum KEYFRAMETYPE {
        NoKeyframe = 0,
        SimpleKeyframe,
        NormalKeyframe,
        GeometryKeyframe,
        AnimatedKeyframe
    };

    explicit KeyframeView(int handleSize, QObject *parent = nullptr);
    virtual ~KeyframeView();

    /** The position of the currently active keyframe */
    int activeKeyframe;
    /** The position of the currently active keyframe before it was moved */
    int originalKeyframe;
    /** @brief The keyframe position from which keyframes will be attached to end (-1 = disabled). */
    int attachToEnd;
    /** @brief Effect duration, must be updated on resize. */
    int duration;

    /** @brief Move the selected keyframe (does not influence the effect, only the display in timeline).
    * @param br the bounding rect for effect drawing
    * @param frame new Position
    * @param y new Value */
    void updateKeyFramePos(const QRectF &br, int frame, const double y);
    double getKeyFrameClipHeight(const QRectF &br, const double y);
    /** @brief Returns the number of keyframes the selected effect has, -1 if none. */
    int keyframesCount();
    double editedKeyFrameValue();
    void editKeyframeType(int type);
    void attachKeyframeToEnd(bool attach);
    mlt_keyframe_type type(int frame);
    void removeKeyframe(int frame);
    void addKeyframe(int frame, double value, mlt_keyframe_type type);
    void addDefaultKeyframe(ProfileInfo profile, int frame, mlt_keyframe_type type);
    const QString serialize(const QString &name = QString(), bool rectAnimation = false);
    /** @brief Loads a rect animation and returns minimas/maximas for x,y,w,h **/
    QList<QPoint> loadKeyframes(const QString &data);
    bool loadKeyframes(const QLocale &locale, const QDomElement &e, int cropStart, int length);
    void reset();
    /** @brief Draw the keyframes of a clip
      * @param painter The painter device for the clip
      */
    void drawKeyFrames(const QRectF &br, int length, bool active, QPainter *painter, const QTransform &transformation);
    /** @brief Draw the x, y, w, h channels of an animated geometry */
    void drawKeyFrameChannels(const QRectF &br, int in, int out, QPainter *painter, const QList<QPoint> &maximas, int limitKeyframes, const QColor &textColor);
    int mouseOverKeyFrames(const QRectF &br, QPointF pos, double scale);
    void showMenu(QWidget *parent, QPoint pos);
    QAction *parseKeyframeActions(const QList<QAction *> &actions);
    static QString cutAnimation(const QString &animation, int start, int duration, int fullduration, bool doCut = true);
    /** @brief when an animation is resized, update in / out point keyframes */
    static QString switchAnimation(const QString &animation, int newPos, int oldPos, int newDuration, int oldDuration, bool isRect);
    /** @brief when loading an animation from a serialized string, check where is the first negative keyframe) */
    static int checkNegatives(const QString &data, int maxDuration);
    /** @brief Add keyframes at start / end points if not existing */
    static const QString addBorderKeyframes(const QString &animation, int start, int duration);
    /** @brief returns true if currently edited parameter name is name */
    bool activeParam(const QString &name) const;
    /** @brief Sets a temporary offset for drawing keyframes when resizing clip start */
    void setOffset(int frames);
    /** @brief Returns a copy of the original anim, with a crop zone (in/out), frame offset, max number of keyframes, and value mapping */
    QString getSingleAnimation(int ix, int in, int out, int offset, int limitKeyframes, QPoint maximas, double min, double max);
    /** @brief Returns a copy of the original anim, with a crop zone (in/out) and frame offset */
    QString getOffsetAnimation(int in, int out, int offset, int limitKeyframes, ProfileInfo profile, bool allowAnimation, bool positionOnly, QPoint rectOffset);

private:
    Mlt::Properties m_keyProperties;
    Mlt::Animation m_keyAnim;
    KEYFRAMETYPE m_keyframeType;
    QString m_inTimeline;
    double m_keyframeDefault;
    double m_keyframeMin;
    double m_keyframeMax;
    double m_keyframeFactor;
    int m_handleSize;
    bool m_useOffset;
    int m_offset;
    QStringList m_notInTimeline;
    double keyframeUnmap(const QRectF &br, double y);
    double keyframeMap(const QRectF &br, double value);
    QPointF keyframeMap(const QRectF &br, int frame, double value);
    QPointF keyframePoint(const QRectF &br, int index);
    QPointF keyframePoint(const QRectF &br, int frame, double value, double factor, double min, double max);
    struct ParameterInfo {
        double factor;
        double min;
        double max;
        QString defaultValue;
    };
    QMap<QString, ParameterInfo> m_paramInfos;

signals:
    void updateKeyframes(const QRectF &r = QRectF());
};

#endif
