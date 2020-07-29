/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#include "keyframeview.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "core.h"
#include "kdenlivesettings.h"

#include <QMouseEvent>
#include <QApplication>
#include <QStylePainter>

#include <KColorScheme>
#include <QFontDatabase>
#include <utility>

KeyframeView::KeyframeView(std::shared_ptr<KeyframeModelList> model, int duration, QWidget *parent)
    : QWidget(parent)
    , m_model(std::move(model))
    , m_duration(duration)
    , m_position(0)
    , m_currentKeyframe(-1)
    , m_currentKeyframeOriginal(-1)
    , m_hoverKeyframe(-1)
    , m_scale(1)
    , m_zoomFactor(1)
    , m_zoomStart(0)
    , m_zoomHandle(0,1)
    , m_hoverZoomIn(false)
    , m_hoverZoomOut(false)
    , m_hoverZoom(false)
    , m_clickOffset(0)
{
    setMouseTracking(true);
    setMinimumSize(QSize(150, 20));
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    QPalette p = palette();
    KColorScheme scheme(p.currentColorGroup(), KColorScheme::Window);
    m_colSelected = palette().highlight().color();
    m_colKeyframe = scheme.foreground(KColorScheme::NormalText).color();
    m_size = QFontInfo(font()).pixelSize() * 3;
    m_lineHeight = m_size / 2.1;
    m_zoomHeight = m_size * 3 / 4;
    m_offset = m_size / 4;
    setFixedHeight(m_size);
    setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
    connect(m_model.get(), &KeyframeModelList::modelChanged, this, &KeyframeView::slotModelChanged);
}

void KeyframeView::slotModelChanged()
{
    int offset = pCore->getItemIn(m_model->getOwnerId());
    emit atKeyframe(m_model->hasKeyframe(m_position + offset), m_model->singleKeyframe());
    emit modified();
    update();
}

void KeyframeView::slotSetPosition(int pos, bool isInRange)
{
    if (!isInRange) {
        m_position = -1;
        update();
        return;
    }
    if (pos != m_position) {
        m_position = pos;
        int offset = pCore->getItemIn(m_model->getOwnerId());
        emit atKeyframe(m_model->hasKeyframe(pos + offset), m_model->singleKeyframe());
        double zoomPos = (double) m_position / m_duration;
        if (zoomPos < m_zoomHandle.x()) {
            double interval = m_zoomHandle.y() - m_zoomHandle.x();
            zoomPos = qBound(0.0, zoomPos - interval / 5, 1.0);
            m_zoomHandle.setX(zoomPos);
            m_zoomHandle.setY(zoomPos + interval);
        } else if (zoomPos > m_zoomHandle.y()) {
            double interval = m_zoomHandle.y() - m_zoomHandle.x();
            zoomPos = qBound(0.0, zoomPos + interval / 5, 1.0);
            m_zoomHandle.setX(zoomPos - interval);
            m_zoomHandle.setY(zoomPos);
        }
        update();
    }
}

void KeyframeView::initKeyframePos()
{
    emit atKeyframe(m_model->hasKeyframe(m_position), m_model->singleKeyframe());
}

void KeyframeView::slotAddKeyframe(int pos)
{
    if (pos < 0) {
        pos = m_position;
    }
    int offset = pCore->getItemIn(m_model->getOwnerId());
    m_model->addKeyframe(GenTime(pos + offset, pCore->getCurrentFps()), (KeyframeType)KdenliveSettings::defaultkeyframeinterp());
}

void KeyframeView::slotAddRemove()
{
    emit activateEffect();
    int offset = pCore->getItemIn(m_model->getOwnerId());
    if (m_model->hasKeyframe(m_position + offset)) {
        slotRemoveKeyframe(m_position);
    } else {
        slotAddKeyframe(m_position);
    }
}

void KeyframeView::slotEditType(int type, const QPersistentModelIndex &index)
{
    int offset = pCore->getItemIn(m_model->getOwnerId());
    if (m_model->hasKeyframe(m_position + offset)) {
        m_model->updateKeyframeType(GenTime(m_position + offset, pCore->getCurrentFps()), type, index);
    }
}

void KeyframeView::slotRemoveKeyframe(int pos)
{
    if (pos < 0) {
        pos = m_position;
    }
    int offset = pCore->getItemIn(m_model->getOwnerId());
    m_model->removeKeyframe(GenTime(pos + offset, pCore->getCurrentFps()));
}

void KeyframeView::setDuration(int dur)
{
    m_duration = dur;
    int offset = pCore->getItemIn(m_model->getOwnerId());
    emit atKeyframe(m_model->hasKeyframe(m_position + offset), m_model->singleKeyframe());
    update();
}

void KeyframeView::slotGoToNext()
{
    emit activateEffect();
    if (m_position == m_duration - 1) {
        return;
    }

    bool ok;
    int offset = pCore->getItemIn(m_model->getOwnerId());
    auto next = m_model->getNextKeyframe(GenTime(m_position + offset, pCore->getCurrentFps()), &ok);

    if (ok) {
        emit seekToPos(qMin((int)next.first.frames(pCore->getCurrentFps()) - offset, m_duration - 1));
    } else {
        // no keyframe after current position
        emit seekToPos(m_duration - 1);
    }
}

void KeyframeView::slotGoToPrev()
{
    emit activateEffect();
    if (m_position == 0) {
        return;
    }

    bool ok;
    int offset = pCore->getItemIn(m_model->getOwnerId());
    auto prev = m_model->getPrevKeyframe(GenTime(m_position + offset, pCore->getCurrentFps()), &ok);

    if (ok) {
        emit seekToPos(qMax(0, (int)prev.first.frames(pCore->getCurrentFps()) - offset));
    } else {
        // no keyframe after current position
        emit seekToPos(m_duration - 1);
    }
}

void KeyframeView::mousePressEvent(QMouseEvent *event)
{
    emit activateEffect();
    int offset = pCore->getItemIn(m_model->getOwnerId());
    double zoomStart = m_zoomHandle.x() * (width() - 2 * m_offset);
    double zoomEnd = m_zoomHandle.y() * (width() - 2 * m_offset);
    double zoomFactor = (width() - 2 * m_offset) / (zoomEnd - zoomStart);
    int pos = ((event->x() - m_offset) / zoomFactor + zoomStart ) / m_scale;
    pos = qBound(0, pos, m_duration - 1);
    if (event->button() == Qt::LeftButton) {
        if (event->y() < m_lineHeight) {
            // mouse click in keyframes area
            bool ok;
            GenTime position(pos + offset, pCore->getCurrentFps());
            auto keyframe = m_model->getClosestKeyframe(position, &ok);
            if (ok && qAbs(keyframe.first.frames(pCore->getCurrentFps()) - pos - offset) * m_scale * m_zoomFactor < QApplication::startDragDistance()) {
                m_currentKeyframeOriginal = keyframe.first.frames(pCore->getCurrentFps()) - offset;
                // Select and seek to keyframe
                m_currentKeyframe = m_currentKeyframeOriginal;
                emit seekToPos(m_currentKeyframeOriginal);
                return;
            }
        } else if (event->y() > m_zoomHeight + 2) {
            // click on zoom area
            if (m_hoverZoom) {
                m_clickOffset = ((double) event->x() - m_offset) / (width() - 2 * m_offset);
            }
            return;
        }
    } else if (event->button() == Qt::RightButton && event->y() > m_zoomHeight + 2) {
        // Right click on zoom, switch between no zoom and last zoom status
        if (m_zoomHandle == QPointF(0, 1)) {
            if (!m_lastZoomHandle.isNull()) {
                m_zoomHandle = m_lastZoomHandle;
                update();
                return;
            }
        } else {
            m_lastZoomHandle = m_zoomHandle;
            m_zoomHandle = QPointF(0, 1);
            update();
            return;
        }
    }

    // no keyframe next to mouse
    m_currentKeyframe = m_currentKeyframeOriginal = -1;
    emit seekToPos(pos);
    update();
}

void KeyframeView::mouseMoveEvent(QMouseEvent *event)
{
    int offset = pCore->getItemIn(m_model->getOwnerId());
    double zoomStart = m_zoomHandle.x() * (width() - 2 * m_offset);
    double zoomEnd = m_zoomHandle.y() * (width() - 2 * m_offset);
    double zoomFactor = (width() - 2 * m_offset) / (zoomEnd - zoomStart);
    int pos = ((event->x() - m_offset) / zoomFactor + zoomStart ) / m_scale;
    pos = qBound(0, pos, m_duration - 1);
    GenTime position(pos + offset, pCore->getCurrentFps());
    if ((event->buttons() & Qt::LeftButton) != 0u) {
        if (m_hoverZoomIn || m_hoverZoomOut || m_hoverZoom) {
            // Moving zoom handles
            if (m_hoverZoomIn) {
                m_zoomHandle.setX(qMin(qMax(0., (double)(event->x() - m_offset) / (width() - 2 * m_offset)), m_zoomHandle.y() - 0.015));
                update();
                return;
            }
            if (m_hoverZoomOut) {
                m_zoomHandle.setY(qMax(qMin(1., (double)(event->x() - m_offset) / (width() - 2 * m_offset)), m_zoomHandle.x() + 0.015));
                update();
                return;
            }
            // moving zoom zone
            if (m_hoverZoom) {
                double clickOffset = ((double)event->x() - m_offset) / (width() - 2 * m_offset) - m_clickOffset;
                double newX = m_zoomHandle.x() + clickOffset;
                if (newX < 0) {
                    clickOffset = - m_zoomHandle.x();
                    newX = 0;
                }
                double newY = m_zoomHandle.y() + clickOffset;
                if (newY > 1) {
                    clickOffset = 1 - m_zoomHandle.y();
                    newY = 1;
                    newX = m_zoomHandle.x() + clickOffset;
                }
                m_clickOffset = ((double)event->x() - m_offset) / (width() - 2 * m_offset);
                m_zoomHandle = QPointF(newX, newY);
                update();
            }
            return;
        }
        if (m_currentKeyframe == pos) {
            return;
        }
        if (m_currentKeyframe > 0) {
            if (!m_model->hasKeyframe(pos + offset)) {
                GenTime currentPos(m_currentKeyframe + offset, pCore->getCurrentFps());
                if (m_model->moveKeyframe(currentPos, position, false)) {
                    m_currentKeyframe = pos;
                }
            }
        }
        emit seekToPos(pos);
        return;
    }
    if (event->y() < m_lineHeight) {
        bool ok;
        auto keyframe = m_model->getClosestKeyframe(position, &ok);
        if (ok && qAbs(keyframe.first.frames(pCore->getCurrentFps()) - pos - offset) * m_scale * m_zoomFactor < QApplication::startDragDistance()) {
            m_hoverKeyframe = keyframe.first.frames(pCore->getCurrentFps()) - offset;
            setCursor(Qt::PointingHandCursor);
            m_hoverZoomIn = false;
            m_hoverZoomOut = false;
            m_hoverZoom = false;
            update();
            return;
        }
    }
    if (event->y() > m_zoomHeight + 2) {
        // Moving in zoom area
        if (qAbs(event->x() - m_offset - (m_zoomHandle.x() * (width() - 2 * m_offset))) < QApplication::startDragDistance()) {
            setCursor(Qt::SizeHorCursor);
            m_hoverZoomIn = true;
            m_hoverZoomOut = false;
            m_hoverZoom = false;
            update();
            return;
        }
        if (qAbs(event->x() - m_offset - (m_zoomHandle.y() * (width() - 2 * m_offset))) < QApplication::startDragDistance()) {
            setCursor(Qt::SizeHorCursor);
            m_hoverZoomOut = true;
            m_hoverZoomIn = false;
            m_hoverZoom = false;
            update();
            return;
        }
        if (m_zoomHandle != QPointF(0, 1) && event->x() > m_offset + (m_zoomHandle.x() * (width() - 2 * m_offset)) && event->x() < m_offset + (m_zoomHandle.y() * (width() - 2 * m_offset))) {
            setCursor(Qt::PointingHandCursor);
            m_hoverZoom = true;
            m_hoverZoomIn = false;
            m_hoverZoomOut = false;
            update();
            return;
        }
    }

    if (m_hoverKeyframe != -1 || m_hoverZoomOut || m_hoverZoomIn || m_hoverZoom) {
        m_hoverKeyframe = -1;
        m_hoverZoomOut = false;
        m_hoverZoomIn = false;
        m_hoverZoom = false;
        setCursor(Qt::ArrowCursor);
        update();
    }
}

void KeyframeView::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    if (m_currentKeyframe >= 0 && m_currentKeyframeOriginal != m_currentKeyframe) {
        int offset = pCore->getItemIn(m_model->getOwnerId());
        GenTime initPos(m_currentKeyframeOriginal + offset, pCore->getCurrentFps());
        GenTime targetPos(m_currentKeyframe + offset, pCore->getCurrentFps());
        bool ok1 = m_model->moveKeyframe(targetPos, initPos, false);
        bool ok2 = m_model->moveKeyframe(initPos, targetPos, true);
        qDebug() << "RELEASING keyframe move" << ok1 << ok2 << initPos.frames(pCore->getCurrentFps()) << targetPos.frames(pCore->getCurrentFps());
    }
}

void KeyframeView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->y() < m_lineHeight) {
        int offset = pCore->getItemIn(m_model->getOwnerId());
        double zoomStart = m_zoomHandle.x() * (width() - 2 * m_offset);
        double zoomEnd = m_zoomHandle.y() * (width() - 2 * m_offset);
        double zoomFactor = (width() - 2 * m_offset) / (zoomEnd - zoomStart);
        int pos = ((event->x() - m_offset) / zoomFactor + zoomStart ) / m_scale;
        pos = qBound(0, pos, m_duration - 1);
        GenTime position(pos + offset, pCore->getCurrentFps());
        bool ok;
        auto keyframe = m_model->getClosestKeyframe(position, &ok);
        if (ok && qAbs(keyframe.first.frames(pCore->getCurrentFps()) - pos - offset)* m_scale * m_zoomFactor < QApplication::startDragDistance()) {
            if (keyframe.first.frames(pCore->getCurrentFps()) != offset) {
                m_model->removeKeyframe(keyframe.first);
                if (keyframe.first.frames(pCore->getCurrentFps()) == m_currentKeyframe + offset) {
                    m_currentKeyframe = m_currentKeyframeOriginal = -1;
                }
                if (keyframe.first.frames(pCore->getCurrentFps()) == m_position + offset) {
                    emit atKeyframe(false, m_model->singleKeyframe());
                }
            }
            return;
        }

        // add new keyframe
        m_model->addKeyframe(position, (KeyframeType)KdenliveSettings::defaultkeyframeinterp());
    } else {
        QWidget::mouseDoubleClickEvent(event);
    }
}

void KeyframeView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::AltModifier) {
        if (event->angleDelta().y() > 0) {
            slotGoToPrev();
        } else {
            slotGoToNext();
        }
        return;
    }
    int change = event->angleDelta().y() > 0 ? -1 : 1;
    int pos = qBound(0, m_position + change, m_duration - 1);
    emit seekToPos(pos);
}

void KeyframeView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QStylePainter p(this);
    int maxWidth = width() - 2 * m_offset;
    m_scale = maxWidth / (double)(m_duration - 1);
    int headOffset = m_lineHeight / 2;
    int offset = pCore->getItemIn(m_model->getOwnerId());
    m_zoomStart = m_zoomHandle.x() * maxWidth;
    double zoomEnd = m_zoomHandle.y() * maxWidth;
    m_zoomFactor = maxWidth / (zoomEnd - m_zoomStart);
    /* ticks */
    double fps = pCore->getCurrentFps();
    int displayedLength = m_duration / m_zoomFactor / fps;
    double factor = 1;
    if (displayedLength < 2) {
        // 1 frame tick
    } else if (displayedLength < 30 ) {
        // 1 sec tick
        factor = fps;
    } else if (displayedLength < 150) {
        // 5 sec tick
        factor = 5 * fps;
    } else if (displayedLength < 300) {
        // 10 sec tick
        factor = 10 * fps;
    } else if (displayedLength < 900) {
        // 30 sec tick
        factor = 30 * fps;
    } else if (displayedLength < 1800) {
        // 1 min. tick
        factor = 60 * fps;
    } else if (displayedLength < 9000) {
        // 5 min tick
        factor = 300 * fps;
    } else if (displayedLength < 18000) {
        // 10 min tick
        factor = 600 * fps;
    } else {
        // 30 min tick
        factor = 1800 * fps;
    }

    // Position of left border in frames
    double tickOffset = m_zoomStart * m_zoomFactor;
    double frameSize = factor * m_scale * m_zoomFactor;
    int base = tickOffset / frameSize;
    tickOffset = frameSize - (tickOffset - (base * frameSize));
    // Draw frame ticks
    int scaledTick = 0;
    for (int i = 0; i < maxWidth / frameSize; i++) {
        scaledTick = m_offset + (i * frameSize) + tickOffset;
        if (scaledTick >= maxWidth + m_offset) {
            break;
        }
        p.drawLine(QPointF(scaledTick , m_lineHeight + 1), QPointF(scaledTick, m_lineHeight - 3));
    }


    /*
     * keyframes
     */
    for (const auto &keyframe : *m_model.get()) {
        int pos = keyframe.first.frames(fps) - offset;
        if (pos < 0) continue;
        if (pos == m_currentKeyframe || pos == m_hoverKeyframe) {
            p.setBrush(m_colSelected);
        } else {
            p.setBrush(m_colKeyframe);
        }
        double scaledPos = pos * m_scale;
        if (scaledPos < m_zoomStart || scaledPos > zoomEnd) {
            continue;
        }
        scaledPos -= m_zoomStart;
        scaledPos *= m_zoomFactor;
        scaledPos += m_offset;
        p.drawLine(QPointF(scaledPos, headOffset), QPointF(scaledPos, m_lineHeight - 1));
        switch (keyframe.second.first) {
        case KeyframeType::Linear: {
            QPolygonF position = QPolygonF() << QPointF(-headOffset / 2.0, headOffset / 2.0) << QPointF(0, 0) << QPointF(headOffset / 2.0, headOffset / 2.0)
                                             << QPointF(0, headOffset);
            position.translate(scaledPos, 0);
            p.drawPolygon(position);
            break;
        }
        case KeyframeType::Discrete:
            p.drawRect(QRectF(scaledPos - headOffset / 2.0, 0, headOffset, headOffset));
            break;
        default:
            p.drawEllipse(QRectF(scaledPos - headOffset / 2.0, 0, headOffset, headOffset));
            break;
        }
    }

    p.setPen(palette().dark().color());

    /*
     * Time-"line"
     */
    p.setPen(m_colKeyframe);
    p.drawLine(m_offset, m_lineHeight, width() - m_offset, m_lineHeight);
    p.drawLine(m_offset, m_lineHeight - headOffset / 2, m_offset, m_lineHeight + headOffset / 2);
    p.drawLine(width() - m_offset, m_lineHeight - headOffset / 2, width() - m_offset, m_lineHeight + headOffset / 2);

    /*
     * current position cursor
     */
    if (m_position >= 0 && m_position < m_duration) {
        double scaledPos = m_position * m_scale;
        if (scaledPos >= m_zoomStart && scaledPos <= zoomEnd) {
            scaledPos -= m_zoomStart;
            scaledPos *= m_zoomFactor;
            scaledPos += m_offset;
            QPolygon pa(3);
            int cursorwidth = (m_zoomHeight - m_lineHeight) / 1.8;
            QPolygonF position = QPolygonF() << QPointF(-cursorwidth, m_zoomHeight - 3) << QPointF(cursorwidth, m_zoomHeight - 3) << QPointF(0, m_lineHeight + 1);
            position.translate(scaledPos, 0);
            p.setBrush(m_colKeyframe);
            p.drawPolygon(position);
        }
    }

    // Zoom bar
    p.setPen(Qt::NoPen);
    p.setBrush(palette().mid());
    p.drawRoundedRect(m_offset, m_zoomHeight + 3, width() - 2 * m_offset, m_size - m_zoomHeight - 3, m_lineHeight / 5, m_lineHeight / 5);
    p.setBrush(palette().highlight());
    p.drawRoundedRect(m_offset + (width() - m_offset) * m_zoomHandle.x(), m_zoomHeight + 3, (width() - 2 * m_offset) * (m_zoomHandle.y() - m_zoomHandle.x()), m_size - m_zoomHeight - 3, m_lineHeight / 5, m_lineHeight / 5);
}
