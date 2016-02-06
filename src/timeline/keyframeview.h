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

    explicit KeyframeView(int handleSize, QObject *parent = 0);
    virtual ~KeyframeView();

        /** The position of the current keyframe when it has moved */
    int editedKeyframe;
    /** The position of the current keyframe before it was moved */
    int selectedKeyframe;
    int duration;

    void updateSelectedKeyFrame(QRectF br);

    /** @brief Move the selected keyframe (does not influence the effect, only the display in timeline).
    * @param pos new Position
    * @param value new Value */
    void updateKeyFramePos(QRectF br, int frame, const double y);
    int checkForSingleKeyframe();
    double getKeyFrameClipHeight(QRectF br, const double y);
     /** @brief Returns the number of keyframes the selected effect has, -1 if none. */
    int keyframesCount();
    int selectedKeyFramePos() const;
    double editedKeyFrameValue();
    void editKeyframeType(int type);
    mlt_keyframe_type type(int frame);
    void removeKeyframe(int frame);
    void addKeyframe(int frame, double value, mlt_keyframe_type type);
    void addDefaultKeyframe(int frame, mlt_keyframe_type type);
    const QString serialize();
    bool loadKeyframes(const QLocale locale, QDomElement e, int length);
    void reset();
          /** @brief Draw the keyframes of a clip
      * @param painter The painter device for the clip
      */
    void drawKeyFrames(QRectF br, int length, bool active, QPainter *painter, const QTransform &transformation);
    int mouseOverKeyFrames(QRectF br, QPointF pos, double maxOffset, double scale);
    void showMenu(QWidget *parent, QPoint pos);
    QAction *parseKeyframeActions(QList <QAction *>actions);
    static QString cutAnimation(const QString &animation, int start, int duration);
	
private:
    Mlt::Properties m_keyProperties;
    Mlt::Animation m_keyAnim;
    KEYFRAMETYPE m_keyframeType;
    double m_keyframeDefault;
    double m_keyframeMin;
    double m_keyframeMax;
    double m_keyframeFactor;
    int m_handleSize;
    double keyframeUnmap(QRectF br, double y);
    double keyframeMap(QRectF br, double value);
    QPointF keyframeMap(QRectF br, int frame, double value);
    QPointF keyframePoint(QRectF br, int index);

signals:
    void updateKeyframes(const QRectF &r = QRectF());
};

#endif
