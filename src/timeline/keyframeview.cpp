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

#include "keyframeview.h"

KeyframeView::KeyframeView(int handleSize, QObject *parent) : QObject(parent)
    , activeKeyframe(-1)
    , attachToEnd(-2)
    , duration(0)
    , m_keyframeType(KEYFRAMETYPE::NoKeyframe)
    , m_keyframeDefault(0)
    , m_keyframeMin(0)
    , m_keyframeMax(1)
    , m_keyframeFactor(1)
    , m_handleSize(handleSize)
{
}


KeyframeView::~KeyframeView()
{
}

double KeyframeView::keyframeUnmap(QRectF br, double y) {
    return ((br.bottom() - y) / br.height() * (m_keyframeMax - m_keyframeMin) + m_keyframeMin) / m_keyframeFactor;
}

double KeyframeView::keyframeMap(QRectF br, double value) {
    return br.bottom() - br.height() * (value * m_keyframeFactor - m_keyframeMin) / (m_keyframeMax - m_keyframeMin);
}

QPointF KeyframeView::keyframeMap(QRectF br, int frame, double value) {
    return QPointF(br.x() + br.width() * frame / duration,
                   br.bottom() - br.height() * (value * m_keyframeFactor - m_keyframeMin) / (m_keyframeMax - m_keyframeMin));
}

QPointF KeyframeView::keyframePoint(QRectF br, int index) {
    int frame = m_keyAnim.key_get_frame(index);
    return keyframeMap(br, frame < 0 ? frame + duration : frame, m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), frame, duration));
}

QPointF KeyframeView::keyframePoint(QRectF br, int frame, double value, double factor, double min, double max) {
    return QPointF(br.x() + br.width() * frame / duration,
                   br.bottom() - br.height() * (value * factor - min) / (max - min));
}

void KeyframeView::drawKeyFrames(QRectF br, int length, bool active, QPainter *painter, const QTransform &transformation)
{
    if (duration == 0 || m_keyframeType == NoKeyframe || m_keyAnim.key_count() < 1)
        return;
    duration = length;
    //m_keyAnim.set_length(length);
    painter->save();
    QPointF h(m_handleSize, m_handleSize);

    // draw keyframes
    // Special case: Geometry keyframes are just vertical lines
    if (m_keyframeType == GeometryKeyframe) {
        for(int i = 0; i < m_keyAnim.key_count(); ++i) {
            int frame = m_keyAnim.key_get_frame(i);
            QColor color = (frame == activeKeyframe) ? QColor(Qt::red) : QColor(Qt::blue);
            if (active)
                painter->setPen(color);
            QPointF k = keyframePoint(br, i);
            painter->drawLine(transformation.map(QLineF(k.x(), br.top(), k.x(), br.height())));
            if (active) {
                k.setY(br.top() + br.height()/2);
                painter->setBrush((m_keyAnim.key_get_frame(i) == activeKeyframe) ? QColor(Qt::red) : QColor(Qt::blue));
                painter->drawEllipse(QRectF(transformation.map(k) - h/2, transformation.map(k) + h/2));
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
    double factor = 1;
    double min;
    double max;
    QStringList paramNames;
    for (int i = 0; i < cnt; i++) {
        paramNames << m_keyProperties.get_name(i);
    }
    paramNames.removeAll(m_inTimeline);
    // Make sure edited param is painted last
    paramNames.append(m_inTimeline);
    foreach (const QString &paramName, paramNames) {
        Mlt::Animation drawAnim = m_keyProperties.get_animation(paramName.toUtf8().constData());
        ParameterInfo info = m_paramInfos.value(paramName);
        QPainterPath path;
        int frame = drawAnim.key_get_frame(0);
        double value = m_keyProperties.anim_get_double(paramName.toUtf8().constData(), frame, duration);
        QPointF start = keyframePoint(br, frame, value, info.factor, info.min, info.max);
        path.moveTo(br.x(), br.bottom());
        path.lineTo(br.x(), start.y());
        path.lineTo(start);
        painter->setPen(paramName == m_inTimeline ? QColor(Qt::white) : Qt::NoPen);
        for(int i = 0; i < drawAnim.key_count(); ++i) {
            if (active && paramName == m_inTimeline) {
                painter->setBrush((drawAnim.key_get_frame(i) == activeKeyframe) ? QColor(Qt::red) : QColor(Qt::blue));
                painter->drawEllipse(QRectF(transformation.map(start) - h/2, transformation.map(start) + h / 2));
            }
            if (i + 1 < drawAnim.key_count()) {
                frame = drawAnim.key_get_frame(i + 1);
                value = m_keyProperties.anim_get_double(paramName.toUtf8().constData(), frame, duration);
                QPointF end = keyframePoint(br, frame, value, info.factor, info.min, info.max);
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
                        value = m_keyProperties.anim_get_double(paramName.toUtf8().constData(), frame, duration);
                        QPointF pre = keyframePoint(br, frame, value, info.factor, info.min, info.max);
                        frame = drawAnim.key_get_frame(qMin(i + 2, drawAnim.key_count() - 1));
                        value = m_keyProperties.anim_get_double(paramName.toUtf8().constData(), frame, duration);
                        QPointF post = keyframePoint(br, frame, value, info.factor, info.min, info.max);
                        QPointF c1 = (end - pre) / 6.0; // + start
                        QPointF c2 = (start - post) / 6.0; // + end
                        double mid = (end.x() - start.x()) / 2;
                        if (c1.x() >  mid) c1 = c1 * mid / c1.x(); // scale down tangent vector to not go beyond middle
                        if (c2.x() < -mid) c2 = c2 * -mid / c2.x();
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

int KeyframeView::mouseOverKeyFrames(QRectF br, QPointF pos, double maxOffset, double scale)
{
    pos.setX(pos.x()*scale);
    int previousEdit = activeKeyframe;
    for(int i = 0; i < m_keyAnim.key_count(); ++i) {
        int key = m_keyAnim.key_get_frame(i);
        if (key < 0) {
            key += duration;
        }
        double value = m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), key, duration);
        QPointF p = keyframeMap(br, key, value);
        p.setX(p.x()*scale);
        if (m_keyframeType == GeometryKeyframe)
            p.setY(br.bottom() - br.height() / 2);
        if ((pos - p).manhattanLength() < maxOffset) {
	    //TODO
            /*setToolTip('[' + QString::number((GenTime(key, m_fps) - cropStart()).seconds(), 'f', 2)
                       + i18n("seconds") + ", " + QString::number(value, 'f', 1) + ']');*/
	    activeKeyframe = key;
            if (previousEdit != activeKeyframe) {
                updateKeyframes();
            }
            return key;
        } /*else if (p.x() > pos.x() + maxOffset) {
            break;
        }*/
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
    return m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), activeKeyframe, duration);
}

void KeyframeView::updateKeyFramePos(QRectF br, int frame, const double y)
{
    if (!m_keyAnim.is_key(activeKeyframe)) {
        return;
    }
    int prev = m_keyAnim.key_count() <= 1 || m_keyAnim.key_get_frame(0) == activeKeyframe ? 0 : m_keyAnim.previous_key(activeKeyframe - 1) + 1;
    int next = m_keyAnim.key_count() <= 1 || m_keyAnim.key_get_frame(m_keyAnim.key_count() - 1) == activeKeyframe ? duration :  m_keyAnim.next_key(activeKeyframe + 1) - 1;
    if (next < 0) next += duration;
    int newpos = qBound(prev, frame, next);
    double newval = keyframeUnmap(br, y);
    m_keyProperties.anim_set(m_inTimeline.toUtf8().constData(), newval, newpos, duration, m_keyAnim.keyframe_type(activeKeyframe));
    if (activeKeyframe != newpos)
        m_keyAnim.remove(activeKeyframe);
    if (attachToEnd == activeKeyframe) {
        attachToEnd = newpos;
    }
    activeKeyframe = newpos;
    emit updateKeyframes();
}

int KeyframeView::checkForSingleKeyframe()
{
    // Check if we have only one keyframe
    int start = 0;
    if (m_keyAnim.key_count() == 1 && m_keyAnim.is_key(start)) {
        double value = m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), start, duration);
        // Add keyframe at end of clip to allow inserting a new keframe in between
        int prevPos = m_keyAnim.previous_key(duration);
        m_keyProperties.anim_set(m_inTimeline.toUtf8().constData(), value, duration, duration, m_keyAnim.keyframe_type(prevPos));
        return value;
    }
    return -1;
}

double KeyframeView::getKeyFrameClipHeight(QRectF br, const double y)
{
    return keyframeUnmap(br, y);
}

int KeyframeView::keyframesCount()
{
    if (duration == 0) return -1;
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
    m_keyProperties.anim_set(m_inTimeline.toUtf8().constData(), value, frame, duration, type);
    // Last keyframe should stick to end
    if (frame == duration - 1) {
        attachToEnd = frame;
    }
}

void KeyframeView::addDefaultKeyframe(int frame, mlt_keyframe_type type)
{
    double value = m_keyframeDefault;
    if (m_keyAnim.key_count() == 1 && frame != m_keyAnim.key_get_frame(0)) {
	value = m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), m_keyAnim.key_get_frame(0), duration);
    }
    m_keyProperties.anim_set(m_inTimeline.toUtf8().constData(), value, frame, duration, type);
    // Last keyframe should stick to end
    if (frame == duration - 1) {
        attachToEnd = frame;
    }
}


void KeyframeView::removeKeyframe(int frame)
{
    m_keyAnim.remove(frame);
    if (frame == duration - 1 && frame == attachToEnd) {
        attachToEnd = -2;
    }
}

QAction *KeyframeView::parseKeyframeActions(QList <QAction *>actions)
{

    mlt_keyframe_type type = m_keyAnim.keyframe_type(activeKeyframe);
    for (int i = 0; i < actions.count(); i++) {
        if (actions.at(i)->data().toInt() == type) {
            return actions.at(i);
        }
    }
    return NULL;
}

void KeyframeView::attachKeyframeToEnd(bool attach)
{
    if (attach) {
        attachToEnd = activeKeyframe;
    } else {
        if (attachToEnd != duration -1) {
            // Check if there is a keyframe at end pos, and attach it to end if it is the case
            if (m_keyAnim.is_key(duration -1)) {
                attachToEnd = duration -1;
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
        double val = m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), activeKeyframe, duration);
        m_keyProperties.anim_set(m_inTimeline.toUtf8().constData(), val, activeKeyframe, duration, (mlt_keyframe_type) type);
    }
}

bool KeyframeView::activeParam(const QString &name) const
{
    return name == m_inTimeline;
}

const QString KeyframeView::serialize()
{
    if (attachToEnd == -2) {
        return m_keyAnim.serialize_cut();
    }
    int pos;
    mlt_keyframe_type type;
    QString key;
    QLocale locale;
    QStringList result;
    for(int i = 0; i < m_keyAnim.key_count(); ++i) {
        pos = m_keyAnim.key_get_frame(i);
        m_keyAnim.key_get(i, pos, type);
        double val = m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), pos, duration);
        if (pos >= attachToEnd) {
            pos = qMin(pos - duration, -1);
        }
        key = QString::number(pos);
        switch (type) {
            case mlt_keyframe_discrete:
                key.append("|=");
                break;
            case mlt_keyframe_smooth:
                key.append("~=");
                break;
            default:
                key.append("=");
                break;
        }
        key.append(locale.toString(val));
        result << key;
    }
    return result.join(";");
}

bool KeyframeView::loadKeyframes(const QLocale locale, QDomElement effect, int length)
{
    m_keyframeType = NoKeyframe;
    duration = length;
    m_inTimeline.clear();
    // reset existing properties
    int max = m_keyProperties.count();
    for (int i = max -1; i >= 0; i--) {
        m_keyProperties.set(m_keyProperties.get_name(i), (char*) NULL);
    }
    attachToEnd = -2;
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.isNull()) continue;
        QString type = e.attribute(QStringLiteral("type"));
        if (type == QLatin1String("keyframe")) m_keyframeType = NormalKeyframe;
        else if (type == QLatin1String("simplekeyframe")) m_keyframeType = SimpleKeyframe;
        else if (type == QLatin1String("geometry")) m_keyframeType = GeometryKeyframe;
        else if (type == QLatin1String("animated")) m_keyframeType = AnimatedKeyframe;
        else continue;
        QString paramName = e.attribute(QStringLiteral("name"));
        ParameterInfo info;
        info.factor = locale.toDouble(e.attribute(QStringLiteral("factor")));
        info.max = locale.toDouble(e.attribute(QStringLiteral("max")));
        info.min = locale.toDouble(e.attribute(QStringLiteral("min")));
        if (info.factor == 0) {
            info.factor = 1;
        }
        m_paramInfos.insert(paramName, info);
        if (!e.hasAttribute(QStringLiteral("intimeline")) || e.attribute(QStringLiteral("intimeline")) == QLatin1String("1")) {
            // Active parameter
            m_keyframeMin = info.min;
            m_keyframeMax = info.max;
            m_keyframeDefault = locale.toDouble(e.attribute(QStringLiteral("default")));
            m_keyframeFactor = info.factor;
            attachToEnd = checkNegatives(e.attribute("value").toUtf8().constData(), duration);
            m_inTimeline = paramName;
        }
        // parse keyframes
        switch (m_keyframeType) {
            case GeometryKeyframe:
                m_keyProperties.set(paramName.toUtf8().constData(), e.attribute("value").toUtf8().constData());
                m_keyProperties.anim_get_rect(paramName.toUtf8().constData(), 0, length);
                break;
            case AnimatedKeyframe:
                m_keyProperties.set(paramName.toUtf8().constData(), e.attribute("value").toUtf8().constData());
                // We need to initialize with length so that negative keyframes are correctly interpreted
                m_keyProperties.anim_get_double(paramName.toUtf8().constData(), 0, length);
                break;
            default:
                m_keyProperties.set(m_inTimeline.toUtf8().constData(), e.attribute(m_inTimeline.toUtf8().constData()).toUtf8().constData());
                m_keyProperties.anim_get_double(m_inTimeline.toUtf8().constData(), 0, length);
        }
        if (paramName == m_inTimeline) {
            m_keyAnim = m_keyProperties.get_animation(m_inTimeline.toUtf8().constData());
            if (m_keyAnim.next_key(activeKeyframe) <= activeKeyframe) activeKeyframe = -1;
        }
    }
    return (!m_inTimeline.isEmpty());
}

// static
int KeyframeView::checkNegatives(const QString &data, int maxDuration)
{
    int result = -2;
    QStringList frames = data.split(";");
    for (int i = 0; i < frames.count(); i++) {
        if (frames.at(i).startsWith("-")) {
            // We found a negative kfr
            QString sub = frames.at(i).section("=", 0, 0);
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
    int max = m_keyProperties.count();
    for (int i = max -1; i >= 0; i--) {
        m_keyProperties.set(m_keyProperties.get_name(i), (char*) NULL);
    }
    emit updateKeyframes(); 
}

//static
QString KeyframeView::cutAnimation(const QString &animation, int start, int duration, int fullduration)
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
	props.anim_set("keyframes", value, start, start + duration, type);
    }
    if (!anim.is_key(start + duration - 1)) {
	double value = props.anim_get_double("keyframes", start + duration - 1, fullduration);
	int previous = anim.previous_key(start + duration - 1);
	mlt_keyframe_type type = anim.keyframe_type(previous);
	props.anim_set("keyframes", value, start + duration - 1, fullduration, type);	
    }
    return anim.serialize_cut(start, start + duration - 1);
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

