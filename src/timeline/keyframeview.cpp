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
    return keyframeMap(br, frame < 0 ? frame + duration : frame, m_keyProperties.anim_get_double("keyframes", frame, duration));
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
    QPointF start = keyframePoint(br, 0);
    QPainterPath path;
    path.moveTo(br.x(), br.bottom());
    path.lineTo(br.x(), start.y());
    path.lineTo(start);
    for(int i = 0; i < m_keyAnim.key_count(); ++i) {
        if (active) {
            painter->setBrush((m_keyAnim.key_get_frame(i) == activeKeyframe) ? QColor(Qt::red) : QColor(Qt::blue));
            painter->drawEllipse(QRectF(transformation.map(start) - h/2, transformation.map(start) + h / 2));
        }
        if (i + 1 < m_keyAnim.key_count()) {
             QPointF end = keyframePoint(br, i + 1);
             switch (m_keyAnim.key_get_type(i)) {
                case mlt_keyframe_discrete:
                    path.lineTo(end.x(), start.y());
                    path.lineTo(end);
                    break;
                case mlt_keyframe_linear:
                    path.lineTo(end);
                    break;
                case mlt_keyframe_smooth:
                    QPointF pre = keyframePoint(br, qMax(i - 1, 0));
                    QPointF post = keyframePoint(br, qMin(i + 2, m_keyAnim.key_count() - 1));
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
    QColor col(Qt::white);//QApplication::palette().highlight().color());
    col.setAlpha(active ? 120 : 80);
    painter->setBrush(col);
    painter->drawPath(transformation.map(path));
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
        double value = m_keyProperties.anim_get_double("keyframes", key, duration);
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
    return m_keyProperties.anim_get_double("keyframes", activeKeyframe, duration);
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
    m_keyProperties.anim_set("keyframes", newval, newpos, duration, m_keyAnim.keyframe_type(activeKeyframe));
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
        double value = m_keyProperties.anim_get_double("keyframes", start, duration);
        // Add keyframe at end of clip to allow inserting a new keframe in between
        int prevPos = m_keyAnim.previous_key(duration);
        m_keyProperties.anim_set("keyframes", value, duration, duration, m_keyAnim.keyframe_type(prevPos));
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
    m_keyProperties.anim_set("keyframes", value, frame, duration, type);
}

void KeyframeView::addDefaultKeyframe(int frame, mlt_keyframe_type type)
{
    double value = m_keyframeDefault;
    if (m_keyAnim.key_count() == 1 && frame != m_keyAnim.key_get_frame(0)) {
	value = m_keyProperties.anim_get_double("keyframes", m_keyAnim.key_get_frame(0), duration);
    }
    m_keyProperties.anim_set("keyframes", value, frame, duration, type);
}


void KeyframeView::removeKeyframe(int frame)
{
    m_keyAnim.remove(frame);
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

void KeyframeView::attachKeyframeToEnd()
{
    attachToEnd = activeKeyframe;
    emit updateKeyframes();
}

void KeyframeView::editKeyframeType(int type)
{
    if (m_keyAnim.is_key(activeKeyframe)) {
        // This is a keyframe
        double val = m_keyProperties.anim_get_double("keyframes", activeKeyframe, duration);
        m_keyProperties.anim_set("keyframes", val, activeKeyframe, duration, (mlt_keyframe_type) type);
    }
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
        double val = m_keyProperties.anim_get_double("keyframes", pos, duration);
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

bool KeyframeView::loadKeyframes(const QLocale locale, QDomElement e, int length)
{
    QString type = e.attribute(QStringLiteral("type"));
    attachToEnd = -2;
    if (type == QLatin1String("keyframe")) m_keyframeType = NormalKeyframe;
    else if (type == QLatin1String("simplekeyframe")) m_keyframeType = SimpleKeyframe;
    else if (type == QLatin1String("geometry")) m_keyframeType = GeometryKeyframe;
    else if (type == QLatin1String("animated")) m_keyframeType = AnimatedKeyframe;
    else {
        m_keyframeType = NoKeyframe;
        return false;
    }
    if (m_keyframeType != NoKeyframe && (!e.hasAttribute(QStringLiteral("intimeline"))
        || e.attribute(QStringLiteral("intimeline")) == QLatin1String("1"))) {
	duration = length;
        m_keyframeMin = locale.toDouble(e.attribute(QStringLiteral("min")));
        m_keyframeMax = locale.toDouble(e.attribute(QStringLiteral("max")));
        m_keyframeDefault = locale.toDouble(e.attribute(QStringLiteral("default")));
        m_keyframeFactor = 1;

        // parse keyframes
        switch (m_keyframeType) {
            case GeometryKeyframe:
                m_keyProperties.set("keyframes", e.attribute("value").toUtf8().constData());
                m_keyProperties.anim_get_rect("keyframes", 0, length);
                break;
            case AnimatedKeyframe:
                m_keyProperties.set("keyframes", e.attribute("value").toUtf8().constData());
                attachToEnd = checkNegatives(e.attribute("value").toUtf8().constData(), duration);
                // We need to initialize with length so that negative keyframes are correctly interpreted
                m_keyProperties.anim_get_double("keyframes", 0, length);
                m_keyframeFactor = locale.toDouble(e.attribute(QStringLiteral("factor")));
                break;
            default:
                m_keyProperties.set("keyframes", e.attribute("keyframes").toUtf8().constData());
                m_keyProperties.anim_get_double("keyframes", 0, length);
        }
        m_keyAnim = m_keyProperties.get_animation("keyframes");
        if (m_keyAnim.next_key(activeKeyframe) <= activeKeyframe) activeKeyframe = -1;
        return true;
    }
    m_keyProperties.set("keyframes", 0, 0);
    return false;
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
    emit updateKeyframes(); 
}

//static
QString KeyframeView::cutAnimation(const QString &animation, int start, int duration)
{
    Mlt::Properties props;
    props.set("keyframes", animation.toUtf8().constData());
    props.anim_get_double("keyframes", 0);
    Mlt::Animation anim = props.get_animation("keyframes");
    if (start > 0 && !anim.is_key(start)) {
	// insert new keyframe at start
	double value = props.anim_get_double("keyframes", start);
	int previous = anim.previous_key(start);
	mlt_keyframe_type type = anim.keyframe_type(previous);
	props.anim_set("keyframes", value, start, start + duration, type);	
    }
    if (!anim.is_key(start + duration)) {
	double value = props.anim_get_double("keyframes", start + duration);
	int previous = anim.previous_key(start + duration);
	mlt_keyframe_type type = anim.keyframe_type(previous);
	props.anim_set("keyframes", value, start + duration, start + duration, type);	
    }
    return anim.serialize_cut(start, start + duration);
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
            m_keyProperties.anim_set("keyframes", m_keyProperties.anim_get_double("keyframes", oldPos), newPos, 0, m_keyAnim.keyframe_type(i));
        }
    }
    QString result = m_keyAnim.serialize_cut();
    param.setAttribute("value", result);
}*/

