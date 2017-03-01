/***************************************************************************
 *   Copyright (C) 2008 by Marco Gittler (g.marco@freenet.de)              *
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "abstractclipitem.h"
#include "customtrackscene.h"
#include "kdenlivesettings.h"
#include "customtrackview.h"

#include "mlt++/Mlt.h"

#include <klocalizedstring.h>

#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>

AbstractClipItem::AbstractClipItem(const ItemInfo &info, const QRectF &rect, double fps) :
    QObject()
    , QGraphicsRectItem(rect)
    , m_info(info)
    , m_visibleParam(0)
    , m_selectedEffect(-1)
    , m_fps(fps)
    , m_isMainSelectedClip(false)
    , m_keyframeView(QFontInfo(QApplication::font()).pixelSize() * 0.7, this)
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    setPen(Qt::NoPen);
    connect(&m_keyframeView, &KeyframeView::updateKeyframes, this, &AbstractClipItem::doUpdate);
    m_selectionTimer.setSingleShot(true);
    m_selectionTimer.setInterval(1000);
    QObject::connect(&m_selectionTimer, &QTimer::timeout, this, &AbstractClipItem::slotSelectItem);
}

AbstractClipItem::~AbstractClipItem()
{
}

void AbstractClipItem::doUpdate(const QRectF &r)
{
    update(r);
}

void AbstractClipItem::updateKeyFramePos(int frame, const double y)
{
    m_keyframeView.updateKeyFramePos(rect(), frame, qBound(0.0, y, rect().height()));
}

void AbstractClipItem::prepareKeyframeMove()
{
    m_keyframeView.originalKeyframe = m_keyframeView.activeKeyframe;
}

int AbstractClipItem::selectedKeyFramePos() const
{
    return m_keyframeView.activeKeyframe;
}

int AbstractClipItem::originalKeyFramePos() const
{
    return m_keyframeView.originalKeyframe;
}

int AbstractClipItem::keyframesCount()
{
    return m_keyframeView.keyframesCount();
}

double AbstractClipItem::editedKeyFrameValue()
{
    return m_keyframeView.editedKeyFrameValue();
}

double AbstractClipItem::getKeyFrameClipHeight(const double y)
{
    return m_keyframeView.getKeyFrameClipHeight(rect(), y);
}

bool AbstractClipItem::isAttachedToEnd() const
{
    return m_keyframeView.activeKeyframe == m_keyframeView.attachToEnd;
}

QAction *AbstractClipItem::parseKeyframeActions(const QList<QAction *> &list)
{
    return m_keyframeView.parseKeyframeActions(list);
}

void AbstractClipItem::closeAnimation()
{
    if (!isEnabled()) {
        return;
    }
    setEnabled(false);
    setFlag(QGraphicsItem::ItemIsSelectable, false);
    if (QApplication::style()->styleHint(QStyle::SH_Widget_Animate, nullptr, QApplication::activeWindow())) {
        // animation disabled
        deleteLater();
        return;
    }
    QPropertyAnimation *closeAnimation = new QPropertyAnimation(this, "rect");
    QPropertyAnimation *closeAnimation2 = new QPropertyAnimation(this, "opacity");
    closeAnimation->setDuration(200);
    closeAnimation2->setDuration(200);
    QRectF r = rect();
    QRectF r2 = r;
    r2.setLeft(r.left() + r.width() / 2);
    r2.setTop(r.top() + r.height() / 2);
    r2.setWidth(1);
    r2.setHeight(1);
    closeAnimation->setStartValue(r);
    closeAnimation->setEndValue(r2);
    closeAnimation->setEasingCurve(QEasingCurve::InQuad);
    closeAnimation2->setStartValue(1.0);
    closeAnimation2->setEndValue(0.0);
    QParallelAnimationGroup *group = new QParallelAnimationGroup;
    connect(group, &QAbstractAnimation::finished, this, &QObject::deleteLater);
    group->addAnimation(closeAnimation);
    group->addAnimation(closeAnimation2);
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

ItemInfo AbstractClipItem::info() const
{
    ItemInfo info = m_info;
    info.cropStart = cropStart();
    info.endPos = endPos();
    return info;
}

GenTime AbstractClipItem::endPos() const
{
    return m_info.startPos + m_info.cropDuration;
}

int AbstractClipItem::track() const
{
    return m_info.track;
}

GenTime AbstractClipItem::cropStart() const
{
    return m_info.cropStart;
}

GenTime AbstractClipItem::cropDuration() const
{
    return m_info.cropDuration;
}

void AbstractClipItem::setCropStart(const GenTime &pos)
{
    m_info.cropStart = pos;
}

void AbstractClipItem::updateItem(int track)
{
    m_info.track = track;
    m_info.startPos = GenTime((int) scenePos().x(), m_fps);
    if (m_info.cropDuration > GenTime()) {
        m_info.endPos = m_info.startPos + m_info.cropDuration;
    }
}

void AbstractClipItem::updateRectGeometry()
{
    setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
}

void AbstractClipItem::resizeStart(int posx, bool hasSizeLimit, bool /*emitChange*/)
{
    GenTime durationDiff = GenTime(posx, m_fps) - m_info.startPos;
    if (durationDiff == GenTime()) {
        return;
    }

    if (type() == AVWidget && hasSizeLimit && (cropStart() + durationDiff < GenTime())) {
        durationDiff = GenTime() - cropStart();
    } else if (durationDiff >= cropDuration()) {
        return;
    }
    m_info.startPos += durationDiff;
    m_keyframeView.setOffset(durationDiff.frames(m_fps));
    // set to true if crop from start is negative (possible for color clips, images as they have no size limit)
    bool negCropStart = false;
    if (type() == AVWidget) {
        m_info.cropStart += durationDiff;
        if (m_info.cropStart < GenTime()) {
            negCropStart = true;
        }
    }
    m_info.cropDuration -= durationDiff;
    setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
    moveBy(durationDiff.frames(m_fps), 0);

    if (m_info.startPos != GenTime(posx, m_fps)) {
        GenTime diff = m_info.startPos - GenTime(posx, m_fps);

        if (type() == AVWidget) {
            m_info.cropStart += diff;
        }

        m_info.cropDuration -= diff;
        setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
    }
    // set crop from start to 0 (isn't relevant as this only happens for color clips, images)
    if (negCropStart) {
        m_info.cropStart = GenTime();
    }
}

void AbstractClipItem::resizeEnd(int posx, bool /*emitChange*/)
{
    GenTime durationDiff = GenTime(posx, m_fps) - endPos();
    if (durationDiff == GenTime()) {
        return;
    }
    if (cropDuration() + durationDiff <= GenTime()) {
        durationDiff = GenTime() - (cropDuration() - GenTime(3, m_fps));
    }

    m_info.cropDuration += durationDiff;
    m_info.endPos += durationDiff;

    setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
    if (durationDiff > GenTime()) {
        QList<QGraphicsItem *> collisionList = collidingItems(Qt::IntersectsItemBoundingRect);
        bool fixItem = false;
        for (int i = 0; i < collisionList.size(); ++i) {
            if (!collisionList.at(i)->isEnabled()) {
                continue;
            }
            QGraphicsItem *item = collisionList.at(i);
            if (item->type() == type() && item->pos().x() > pos().x()) {
                GenTime diff = static_cast<AbstractClipItem *>(item)->startPos() - startPos();
                if (fixItem == false || diff < m_info.cropDuration) {
                    fixItem = true;
                    m_info.cropDuration = diff;
                }
            }
        }
        if (fixItem) {
            setRect(0, 0, cropDuration().frames(m_fps) - 0.02, rect().height());
        }
    }
}

GenTime AbstractClipItem::startPos() const
{
    return m_info.startPos;
}

double AbstractClipItem::fps() const
{
    return m_fps;
}

void AbstractClipItem::updateFps(double fps)
{
    m_fps = fps;
    setPos((qreal) startPos().frames(m_fps), pos().y());
    updateRectGeometry();
}

GenTime AbstractClipItem::maxDuration() const
{
    return m_maxDuration;
}

CustomTrackScene *AbstractClipItem::projectScene()
{
    if (scene()) {
        return static_cast <CustomTrackScene *>(scene());
    }
    return nullptr;
}

void AbstractClipItem::setItemLocked(bool locked)
{
    if (locked) {
        setSelected(false);
    }

    // Allow move only if not in a group
    if (locked || parentItem() == nullptr) {
        setFlag(QGraphicsItem::ItemIsMovable, !locked);
    }
    setFlag(QGraphicsItem::ItemIsSelectable, !locked);
}

bool AbstractClipItem::isItemLocked() const
{
    return !(flags() & (QGraphicsItem::ItemIsSelectable));
}

// virtual
void AbstractClipItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->modifiers() & Qt::ShiftModifier) {
        // User want to do a rectangle selection, so ignore the event to pass it to the view
        event->ignore();
    } else {
        QGraphicsItem::mousePressEvent(event);
    }
}

// virtual
void AbstractClipItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // User want to do a rectangle selection, so ignore the event to pass it to the view
        event->ignore();
    } else {
        QGraphicsItem::mouseReleaseEvent(event);
    }
}

void AbstractClipItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->buttons() !=  Qt::LeftButton || event->modifiers() & Qt::ControlModifier) {
        // User want to do a rectangle selection, so ignore the event to pass it to the view
        event->ignore();
    } else {
        QGraphicsItem::mouseMoveEvent(event);
    }
}

void AbstractClipItem::slotSelectItem()
{
    emit selectItem(this);
}

int AbstractClipItem::itemHeight()
{
    return 0;
}

int AbstractClipItem::itemOffset()
{
    return 0;
}

void AbstractClipItem::setMainSelectedClip(bool selected)
{
    if (selected == m_isMainSelectedClip) {
        return;
    }
    m_isMainSelectedClip = selected;
    update();
}

bool AbstractClipItem::isMainSelectedClip()
{
    return m_isMainSelectedClip;
}

int AbstractClipItem::trackForPos(int position)
{
    int track = 1;
    if (!scene() || scene()->views().isEmpty()) {
        return track;
    }
    CustomTrackView *view = static_cast<CustomTrackView *>(scene()->views()[0]);
    if (view) {
        track = view->getTrackFromPos(position);
    }
    return track;
}

int AbstractClipItem::posForTrack(int track)
{
    int pos = 0;
    if (!scene() || scene()->views().isEmpty()) {
        return pos;
    }
    CustomTrackView *view = static_cast<CustomTrackView *>(scene()->views()[0]);
    if (view) {
        pos = view->getPositionFromTrack(track) + 1;
    }
    return pos;
}

void AbstractClipItem::attachKeyframeToEnd(const QDomElement &effect, bool attach)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.isNull()) {
            continue;
        }
        QString paramName = e.attribute(QStringLiteral("name"));
        if (m_keyframeView.activeParam(paramName) && e.attribute(QStringLiteral("type")) == QLatin1String("animated")) {
            m_keyframeView.attachKeyframeToEnd(attach);
            e.setAttribute(QStringLiteral("value"), m_keyframeView.serialize());
        }
    }
}

void AbstractClipItem::editKeyframeType(const QDomElement &effect, int type)
{
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.isNull()) {
            continue;
        }
        QString paramName = e.attribute(QStringLiteral("name"));
        if (m_keyframeView.activeParam(paramName) && e.attribute(QStringLiteral("type")) == QLatin1String("animated")) {
            m_keyframeView.editKeyframeType(type);
            e.setAttribute(QStringLiteral("value"), m_keyframeView.serialize());
        }
    }
}

void AbstractClipItem::insertKeyframe(ProfileInfo profile, QDomElement effect, int pos, double val, bool defaultValue)
{
    if (effect.attribute(QStringLiteral("disable")) == QLatin1String("1")) {
        return;
    }
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    effect.setAttribute(QStringLiteral("active_keyframe"), pos);
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.isNull()) {
            continue;
        }
        QString paramName = e.attribute(QStringLiteral("name"));
        if (e.attribute(QStringLiteral("type")) == QLatin1String("animated")) {
            if (!m_keyframeView.activeParam(paramName)) {
                continue;
            }
            if (defaultValue) {
                m_keyframeView.addDefaultKeyframe(profile, pos, m_keyframeView.type(pos));
            } else {
                m_keyframeView.addKeyframe(pos, val, m_keyframeView.type(pos));
            }
            // inserting a keyframe touches all animated params
            for (int j = 0; j < params.count(); ++j) {
                QDomElement f = params.item(j).toElement();
                if (f.attribute(QStringLiteral("type")) != QLatin1String("animated") && f.attribute(QStringLiteral("type")) != QLatin1String("animatedrect")) {
                    continue;
                }
                f.setAttribute(QStringLiteral("value"), m_keyframeView.serialize(f.attribute(QStringLiteral("name")), f.attribute(QStringLiteral("type")) == QLatin1String("animatedrect")));
            }
        } else if (e.attribute(QStringLiteral("type")) == QLatin1String("keyframe")
                   || e.attribute(QStringLiteral("type")) == QLatin1String("simplekeyframe")) {
            QString kfr = e.attribute(QStringLiteral("keyframes"));
            const QStringList keyframes = kfr.split(QLatin1Char(';'), QString::SkipEmptyParts);
            QStringList newkfr;
            bool added = false;
            foreach (const QString &str, keyframes) {
                int kpos = str.section(QLatin1Char('='), 0, 0).toInt();
                double newval = locale.toDouble(str.section(QLatin1Char('='), 1, 1));
                if (kpos < pos) {
                    newkfr.append(str);
                } else if (!added) {
                    if (i == m_visibleParam) {
                        newkfr.append(QString::number(pos) + QLatin1Char('=') + QString::number((int)val));
                    } else {
                        newkfr.append(QString::number(pos) + QLatin1Char('=') + locale.toString(newval));
                    }
                    if (kpos > pos) {
                        newkfr.append(str);
                    }
                    added = true;
                } else {
                    newkfr.append(str);
                }
            }
            if (!added) {
                if (i == m_visibleParam) {
                    newkfr.append(QString::number(pos) + QLatin1Char('=') + QString::number((int)val));
                } else {
                    newkfr.append(QString::number(pos) + QLatin1Char('=') + e.attribute(QStringLiteral("default")));
                }
            }
            e.setAttribute(QStringLiteral("keyframes"), newkfr.join(QLatin1Char(';')));
        }
    }
}

void AbstractClipItem::movedKeyframe(QDomElement effect, int newpos, int oldpos, double value)
{
    if (effect.attribute(QStringLiteral("disable")) == QLatin1String("1")) {
        return;
    }
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    effect.setAttribute(QStringLiteral("active_keyframe"), newpos);
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    int start = cropStart().frames(m_fps);
    int end = (cropStart() + cropDuration()).frames(m_fps) - 1;
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.isNull()) {
            continue;
        }
        QString paramName = e.attribute(QStringLiteral("name"));
        if (e.attribute(QStringLiteral("type")).startsWith(QLatin1String("animated"))) {
            if (m_keyframeView.activeParam(paramName)) {
                // inserting a keyframe touches all animated params
                for (int j = 0; j < params.count(); ++j) {
                    QDomElement f = params.item(j).toElement();
                    if (f.attribute(QStringLiteral("type")) != QLatin1String("animated") && f.attribute(QStringLiteral("type")) != QLatin1String("animatedrect")) {
                        continue;
                    }
                    f.setAttribute(QStringLiteral("value"), m_keyframeView.serialize(f.attribute(QStringLiteral("name")), f.attribute(QStringLiteral("type")) == QLatin1String("animatedrect")));
                }
            }
        } else if ((e.attribute(QStringLiteral("type")) == QLatin1String("keyframe") || e.attribute(QStringLiteral("type")) == QLatin1String("simplekeyframe"))) {
            QString kfr = e.attribute(QStringLiteral("keyframes"));
            const QStringList keyframes = kfr.split(QLatin1Char(';'), QString::SkipEmptyParts);
            QStringList newkfr;
            foreach (const QString &str, keyframes) {
                if (str.section(QLatin1Char('='), 0, 0).toInt() != oldpos) {
                    newkfr.append(str);
                } else if (newpos != -1) {
                    newpos = qBound(start, newpos, end);
                    if (i == m_visibleParam) {
                        newkfr.append(QString::number(newpos) + QLatin1Char('=') + locale.toString(value));
                    } else {
                        newkfr.append(QString::number(newpos) + QLatin1Char('=') + str.section(QLatin1Char('='), 1, 1));
                    }
                }
            }
            e.setAttribute(QStringLiteral("keyframes"), newkfr.join(QLatin1Char(';')));
        } else if (e.attribute(QStringLiteral("type")) == QLatin1String("geometry")) {
            QString kfr = e.attribute(QStringLiteral("value"));
            const QStringList keyframes = kfr.split(QLatin1Char(';'), QString::SkipEmptyParts);
            QStringList newkfr;
            for (const QString &str : keyframes) {
                if (str.section(QLatin1Char('='), 0, 0).toInt() != oldpos) {
                    newkfr.append(str);
                } else if (newpos != -1) {
                    newpos = qBound(0, newpos, end);
                    newkfr.append(QString::number(newpos) + QLatin1Char('=') + str.section(QLatin1Char('='), 1, 1));
                }
            }
            e.setAttribute(QStringLiteral("value"), newkfr.join(QLatin1Char(';')));
        }
    }

    updateKeyframes(effect);
    update();
}

void AbstractClipItem::removeKeyframe(QDomElement effect, int frame)
{
    if (effect.attribute(QStringLiteral("disable")) == QLatin1String("1")) {
        return;
    }
    effect.setAttribute(QStringLiteral("active_keyframe"), 0);
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (e.isNull()) {
            continue;
        }
        if ((e.attribute(QStringLiteral("type")) == QLatin1String("keyframe") || e.attribute(QStringLiteral("type")) == QLatin1String("simplekeyframe"))) {
            QString kfr = e.attribute(QStringLiteral("keyframes"));
            const QStringList keyframes = kfr.split(QLatin1Char(';'), QString::SkipEmptyParts);
            QStringList newkfr;
            for (const QString &str : keyframes) {
                if (str.section(QLatin1Char('='), 0, 0).toInt() != frame) {
                    newkfr.append(str);
                }
            }
            e.setAttribute(QStringLiteral("keyframes"), newkfr.join(QLatin1Char(';')));
        } else if (e.attribute(QStringLiteral("type")) == QLatin1String("geometry")) {
            QString kfr = e.attribute(QStringLiteral("value"));
            const QStringList keyframes = kfr.split(QLatin1Char(';'), QString::SkipEmptyParts);
            QStringList newkfr;
            foreach (const QString &str, keyframes) {
                if (str.section(QLatin1Char('='), 0, 0).toInt() != frame) {
                    newkfr.append(str);
                }
            }
            e.setAttribute(QStringLiteral("value"), newkfr.join(QLatin1Char(';')));
        } else if (e.attribute(QStringLiteral("type")) == QLatin1String("animated")) {
            m_keyframeView.removeKeyframe(frame);
            // inserting a keyframe touches all animated params
            for (int j = 0; j < params.count(); ++j) {
                QDomElement f = params.item(j).toElement();
                if (f.attribute(QStringLiteral("type")) != QLatin1String("animated") && f.attribute(QStringLiteral("type")) != QLatin1String("animatedrect")) {
                    continue;
                }
                f.setAttribute(QStringLiteral("value"), m_keyframeView.serialize(f.attribute(QStringLiteral("name")), f.attribute(QStringLiteral("type")) == QLatin1String("animatedrect")));
            }
        }
    }

    updateKeyframes(effect);
    update();
}

bool AbstractClipItem::resizeGeometries(QDomElement effect, int width, int height, int previousDuration, int start, int duration, int cropstart)
{
    QString geom;
    bool modified = false;
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    bool cut = effect.attribute(QStringLiteral("sync_in_out")).toInt() == 1 || effect.tagName() == QLatin1String("transition");
    if (!cut) {
        return false;
    }
    effect.setAttribute(QStringLiteral("in"), QString::number(cropstart));
    effect.setAttribute(QStringLiteral("out"), QString::number(cropstart + duration));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull() && e.attribute(QStringLiteral("type")) == QLatin1String("geometry")) {
            geom = e.attribute(QStringLiteral("value"));
            Mlt::Geometry geometry(geom.toUtf8().data(), previousDuration, width, height);
            e.setAttribute(QStringLiteral("value"), QString::fromLatin1(geometry.serialise(start, start + duration)));
            modified = true;
        }
    }
    return modified;
}

QString AbstractClipItem::resizeAnimations(QDomElement effect, int previousDuration, int start, int duration, int cropstart)
{
    QString animation;
    QString keyframes;
    QDomNodeList params = effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); ++i) {
        QDomElement e = params.item(i).toElement();
        if (!e.isNull() && e.attribute(QStringLiteral("type")) == QLatin1String("animated")) {
            animation = e.attribute(QStringLiteral("value"));
            if (effect.attribute(QStringLiteral("sync_in_out")) == QLatin1String("1")) {
                keyframes = KeyframeView::cutAnimation(animation, start, duration, previousDuration);
                effect.setAttribute(QStringLiteral("in"), QString::number(start));
                effect.setAttribute(QStringLiteral("out"), QString::number(start + duration));
            } else {
                keyframes = KeyframeView::addBorderKeyframes(animation, cropstart, duration);
            }
            // TODO: in case of multiple animated params, use _intimeline to detect active one
            e.setAttribute(QStringLiteral("value"), keyframes);
        }
    }
    return keyframes;
}

bool AbstractClipItem::switchKeyframes(QDomElement param, int in, int oldin, int out, int oldout)
{
    QString animation = param.attribute(QStringLiteral("value"));
    if (in != oldin) {
        animation = KeyframeView::switchAnimation(animation, in, oldin, out, oldout, param.attribute(QStringLiteral("type")) == QLatin1String("animatedrect"));
    }
    if (out != oldout) {
        animation = KeyframeView::switchAnimation(animation, out, oldout, out, oldout, param.attribute(QStringLiteral("type")) == QLatin1String("animatedrect"));
    }
    if (animation != param.attribute(QStringLiteral("value"))) {
        param.setAttribute(QStringLiteral("value"), animation);
        return true;
    }
    return false;
}

