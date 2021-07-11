/***************************************************************************
 *   Copyright (C) 2021 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "timeremap.h"

#include "core.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "bin/projectclip.h"
#include "project/projectmanager.h"
#include "monitor/monitor.h"
#include "profiles/profilemodel.hpp"
#include "mainwindow.h"
#include "timeline2/view/timelinewidget.h"
#include "timeline2/view/timelinecontroller.h"
#include "macros.hpp"

#include "kdenlive_debug.h"
#include <QFontDatabase>
#include <QWheelEvent>
#include <QStylePainter>
#include <QtMath>

#include <KColorScheme>
#include "klocalizedstring.h"

RemapView::RemapView(QWidget *parent)
    : QWidget(parent)
    , m_duration(1)
    , m_position(0)
    , m_scale(1.)
    , m_zoomFactor(1)
    , m_zoomStart(0)
    , m_zoomHandle(0,1)
    , m_moveKeyframeMode(NoMove)
    , m_clip(nullptr)
    , m_service(nullptr)
    , m_clickPoint(-1)
    , m_moveNext(true)
{
    setMouseTracking(true);
    setMinimumSize(QSize(150, 80));
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    int size = QFontInfo(font()).pixelSize() * 3;
    m_lineHeight = int(size / 2.);
    m_zoomHeight = m_lineHeight * 0.5;
    m_offset = qCeil(m_lineHeight / 4);
    setFixedHeight(size * 4);
    setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
    timer.setInterval(500);
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, &RemapView::reloadProducer);
}

void RemapView::updateInPos(int pos)
{
    if (m_currentKeyframe.first > -1) {
        if (m_keyframes.contains(pos)) {
            // Cannot move kfr over an existing one
            return;
        }
        m_keyframes.insert(pos, m_currentKeyframe.second);
        m_keyframes.remove(m_currentKeyframe.first);
        m_currentKeyframe.first = pos;
        update();
    }
}

void RemapView::updateOutPos(int pos)
{
    if (m_currentKeyframe.second > -1) {
        if (m_keyframes.values().contains(pos)) {
            // Cannot move kfr over an existing one
            return;
        }
        m_keyframes.insert(m_currentKeyframe.first, pos);
        m_currentKeyframe.second = pos;
        update();
    }
}

int RemapView::remapDuration() const
{
    int maxDuration = 0;
    QMapIterator<int, int> i(m_keyframes);
    while (i.hasNext()) {
        i.next();
        if (i.value() > maxDuration) {
            maxDuration = i.value();
        }
    }
    return maxDuration;
}

void RemapView::setBinClipDuration(std::shared_ptr<ProjectClip> clip, int duration)
{
    m_clip = clip;
    m_service = clip->originalProducer();
    m_duration = duration;
    m_currentKeyframe = m_currentKeyframeOriginal = {-1,-1};
}

void RemapView::setDuration(std::shared_ptr<Mlt::Producer> service, int duration)
{
    m_clip = nullptr;
    m_service = service;
    m_duration = duration;
    m_currentKeyframe = m_currentKeyframeOriginal = {-1,-1};
}

void RemapView::loadKeyframes(const QString &mapData)
{
    m_keyframes.clear();
    if (mapData.isEmpty()) {
        m_keyframes.insert(0, 0);
        m_keyframes.insert(m_duration - 1, m_duration - 1);
    } else {
        QStringList str = mapData.split(QLatin1Char(';'));
        for (auto &s : str) {
            int pos = m_service->time_to_frames(s.section(QLatin1Char('='), 0, 0).toUtf8().constData());
            int val = GenTime(s.section(QLatin1Char('='), 1).toDouble()).frames(pCore->getCurrentFps());
            m_keyframes.insert(val, pos);
            m_duration = qMax(m_duration, pos);
            m_duration = qMax(m_duration, val);
        }
        emit updateMaxDuration(m_duration);
        if (m_keyframes.contains(m_currentKeyframe.first)) {
            emit atKeyframe(true);
            std::pair<double,double>speeds = getSpeed(m_currentKeyframe);
            emit selectedKf(m_currentKeyframe, speeds);
        } else {
            emit atKeyframe(false);
            m_currentKeyframe = {-1,-1};
            emit selectedKf(m_currentKeyframe, {-1,-1});
        }
    }
    update();
}

void RemapView::mouseMoveEvent(QMouseEvent *event)
{
    event->accept();
    double zoomStart = m_zoomHandle.x() * (width() - 2 * m_offset);
    double zoomEnd = m_zoomHandle.y() * (width() - 2 * m_offset);
    double zoomFactor = (width() - 2 * m_offset) / (zoomEnd - zoomStart);
    int pos = int(((double(event->x()) - m_offset) / zoomFactor + zoomStart ) / m_scale);
    pos = qBound(0, pos, m_duration - 1);
    GenTime position(pos, pCore->getCurrentFps());
    if ((event->buttons() & Qt::LeftButton) != 0u) {
        if (m_hoverZoomIn || m_hoverZoomOut || m_hoverZoom) {
            // Moving zoom handles
            if (m_hoverZoomIn) {
                m_zoomHandle.setX(qMin(qMax(0., double(event->x() - m_offset) / (width() - 2 * m_offset)), m_zoomHandle.y() - 0.015));
                update();
                return;
            }
            if (m_hoverZoomOut) {
                m_zoomHandle.setY(qMax(qMin(1., double(event->x() - m_offset) / (width() - 2 * m_offset)), m_zoomHandle.x() + 0.015));
                update();
                return;
            }
            // moving zoom zone
            if (m_hoverZoom) {
                double clickOffset = (double(event->x()) - m_offset) / (width() - 2 * m_offset) - m_clickOffset;
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
                m_clickOffset = (double(event->x()) - m_offset) / (width() - 2 * m_offset);
                m_zoomHandle = QPointF(newX, newY);
                update();
            }
            return;
        }
        //qDebug()<<"=== MOVING MOUSE: "<<pos<<" = "<<m_currentKeyframe<<", MOVE KFMODE: "<<m_moveKeyframeMode;
        if ((m_currentKeyframe.first == pos && m_moveKeyframeMode == TopMove) || (m_currentKeyframe.second == pos && m_moveKeyframeMode == BottomMove)) {
            return;
        }
        if (m_currentKeyframe.first >= 0 && m_moveKeyframeMode != NoMove) {
            if (m_moveKeyframeMode == TopMove) {
                // Moving top keyframe
                if (!m_keyframes.contains(pos)) {
                    int delta = pos - m_currentKeyframe.first;
                    // Check that the move is possible
                    QMapIterator<int, int> i(m_selectedKeyframes);
                    while (i.hasNext()) {
                        i.next();
                        int updatedPos = i.key() + delta;
                        if (!m_selectedKeyframes.contains(updatedPos) && m_keyframes.contains(updatedPos)) {
                            // Don't allow moving over another keyframe
                            qDebug()<<"== MOVE ABORTED; OVERLAPPING EXISTING";
                            return;
                        }
                    }
                    i.toFront();
                    QMap <int,int> updated;
                    while (i.hasNext()) {
                        i.next();
                        //qDebug()<<"=== MOVING KFR: "<<i.key()<<" > "<<(i.key() + delta);
                        m_keyframes.insert(i.key() + delta, i.value());
                        updated.insert(i.key() + delta, i.value());
                        m_keyframes.remove(i.key());
                        if (i.key() == m_currentKeyframe.first) {
                            m_currentKeyframe.first = pos;
                            std::pair<double,double>speeds = getSpeed(m_currentKeyframe);
                            emit selectedKf(m_currentKeyframe, speeds);
                        }
                    }
                    m_selectedKeyframes = updated;
                    updateKeyframes();
                    update();
                } else {
                    qDebug()<<"=== KEYFRAME :"<< pos<<" ALREADY EXISTS";
                }
            } else {
                // Moving bottom keyframe
                auto kfrValues = m_keyframes.values();
                //pos = GenTime(m_remapLink->anim_get_double("map", pos)).frames(pCore->getCurrentFps());
                if (!kfrValues.contains(pos)) {
                    int delta = pos - m_currentKeyframe.second;
                    // Check that the move is possible
                    auto selectedValues = m_selectedKeyframes.values();
                    QMapIterator<int, int> i(m_selectedKeyframes);
                    while (i.hasNext()) {
                        i.next();
                        int updatedPos = i.value() + delta;
                        if (!selectedValues.contains(updatedPos) && kfrValues.contains(updatedPos)) {
                            // Don't allow moving over another keyframe
                            return;
                        }
                    }
                    i.toFront();
                    QMap <int,int> updated;
                    while (i.hasNext()) {
                        i.next();
                        m_keyframes.insert(i.key(), i.value() + delta);
                        updated.insert(i.key(), i.value() + delta);
                        if (i.value() == m_currentKeyframe.second) {
                            m_currentKeyframe.second = pos;
                        }
                    }
                    // Update all keyframes after selection
                    if (m_moveNext && m_selectedKeyframes.count() == 1) {
                        QMapIterator<int, int> j(m_keyframes);
                        while (j.hasNext()) {
                            j.next();
                            if (j.value() != m_currentKeyframe.second && j.value() > m_currentKeyframe.second - delta) {
                                m_keyframes.insert(j.key(), j.value() + delta);
                            }
                        }
                    }
                    std::pair<double,double> speeds = getSpeed(m_currentKeyframe);
                    emit selectedKf(m_currentKeyframe, speeds);
                    m_selectedKeyframes = updated;
                    updateKeyframes();
                    update();
                }
            }
        }
        // Rubberband selection
        if (m_clickPoint >= 0) {
            m_clickEnd = pos;
            int min = qMin(m_clickPoint, m_clickEnd);
            int max = qMax(m_clickPoint, m_clickEnd);
            min = qMax(1, min);
            m_selectedKeyframes.clear();
            m_currentKeyframeOriginal = m_currentKeyframe = {-1,-1};
            QMapIterator<int, int> i(m_keyframes);
            while (i.hasNext()) {
                i.next();
                if (i.key() > min && i.key() <= max) {
                    m_selectedKeyframes.insert(i.key(), i.value());
                }
            }
            if (!m_selectedKeyframes.isEmpty()) {
                m_currentKeyframe = {m_selectedKeyframes.firstKey(), m_selectedKeyframes.value(m_selectedKeyframes.firstKey())};
                m_currentKeyframeOriginal = m_currentKeyframe;
            }
            update();
            return;
        }

        if (m_moveKeyframeMode == CursorMove || (event->y() < 2 * m_lineHeight)) {
            if (pos != m_position) {
                qDebug()<<"=== CURSOR MOVE MODE!!!";
                slotSetPosition(pos, true);
                emit seekToPos(pos);
            }
        }
        if (m_moveKeyframeMode == CursorMoveBottom || (event->y() > m_bottomView)) {
            pos = GenTime(m_remapLink->anim_get_double("map", pos)).frames(pCore->getCurrentFps());
            if (pos != getKeyframePosition()) {
                qDebug()<<"=== CURSOR MOVE MODE!!!";
                slotSetPosition(pos, true);
                emit seekToPos2(getKeyframePosition());
            }
        }
        return;
    }
    if (event->y() < m_lineHeight) {
        int closest = getClosestKeyframe(pos);
        if (closest > -1 && qAbs(((pos - closest) * m_scale) * m_zoomFactor) < QApplication::startDragDistance()) {
            m_hoverKeyframe = {closest, false};
            setCursor(Qt::PointingHandCursor);
            m_hoverZoomIn = false;
            m_hoverZoomOut = false;
            m_hoverZoom = false;
            update();
            return;
        }
    } else if (event->y() > m_bottomView + m_lineHeight) {
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
    } else if (event->y() > m_bottomView) {
        // Bottom keyframes
        int closest = getClosestKeyframe(pos, true);
        if (closest > -1 && qAbs(((pos - closest) * m_scale) * m_zoomFactor) < QApplication::startDragDistance()) {
            m_hoverKeyframe = {closest, true};
            setCursor(Qt::PointingHandCursor);
            m_hoverZoomIn = false;
            m_hoverZoomOut = false;
            m_hoverZoom = false;
            update();
            return;
        }
    }

    if (m_hoverKeyframe.first != -1 || m_hoverZoomOut || m_hoverZoomIn || m_hoverZoom) {
        m_hoverKeyframe.first = -1;
        m_hoverZoomOut = false;
        m_hoverZoomIn = false;
        m_hoverZoom = false;
        setCursor(Qt::ArrowCursor);
        update();
    }
}

int RemapView::position() const
{
    return m_position;
}

int RemapView::getClosestKeyframe(int pos, bool bottomKeyframe) const
{
    int deltaMin = -1;
    int closest = -1;
    QMapIterator<int, int> i(m_keyframes);
    while (i.hasNext()) {
        i.next();
        int val = bottomKeyframe ? i.value() : i.key();
        int delta = qAbs(val - pos);
        if (deltaMin == -1 || delta < deltaMin) {
            deltaMin = delta;
            closest = val;
        }
    }
    return closest;
}

void RemapView::mouseReleaseEvent(QMouseEvent *event)
{
    event->accept();
    m_moveKeyframeMode = NoMove;
    if (m_keyframesOrigin != m_keyframes) {
        Fun undo = [this, kfr = m_keyframesOrigin]() {
            m_keyframes = kfr;
            if (m_keyframes.contains(m_currentKeyframe.first)) {
                emit atKeyframe(true);
                std::pair<double,double>speeds = getSpeed(m_currentKeyframe);
                emit selectedKf(m_currentKeyframe, speeds);
            } else {
                emit atKeyframe(false);
                m_currentKeyframe = {-1,-1};
                emit selectedKf(m_currentKeyframe, {-1,-1});
            }
            update();
            return true;
        };
        Fun redo = [this, kfr2 = m_keyframes]() {
            m_keyframes = kfr2;
            if (m_keyframes.contains(m_currentKeyframe.first)) {
                emit atKeyframe(true);
                std::pair<double,double>speeds = getSpeed(m_currentKeyframe);
                emit selectedKf(m_currentKeyframe, speeds);
            } else {
                emit atKeyframe(false);
                m_currentKeyframe = {-1,-1};
                emit selectedKf(m_currentKeyframe, {-1,-1});
            }
            update();
            return true;
        };
        pCore->pushUndo(undo, redo, i18n("Edit Timeremap keyframes"));
    }
    qDebug()<<"=== MOUSE RELEASE!!!!!!!!!!!!!";
}

void RemapView::mousePressEvent(QMouseEvent *event)
{
    event->accept();
    double zoomStart = m_zoomHandle.x() * (width() - 2 * m_offset);
    double zoomEnd = m_zoomHandle.y() * (width() - 2 * m_offset);
    double zoomFactor = (width() - 2 * m_offset) / (zoomEnd - zoomStart);
    int pos = int(((event->x() - m_offset) / zoomFactor + zoomStart ) / m_scale);
    pos = qBound(0, pos, m_duration - 1);
    m_moveKeyframeMode = NoMove;
    m_keyframesOrigin = m_keyframes;
    if (event->button() == Qt::LeftButton) {
        if (event->y() < m_lineHeight) {
            // mouse click in top keyframes area
            if (event->modifiers() & Qt::ShiftModifier) {
                m_clickPoint = pos;
                return;
            }
            int keyframe = getClosestKeyframe(pos);
            qDebug()<<"==== KEYFRAME AREA CLICK! CLOSEST KFR: "<<keyframe;
            if (keyframe > -1 && qAbs(keyframe - pos) * m_scale * m_zoomFactor < QApplication::startDragDistance()) {
                m_currentKeyframeOriginal = {keyframe, m_keyframes.value(keyframe)};
                if (event->modifiers() & Qt::ControlModifier) {
                    if (m_selectedKeyframes.contains(m_currentKeyframeOriginal.first)) {
                        m_selectedKeyframes.remove(m_currentKeyframeOriginal.first);
                        m_currentKeyframeOriginal.first = -1;
                    } else {
                        m_selectedKeyframes.insert(m_currentKeyframeOriginal.first, m_currentKeyframeOriginal.second);
                    }
                } else if (!m_selectedKeyframes.contains(m_currentKeyframeOriginal.first)) {
                    m_selectedKeyframes = {m_currentKeyframeOriginal};
                }
                // Select and seek to keyframe
                m_currentKeyframe = m_currentKeyframeOriginal;
                // Calculate speeds
                std::pair<double,double> speeds = getSpeed(m_currentKeyframe);
                emit selectedKf(m_currentKeyframe, speeds);
                if (m_currentKeyframeOriginal.first > -1) {
                    qDebug()<<"=== SETTING CURRENT KEYFRAME: "<<m_currentKeyframe.first;
                    m_moveKeyframeMode = TopMove;
                    if (KdenliveSettings::keyframeseek()) {
                        slotSetPosition(m_currentKeyframeOriginal.first, true);
                        emit seekToPos(m_currentKeyframeOriginal.first);
                        emit seekToPos2(getKeyframePosition());
                    } else {
                        update();
                    }
                } else {
                    update();
                }
                return;
            }
            // no keyframe next to mouse
            m_selectedKeyframes.clear();
            m_currentKeyframe = m_currentKeyframeOriginal = {-1,-1};
        } else if (event->y() > m_bottomView + m_lineHeight + 2) {
            // click on zoom area
            if (m_hoverZoom) {
                m_clickOffset = (double(event->x()) - m_offset) / (width() - 2 * m_offset);
            }
            return;
        } else if (event->y() > m_bottomView) {
            // click in bottom keyframe area
            if (event->modifiers() & Qt::ShiftModifier) {
                m_clickPoint = pos;
                return;
            }
            int keyframe = getClosestKeyframe(pos, true);
            qDebug()<<"==== KEYFRAME AREA CLICK! CLOSEST KFR: "<<keyframe;
            if (keyframe > -1 && qAbs(keyframe - pos) * m_scale * m_zoomFactor < QApplication::startDragDistance()) {
                m_currentKeyframeOriginal = {m_keyframes.key(keyframe),keyframe};
                if (event->modifiers() & Qt::ControlModifier) {
                    if (m_selectedKeyframes.values().contains(m_currentKeyframeOriginal.second)) {
                        m_selectedKeyframes.remove(m_currentKeyframeOriginal.first);
                        m_currentKeyframeOriginal.second = -1;
                    } else {
                        m_selectedKeyframes.insert(m_currentKeyframeOriginal.first, m_currentKeyframeOriginal.second);
                    }
                } else if (!m_selectedKeyframes.values().contains(m_currentKeyframeOriginal.second)) {
                    m_selectedKeyframes = {m_currentKeyframeOriginal};
                }
                // Select and seek to keyframe
                m_currentKeyframe = m_currentKeyframeOriginal;
                std::pair<double,double> speeds = getSpeed(m_currentKeyframe);
                emit selectedKf(m_currentKeyframe, speeds);
                if (m_currentKeyframeOriginal.second > -1) {
                    m_moveKeyframeMode = BottomMove;
                    if (KdenliveSettings::keyframeseek()) {
                        slotSetPosition(m_currentKeyframeOriginal.first, true);
                        emit seekToPos(m_currentKeyframeOriginal.first);
                        emit seekToPos2(getKeyframePosition());
                    } else {
                        update();
                    }
                } else {
                    update();
                }
                return;
            }
            // no keyframe next to mouse
            m_selectedKeyframes.clear();
            m_currentKeyframe = m_currentKeyframeOriginal = {-1,-1};
        } else if (event->y() < 2 * m_lineHeight) {
            qDebug()<<"=== PRESSED WITH Y: "<<event->y() <<" <  "<<(2 * m_lineHeight);
            if (pos != m_position) {
                m_moveKeyframeMode = CursorMove;
                slotSetPosition(pos, true);
                emit seekToPos(pos);
                update();
            }
        } else if (event->y() > m_bottomView) {
            qDebug()<<"=== PRESSED WITH Y: "<<event->y() <<" <  "<<(2 * m_lineHeight);
            if (pos != m_position) {
                m_moveKeyframeMode = CursorMoveBottom;
                slotSetPosition(pos, true);
                emit seekToPos2(getKeyframePosition());
                update();
            }
        }
    } else if (event->button() == Qt::RightButton && event->y() > m_bottomView + m_lineHeight) {
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
    if (pos != m_position) {
        //emit seekToPos(pos);
        update();
    }
}

void RemapView::slotSetPosition(int pos, bool force)
{
    if (pos != m_position) {
        if (!force && m_moveKeyframeMode != NoMove) {
            return;
        }
        m_position = pos;
        //int offset = pCore->getItemIn(m_model->getOwnerId());
        emit atKeyframe(m_keyframes.contains(pos));
        double zoomPos = double(m_position) / m_duration;
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

void RemapView::goNext()
{
    // insert keyframe at interpolated position
    QMapIterator<int, int> i(m_keyframes);
    while (i.hasNext()) {
        i.next();
        if (i.key() > m_position) {
            m_currentKeyframe = {i.key(),i.value()};
            slotSetPosition(i.key());
            emit seekToPos(i.key());
            emit seekToPos2(getKeyframePosition());
            std::pair<double,double> speeds = getSpeed(m_currentKeyframe);
            emit selectedKf(m_currentKeyframe, speeds);
            break;
        }
    }
}

void RemapView::goPrev()
{
    // insert keyframe at interpolated position
    bool previousFound = false;
    QMap<int, int>::const_iterator it = m_keyframes.constBegin();
    while (it.key() < m_position && it != m_keyframes.constEnd()) {
        it++;
    }
    if (it != m_keyframes.constEnd()) {
        if (it != m_keyframes.constBegin()) {
            it--;
        }
        m_currentKeyframe = {it.key(), it.value()};
        slotSetPosition(m_currentKeyframe.first);
        emit seekToPos(m_currentKeyframe.first);
        emit seekToPos2(getKeyframePosition());
        std::pair<double,double> speeds = getSpeed(m_currentKeyframe);
        emit selectedKf(m_currentKeyframe, speeds);
        previousFound = true;
    }

    if (!previousFound && !m_keyframes.isEmpty()) {
        // We are after the last keyframe
        m_currentKeyframe = {m_keyframes.lastKey(), m_keyframes.value(m_keyframes.lastKey())};
        slotSetPosition(m_currentKeyframe.first);
        emit seekToPos(m_currentKeyframe.first);
        emit seekToPos2(getKeyframePosition());
        std::pair<double,double> speeds = getSpeed(m_currentKeyframe);
        emit selectedKf(m_currentKeyframe, speeds);
    }
}

void RemapView::updateBeforeSpeed(double speed)
{
    QMap<int, int>::iterator it = m_keyframes.find(m_currentKeyframe.first);
    if (*it != m_keyframes.first()) {
        it--;
        int updatedLength = (m_currentKeyframe.first - it.key()) * 100. / speed;
        int offset = it.value() + updatedLength - m_currentKeyframe.second;
        m_currentKeyframe.second = it.value() + updatedLength;
        m_keyframes.insert(m_currentKeyframe.first, m_currentKeyframe.second);
        // Update all keyframes after that so that we don't alter the speeds
        while (m_moveNext && it != m_keyframes.end()) {
            it++;
            m_keyframes.insert(it.key(), it.value() + offset);
        }
        update();
    }
}

void RemapView::updateAfterSpeed(double speed)
{
    QMap<int, int>::iterator it = m_keyframes.find(m_currentKeyframe.first);
    if (*it != m_keyframes.last()) {
        it++;
        int updatedLength = (it.key() - m_currentKeyframe.first) * 100. / speed;
        m_keyframes.insert(it.key(), m_currentKeyframe.second + updatedLength);
        update();
    }
}

const QString RemapView::getKeyframesData() const
{
    QStringList result;
    QMapIterator<int, int> i(m_keyframes);
    while (i.hasNext()) {
        i.next();
        result << QString("%1=%2").arg(m_service->frames_to_time(i.value(), mlt_time_clock)).arg(GenTime(i.key(), pCore->getCurrentFps()).seconds());
    }
    return result.join(QLatin1Char(';'));
}

void RemapView::reloadProducer()
{
    if (!m_clip || !m_clip->clipUrl().endsWith(QLatin1String(".mlt"))) {
        qDebug()<<"==== this is not a playlist clip, aborting";
        return;
    }
    Mlt::Consumer c(pCore->getCurrentProfile()->profile(), "xml", m_clip->clipUrl().toUtf8().constData());
    QScopedPointer<Mlt::Service> serv(m_clip->originalProducer()->producer());
    if (serv == nullptr) {
        return;
    }
    qDebug()<<"==== GOR PLAYLIST SERVICE: "<<serv->type()<<" / "<<serv->consumer()->type()<<", SAVING TO "<<m_clip->clipUrl();
    Mlt::Multitrack s2(*serv.data());
    qDebug()<<"==== MULTITRACK: "<<s2.count();
    Mlt::Tractor s(pCore->getCurrentProfile()->profile());
    s.set_track(*s2.track(0), 0);
    qDebug()<<"==== GOT TRACKS: "<<s.count();
    int ignore = s.get_int("ignore_points");
    if (ignore) {
        s.set("ignore_points", 0);
    }
    c.connect(s);
    c.set("time_format", "frames");
    c.set("no_meta", 1);
    c.set("no_root", 1);
    //c.set("no_profile", 1);
    c.set("root", "/");
    c.set("store", "kdenlive");
    c.run();
    if (ignore) {
        s.set("ignore_points", ignore);
    }
}

std::pair<double,double> RemapView::getSpeed(std::pair<int,int>kf)
{
    std::pair<double,double> speeds = {-1,-1};
    QMap<int, int>::const_iterator it = m_keyframes.constFind(kf.first);
    if (it == m_keyframes.constEnd()) {
        // Not a keyframe
        return speeds;
    }
    if (*it != m_keyframes.first()) {
        it--;
        speeds.first = (double)qAbs(kf.first - it.key()) / qAbs(kf.second - it.value());
        it++;
    }
    if (*it != m_keyframes.last()) {
        it++;
        speeds.second = (double)qAbs(kf.first - it.key()) / qAbs(kf.second - it.value());
    }
    return speeds;
}

int RemapView::getKeyframePosition() const
{
    QMapIterator<int, int> i(m_keyframes);
    std::pair<int, int> newKeyframe = {-1,-1};
    std::pair<int, int> previous = {-1,-1};
    newKeyframe.first = m_position;
    while (i.hasNext()) {
        i.next();
        if (i.key() > m_position) {
            if (i.key() == m_keyframes.firstKey()) {
                // This is the first keyframe
                double ratio = (double)m_position / i.key();
                return i.value() * ratio;
                break;
            } else if (previous.first > -1) {
                std::pair<int,int> current = {i.key(), i.value()};
                double ratio = (double)(m_position - previous.first) / (current.first - previous.first);
                return previous.second + (qAbs(current.second - previous.second) * ratio);
                break;
            }
        }
        previous = {i.key(), i.value()};
    }
    if (newKeyframe.second == -1) {
        // We are after the last keyframe
        if (m_keyframes.isEmpty()) {
            return m_position;
        } else {
            double ratio = (double)(m_position - m_keyframes.lastKey()) / (m_duration - m_keyframes.lastKey());
            return m_keyframes.value(m_keyframes.lastKey()) + (qAbs(m_duration - m_keyframes.value(m_keyframes.lastKey())) * ratio);
        }
    }
    return m_position;
}

void RemapView::addKeyframe()
{
    // insert or remove keyframe at interpolated position
    if (m_keyframes.contains(m_position)) {
        m_keyframes.remove(m_position);
        if (m_currentKeyframe.first == m_position) {
            m_currentKeyframe = m_currentKeyframeOriginal = {-1,-1};
            std::pair<double,double> speeds = {-1,-1};
            emit selectedKf(m_currentKeyframe, speeds);
        }
        emit atKeyframe(false);
        updateKeyframes();
        update();
        return;
    }
    QMapIterator<int, int> i(m_keyframes);
    std::pair<int, int> newKeyframe = {-1,-1};
    std::pair<int, int> previous = {-1,-1};
    newKeyframe.first = m_position;
    while (i.hasNext()) {
        i.next();
        if (i.key() > m_position) {
            if (i.key() == m_keyframes.firstKey()) {
                // This is the first keyframe
                double ratio = (double)m_position / i.key();
                newKeyframe.second = i.value() * ratio;
                break;
            } else if (previous.first > -1) {
                std::pair<int,int> current = {i.key(), i.value()};
                double ratio = (double)(m_position - previous.first) / (current.first - previous.first);
                qDebug()<<"=== RATIO: "<<ratio;
                newKeyframe.second = previous.second + (qAbs(current.second - previous.second) * ratio);
                break;
            }
        }
        previous = {i.key(), i.value()};
    }
    if (newKeyframe.second == -1) {
        // We are after the last keyframe
        if (m_keyframes.isEmpty()) {
            newKeyframe.second = m_position;
        } else {
            double ratio = (double)(m_position - m_keyframes.lastKey()) / (m_duration - m_keyframes.lastKey());
            newKeyframe.second = m_keyframes.value(m_keyframes.lastKey()) + (qAbs(m_duration - m_keyframes.value(m_keyframes.lastKey())) * ratio);
        }
    }
    m_keyframes.insert(newKeyframe.first, newKeyframe.second);
    m_currentKeyframe = newKeyframe;
    std::pair<double,double> speeds = getSpeed(m_currentKeyframe);
    emit selectedKf(newKeyframe, speeds);
    emit atKeyframe(true);
    updateKeyframes();
    update();
}

void RemapView::toggleMoveNext(bool moveNext)
{
    m_moveNext = moveNext;
}

void RemapView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPalette pal = palette();
    KColorScheme scheme(pal.currentColorGroup(), KColorScheme::Window);
    m_colSelected = palette().highlight().color();
    m_colKeyframe = scheme.foreground(KColorScheme::NormalText).color();
    QColor bg = scheme.background(KColorScheme::AlternateBackground ).color();
    QStylePainter p(this);
    int maxWidth = width() - (2 * m_offset);
    m_scale = maxWidth / double(qMax(1, m_duration));
    int headOffset = m_lineHeight / 2;
    m_zoomStart = m_zoomHandle.x() * maxWidth;
    m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
    int zoomEnd = qCeil(m_zoomHandle.y() * maxWidth);
    p.fillRect(m_offset, m_lineHeight, maxWidth, m_lineHeight, bg);
    /* ticks */
    double fps = pCore->getCurrentFps();
    int displayedLength = int(m_duration / m_zoomFactor / fps);
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
    int base = int(tickOffset / frameSize);
    tickOffset = frameSize - (tickOffset - (base * frameSize));
    // Draw frame ticks
    int scaledTick = 0;
    for (int i = 0; i < maxWidth / frameSize; i++) {
        scaledTick = int(m_offset + (i * frameSize) + tickOffset);
        if (scaledTick >= maxWidth + m_offset) {
            break;
        }
        p.drawLine(QPointF(scaledTick , m_lineHeight + 1), QPointF(scaledTick, m_lineHeight - 3));
    }



    p.setPen(palette().dark().color());

    /*
     * Time-"line"
     */
    m_bottomView = height() - m_zoomHeight - m_lineHeight - 5;
    p.setPen(m_colKeyframe);
    p.drawLine(m_offset, m_lineHeight, maxWidth, m_lineHeight);
    p.drawLine(m_offset, m_bottomView, maxWidth, m_bottomView);
    p.drawLine(m_offset, m_lineHeight - headOffset / 2, m_offset, m_lineHeight + headOffset / 2);
    p.drawLine(maxWidth, m_lineHeight - headOffset / 2, maxWidth, m_lineHeight + headOffset / 2);

    /*
     * Keyframes
     */
    QMapIterator<int, int> i(m_keyframes);
    while (i.hasNext()) {
        i.next();
        double inPos = (double)i.key() * m_scale;
        double outPos = (double)i.value() * m_scale;
        if ((inPos < m_zoomStart && outPos < m_zoomStart) || (qFloor(inPos) > zoomEnd && qFloor(outPos) > zoomEnd)) {
            continue;
        }
        if ((m_hoverKeyframe.second == false && m_hoverKeyframe.first == i.key()) || (m_hoverKeyframe.second == true && m_hoverKeyframe.first == i.value())) {
            p.setPen(Qt::red);
            p.setBrush(Qt::darkRed);
        } else if (m_selectedKeyframes.contains(i.key()) || m_currentKeyframe.first == i.key() || m_currentKeyframe.second == i.value()) {
            p.setPen(m_colSelected);
            p.setBrush(m_colSelected);
        } else {
            p.setPen(m_colKeyframe);
            p.setBrush(m_colKeyframe);
        }
        inPos -= m_zoomStart;
        inPos *= m_zoomFactor;
        inPos += m_offset;
        outPos -= m_zoomStart;
        outPos *= m_zoomFactor;
        outPos += m_offset;

        p.drawLine(inPos, m_lineHeight, outPos, m_bottomView);
        p.drawLine(inPos, m_lineHeight, inPos, m_lineHeight - headOffset);
        p.drawLine(outPos, m_bottomView, outPos, m_bottomView + headOffset);
        p.drawEllipse(QRectF(inPos - headOffset / 2.0, 0, headOffset, headOffset));
        p.drawEllipse(QRectF(outPos - headOffset / 2.0, m_bottomView + headOffset, headOffset, headOffset));
    }

    /*
     * current position cursor
     */
    p.setPen(m_colSelected);
    if (m_position >= 0 && m_position < m_duration) {
        p.setBrush(m_colSelected);
        double scaledPos = m_position * m_scale;
        int cursorwidth = int(m_lineHeight / 3);
        if (scaledPos >= m_zoomStart && qFloor(scaledPos) <= zoomEnd) {
            scaledPos -= m_zoomStart;
            scaledPos *= m_zoomFactor;
            scaledPos += m_offset;
            QPolygonF position = QPolygonF() << QPointF(-cursorwidth, m_lineHeight * 0.5) << QPointF(cursorwidth, m_lineHeight * 0.5) << QPointF(0, 0);
            position.translate(scaledPos, m_lineHeight);
            p.drawPolygon(position);
        }
        int projectPos = getKeyframePosition();
        double scaledPos2 = projectPos * m_scale;
        if (scaledPos2 >= m_zoomStart && qFloor(scaledPos2) <= zoomEnd) {
            scaledPos2 -= m_zoomStart;
            scaledPos2 *= m_zoomFactor;
            scaledPos2 += m_offset;
            p.drawLine(scaledPos, m_lineHeight * 1.5, scaledPos2, m_bottomView - (m_lineHeight * 0.5));
            QPolygonF position = QPolygonF() << QPointF(-cursorwidth, 0) << QPointF(cursorwidth, 0) << QPointF(0, m_lineHeight * 0.5);
            position.translate(scaledPos2, m_bottomView - (m_lineHeight * 0.5));
            p.setBrush(m_colSelected);
            p.drawPolygon(position);
        }
    }

    // Zoom bar
    p.setPen(Qt::NoPen);
    p.setBrush(palette().mid());
    p.drawRoundedRect(0, m_bottomView + m_lineHeight + 4, width() - 2 * 0, m_zoomHeight, m_lineHeight / 3, m_lineHeight / 3);
    p.setBrush(palette().highlight());
    p.drawRoundedRect(int((width()) * m_zoomHandle.x()),
                      m_bottomView + m_lineHeight + 4,
                      int((width()) * (m_zoomHandle.y() - m_zoomHandle.x())),
                      m_zoomHeight,
                      m_lineHeight / 3, m_lineHeight / 3);
}


TimeRemap::TimeRemap(QWidget *parent)
    : QWidget(parent)
    , m_cid(-1)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);

    m_in = new TimecodeDisplay(pCore->timecode(), this);
    inLayout->addWidget(m_in);
    m_out = new TimecodeDisplay(pCore->timecode(), this);
    outLayout->addWidget(m_out);
    m_view = new RemapView(this);
    time_box->setEnabled(false);
    speed_box->setEnabled(false);
    remapLayout->addWidget(m_view);
    connect(m_view, &RemapView::selectedKf, [this](std::pair<int,int>selection, std::pair<double,double>speeds) {
        qDebug()<<"=== SELECTED KFR SPEEDS: "<<speeds;
        time_box->setEnabled(true);
        speed_box->setEnabled(true);
        QSignalBlocker bk(m_in);
        QSignalBlocker bk2(m_out);
        m_in->setEnabled(selection.first >= 0);
        m_out->setEnabled(selection.first >= 0);
        m_in->setValue(selection.first);
        m_out->setValue(selection.second);
        QSignalBlocker bk3(speedBefore);
        QSignalBlocker bk4(speedAfter);
        speedBefore->setEnabled(speeds.first > 0);
        speedBefore->setValue(100. * speeds.first);
        speedAfter->setEnabled(speeds.second > 0);
        speedAfter->setValue(100. * speeds.second);
    });
    connect(m_view, &RemapView::updateKeyframes, this, &TimeRemap::updateKeyframes);
    connect(m_in, &TimecodeDisplay::timeCodeUpdated, [this]() {
        m_view->updateInPos(m_in->getValue());
    });
    connect(m_out, &TimecodeDisplay::timeCodeUpdated, [this]() {
        m_view->updateOutPos(m_out->getValue());
    });
    connect(m_view, &RemapView::atKeyframe, button_add, [&](bool atKeyframe) {
        button_add->setIcon(atKeyframe ? QIcon::fromTheme(QStringLiteral("list-remove")) : QIcon::fromTheme(QStringLiteral("list-add")));
    });
    connect(speedBefore, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [&](double speed) {
        m_view->updateBeforeSpeed(speed);
    });
    connect(speedAfter, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [&](double speed) {
        m_view->updateAfterSpeed(speed);
    });
    connect(button_add, &QToolButton::clicked, m_view, &RemapView::addKeyframe);
    connect(button_next, &QToolButton::clicked, m_view, &RemapView::goNext);
    connect(button_prev, &QToolButton::clicked, m_view, &RemapView::goPrev);
    connect(move_next, &QCheckBox::toggled, m_view, &RemapView::toggleMoveNext);
    connect(m_view, &RemapView::updateMaxDuration, [this](int duration) {
        m_out->setRange(m_out->minimum(), duration);
        m_in->setRange(m_in->minimum(), duration);
    });
    setEnabled(false);
}

void TimeRemap::selectedClip(int cid, int splitId)
{
    if (cid == -1 && cid == m_cid) {
        return;
    }
    QObject::disconnect( m_seekConnection1 );
    QObject::disconnect( m_seekConnection2 );
    QObject::disconnect( m_seekConnection3 );
    m_cid = cid;
    m_splitId = splitId;
    if (cid == -1) {
        m_view->setDuration(nullptr, 0);
        setEnabled(false);
        return;
    }
    m_view->m_remapLink.reset();
    bool keyframesLoaded = false;
    std::shared_ptr<TimelineItemModel> model = pCore->window()->getCurrentTimeline()->controller()->getModel();
    const QString binId = pCore->getTimelineClipBinId(cid);
    int min = pCore->getItemIn({ObjectType::TimelineClip,cid});
    m_lastLength = pCore->getItemDuration({ObjectType::TimelineClip,cid});
    int max = min + m_lastLength;
    pCore->selectBinClip(binId, true, min, {min,max});
    m_view->m_startPos = pCore->getItemPosition({ObjectType::TimelineClip,cid});
    m_in->setRange(min, max);
    m_out->setRange(min, max);
    std::shared_ptr<Mlt::Producer> prod = model->getClipProducer(cid);
    m_view->setDuration(prod, max - min);
    qDebug()<<"===== GOT PRODUCER TYPE: "<<prod->parent().type();
    if (prod->parent().type() == mlt_service_chain_type) {
        Mlt::Chain fromChain(prod->parent());
        int count = fromChain.link_count();
        for (int i = 0; i < count; i++) {
            QScopedPointer<Mlt::Link> fromLink(fromChain.link(i));
            if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
                if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                    // Found a timeremap effect, read params
                    m_view->m_remapLink = std::make_shared<Mlt::Link>(fromChain.link(i)->get_link());
                    if (m_splitId > -1) {
                        std::shared_ptr<Mlt::Producer> prod2 = model->getClipProducer(m_splitId);
                        if (prod2->parent().type() == mlt_service_chain_type) {
                            Mlt::Chain fromChain2(prod2->parent());
                            count = fromChain2.link_count();
                            for (int j = 0; j < count; j++) {
                                QScopedPointer<Mlt::Link> fromLink2(fromChain2.link(j));
                                if (fromLink2 && fromLink2->is_valid() && fromLink2->get("mlt_service")) {
                                    if (fromLink2->get("mlt_service") == QLatin1String("timeremap")) {
                                        m_splitRemap = std::make_shared<Mlt::Link>(fromChain2.link(j)->get_link());
                                    }
                                }
                            }
                        }
                    }
                    QString mapData(fromLink->get("map"));
                    qDebug()<<"==== LOADING KEYFRAMES FROM MAP: "<<mapData;
                    m_view->loadKeyframes(mapData);
                    keyframesLoaded = true;
                    setEnabled(true);
                    break;
                }
            }
        }
    }
    m_seekConnection3 = connect(pCore->getMonitor(Kdenlive::ClipMonitor), &Monitor::seekPosition, [this](int pos) {
        m_view->slotSetPosition(pos);
    });
    m_seekConnection1 = connect(m_view, &RemapView::seekToPos, [this](int pos) {
        pCore->getMonitor(Kdenlive::ClipMonitor)->requestSeek(pos);
    });
    connect(m_view, &RemapView::seekToPos2, [this](int pos) {
        pCore->getMonitor(Kdenlive::ProjectMonitor)->requestSeek(pos + m_view->m_startPos);
    });
    m_seekConnection2 = connect(pCore->getMonitor(Kdenlive::ProjectMonitor), &Monitor::seekPosition, [this](int pos) {
        qDebug()<<"=== PROJECT SEEK: "<<pos<<", START: "<<m_view->m_startPos<<", MAPPED: "<<GenTime(m_view->m_remapLink->anim_get_double("map", pos - m_view->m_startPos)).frames(pCore->getCurrentFps());
        m_view->slotSetPosition(GenTime(m_view->m_remapLink->anim_get_double("map", pos - m_view->m_startPos)).frames(pCore->getCurrentFps()));
    });
}

void TimeRemap::setClip(std::shared_ptr<ProjectClip> clip, int in, int out)
{
    QObject::disconnect( m_seekConnection1 );
    QObject::disconnect( m_seekConnection2 );
    QObject::disconnect( m_seekConnection3 );
    m_cid = -1;
    if (!clip->statusReady() || clip->clipType() != ClipType::Playlist) {
        qDebug()<<"===== CLIP NOT READY; TYPE; "<<clip->clipType();
        m_view->setDuration(nullptr, 0);
        setEnabled(false);
        return;
    }
    m_view->m_remapLink.reset();
    bool keyframesLoaded = false;
    if (clip != nullptr) {
        int min = in == -1 ? 0 : in;
        int max = out == -1 ? clip->getFramePlaytime() : out;
        m_in->setRange(min, max);
        m_out->setRange(min, max);
        m_view->m_startPos = 0;
        m_view->setBinClipDuration(clip, max - min);
        if (clip->clipType() == ClipType::Playlist) {
            Mlt::Service service(clip->originalProducer()->producer()->get_service());
            qDebug()<<"==== producer type: "<<service.type();
            if (service.type() == mlt_service_multitrack_type) {
                Mlt::Multitrack multi(service);
                for (int i = 0; i < multi.count(); i++) {
                    std::unique_ptr<Mlt::Producer> track(multi.track(i));
                    qDebug()<<"==== GOT TRACK TYPE: "<<track->type();
                    switch (track->type()) {
                        case mlt_service_chain_type: {
                            Mlt::Chain fromChain(*track.get());
                            int count = fromChain.link_count();
                            for (int i = 0; i < count; i++) {
                                QScopedPointer<Mlt::Link> fromLink(fromChain.link(i));
                                if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
                                    if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                                        // Found a timeremap effect, read params
                                        m_view->m_remapLink = std::make_shared<Mlt::Link>(fromChain.link(i)->get_link());
                                        QString mapData(fromLink->get("map"));
                                        m_view->loadKeyframes(mapData);
                                        keyframesLoaded = true;
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                        case mlt_service_playlist_type: {
                            // that is a single track
                            Mlt::Playlist local_playlist(*track);
                            int max = local_playlist.count();
                            qDebug()<<"==== PLAYLIST COUNT: "<<max;
                            if (max == 1) {
                                Mlt::Producer prod = local_playlist.get_clip(0)->parent();
                                qDebug()<<"==== GOT PROD TYPE: "<<prod.type()<<" = "<<prod.get("mlt_service")<<" = "<<prod.get("resource");
                                if (prod.type() == mlt_service_chain_type) {
                                    Mlt::Chain fromChain(prod);
                                    int count = fromChain.link_count();
                                    for (int i = 0; i < count; i++) {
                                        QScopedPointer<Mlt::Link> fromLink(fromChain.link(i));
                                        if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
                                            if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                                                // Found a timeremap effect, read params
                                                m_view->m_remapLink = std::make_shared<Mlt::Link>(fromChain.link(i)->get_link());
                                                QString mapData(fromLink->get("map"));
                                                m_view->loadKeyframes(mapData);
                                                keyframesLoaded = true;
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                            break;
                        }
                        default:
                            qDebug()<<"=== UNHANDLED TRACK TYPE";
                            break;
                    }
                }
            }
        }
        if (!keyframesLoaded) {
            m_view->loadKeyframes(QString());
        }
        m_seekConnection1 = connect(m_view, &RemapView::seekToPos, pCore->getMonitor(Kdenlive::ClipMonitor), &Monitor::requestSeek, Qt::UniqueConnection);
        m_seekConnection2 = connect(pCore->getMonitor(Kdenlive::ClipMonitor), &Monitor::seekPosition, [&](int pos) {
            m_view->slotSetPosition(pos);
        });
        setEnabled(m_view->m_remapLink != nullptr);
    } else {
        setEnabled(false);
    }
}

void TimeRemap::updateKeyframes()
{
    QString kfData = m_view->getKeyframesData();
    if (m_view->m_remapLink) {
        m_view->m_remapLink->set("map", kfData.toUtf8().constData());
        if (m_splitRemap) {
            m_splitRemap->set("map", kfData.toUtf8().constData());
        }
        if (m_cid == -1) {
            // This is a playlist clip
            m_view->timer.start();
        } else if (m_lastLength != m_view->remapDuration()) {
            // Resize timeline clip
            m_lastLength = m_view->remapDuration();
            std::shared_ptr<TimelineItemModel> model = pCore->window()->getCurrentTimeline()->controller()->getModel();
            model->requestItemResize(m_cid, m_lastLength, true, true, -1, false);
        }
    }
}

TimeRemap::~TimeRemap()
{
    //delete m_previewTimer;
}


