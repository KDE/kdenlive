/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
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

#include "keyframeview.h"

KeyframeView::KeyframeView(int handleSize, QObject *parent) : QObject(parent)
    , editedKeyframe(-1)
    , selectedKeyframe(0)
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
    return keyframeMap(br, frame, m_keyProperties.anim_get_double("keyframes", frame));
}

void KeyframeView::drawKeyFrames(QRectF br, int length, bool active, QPainter *painter, const QTransform &transformation)
{
    if (duration == 0 || m_keyframeType == NoKeyframe || m_keyAnim.key_count() < 1)
        return;
    duration = length;
    painter->save();
    QPointF h(m_handleSize, m_handleSize);

    // draw keyframes
    // Special case: Geometry keyframes are just vertical lines
    if (m_keyframeType == GeometryKeyframe) {
        for(int i = 0; i < m_keyAnim.key_count(); ++i) {
            int key = m_keyAnim.key_get_frame(i);
            QColor color = (key == editedKeyframe) ? QColor(Qt::red) : QColor(Qt::blue);
            if (active)
                painter->setPen(color);
            QPointF k = keyframePoint(br, i);
            painter->drawLine(transformation.map(QLineF(k.x(), br.top(), k.x(), br.height())));
            if (active) {
                k.setY(br.top() + br.height()/2);
                painter->setBrush((m_keyAnim.key_get_frame(i) == editedKeyframe) ? QColor(Qt::red) : QColor(Qt::blue));
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
    }

    QPointF start = keyframePoint(br, 0);
    QPainterPath path;
    path.moveTo(br.x(), br.bottom());
    path.lineTo(br.x(), start.y());
    path.lineTo(start);
    for(int i = 0; i < m_keyAnim.key_count(); ++i) {
        if (active) {
            painter->setBrush((m_keyAnim.key_get_frame(i) == editedKeyframe) ? QColor(Qt::red) : QColor(Qt::blue));
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
    for(int i = 0; i < m_keyAnim.key_count(); ++i) {
        int key = m_keyAnim.key_get_frame(i);
        double value = m_keyProperties.anim_get_double("keyframes", key);
        QPointF p = keyframeMap(br, key, value);
        p.setX(p.x()*scale);
        if (m_keyframeType == GeometryKeyframe)
            p.setY(br.bottom() - br.height() / 2);
        if ((pos - p).manhattanLength() < maxOffset) {
	    //TODO
            /*setToolTip('[' + QString::number((GenTime(key, m_fps) - cropStart()).seconds(), 'f', 2)
                       + i18n("seconds") + ", " + QString::number(value, 'f', 1) + ']');*/
	    editedKeyframe = key;
            return key;
        } else if (p.x() > pos.x() + maxOffset) {
            break;
        }
    }
    //setToolTip(QString());
    editedKeyframe = -1;
    return -1;
}

void KeyframeView::updateSelectedKeyFrame(QRectF br)
{
    if (editedKeyframe == -1)
        return;
    QPointF h(12, 12);
    QPointF p = keyframeMap(br, selectedKeyframe, m_keyProperties.anim_get_double("keyframes", selectedKeyframe));
    emit updateKeyframes(QRectF(p - h/2, p + h/2));
    selectedKeyframe = editedKeyframe;
    p = keyframeMap(br, selectedKeyframe, m_keyProperties.anim_get_double("keyframes", selectedKeyframe));
    updateKeyframes(QRectF(p - h/2, p + h/2));
}

int KeyframeView::editedKeyFramePos() const
{
    return editedKeyframe;
}

double KeyframeView::editedKeyFrameValue()
{
    return m_keyProperties.anim_get_double("keyframes", editedKeyframe);
}

int KeyframeView::selectedKeyFramePos() const
{
    return selectedKeyframe;
}

void KeyframeView::updateKeyFramePos(QRectF br, int frame, const double y)
{
    if (!m_keyAnim.is_key(editedKeyframe))
        return;
    int prev = m_keyAnim.key_count() <= 1 || m_keyAnim.key_get_frame(0) == editedKeyframe ? 0 : m_keyAnim.previous_key(editedKeyframe - 1) + 1;
    int next = m_keyAnim.key_count() <= 1 || m_keyAnim.key_get_frame(m_keyAnim.key_count() - 1) == editedKeyframe ? duration :  m_keyAnim.next_key(editedKeyframe + 1) - 1;
    int newpos = qBound(prev, frame, next);
    double newval = keyframeUnmap(br, y);
    m_keyProperties.anim_set("keyframes", newval, newpos, 0, m_keyAnim.keyframe_type(editedKeyframe));
    if (editedKeyframe != newpos)
        m_keyAnim.remove(editedKeyframe);
    editedKeyframe = newpos;
    emit updateKeyframes();
}

int KeyframeView::checkForSingleKeyframe()
{
    // Check if we have only one keyframe
    int start = 0;
    if (m_keyAnim.key_count() == 1 && m_keyAnim.is_key(start)) {
        double value = m_keyProperties.anim_get_double("keyframes", start);
        // Add keyframe at end of clip to allow inserting a new keframe in between
        int prevPos = m_keyAnim.previous_key(duration);
        m_keyProperties.anim_set("keyframes", value, duration, 0, m_keyAnim.keyframe_type(prevPos));
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
    m_keyProperties.anim_set("keyframes", value, frame, 0, type);
}

void KeyframeView::addDefaultKeyframe(int frame, mlt_keyframe_type type)
{
    double value = m_keyframeDefault;
    if (m_keyAnim.key_count() == 1) {
	value = m_keyProperties.anim_get_double("keyframes", m_keyAnim.key_get_frame(0));
    }
    m_keyProperties.anim_set("keyframes", value, frame, 0, type);
}


void KeyframeView::removeKeyframe(int frame)
{
    m_keyAnim.remove(frame);
}

const QString KeyframeView::serialize()
{
    return m_keyAnim.serialize_cut();
}

bool KeyframeView::loadKeyframes(const QLocale locale, QDomElement e, int length)
{
    QString type = e.attribute(QStringLiteral("type"));
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
        selectedKeyframe = 0;

        // parse keyframes
        switch (m_keyframeType) {
            case GeometryKeyframe:
                m_keyProperties.set("keyframes", e.attribute("value").toUtf8().constData());
                m_keyProperties.anim_get_rect("keyframes", 0);
                break;
            case AnimatedKeyframe:
                m_keyProperties.set("keyframes", e.attribute("value").toUtf8().constData());
                m_keyProperties.anim_get_double("keyframes", 0);
                m_keyframeFactor = locale.toDouble(e.attribute(QStringLiteral("factor")));
                break;
            default:
                m_keyProperties.set("keyframes", e.attribute("keyframes").toUtf8().constData());
                m_keyProperties.anim_get_double("keyframes", 0);
        }
        m_keyAnim = m_keyProperties.get_animation("keyframes");
        if (m_keyAnim.next_key(editedKeyframe) <= editedKeyframe) editedKeyframe = -1;
        return true;
    }
    m_keyProperties.set("keyframes", 0, 0);
    return false;
}


void KeyframeView::reset()
{
    if (m_keyframeType == NoKeyframe) {
	// nothing to do
	return;
    }
    m_keyframeType = NoKeyframe;
    duration = 0;
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
	props.anim_set("keyframes", value, start, 0, type);	
    }
    if (!anim.is_key(start + duration)) {
	double value = props.anim_get_double("keyframes", start + duration);
	int previous = anim.previous_key(start + duration);
	mlt_keyframe_type type = anim.keyframe_type(previous);
	props.anim_set("keyframes", value, start + duration, 0, type);	
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

