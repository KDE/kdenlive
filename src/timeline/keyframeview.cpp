/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
Copyright (C) 2016  Vincent Pinon <vpinon@kde.org>
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

#include <QPainter>
#include <QAction>
#include "klocalizedstring.h"

#include "keyframeview.h"
#include "mltcontroller/effectscontroller.h"

KeyframeView::KeyframeView(int handleSize, QObject *parent) : QObject(parent)
    , activeKeyframe(-1)
    , originalKeyframe(-1)
    , attachToEnd(-2)
    , duration(0)
    , m_keyframeType(KEYFRAMETYPE::NoKeyframe)
    , m_keyframeDefault(0)
    , m_keyframeMin(0)
    , m_keyframeMax(1)
    , m_keyframeFactor(1)
    , m_handleSize(handleSize)
    , m_useOffset(false)
    , m_offset(0)
{
}

KeyframeView::~KeyframeView()
{
}

double KeyframeView::keyframeUnmap(const QRectF &br, double y)
{
    return ((br.bottom() - y) / br.height() * (m_keyframeMax - m_keyframeMin) + m_keyframeMin) / m_keyframeFactor;
}

double KeyframeView::keyframeMap(const QRectF &br, double value)
{
    return br.bottom() - br.height() * (value * m_keyframeFactor - m_keyframeMin) / (m_keyframeMax - m_keyframeMin);
}

QPointF KeyframeView::keyframeMap(const QRectF &br, int frame, double value)
{
    return QPointF(br.x() + br.width() * frame / duration,
                   br.bottom() - br.height() * (value * m_keyframeFactor - m_keyframeMin) / (m_keyframeMax - m_keyframeMin));
}

QPointF KeyframeView::keyframePoint(const QRectF &br, int index)
{
    int frame = m_keyAnim.key_get_frame(index);
    return keyframeMap(br, frame < 0 ? frame + duration + m_offset : frame + m_offset, m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), frame, duration - m_offset));
}

QPointF KeyframeView::keyframePoint(const QRectF &br, int frame, double value, double factor, double min, double max)
{
    return QPointF(br.x() + br.width() * frame / duration,
                   br.bottom() - br.height() * (value * factor - min) / (max - min));
}

void KeyframeView::drawKeyFrames(const QRectF &br, int length, bool active, QPainter *painter, const QTransform &transformation)
{
    if (duration == 0 || m_inTimeline.isEmpty() || m_keyframeType == NoKeyframe || !m_keyAnim.is_valid() || m_keyAnim.key_count() < 1) {
        return;
    }
    duration = length;
    //m_keyAnim.set_length(length);
    painter->save();
    QPointF h(m_handleSize, m_handleSize);

    // draw keyframes
    // Special case: Geometry keyframes are just vertical lines
    if (m_keyframeType == GeometryKeyframe) {
        for (int i = 0; i < m_keyAnim.key_count(); ++i) {
            int frame = m_keyAnim.key_get_frame(i);
            QColor color = (frame == activeKeyframe) ? QColor(Qt::red) : QColor(Qt::blue);
            if (active) {
                painter->setPen(color);
            }
            QPointF k = keyframePoint(br, i);
            painter->drawLine(transformation.map(QLineF(k.x(), br.top(), k.x(), br.height())));
            if (active) {
                k.setY(br.top() + br.height() / 2);
                painter->setBrush((m_keyAnim.key_get_frame(i) == activeKeyframe) ? QColor(Qt::red) : QColor(Qt::blue));
                painter->drawEllipse(QRectF(transformation.map(k) - h / 2, transformation.map(k) + h / 2));
            }
        }
        painter->restore();
        return;
    }

    // draw line showing default value
    if (active) {
        QColor col(Qt::black);
        col.setAlpha(140);
        painter->fillRect(QRectF(transformation.map(br.topLeft()), transformation.map(br.bottomRight())), col);
        double y = keyframeMap(br, m_keyframeDefault);
        QLineF line = transformation.map(QLineF(br.x(), y, br.right(), y));
        painter->setPen(QColor(168, 168, 168, 180));
        painter->drawLine(line);
        painter->setPen(QColor(108, 108, 108, 180));
        painter->drawLine(line.translated(0, 1));
        painter->setPen(QColor(Qt::white));
        painter->setRenderHint(QPainter::Antialiasing);
        // Draw zone where keyframes are attached to end
        if (attachToEnd > -2) {
            QRectF negZone = br;
            negZone.setLeft(br.x() + br.width() * attachToEnd / (double) duration);
            QColor neg(Qt::darkYellow);
            neg.setAlpha(190);
            painter->fillRect(QRectF(transformation.map(negZone.topLeft()), transformation.map(negZone.bottomRight())), neg);
        }
    }

    int cnt = m_keyProperties.count();
    QStringList paramNames;
    paramNames.reserve(cnt);
    for (int i = 0; i < cnt; i++) {
        paramNames << m_keyProperties.get_name(i);
    }
    paramNames.removeAll(m_inTimeline);
    // Make sure edited param is painted last
    paramNames.append(m_inTimeline);
    foreach (const QString &paramName, paramNames) {
        if (m_notInTimeline.contains(paramName)) {
            continue;
        }
        ParameterInfo info = m_paramInfos.value(paramName);
        if (info.max == info.min) {
            // this is probably an animated rect
            continue;
        }
        Mlt::Animation drawAnim = m_keyProperties.get_animation(paramName.toUtf8().constData());
        if (!drawAnim.is_valid()) {
            continue;
        }
        QPainterPath path;
        // Find first key before our clip start, get frame for rect left first
        int firstKF = qMax(0, drawAnim.previous_key(-m_offset));
        int lastKF = drawAnim.next_key(duration - m_offset);
        if (lastKF < duration - m_offset) {
            lastKF = duration - m_offset;
        }
        int frame = firstKF;
        double value = m_keyProperties.anim_get_double(paramName.toUtf8().constData(), frame, duration - m_offset);
        QPointF start = keyframePoint(br, frame + m_offset, value, info.factor, info.min, info.max);
        path.moveTo(br.x(), br.bottom());
        path.lineTo(br.x(), start.y());
        path.lineTo(start);
        painter->setPen(paramName == m_inTimeline ? QColor(Qt::white) : Qt::NoPen);
        for (int i = 0; i < drawAnim.key_count(); ++i) {
            int currentFrame = drawAnim.key_get_frame(i);
            if (currentFrame < firstKF) {
                continue;
            }
            if (currentFrame > lastKF) {
                break;
            }
            if (active && paramName == m_inTimeline) {
                painter->setBrush((drawAnim.key_get_frame(i) == activeKeyframe) ? QColor(Qt::red) : QColor(Qt::blue));
                painter->drawEllipse(QRectF(transformation.map(start) - h / 2, transformation.map(start) + h / 2));
            }
            if (i + 1 < drawAnim.key_count()) {
                frame = drawAnim.key_get_frame(i + 1);
                value = m_keyProperties.anim_get_double(paramName.toUtf8().constData(), frame, duration - m_offset);
                QPointF end = keyframePoint(br, frame + m_offset, value, info.factor, info.min, info.max);
                //QPointF end = keyframePoint(br, i + 1);
                switch (drawAnim.key_get_type(i)) {
                case mlt_keyframe_discrete:
                    path.lineTo(end.x(), start.y());
                    path.lineTo(end);
                    break;
                case mlt_keyframe_linear:
                    path.lineTo(end);
                    break;
                case mlt_keyframe_smooth:
                    frame = drawAnim.key_get_frame(qMax(i - 1, 0));
                    value = m_keyProperties.anim_get_double(paramName.toUtf8().constData(), frame, duration - m_offset);
                    QPointF pre = keyframePoint(br, frame + m_offset, value, info.factor, info.min, info.max);
                    frame = drawAnim.key_get_frame(qMin(i + 2, drawAnim.key_count() - 1));
                    value = m_keyProperties.anim_get_double(paramName.toUtf8().constData(), frame, duration - m_offset);
                    QPointF post = keyframePoint(br, frame + m_offset, value, info.factor, info.min, info.max);
                    QPointF c1 = (end - pre) / 6.0; // + start
                    QPointF c2 = (start - post) / 6.0; // + end
                    double mid = (end.x() - start.x()) / 2;
                    if (c1.x() >  mid) {
                        c1 = c1 * mid / c1.x();    // scale down tangent vector to not go beyond middle
                    }
                    if (c2.x() < -mid) {
                        c2 = c2 * -mid / c2.x();
                    }
                    path.cubicTo(start + c1, end + c2, end);
                    break;
                }
                start = end;
            } else {
                path.lineTo(br.right(), start.y());
            }
        }
        path.lineTo(br.right(), br.bottom());
        if (paramName == m_inTimeline) {
            QColor col(Qt::white);
            col.setAlpha(active ? 120 : 80);
            painter->setBrush(col);
        } else {
            QColor col;
            switch (paramNames.indexOf(paramName)) {
            case 0:
                col = Qt::blue;
                break;
            case 1:
                col = Qt::green;
                break;
            case 2:
                col = Qt::yellow;
                break;
            case 3:
                col = Qt::red;
                break;
            case 4:
                col = Qt::magenta;
                break;
            default:
                col = Qt::cyan;
                break;
            }
            col.setAlpha(80);
            painter->setBrush(col);
        }
        painter->drawPath(transformation.map(path));
    }
    painter->restore();
}

void KeyframeView::drawKeyFrameChannels(const QRectF &br, int in, int out, QPainter *painter, const QList<QPoint> &maximas, int limitKeyframes, const QColor &textColor)
{
    double frameFactor = (double)(out - in) / br.width();
    int offset = 1;
    if (limitKeyframes > 0) {
        offset = (out - in) / limitKeyframes / frameFactor;
    }
    double xDist = maximas.at(0).y() - maximas.at(0).x();
    double yDist = maximas.at(1).y() - maximas.at(1).x();
    double wDist = maximas.at(2).y() - maximas.at(2).x();
    double hDist = maximas.at(3).y() - maximas.at(3).x();
    double xOffset = maximas.at(0).x();
    double yOffset = maximas.at(1).x();
    double wOffset = maximas.at(2).x();
    double hOffset = maximas.at(3).x();
    QColor cX(255, 0, 0, 100);
    QColor cY(0, 255, 0, 100);
    QColor cW(0, 0, 255, 100);
    QColor cH(255, 255, 0, 100);
    // Draw curves labels
    QRectF txtRect = painter->boundingRect(br, QStringLiteral("t"));
    txtRect.setX(2);
    txtRect.setWidth(br.width() - 4);
    txtRect.moveTop(br.height() - txtRect.height());
    QRectF drawnText;
    int maxHeight = br.height() - txtRect.height() - 2;
    painter->setPen(textColor);
    int rectSize = txtRect.height() / 2;
    if (xDist > 0) {
        painter->fillRect(txtRect.x(), txtRect.top() + rectSize / 2, rectSize, rectSize, cX);
        txtRect.setX(txtRect.x() + rectSize * 2);
        painter->drawText(txtRect, 0, i18nc("X as in x coordinate", "X") + QStringLiteral(" (%1-%2)").arg(maximas.at(0).x()).arg(maximas.at(0).y()), &drawnText);
    }
    if (yDist > 0) {
        if (drawnText.isValid()) {
            txtRect.setX(drawnText.right() + rectSize);
        }
        painter->fillRect(txtRect.x(), txtRect.top() + rectSize / 2, rectSize, rectSize, cY);
        txtRect.setX(txtRect.x() + rectSize * 2);
        painter->drawText(txtRect, 0, i18nc("Y as in y coordinate", "Y") + QStringLiteral(" (%1-%2)").arg(maximas.at(1).x()).arg(maximas.at(1).y()), &drawnText);
    }
    if (wDist > 0) {
        if (drawnText.isValid()) {
            txtRect.setX(drawnText.right() + rectSize);
        }
        painter->fillRect(txtRect.x(), txtRect.top() + rectSize / 2, rectSize, rectSize, cW);
        txtRect.setX(txtRect.x() + rectSize * 2);
        painter->drawText(txtRect, 0, i18n("Width") + QStringLiteral(" (%1-%2)").arg(maximas.at(2).x()).arg(maximas.at(2).y()), &drawnText);
    }
    if (hDist > 0) {
        if (drawnText.isValid()) {
            txtRect.setX(drawnText.right() + rectSize);
        }
        painter->fillRect(txtRect.x(), txtRect.top() + rectSize / 2, rectSize, rectSize, cH);
        txtRect.setX(txtRect.x() + rectSize * 2);
        painter->drawText(txtRect, 0, i18n("Height") + QStringLiteral(" (%1-%2)").arg(maximas.at(3).x()).arg(maximas.at(3).y()), &drawnText);
    }

    // Draw curves
    for (int i = 0; i < br.width(); i++) {
        mlt_rect rect = m_keyProperties.anim_get_rect(m_inTimeline.toUtf8().constData(), (int)(i * frameFactor) + in);
        if (xDist > 0) {
            painter->setPen(cX);
            int val = (rect.x - xOffset) * maxHeight / xDist;
            painter->drawLine(i, maxHeight - val, i, maxHeight);
        }
        if (yDist > 0) {
            painter->setPen(cY);
            int val = (rect.y - yOffset) * maxHeight / yDist;
            painter->drawLine(i, maxHeight - val, i, maxHeight);
        }
        if (wDist > 0) {
            painter->setPen(cW);
            int val = (rect.w - wOffset) * maxHeight / wDist;
            painter->drawLine(i, maxHeight - val, i, maxHeight);
        }
        if (hDist > 0) {
            painter->setPen(cH);
            int val = (rect.h - hOffset) * maxHeight / hDist;
            painter->drawLine(i, maxHeight - val, i, maxHeight);
        }
    }
    if (offset > 1) {
        // Overlay limited keyframes curve
        cX.setAlpha(255);
        cY.setAlpha(255);
        cW.setAlpha(255);
        cH.setAlpha(255);
        mlt_rect rect1 = m_keyProperties.anim_get_rect(m_inTimeline.toUtf8().constData(), in);
        int prevPos = 0;
        for (int i = offset; i < br.width(); i += offset) {
            mlt_rect rect2 = m_keyProperties.anim_get_rect(m_inTimeline.toUtf8().constData(), (int)(i * frameFactor) + in);
            if (xDist > 0) {
                painter->setPen(cX);
                int val1 = (rect1.x - xOffset) * maxHeight / xDist;
                int val2 = (rect2.x - xOffset) * maxHeight / xDist;
                painter->drawLine(prevPos, maxHeight - val1, i, maxHeight - val2);
            }
            if (yDist > 0) {
                painter->setPen(cY);
                int val1 = (rect1.y - yOffset) * maxHeight / yDist;
                int val2 = (rect2.y - yOffset) * maxHeight / yDist;
                painter->drawLine(prevPos, maxHeight - val1, i, maxHeight - val2);
            }
            if (wDist > 0) {
                painter->setPen(cW);
                int val1 = (rect1.w - wOffset) * maxHeight / wDist;
                int val2 = (rect2.w - wOffset) * maxHeight / wDist;
                painter->drawLine(prevPos, maxHeight - val1, i, maxHeight - val2);
            }
            if (hDist > 0) {
                painter->setPen(cH);
                int val1 = (rect1.h - hOffset) * maxHeight / hDist;
                int val2 = (rect2.h - hOffset) * maxHeight / hDist;
                painter->drawLine(prevPos, maxHeight - val1, i, maxHeight - val2);
            }
            rect1 = rect2;
            prevPos = i;
        }
    }
}

QString KeyframeView::getSingleAnimation(int ix, int in, int out, int offset, int limitKeyframes, QPoint maximas, double min, double max)
{
    m_keyProperties.set("kdenlive_import", "");
    int newduration = out - in + offset;
    m_keyProperties.anim_get_double("kdenlive_import", 0, newduration);
    Mlt::Animation anim = m_keyProperties.get_animation("kdenlive_import");
    double factor = (max - min) / (maximas.y() - maximas.x());
    mlt_rect rect = m_keyProperties.anim_get_rect(m_inTimeline.toUtf8().constData(), in, duration);
    double value;
    switch (ix) {
    case 1:
        value = rect.y;
        break;
    case 2:
        value = rect.w;
        break;
    case 3:
        value = rect.h;
        break;
    default:
        value = rect.x;
        break;
    }
    if (maximas.x() > 0) {
        value -= maximas.x();
    }
    value = value * factor + min;
    m_keyProperties.anim_set("kdenlive_import", value, offset, newduration, limitKeyframes > 0 ? mlt_keyframe_smooth : mlt_keyframe_linear);
    if (limitKeyframes > 0) {
        int step = (out - in) / limitKeyframes;
        for (int i = step; i < out; i += step) {
            rect = m_keyProperties.anim_get_rect(m_inTimeline.toUtf8().constData(), in + i, duration);
            switch (ix) {
            case 1:
                value = rect.y;
                break;
            case 2:
                value = rect.w;
                break;
            case 3:
                value = rect.h;
                break;
            default:
                value = rect.x;
                break;
            }
            if (maximas.x() > 0) {
                value -= maximas.x();
            }
            value = value * factor + min;
            m_keyProperties.anim_set("kdenlive_import", value, offset + i, newduration, mlt_keyframe_smooth);
        }
    } else {
        int next = m_keyAnim.next_key(in + 1);
        while (next < out && next > 0) {
            rect = m_keyProperties.anim_get_rect(m_inTimeline.toUtf8().constData(), next, duration);
            switch (ix) {
            case 1:
                value = rect.y;
                break;
            case 2:
                value = rect.w;
                break;
            case 3:
                value = rect.h;
                break;
            default:
                value = rect.x;
                break;
            }
            if (maximas.x() > 0) {
                value -= maximas.x();
            }
            value = value * factor + min;
            m_keyProperties.anim_set("kdenlive_import", value, offset + next - in, newduration, mlt_keyframe_linear);
            next = m_keyAnim.next_key(next + 1);
        }
    }
    QString result = anim.serialize_cut();
    m_keyProperties.set("kdenlive_import", (char *) nullptr);
    return result;
}

QString KeyframeView::getOffsetAnimation(int in, int out, int offset, int limitKeyframes, ProfileInfo profile, bool allowAnimation, bool positionOnly, QPoint rectOffset)
{
    m_keyProperties.set("kdenlive_import", "");
    int newduration = out - in + offset;
    int pWidth = profile.profileSize.width();
    int pHeight = profile.profileSize.height();
    m_keyProperties.anim_get_double("kdenlive_import", 0, newduration);
    Mlt::Animation anim = m_keyProperties.get_animation("kdenlive_import");
    mlt_keyframe_type kftype = (limitKeyframes > 0 && allowAnimation) ? mlt_keyframe_smooth : mlt_keyframe_linear;
    mlt_rect rect = m_keyProperties.anim_get_rect(m_inTimeline.toUtf8().constData(), in, duration);
    rect.x = (int) rect.x;
    rect.y = (int) rect.y;
    if (positionOnly) {
        rect.x -= rectOffset.x();
        rect.y -= rectOffset.y();
        rect.w = pWidth;
        rect.h = pHeight;
        rect.o = 100;
    }
    m_keyProperties.anim_set("kdenlive_import", rect, offset, newduration, kftype);
    if (limitKeyframes > 0 && m_keyAnim.key_count() > limitKeyframes) {
        int step = (out - in) / limitKeyframes;
        for (int i = step; i < out; i += step) {
            rect = m_keyProperties.anim_get_rect(m_inTimeline.toUtf8().constData(), in + i, duration);
            rect.x = (int) rect.x;
            rect.y = (int) rect.y;
            if (positionOnly) {
                rect.x -= rectOffset.x();
                rect.y -= rectOffset.y();
                rect.w = pWidth;
                rect.h = pHeight;
                rect.o = 100;
            }
            m_keyProperties.anim_set("kdenlive_import", rect, offset + i, newduration, kftype);
        }
    } else {
        int pos;
        for (int i = 0; i < m_keyAnim.key_count(); ++i) {
            m_keyAnim.key_get(i, pos, kftype);
            rect = m_keyProperties.anim_get_rect(m_inTimeline.toUtf8().constData(), pos, duration);
            rect.x = (int) rect.x;
            rect.y = (int) rect.y;
            if (positionOnly) {
                rect.x -= rectOffset.x();
                rect.y -= rectOffset.y();
                rect.w = pWidth;
                rect.h = pHeight;
                rect.o = 100;
            }
            m_keyProperties.anim_set("kdenlive_import", rect, offset + pos - in, newduration, kftype);
        }
    }
    QString result = anim.serialize_cut();
    m_keyProperties.set("kdenlive_import", (char *) nullptr);
    return result;
}

int KeyframeView::mouseOverKeyFrames(const QRectF &br, QPointF pos, double scale)
{
    if (m_keyframeType == NoKeyframe) {
        return -1;
    }
    pos.setX((pos.x() - m_offset) * scale);
    int previousEdit = activeKeyframe;
    for (int i = 0; i < m_keyAnim.key_count(); ++i) {
        int key = m_keyAnim.key_get_frame(i);
        if (key < 0) {
            key += duration;
        }
        double value = m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), key, duration - m_offset);
        QPointF p = keyframeMap(br, key, value);
        p.setX(p.x() * scale);
        if (m_keyframeType == GeometryKeyframe) {
            p.setY(br.bottom() - br.height() / 2);
        }
        if ((pos - p).manhattanLength() <= m_handleSize / 2) {
            //TODO
            /*setToolTip(QLatin1Char('[') + QString::number((GenTime(key, m_fps) - cropStart()).seconds(), 'f', 2)
                       + i18n("seconds") + QStringLiteral(", ") + QString::number(value, 'f', 1) + ']');*/
            activeKeyframe = key;
            if (previousEdit != activeKeyframe) {
                updateKeyframes();
            }
            return key;
        }
        if (p.x() > pos.x()) {
            break;
        }
    }
    //setToolTip(QString());
    activeKeyframe = -1;
    if (previousEdit != activeKeyframe) {
        updateKeyframes();
    }
    return -1;
}

double KeyframeView::editedKeyFrameValue()
{
    return m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), activeKeyframe, duration - m_offset);
}

void KeyframeView::updateKeyFramePos(const QRectF &br, int frame, const double y)
{
    if (!m_keyAnim.is_key(activeKeyframe)) {
        return;
    }
    int prev = m_keyAnim.key_count() <= 1 || m_keyAnim.key_get_frame(0) == activeKeyframe ? 0 : m_keyAnim.previous_key(activeKeyframe - 1) + 1;
    prev = qMax(prev, -m_offset);
    int next = m_keyAnim.key_count() <= 1 || m_keyAnim.key_get_frame(m_keyAnim.key_count() - 1) == activeKeyframe ? duration - m_offset - 1 :  m_keyAnim.next_key(activeKeyframe + 1) - 1;
    if (next < 0) {
        next += duration;
    }
    int newpos = qBound(prev, frame - m_offset, next);
    double newval = keyframeUnmap(br, y);
    mlt_keyframe_type type = m_keyAnim.keyframe_type(activeKeyframe);
    if (m_keyframeType == GeometryKeyframe) {
        // Animated rect
        mlt_rect rect = m_keyProperties.anim_get_rect(m_inTimeline.toUtf8().constData(), activeKeyframe - m_offset, duration - m_offset);
        m_keyProperties.anim_set(m_inTimeline.toUtf8().constData(), rect, newpos, duration - m_offset, type);
    } else {
        m_keyProperties.anim_set(m_inTimeline.toUtf8().constData(), newval, newpos, duration - m_offset, type);
    }
    if (activeKeyframe != newpos) {
        m_keyAnim.remove(activeKeyframe);
        // Move keyframe in other geometries
        int cnt = m_keyProperties.count();
        QStringList paramNames;
        for (int i = 0; i < cnt; i++) {
            paramNames << m_keyProperties.get_name(i);
        }
        paramNames.removeAll(m_inTimeline);
        foreach (const QString &paramName, paramNames) {
            ParameterInfo info = m_paramInfos.value(paramName);
            if (info.max == info.min) {
                // this is probably an animated rect
                mlt_rect rect = m_keyProperties.anim_get_rect(paramName.toUtf8().constData(), activeKeyframe - m_offset, duration - m_offset);
                m_keyProperties.anim_set(paramName.toUtf8().constData(), rect, newpos, duration - m_offset, type);

            } else {
                double val = m_keyProperties.anim_get_double(paramName.toUtf8().constData(), activeKeyframe - m_offset, duration - m_offset);
                m_keyProperties.anim_set(paramName.toUtf8().constData(), val, newpos, duration - m_offset, type);
            }
            // Remove kfr at previous position
            m_keyAnim = m_keyProperties.get_animation(paramName.toUtf8().constData());
            m_keyAnim.remove(activeKeyframe);
        }
        m_keyAnim = m_keyProperties.get_animation(m_inTimeline.toUtf8().constData());
    }
    if (attachToEnd == activeKeyframe) {
        attachToEnd = newpos;
    }
    activeKeyframe = newpos;
    emit updateKeyframes();
}

double KeyframeView::getKeyFrameClipHeight(const QRectF &br, const double y)
{
    return keyframeUnmap(br, y);
}

int KeyframeView::keyframesCount()
{
    if (duration == 0) {
        return -1;
    }
    return m_keyAnim.key_count();
}

mlt_keyframe_type KeyframeView::type(int frame)
{
    if (m_keyAnim.is_key(frame)) {
        return m_keyAnim.keyframe_type(frame);
    }
    // no keyframe at position frame, try to get previous key's type
    int previous = m_keyAnim.previous_key(frame);
    return m_keyAnim.keyframe_type(previous);
}

void KeyframeView::addKeyframe(int frame, double value, mlt_keyframe_type type)
{
    m_keyProperties.anim_set(m_inTimeline.toUtf8().constData(), value, frame - m_offset, duration - m_offset, type);
    // Last keyframe should stick to end
    if (frame == duration - 1) {
        attachToEnd = frame - m_offset;
    }
    // Add keyframe in other animations if any
    int cnt = m_keyProperties.count();
    QStringList paramNames;
    for (int i = 0; i < cnt; i++) {
        paramNames << m_keyProperties.get_name(i);
    }
    paramNames.removeAll(m_inTimeline);
    foreach (const QString &paramName, paramNames) {
        ParameterInfo info = m_paramInfos.value(paramName);
        if (info.max == info.min) {
            // this is probably an animated rect
            mlt_rect rect = m_keyProperties.anim_get_rect(paramName.toUtf8().constData(), frame - m_offset, duration - m_offset);
            m_keyProperties.anim_set(paramName.toUtf8().constData(), rect, frame - m_offset, duration - m_offset, type);

        } else {
            double val = m_keyProperties.anim_get_double(paramName.toUtf8().constData(), frame - m_offset, duration - m_offset);
            m_keyProperties.anim_set(paramName.toUtf8().constData(), val, frame - m_offset, duration - m_offset, type);
        }
    }
}

void KeyframeView::addDefaultKeyframe(ProfileInfo profile, int frame, mlt_keyframe_type type)
{
    double value = m_keyframeDefault;
    if (m_keyAnim.key_count() == 1 && frame != m_keyAnim.key_get_frame(0)) {
        value = m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), m_keyAnim.key_get_frame(0), duration - m_offset);
    }
    m_keyProperties.anim_set(m_inTimeline.toUtf8().constData(), value, frame, duration - m_offset, type);
    // Last keyframe should stick to end
    if (frame >= duration - 1) {
        attachToEnd = frame - m_offset;
    }
    // Add keyframe in other animations if any
    int cnt = m_keyProperties.count();
    QStringList paramNames;
    QLocale locale;
    for (int i = 0; i < cnt; i++) {
        paramNames << m_keyProperties.get_name(i);
    }
    paramNames.removeAll(m_inTimeline);
    foreach (const QString &paramName, paramNames) {
        ParameterInfo info = m_paramInfos.value(paramName);
        if (info.max == info.min) {
            // this is probably an animated rect
            QString defaultVal = info.defaultValue;
            if (defaultVal.contains(QLatin1Char('%'))) {
                defaultVal = EffectsController::getStringRectEval(profile, defaultVal).simplified();
            }
            mlt_rect rect;
            rect.x = locale.toDouble(defaultVal.section(QLatin1Char(' '), 0, 0));
            rect.y = locale.toDouble(defaultVal.section(QLatin1Char(' '), 1, 1));
            rect.w = locale.toDouble(defaultVal.section(QLatin1Char(' '), 2, 2));
            rect.h = locale.toDouble(defaultVal.section(QLatin1Char(' '), 3, 3));
            rect.o = defaultVal.count(QLatin1Char(' ')) > 3 ? locale.toDouble(defaultVal.section(QLatin1Char(' '), 4, 4)) : 1.0;
            m_keyProperties.anim_set(paramName.toUtf8().constData(), rect, frame - m_offset, duration - m_offset, type);

        } else {
            double val = locale.toDouble(info.defaultValue);
            m_keyProperties.anim_set(paramName.toUtf8().constData(), val, frame - m_offset, duration - m_offset, type);
        }
    }
}

void KeyframeView::removeKeyframe(int frame)
{
    m_keyAnim.remove(frame);
    if (frame == duration - 1 && frame == attachToEnd) {
        attachToEnd = -2;
    }
    // Remove keyframe in other animations if any
    int cnt = m_keyProperties.count();
    QStringList paramNames;
    for (int i = 0; i < cnt; i++) {
        paramNames << m_keyProperties.get_name(i);
    }
    paramNames.removeAll(m_inTimeline);
    foreach (const QString &paramName, paramNames) {
        m_keyAnim = m_keyProperties.get_animation(paramName.toUtf8().constData());
        m_keyAnim.remove(frame);
    }
    m_keyAnim = m_keyProperties.get_animation(m_inTimeline.toUtf8().constData());
}

QAction *KeyframeView::parseKeyframeActions(const QList<QAction *> &actions)
{

    mlt_keyframe_type type = m_keyAnim.keyframe_type(activeKeyframe);
    for (int i = 0; i < actions.count(); i++) {
        if (actions.at(i)->data().toInt() == type) {
            return actions.at(i);
        }
    }
    return nullptr;
}

void KeyframeView::attachKeyframeToEnd(bool attach)
{
    if (attach) {
        attachToEnd = activeKeyframe;
    } else {
        if (attachToEnd != duration - 1) {
            // Check if there is a keyframe at end pos, and attach it to end if it is the case
            if (m_keyAnim.is_key(duration - 1)) {
                attachToEnd = duration - 1;
            }
        } else {
            // We want to detach last keyframe from end
            attachToEnd = -2;
        }
    }
    emit updateKeyframes();
}

void KeyframeView::editKeyframeType(int type)
{
    if (m_keyAnim.is_key(activeKeyframe)) {
        // This is a keyframe
        double val = m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), activeKeyframe, duration - m_offset);
        m_keyProperties.anim_set(m_inTimeline.toUtf8().constData(), val, activeKeyframe, duration - m_offset, (mlt_keyframe_type) type);
    }
}

bool KeyframeView::activeParam(const QString &name) const
{
    return name == m_inTimeline;
}

const QString KeyframeView::serialize(const QString &name, bool rectAnimation)
{
    if (!name.isEmpty()) {
        m_keyAnim = m_keyProperties.get_animation(name.toUtf8().constData());
    }
    if (attachToEnd == -2 || rectAnimation) {
        return m_keyAnim.serialize_cut();
    }
    mlt_keyframe_type type;
    QString key;
    QLocale locale;
    QStringList result;
    int pos;
    for (int i = 0; i < m_keyAnim.key_count(); ++i) {
        m_keyAnim.key_get(i, pos, type);
        double val = m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), pos, duration - m_offset);
        if (pos >= attachToEnd) {
            pos = qMin(pos - (duration - m_offset), -1);
        }
        key = QString::number(pos);
        switch (type) {
        case mlt_keyframe_discrete:
            key.append(QStringLiteral("|="));
            break;
        case mlt_keyframe_smooth:
            key.append(QStringLiteral("~="));
            break;
        default:
            key.append(QStringLiteral("="));
            break;
        }
        key.append(locale.toString(val));
        result << key;
    }
    if (!name.isEmpty()) {
        m_keyAnim = m_keyProperties.get_animation(m_inTimeline.toUtf8().constData());
    }
    return result.join(QLatin1Char(';'));
}

QList<QPoint> KeyframeView::loadKeyframes(const QString &data)
{
    QList<QPoint> result;
    m_keyframeType = NoKeyframe;
    m_inTimeline = QStringLiteral("imported");
    m_keyProperties.set(m_inTimeline.toUtf8().constData(), data.toUtf8().constData());
    // We need to initialize with length so that negative keyframes are correctly interpreted
    m_keyProperties.anim_get_rect(m_inTimeline.toUtf8().constData(), 0, duration);
    m_keyAnim = m_keyProperties.get_animation(m_inTimeline.toUtf8().constData());
    duration = m_keyAnim.length();
    // calculate minimas / maximas
    int max = m_keyAnim.key_count();
    if (max == 0) {
        // invalid geometry
        result << QPoint() << QPoint() << QPoint() << QPoint();
        return result;
    }
    int frame = m_keyAnim.key_get_frame(0);
    mlt_rect rect = m_keyProperties.anim_get_rect(m_inTimeline.toUtf8().constData(), frame, duration);
    QPoint pX(rect.x, rect.x);
    QPoint pY(rect.y, rect.y);
    QPoint pW(rect.w, rect.w);
    QPoint pH(rect.h, rect.h);
    for (int i = 1; i < max; i++) {
        frame = m_keyAnim.key_get_frame(i);
        rect = m_keyProperties.anim_get_rect(m_inTimeline.toUtf8().constData(), frame, duration);
        // Check x bounds
        if (rect.x < pX.x()) {
            pX.setX(rect.x);
        }
        if (rect.x > pX.y()) {
            pX.setY(rect.x);
        }
        // Check y bounds
        if (rect.y < pY.x()) {
            pY.setX(rect.y);
        }
        if (rect.y > pY.y()) {
            pY.setY(rect.y);
        }
        // Check w bounds
        if (rect.w < pW.x()) {
            pW.setX(rect.w);
        }
        if (rect.w > pW.y()) {
            pW.setY(rect.w);
        }
        // Check h bounds
        if (rect.h < pH.x()) {
            pH.setX(rect.h);
        }
        if (rect.h > pH.y()) {
            pH.setY(rect.h);
        }
    }
    result << pX << pY << pW << pH;
    return result;
}

bool KeyframeView::loadKeyframes(const QLocale &locale, const QDomElement &effect, int cropStart, int length)
{
    m_keyframeType = NoKeyframe;
    duration = length;
    m_inTimeline.clear();
    m_notInTimeline.clear();
    // reset existing properties
    int max = m_keyProperties.count();
    for (int i = max - 1; i >= 0; i--) {
        m_keyProperties.set(m_keyProperties.get_name(i), (char *) nullptr);
    }
    attachToEnd = -2;
    m_useOffset = effect.attribute(QStringLiteral("kdenlive:sync_in_out")) != QLatin1String("1");
    m_offset = effect.attribute(QStringLiteral("in")).toInt() - cropStart;
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    QStringList keyframeTypes;
    keyframeTypes << QStringLiteral("keyframe") << QStringLiteral("simplekeyframe") << QStringLiteral("geometry") << QStringLiteral("animatedrect") << QStringLiteral("animated");
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.isNull()) {
            continue;
        }
        QString type = e.attribute(QStringLiteral("type"));
        if (!keyframeTypes.contains(type)) {
            continue;
        }
        QString paramName = e.attribute(QStringLiteral("name"));
        ParameterInfo info;
        info.factor = locale.toDouble(e.attribute(QStringLiteral("factor")));
        info.max = locale.toDouble(e.attribute(QStringLiteral("max")));
        info.min = locale.toDouble(e.attribute(QStringLiteral("min")));
        info.defaultValue = e.attribute(QStringLiteral("default"));
        if (info.factor == 0) {
            info.factor = 1;
        }
        m_paramInfos.insert(paramName, info);
        if (e.hasAttribute(QStringLiteral("notintimeline"))) {
            // This param should not be drawn in timeline
            m_notInTimeline << paramName;
        } else {
            if (type == QLatin1String("keyframe")) {
                m_keyframeType = NormalKeyframe;
            } else if (type == QLatin1String("simplekeyframe")) {
                m_keyframeType = SimpleKeyframe;
            } else if (type == QLatin1String("geometry") || type == QLatin1String("animatedrect")) {
                m_keyframeType = GeometryKeyframe;
            } else if (type == QLatin1String("animated")) {
                m_keyframeType = AnimatedKeyframe;
            }
            if (!e.hasAttribute(QStringLiteral("intimeline")) || e.attribute(QStringLiteral("intimeline")) == QLatin1String("1")) {
                // Active parameter
                m_keyframeMin = info.min;
                m_keyframeMax = info.max;
                m_keyframeDefault = locale.toDouble(info.defaultValue);
                m_keyframeFactor = info.factor;
                attachToEnd = checkNegatives(e.attribute(QStringLiteral("value")).toUtf8().constData(), duration - m_offset);
                m_inTimeline = paramName;
            }
        }
        // parse keyframes
        QString value = e.attribute(QStringLiteral("value"));
        if (value.isEmpty()) {
            value = e.attribute(QStringLiteral("default"));
        }
        switch (m_keyframeType) {
        case GeometryKeyframe:
            m_keyProperties.set(paramName.toUtf8().constData(), value.toUtf8().constData());
            m_keyProperties.anim_get_rect(paramName.toUtf8().constData(), 0, length);
            break;
        case AnimatedKeyframe:
            m_keyProperties.set(paramName.toUtf8().constData(), value.toUtf8().constData());
            // We need to initialize with length so that negative keyframes are correctly interpreted
            m_keyProperties.anim_get_double(paramName.toUtf8().constData(), 0, length);
            break;
        default:
            m_keyProperties.set(m_inTimeline.toUtf8().constData(), e.attribute(m_inTimeline.toUtf8().constData()).toUtf8().constData());
            m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), 0, length);
        }
        if (paramName == m_inTimeline) {
            m_keyAnim = m_keyProperties.get_animation(m_inTimeline.toUtf8().constData());
            if (m_keyAnim.next_key(activeKeyframe) <= activeKeyframe) {
                activeKeyframe = -1;
            }
        }
    }
    return (!m_inTimeline.isEmpty());
}

void KeyframeView::setOffset(int frames)
{
    if (duration == 0 || !m_keyAnim.is_valid()) {
        return;
    }
    if (m_keyAnim.is_key(-m_offset)) {
        mlt_keyframe_type type = m_keyAnim.keyframe_type(-m_offset);
        double value = m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), -m_offset, duration - m_offset);
        m_keyAnim.remove(-m_offset);
        if (activeKeyframe == -m_offset) {
            activeKeyframe -= frames;
        }
        m_offset -= frames;
        m_keyProperties.anim_set(m_inTimeline.toUtf8().constData(), value, - m_offset, duration - m_offset, type);
        //addKeyframe(-m_offset, value, type);
    } else {
        m_offset -= frames;
    }
    //double value = m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), 0, duration - m_offset);
    //}
}

// static
int KeyframeView::checkNegatives(const QString &data, int maxDuration)
{
    int result = -2;
    QStringList frames = data.split(QLatin1Char(';'));
    for (int i = 0; i < frames.count(); i++) {
        if (frames.at(i).startsWith(QLatin1String("-"))) {
            // We found a negative kfr
            QString sub = frames.at(i).section(QLatin1Char('='), 0, 0);
            if (!sub.at(sub.length() - 1).isDigit()) {
                // discrete or smooth keyframe, we need to remove the tag (| or ~)
                sub.chop(1);
            }
            int negPos = sub.toInt() + maxDuration;
            if (result == -2 || result > negPos) {
                result = negPos;
            }
        }
    }
    return result;
}

void KeyframeView::reset()
{
    if (m_keyframeType == NoKeyframe) {
        // nothing to do
        return;
    }
    m_keyframeType = NoKeyframe;
    duration = 0;
    attachToEnd = -2;
    activeKeyframe = -1;
    m_inTimeline.clear();
    m_notInTimeline.clear();
    int max = m_keyProperties.count();
    for (int i = max - 1; i >= 0; i--) {
        m_keyProperties.set(m_keyProperties.get_name(i), (char *) nullptr);
    }
    emit updateKeyframes();
}

//static
QString KeyframeView::cutAnimation(const QString &animation, int start, int duration, int fullduration, bool doCut)
{
    Mlt::Properties props;
    props.set("keyframes", animation.toUtf8().constData());
    props.anim_get_double("keyframes", 0, fullduration);
    Mlt::Animation anim = props.get_animation("keyframes");
    if (start > 0 && !anim.is_key(start)) {
        // insert new keyframe at start
        double value = props.anim_get_double("keyframes", start, fullduration);
        int previous = anim.previous_key(start);
        mlt_keyframe_type type = anim.keyframe_type(previous);
        props.anim_set("keyframes", value, start, fullduration, type);
    }
    if (!anim.is_key(start + duration)) {
        double value = props.anim_get_double("keyframes", start + duration, fullduration);
        int previous = anim.previous_key(start + duration);
        mlt_keyframe_type type = anim.keyframe_type(previous);
        props.anim_set("keyframes", value, start + duration, fullduration, type);
    }
    if (!doCut) {
        return anim.serialize_cut();
    }
    return anim.serialize_cut(start, start + duration);
}

//static
const QString KeyframeView::addBorderKeyframes(const QString &animation, int start, int duration)
{
    bool modified = false;
    Mlt::Properties props;
    props.set("keyframes", animation.toUtf8().constData());
    props.anim_get_double("keyframes", 0, start + duration);
    Mlt::Animation anim = props.get_animation("keyframes");
    if (!anim.is_key(start)) {
        double value = props.anim_get_double("keyframes", start, start + duration);
        int previous = anim.previous_key(start);
        mlt_keyframe_type type = anim.keyframe_type(previous);
        props.anim_set("keyframes", value, start, start + duration, type);
        modified = true;
    }
    if (!anim.is_key(start + duration)) {
        double value = props.anim_get_double("keyframes", start + duration, start + duration);
        int previous = anim.previous_key(start + duration);
        mlt_keyframe_type type = anim.keyframe_type(previous);
        props.anim_set("keyframes", value, start + duration, start + duration, type);
        modified = true;
    }
    if (modified) {
        return anim.serialize_cut();
    }
    return animation;
}

//static
QString KeyframeView::switchAnimation(const QString &animation, int newPos, int oldPos, int newDuration, int oldDuration, bool isRect)
{
    Mlt::Properties props;
    props.set("keyframes", animation.toUtf8().constData());
    props.anim_get_double("keyframes", 0, oldDuration);
    Mlt::Animation anim = props.get_animation("keyframes");
    if (anim.is_key(oldPos)) {
        // insert new keyframe at start
        if (isRect) {
            mlt_rect rect = props.anim_get_rect("keyframes", oldPos, oldDuration);
            props.anim_set("keyframes", rect, newPos, newDuration, anim.keyframe_type(oldPos));
            anim.remove(oldPos);
        } else {
            double value = props.anim_get_double("keyframes", oldPos, oldDuration);
            props.anim_set("keyframes", value, newPos, newDuration, anim.keyframe_type(oldPos));
            anim.remove(oldPos);
        }
    }
    return anim.serialize_cut();
    //return anim.serialize_cut(start, start + duration);
}

/*
void KeyframeView::updateAnimatedKeyframes(QDomElement effect, int paramIndex, ItemInfo oldInfo)
{
    QDomElement param = effect.elementsByTagName("parameter").item(paramIndex).toElement();
    int offset = oldInfo.cropStart.frames(m_fps);
    if (offset > 0) {
        for(int i = 0; i < m_keyAnim.key_count(); ++i){
            int oldPos = m_keyAnim.key_get_frame(i);
            int newPos = oldPos + offset;
            m_keyAnim.remove(oldPos);
            m_keyProperties.anim_set(m_inTimeline.toUtf8().constData(), m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), oldPos), newPos, 0, m_keyAnim.keyframe_type(i));
        }
    }
    QString result = m_keyAnim.serialize_cut();
    param.setAttribute("value", result);
}*/

