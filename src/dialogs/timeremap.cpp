/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "timeremap.h"

#include "bin/projectclip.h"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "doc/kthumb.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "profiles/profilemodel.hpp"
#include "project/projectmanager.h"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/view/timelinecontroller.h"
#include "timeline2/view/timelinewidget.h"
#include "widgets/timecodedisplay.h"

#include "kdenlive_debug.h"
#include <QFontDatabase>
#include <QStylePainter>
#include <QWheelEvent>
#include <QtMath>

#include "klocalizedstring.h"
#include <KColorScheme>

RemapView::RemapView(QWidget *parent)
    : QWidget(parent)
    , m_inFrame(0)
    , m_duration(1)
    , m_position(0)
    , m_bottomPosition(0)
    , m_scale(1.)
    , m_zoomFactor(1)
    , m_zoomStart(0)
    , m_zoomHandle(0, 1)
    , m_clip(nullptr)
    , m_service(nullptr)
    , m_moveKeyframeMode(NoMove)
    , m_clickPoint(-1)
    , m_moveNext(true)
{
    setMouseTracking(true);
    setMinimumSize(QSize(150, 80));
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    int size = QFontInfo(font()).pixelSize() * 3;
    setFixedHeight(size * 4);
    // Reference height of the rulers
    m_lineHeight = int(size / 2.);
    // Height of the zoom bar
    m_zoomHeight = m_lineHeight * 0.5;
    // Center of the view
    m_centerPos = (size * 4 - m_zoomHeight - 2) / 2 - 1;
    m_offset = qCeil(m_lineHeight / 4);
    // Bottom of the view (just above zoombar)
    m_bottomView = height() - m_zoomHeight - 2;
    setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
    int maxWidth = width() - (2 * m_offset);
    m_scale = 1.;
    m_zoomStart = m_zoomHandle.x() * maxWidth;
    m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
    timer.setInterval(500);
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, &RemapView::reloadProducer);
}

bool RemapView::isInRange() const
{
    return m_bottomPosition != -1;
}

void RemapView::updateInPos(int pos)
{
    if (m_currentKeyframe.second > -1) {
        m_keyframesOrigin = m_keyframes;
        if (m_moveNext) {
            int offset = pos - m_currentKeyframe.second;
            QMap<int, int>::iterator it = m_keyframes.find(m_currentKeyframe.first);
            while (it != m_keyframes.end()) {
                m_keyframes.insert(it.key(), it.value() + offset);
                it++;
            }
            m_currentKeyframe.second = pos;
        } else {
            m_currentKeyframe.second = pos;
            m_keyframes.insert(m_currentKeyframe.first, m_currentKeyframe.second);
        }
        slotSetPosition(pos);
        std::pair<double, double> speeds = getSpeed(m_currentKeyframe);
        Q_EMIT updateSpeeds(speeds);
        Q_EMIT updateKeyframesWithUndo(m_keyframes, m_keyframesOrigin);
        update();
    }
}

void RemapView::updateOutPos(int pos)
{
    if (m_currentKeyframe.first > -1) {
        if (m_keyframes.contains(pos)) {
            // Cannot move kfr over an existing one
            qDebug() << "==== KEYFRAME ALREADY EXISTS AT: " << pos;
            return;
        }
        m_keyframesOrigin = m_keyframes;
        QMap<int, int> updated;
        int offset = pos - m_currentKeyframe.first;
        if (m_moveNext) {
            QMap<int, int>::iterator it = m_keyframes.find(m_currentKeyframe.first);
            while (it != m_keyframes.end()) {
                updated.insert(it.key(), it.value());
                it++;
            }
            m_currentKeyframe.first = pos;
        } else {
            m_keyframes.remove(m_currentKeyframe.first);
            m_currentKeyframe.first = pos;
            m_keyframes.insert(m_currentKeyframe.first, m_currentKeyframe.second);
        }
        m_selectedKeyframes = {m_currentKeyframe};
        QMapIterator<int, int> i(updated);
        while (i.hasNext()) {
            i.next();
            m_keyframes.remove(i.key());
        }
        i.toFront();
        while (i.hasNext()) {
            i.next();
            m_keyframes.insert(i.key() + offset, i.value());
        }
        m_bottomPosition = pos - m_inFrame;
        std::pair<double, double> speeds = getSpeed(m_currentKeyframe);
        Q_EMIT updateSpeeds(speeds);
        Q_EMIT updateKeyframesWithUndo(m_keyframes, m_keyframesOrigin);
        update();
    }
}

int RemapView::remapDuration() const
{
    int maxDuration = 0;
    if (m_keyframes.isEmpty()) {
        return 0;
    }
    QMapIterator<int, int> i(m_keyframes);
    while (i.hasNext()) {
        i.next();
        if (i.key() > maxDuration) {
            maxDuration = i.key();
        }
    }
    return maxDuration - m_inFrame + 1;
}

int RemapView::remapMax() const
{
    int maxDuration = 0;
    if (m_keyframes.isEmpty()) {
        return 0;
    }
    QMapIterator<int, int> i(m_keyframes);
    while (i.hasNext()) {
        i.next();
        if (i.value() > maxDuration) {
            maxDuration = i.value();
        }
        if (i.key() > maxDuration) {
            maxDuration = i.key();
        }
    }
    return maxDuration - m_inFrame + 1;
}

bool RemapView::movingKeyframe() const
{
    return m_moveKeyframeMode == BottomMove;
}

void RemapView::setBinClipDuration(std::shared_ptr<ProjectClip> clip, int duration)
{
    m_clip = clip;
    m_service = clip->originalProducer();
    m_duration = duration;
    int maxWidth = width() - (2 * m_offset);
    m_scale = maxWidth / double(qMax(1, remapMax()));
    m_currentKeyframe = m_currentKeyframeOriginal = {-1, -1};
}

void RemapView::setDuration(std::shared_ptr<Mlt::Producer> service, int duration, int sourceDuration)
{
    m_clip.reset();
    m_sourceDuration = sourceDuration;
    if (duration < 0) {
        // reset
        m_service.reset();
        m_inFrame = 0;
        m_duration = -1;
        m_selectedKeyframes.clear();
        m_keyframes.clear();
        return;
    }
    if (service) {
        m_service = service;
        m_inFrame = 0;
        m_duration = -1;
    }
    bool keyframeAdded = false;
    if (m_duration > 0 && m_service && !m_keyframes.isEmpty()) {
        m_keyframesOrigin = m_keyframes;
        if (duration > m_duration) {
            // The clip was resized, ensure we have a keyframe at the end of the clip will freeze at last keyframe
            QMap<int, int>::iterator it = m_keyframes.end();
            it--;
            int lastKeyframePos = it.key();
            int lastKeyframeValue = it.value();
            int lastPos = m_inFrame + duration - 1;
            if (lastPos > lastKeyframePos) {
                keyframeAdded = true;
                std::pair<double, double> speeds = getSpeed({lastKeyframePos, lastKeyframeValue});
                // Move last keyframe
                it--;
                int updatedVal = it.value() + ((lastPos - it.key()) * speeds.first);
                m_keyframes.remove(lastKeyframePos);
                m_keyframes.insert(lastPos, updatedVal);
            }
        } else if (duration < m_duration) {
            keyframeAdded = true;
            // Remove all Keyframes after duration
            int lastPos = m_inFrame + duration - 1;
            QList<int> toDelete;
            QMapIterator<int, int> i(m_keyframes);
            while (i.hasNext()) {
                i.next();
                if (i.key() > duration + m_inFrame) {
                    toDelete << i.key();
                }
            }
            if (!toDelete.isEmpty()) {
                // Move last keyframe to end pos
                int posToMove = toDelete.takeFirst();
                if (!m_keyframes.contains(lastPos)) {
                    int lastKeyframeValue = m_keyframes.value(posToMove);
                    std::pair<double, double> speeds = getSpeed({posToMove, lastKeyframeValue});
                    m_keyframes.remove(posToMove);
                    while (!toDelete.isEmpty()) {
                        m_keyframes.remove(toDelete.takeFirst());
                    }
                    int updatedVal = m_keyframes.value(m_keyframes.lastKey()) + ((lastPos - m_keyframes.lastKey()) / speeds.first);
                    m_keyframes.insert(lastPos, updatedVal);
                }
            }
        }
        if (m_keyframes != m_keyframesOrigin) {
            Q_EMIT updateKeyframesWithUndo(m_keyframes, m_keyframesOrigin);
        }
    }
    if (service == nullptr) {
        // We are updating an existing remap effect due to duration change
        m_duration = qMax(duration, remapMax());
    } else {
        m_duration = duration;
        m_inFrame = m_service->get_in();
        m_originalRange = {m_inFrame, duration + m_inFrame};
    }
    int maxWidth = width() - (2 * m_offset);
    m_scale = maxWidth / double(qMax(1, remapMax() - 1));
    m_zoomStart = m_zoomHandle.x() * maxWidth;
    m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
    if (!m_keyframes.contains(m_currentKeyframe.first)) {
        m_currentKeyframe = m_currentKeyframeOriginal = {-1, -1};
        update();
    }
    if (keyframeAdded) {
        Q_EMIT updateKeyframes(false);
    }
}

void RemapView::loadKeyframes(const QString &mapData)
{
    m_keyframes.clear();
    if (mapData.isEmpty()) {
        if (m_inFrame > 0) {
            // Necessary otherwise clip will freeze before first keyframe
            m_keyframes.insert(0, 0);
        }
        m_currentKeyframe = {m_inFrame, m_inFrame};
        m_keyframes.insert(m_currentKeyframe.first, m_currentKeyframe.second);
        m_keyframes.insert(m_inFrame + m_duration - 1, m_inFrame + m_duration - 1);
        std::pair<double, double> speeds = getSpeed(m_currentKeyframe);
        std::pair<bool, bool> atEnd = {m_currentKeyframe.first == m_inFrame, m_currentKeyframe.first == m_keyframes.lastKey()};
        Q_EMIT selectedKf(m_currentKeyframe, speeds, atEnd);
        Q_EMIT atKeyframe(true, true);
        Q_EMIT updateKeyframes(false);
    } else {
        QStringList str = mapData.split(QLatin1Char(';'));
        for (auto &s : str) {
            int pos = m_service->time_to_frames(s.section(QLatin1Char('='), 0, 0).toUtf8().constData());
            int val = GenTime(s.section(QLatin1Char('='), 1).toDouble()).frames(pCore->getCurrentFps());
            // HACK: we always set last keyframe 1 frame after in MLT to ensure we have a correct last frame
            if (s == str.constLast()) {
                pos--;
            }
            m_keyframes.insert(pos, val);
            m_duration = qMax(m_duration, pos - m_inFrame);
            m_duration = qMax(m_duration, val - m_inFrame);
        }
        bool isKfr = m_keyframes.contains(m_bottomPosition + m_inFrame);
        if (isKfr) {
            bool isLast = m_bottomPosition + m_inFrame == m_keyframes.firstKey() || m_bottomPosition + m_inFrame == m_keyframes.lastKey();
            Q_EMIT atKeyframe(isKfr, isLast);
        } else {
            Q_EMIT atKeyframe(false, false);
        }
        if (m_keyframes.contains(m_currentKeyframe.first)) {
            // bool isLast = m_currentKeyframe.first == m_keyframes.firstKey() || m_currentKeyframe.first == m_keyframes.lastKey();
            // Q_EMIT atKeyframe(true, isLast);
            std::pair<double, double> speeds = getSpeed(m_currentKeyframe);
            std::pair<bool, bool> atEnd = {m_currentKeyframe.first == m_inFrame, m_currentKeyframe.first == m_keyframes.lastKey()};
            Q_EMIT selectedKf(m_currentKeyframe, speeds, atEnd);
        } else {
            m_currentKeyframe = {-1, -1};
            Q_EMIT selectedKf(m_currentKeyframe, {-1, -1});
        }
    }
    int maxWidth = width() - (2 * m_offset);
    m_scale = maxWidth / double(qMax(1, remapMax()));
    m_zoomStart = m_zoomHandle.x() * maxWidth;
    m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
    Q_EMIT updateMaxDuration();
    update();
}

void RemapView::mouseMoveEvent(QMouseEvent *event)
{
    event->accept();
    // Check for start/end snapping on drag
    int pos = -1;
    if (m_moveKeyframeMode == BottomMove || m_moveKeyframeMode == TopMove) {
        double snapPos = ((m_originalRange.first - m_inFrame) * m_scale) - m_zoomStart;
        snapPos *= m_zoomFactor;
        snapPos += m_offset;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        if (qAbs(snapPos - event->x()) < QApplication::startDragDistance()) {
#else
        if (qAbs(snapPos - event->position().x()) < QApplication::startDragDistance()) {
#endif
            pos = m_originalRange.first - m_inFrame;
        } else {
            snapPos = ((m_originalRange.second - m_inFrame) * m_scale) - m_zoomStart;
            snapPos *= m_zoomFactor;
            snapPos += m_offset;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            if (qAbs(snapPos - event->x()) < QApplication::startDragDistance()) {
#else
            if (qAbs(snapPos - event->position().x()) < QApplication::startDragDistance()) {
#endif
                pos = m_originalRange.second - m_inFrame;
            }
        }
        if (pos == -1) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            pos = int(((event->x() - m_offset) / m_zoomFactor + m_zoomStart) / m_scale);
#else
            pos = int(((event->position().x() - m_offset) / m_zoomFactor + m_zoomStart) / m_scale);
#endif
        }
    } else {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        pos = int(((event->x() - m_offset) / m_zoomFactor + m_zoomStart) / m_scale);
#else
        pos = int(((event->position().x() - m_offset) / m_zoomFactor + m_zoomStart) / m_scale);
#endif
    }
    pos = qBound(0, pos, m_maxLength - 1);
    GenTime position(pos, pCore->getCurrentFps());
    if (event->buttons() == Qt::NoButton) {
        bool hoverKeyframe = false;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        if (event->y() < 2 * m_lineHeight && event->y() > m_lineHeight) {
#else
        if (event->position().y() < 2 * m_lineHeight && event->position().y() > m_lineHeight) {
#endif
            // mouse move in top keyframes area
            std::pair<int, int> keyframe = getClosestKeyframe(pos + m_inFrame);
            if (keyframe.first > -1 && qAbs(keyframe.second - pos - m_inFrame) * m_scale * m_zoomFactor <= m_lineHeight / 2) {
                hoverKeyframe = true;
            }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        } else if (event->y() > m_bottomView - 2 * m_lineHeight && event->y() < m_bottomView - m_lineHeight) {
#else
        } else if (event->position().y() > m_bottomView - 2 * m_lineHeight && event->position().y() < m_bottomView - m_lineHeight) {
#endif
            // move in bottom keyframe area
            std::pair<int, int> keyframe = getClosestKeyframe(pos + m_inFrame, true);
            if (keyframe.first > -1 && qAbs(keyframe.first - pos - m_inFrame) * m_scale * m_zoomFactor <= m_lineHeight / 2) {
                hoverKeyframe = true;
            }
        }
        if (hoverKeyframe) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    } else if ((event->buttons() & Qt::LeftButton) != 0u) {
        if (m_hoverZoomIn || m_hoverZoomOut || m_hoverZoom) {
            // Moving zoom handles
            if (m_hoverZoomIn) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                m_zoomHandle.setX(qMin(qMax(0., double(event->x() - m_offset) / (width() - 2 * m_offset)), m_zoomHandle.y() - 0.015));
#else
                m_zoomHandle.setX(qMin(qMax(0., double(event->position().x() - m_offset) / (width() - 2 * m_offset)), m_zoomHandle.y() - 0.015));
#endif
                int maxWidth = width() - (2 * m_offset);
                m_zoomStart = m_zoomHandle.x() * maxWidth;
                m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
                update();
                return;
            }
            if (m_hoverZoomOut) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                m_zoomHandle.setY(qMax(qMin(1., double(event->x() - m_offset) / (width() - 2 * m_offset)), m_zoomHandle.x() + 0.015));
#else
                m_zoomHandle.setY(qMax(qMin(1., double(event->position().x() - m_offset) / (width() - 2 * m_offset)), m_zoomHandle.x() + 0.015));
#endif
                int maxWidth = width() - (2 * m_offset);
                m_zoomStart = m_zoomHandle.x() * maxWidth;
                m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
                update();
                return;
            }
            // moving zoom zone
            if (m_hoverZoom) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                double clickOffset = (double(event->x()) - m_offset) / (width() - 2 * m_offset) - m_clickOffset;
#else
                double clickOffset = (double(event->position().x()) - m_offset) / (width() - 2 * m_offset) - m_clickOffset;
#endif
                double newX = m_zoomHandle.x() + clickOffset;
                if (newX < 0) {
                    clickOffset = -m_zoomHandle.x();
                    newX = 0;
                }
                double newY = m_zoomHandle.y() + clickOffset;
                if (newY > 1) {
                    clickOffset = 1 - m_zoomHandle.y();
                    newY = 1;
                    newX = m_zoomHandle.x() + clickOffset;
                }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                m_clickOffset = (double(event->x()) - m_offset) / (width() - 2 * m_offset);
#else
                m_clickOffset = (double(event->position().x()) - m_offset) / (width() - 2 * m_offset);
#endif
                m_zoomHandle = QPointF(newX, newY);
                int maxWidth = width() - (2 * m_offset);
                m_zoomStart = m_zoomHandle.x() * maxWidth;
                m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
                update();
            }
            return;
        }
        // qDebug()<<"=== MOVING MOUSE: "<<pos<<" = "<<m_currentKeyframe<<", MOVE KFMODE: "<<m_moveKeyframeMode;
        if ((m_currentKeyframe.second == pos + m_inFrame && m_moveKeyframeMode == TopMove) ||
            (m_currentKeyframe.first == pos + m_inFrame && m_moveKeyframeMode == BottomMove)) {
            return;
        }
        if (m_currentKeyframe.first >= 0 && m_moveKeyframeMode != NoMove) {
            if (m_moveKeyframeMode == BottomMove) {
                // Moving bottom keyframe
                if (!m_keyframes.contains(pos + m_inFrame)) {
                    int delta = pos + m_inFrame - m_currentKeyframe.first;
                    // Check that the move is possible
                    QMapIterator<int, int> i(m_selectedKeyframes);
                    while (i.hasNext()) {
                        i.next();
                        int updatedPos = i.key() + delta;
                        if (!m_selectedKeyframes.contains(updatedPos) && m_keyframes.contains(updatedPos)) {
                            // Don't allow moving over another keyframe
                            qDebug() << "== MOVE ABORTED; OVERLAPPING EXISTING";
                            return;
                        }
                    }
                    i.toFront();
                    QMap<int, int> updated;
                    while (i.hasNext()) {
                        i.next();
                        if (i.key() < m_currentKeyframe.first) {
                            continue;
                        }
                        // qDebug()<<"=== MOVING KFR: "<<i.key()<<" > "<<(i.key() + delta);
                        // m_keyframes.insert(i.key() + delta, i.value());
                        updated.insert(i.key() + delta, i.value());
                        m_keyframes.remove(i.key());
                    }
                    QMapIterator<int, int> j(updated);
                    while (j.hasNext()) {
                        j.next();
                        m_keyframes.insert(j.key(), j.value());
                    }
                    QMap<int, int> updatedSelection;
                    QMapIterator<int, int> k(m_previousSelection);
                    while (k.hasNext()) {
                        k.next();
                        updatedSelection.insert(k.key() + delta, k.value());
                    }
                    m_previousSelection = updatedSelection;
                    m_currentKeyframe.first += delta;
                    std::pair<double, double> speeds = getSpeed(m_currentKeyframe);
                    std::pair<bool, bool> atEnd = {m_currentKeyframe.first == m_inFrame, m_currentKeyframe.first == m_keyframes.lastKey()};
                    Q_EMIT selectedKf(m_currentKeyframe, speeds, atEnd);

                    m_selectedKeyframes = updated;
                    Q_EMIT seekToPos(-1, pos);
                    Q_EMIT updateKeyframes(false);
                    if (remapMax() > m_lastMaxDuration) {
                        m_lastMaxDuration = remapMax();
                        int maxWidth = width() - (2 * m_offset);
                        m_scale = maxWidth / double(qMax(1, m_lastMaxDuration));
                        m_zoomStart = m_zoomHandle.x() * maxWidth;
                        m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
                    }
                    update();
                    return;
                } else {
                    qDebug() << "=== KEYFRAME :" << pos << " ALREADY EXISTS";
                }
            } else if (m_moveKeyframeMode == TopMove) {
                // Moving top keyframe
                int realPos = qMax(m_inFrame, pos + m_inFrame);
                int delta = realPos - m_currentKeyframe.second;
                // Check that the move is possible
                QMapIterator<int, int> i(m_selectedKeyframes);
                while (i.hasNext()) {
                    i.next();
                    if (i.value() + delta >= m_sourceDuration) {
                        delta = qMin(delta, m_sourceDuration - i.value() - 1);
                        realPos = m_currentKeyframe.second + delta;
                        pos = realPos - m_inFrame;
                        if (delta == 0) {
                            pCore->displayMessage(i18n("Cannot move last source keyframe past clip end"), MessageType::ErrorMessage, 500);
                            return;
                        }
                    }
                    if (i.value() + delta < 0) {
                        delta = qMax(delta, -i.value());
                        realPos = m_currentKeyframe.second + delta;
                        pos = realPos - m_inFrame;
                        if (delta == 0) {
                            pCore->displayMessage(i18n("Cannot move first source keyframe before clip start"), MessageType::ErrorMessage, 500);
                            return;
                        }
                    }
                }
                i.toFront();
                QMap<int, int> updated;
                while (i.hasNext()) {
                    i.next();
                    m_keyframes.insert(i.key(), i.value() + delta);
                    updated.insert(i.key(), i.value() + delta);
                    if (i.value() == m_currentKeyframe.second) {
                        m_currentKeyframe.second = realPos;
                    }
                }
                QMap<int, int> updatedSelection;
                QMapIterator<int, int> k(m_previousSelection);
                while (k.hasNext()) {
                    k.next();
                    updatedSelection.insert(k.key(), k.value() + delta);
                }
                m_previousSelection = updatedSelection;
                std::pair<double, double> speeds = getSpeed(m_currentKeyframe);
                std::pair<bool, bool> atEnd = {m_currentKeyframe.first == m_inFrame, m_currentKeyframe.first == m_keyframes.lastKey()};
                Q_EMIT selectedKf(m_currentKeyframe, speeds, atEnd);
                m_selectedKeyframes = updated;
                slotSetPosition(pos + m_inFrame);
                Q_EMIT seekToPos(pos + m_inFrame, -1);
                Q_EMIT updateKeyframes(false);
                if (remapMax() > m_lastMaxDuration) {
                    m_lastMaxDuration = remapMax();
                    int maxWidth = width() - (2 * m_offset);
                    m_scale = maxWidth / double(qMax(1, m_lastMaxDuration));
                    m_zoomStart = m_zoomHandle.x() * maxWidth;
                    m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
                }
                update();
                return;
            }
        }
        // Rubberband selection
        if (m_clickPoint >= 0) {
            m_clickEnd = pos + m_inFrame;
            int min = qMin(m_clickPoint, m_clickEnd);
            int max = qMax(m_clickPoint, m_clickEnd);
            min = qMax(1, min);
            m_selectedKeyframes.clear();
            m_currentKeyframeOriginal = m_currentKeyframe = {-1, -1};
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

        if (m_moveKeyframeMode == CursorMove) {
            if (pos != m_position) {
                slotSetPosition(pos + m_inFrame);
                Q_EMIT seekToPos(pos + m_inFrame, -1);
            }
        }
        if (m_moveKeyframeMode == CursorMoveBottom) {
            pos = qMin(pos, m_keyframes.lastKey() - m_inFrame);
            if (pos != m_bottomPosition) {
                m_bottomPosition = pos;
                bool isKfr = m_keyframes.contains(m_bottomPosition + m_inFrame);
                if (isKfr) {
                    bool isLast = m_bottomPosition + m_inFrame == m_keyframes.firstKey() || m_bottomPosition + m_inFrame == m_keyframes.lastKey();
                    Q_EMIT atKeyframe(isKfr, isLast);
                } else {
                    Q_EMIT atKeyframe(false, false);
                }
                pos = GenTime(m_remapProps.anim_get_double("time_map", pos + m_inFrame)).frames(pCore->getCurrentFps());
                slotSetPosition(pos);
                Q_EMIT seekToPos(-1, m_bottomPosition);
            }
        }
        return;
    }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (event->y() > m_bottomView) {
        // Moving in zoom area
        if (qAbs(event->x() - m_offset - (m_zoomHandle.x() * (width() - 2 * m_offset))) < QApplication::startDragDistance()) {
#else
    if (event->position().y() > m_bottomView) {
        // Moving in zoom area
        if (qAbs(event->position().x() - m_offset - (m_zoomHandle.x() * (width() - 2 * m_offset))) < QApplication::startDragDistance()) {
#endif
            setCursor(Qt::SizeHorCursor);
            m_hoverZoomIn = true;
            m_hoverZoomOut = false;
            m_hoverZoom = false;
            update();
            return;
        }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        if (qAbs(event->x() - m_offset - (m_zoomHandle.y() * (width() - 2 * m_offset))) < QApplication::startDragDistance()) {
#else
        if (qAbs(event->position().x() - m_offset - (m_zoomHandle.y() * (width() - 2 * m_offset))) < QApplication::startDragDistance()) {
#endif
            setCursor(Qt::SizeHorCursor);
            m_hoverZoomOut = true;
            m_hoverZoomIn = false;
            m_hoverZoom = false;
            update();
            return;
        }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        if (m_zoomHandle != QPointF(0, 1) && event->x() > m_offset + (m_zoomHandle.x() * (width() - 2 * m_offset)) &&
            event->x() < m_offset + (m_zoomHandle.y() * (width() - 2 * m_offset))) {
#else
        if (m_zoomHandle != QPointF(0, 1) && event->position().x() > m_offset + (m_zoomHandle.x() * (width() - 2 * m_offset)) &&
            event->position().x() < m_offset + (m_zoomHandle.y() * (width() - 2 * m_offset))) {
#endif
            setCursor(Qt::PointingHandCursor);
            m_hoverZoom = true;
            m_hoverZoomIn = false;
            m_hoverZoomOut = false;
            update();
            return;
        }
    }

    if (m_hoverZoomOut || m_hoverZoomIn || m_hoverZoom) {
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

void RemapView::centerCurrentKeyframe()
{
    if (m_currentKeyframe.first == -1) {
        // No keyframe selected, abort
        return;
    }
    QMap<int, int> nextKeyframes;
    if (m_moveNext) {
        QMap<int, int>::iterator it = m_keyframes.find(m_currentKeyframe.first);
        if (it != m_keyframes.end() && *it != m_keyframes.last()) {
            it++;
            while (it != m_keyframes.end()) {
                nextKeyframes.insert(it.key(), it.value());
                it++;
            }
        }
    }
    m_keyframes.remove(m_currentKeyframe.first);
    int offset = m_bottomPosition + m_inFrame - m_currentKeyframe.first;
    m_currentKeyframe.first = m_bottomPosition + m_inFrame;
    if (offset == 0) {
        // no move
        return;
    }
    m_keyframes.insert(m_currentKeyframe.first, m_currentKeyframe.second);
    QMapIterator<int, int> i(nextKeyframes);
    while (i.hasNext()) {
        i.next();
        m_keyframes.remove(i.key());
    }
    i.toFront();
    while (i.hasNext()) {
        i.next();
        m_keyframes.insert(i.key() + offset, i.value());
    }
    std::pair<double, double> speeds = getSpeed(m_currentKeyframe);
    std::pair<bool, bool> atEnd = {m_currentKeyframe.first == m_inFrame, m_currentKeyframe.first == m_keyframes.lastKey()};
    Q_EMIT selectedKf(m_currentKeyframe, speeds, atEnd);
    bool isLast = m_currentKeyframe.first == m_keyframes.firstKey() || m_currentKeyframe.first == m_keyframes.lastKey();
    Q_EMIT atKeyframe(true, isLast);
    Q_EMIT updateKeyframes(true);
    update();
}

void RemapView::centerCurrentTopKeyframe()
{
    if (m_currentKeyframe.first == -1) {
        // No keyframe selected, abort
        return;
    }
    // std::pair<int,int> range = getRange(m_currentKeyframe);
    QMap<int, int> nextKeyframes;
    int offset = m_position + m_inFrame - m_currentKeyframe.second;
    if (m_moveNext) {
        QMap<int, int>::iterator it = m_keyframes.find(m_currentKeyframe.first);
        if (it != m_keyframes.end() && *it != m_keyframes.last()) {
            it++;
            while (it != m_keyframes.end()) {
                nextKeyframes.insert(it.key(), it.value());
                // Check that the move is possible
                if (it.value() + offset >= m_sourceDuration) {
                    pCore->displayMessage(i18n("Cannot move last source keyframe past clip end"), MessageType::ErrorMessage, 500);
                    return;
                }
                if (it.value() + offset < 0) {
                    pCore->displayMessage(i18n("Cannot move first source keyframe before clip start"), MessageType::ErrorMessage, 500);
                    return;
                }
                it++;
            }
        }
    }
    m_currentKeyframe.second = m_position + m_inFrame;
    m_keyframes.insert(m_currentKeyframe.first, m_currentKeyframe.second);
    QMapIterator<int, int> i(nextKeyframes);
    while (i.hasNext()) {
        i.next();
        m_keyframes.insert(i.key(), i.value() + offset);
    }
    std::pair<double, double> speeds = getSpeed(m_currentKeyframe);
    std::pair<bool, bool> atEnd = {m_currentKeyframe.first == m_inFrame, m_currentKeyframe.first == m_keyframes.lastKey()};
    Q_EMIT selectedKf(m_currentKeyframe, speeds, atEnd);
    bool isLast = m_currentKeyframe.first == m_keyframes.firstKey() || m_currentKeyframe.first == m_keyframes.lastKey();
    Q_EMIT atKeyframe(true, isLast);
    Q_EMIT updateKeyframes(true);
    update();
}

std::pair<int, int> RemapView::getClosestKeyframe(int pos, bool bottomKeyframe) const
{
    int deltaMin = -1;
    std::pair<int, int> result = {-1, -1};
    QMapIterator<int, int> i(m_keyframes);
    while (i.hasNext()) {
        i.next();
        int val = bottomKeyframe ? i.key() : i.value();
        int delta = qAbs(val - pos);
        if (deltaMin == -1 || delta < deltaMin) {
            deltaMin = delta;
            result = {i.key(), i.value()};
        }
    }
    return result;
}

void RemapView::mouseReleaseEvent(QMouseEvent *event)
{
    event->accept();
    bool keyframesEdited = m_keyframesOrigin != m_keyframes;
    if (m_moveKeyframeMode == TopMove || m_moveKeyframeMode == BottomMove) {
        // Restore original selection
        m_selectedKeyframes = m_previousSelection;
        int maxWidth = width() - (2 * m_offset);
        m_scale = maxWidth / double(qMax(1, remapMax()));
        m_zoomStart = m_zoomHandle.x() * maxWidth;
        m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
        update();
        if (!keyframesEdited) {
            Q_EMIT seekToPos(m_currentKeyframeOriginal.second, m_bottomPosition);
        }
    }
    m_moveKeyframeMode = NoMove;
    if (keyframesEdited) {
        Q_EMIT updateKeyframesWithUndo(m_keyframes, m_keyframesOrigin);
    }
}

void RemapView::mousePressEvent(QMouseEvent *event)
{
    event->accept();
    m_lastMaxDuration = remapMax();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    int pos = int(((event->x() - m_offset) / m_zoomFactor + m_zoomStart) / m_scale);
#else
    int pos = int(((event->position().x() - m_offset) / m_zoomFactor + m_zoomStart) / m_scale);
#endif
    pos = qBound(0, pos, m_maxLength);
    m_moveKeyframeMode = NoMove;
    m_keyframesOrigin = m_keyframes;
    m_oldInFrame = m_inFrame;
    if (event->button() == Qt::LeftButton) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        if (event->y() < m_centerPos) {
#else
        if (event->position().y() < m_centerPos) {
#endif
            // mouse click in top area
            if (event->modifiers() & Qt::ShiftModifier) {
                m_clickPoint = pos + m_inFrame;
                return;
            }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            if (event->y() < 2 * m_lineHeight && event->y() > m_lineHeight) {
#else
            if (event->position().y() < 2 * m_lineHeight && event->position().y() > m_lineHeight) {
#endif
                std::pair<int, int> keyframe = getClosestKeyframe(pos + m_inFrame);
                if (keyframe.first > -1 && qAbs(keyframe.second - (pos + m_inFrame)) * m_scale * m_zoomFactor <= m_lineHeight / 2) {
                    // Clicked on a top keyframe
                    m_currentKeyframeOriginal = keyframe;
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
                    std::pair<double, double> speeds = getSpeed(m_currentKeyframe);
                    std::pair<bool, bool> atEnd = {m_currentKeyframe.first == m_inFrame, m_currentKeyframe.first == m_keyframes.lastKey()};
                    Q_EMIT selectedKf(m_currentKeyframe, speeds, atEnd);
                    if (m_currentKeyframeOriginal.first > -1) {
                        m_moveKeyframeMode = TopMove;
                        m_previousSelection = m_selectedKeyframes;
                        if (m_moveNext) {
                            QMapIterator<int, int> i(m_keyframes);
                            while (i.hasNext()) {
                                i.next();
                                if (i.key() > m_currentKeyframeOriginal.first) {
                                    m_selectedKeyframes.insert(i.key(), i.value());
                                }
                            }
                        }
                        if (KdenliveSettings::keyframeseek()) {
                            slotSetPosition(m_currentKeyframeOriginal.second);
                            m_bottomPosition = m_currentKeyframeOriginal.first - m_inFrame;
                            bool isLast = m_currentKeyframe.first == m_keyframes.firstKey() || m_currentKeyframe.first == m_keyframes.lastKey();
                            Q_EMIT atKeyframe(true, isLast);
                        } else {
                            update();
                        }
                    } else {
                        update();
                    }
                    return;
                }
            }
            // no keyframe next to mouse
            // m_selectedKeyframes.clear();
            // m_currentKeyframe = m_currentKeyframeOriginal = {-1,-1};
            m_moveKeyframeMode = CursorMove;
            if (pos != m_position) {
                slotSetPosition(pos + m_inFrame);
                Q_EMIT seekToPos(pos + m_inFrame, -1);
                update();
            }
            return;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        } else if (event->y() > m_bottomView) {
#else
        } else if (event->position().y() > m_bottomView) {
#endif
            // click on zoom area
            if (m_hoverZoom) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                m_clickOffset = (double(event->x()) - m_offset) / (width() - 2 * m_offset);
#else
                m_clickOffset = (double(event->position().x()) - m_offset) / (width() - 2 * m_offset);
#endif
            }
            return;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        } else if (event->y() > m_centerPos && event->y() < m_bottomView) {
#else
        } else if (event->position().y() > m_centerPos && event->position().y() < m_bottomView) {
#endif
            // click in bottom area
            if (event->modifiers() & Qt::ShiftModifier) {
                m_clickPoint = pos + m_inFrame;
                return;
            }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
            if (event->y() > (m_bottomView - 2 * m_lineHeight) && (event->y() < m_bottomView - m_lineHeight)) {
#else
            if (event->position().y() > (m_bottomView - 2 * m_lineHeight) && (event->position().y() < m_bottomView - m_lineHeight)) {
#endif
                std::pair<int, int> keyframe = getClosestKeyframe(pos + m_inFrame, true);
                if (keyframe.first > -1 && qAbs(keyframe.first - (pos + m_inFrame)) * m_scale * m_zoomFactor <= m_lineHeight / 2) {
                    m_currentKeyframeOriginal = keyframe;
                    if (event->modifiers() & Qt::ControlModifier) {
                        if (m_selectedKeyframes.contains(m_currentKeyframeOriginal.first)) {
                            m_selectedKeyframes.remove(m_currentKeyframeOriginal.first);
                            m_currentKeyframeOriginal.second = -1;
                        } else {
                            m_selectedKeyframes.insert(m_currentKeyframeOriginal.first, m_currentKeyframeOriginal.second);
                        }
                    } else if (!m_selectedKeyframes.contains(m_currentKeyframeOriginal.first)) {
                        m_selectedKeyframes = {m_currentKeyframeOriginal};
                    }
                    // Select and seek to keyframe
                    m_currentKeyframe = m_currentKeyframeOriginal;
                    std::pair<double, double> speeds = getSpeed(m_currentKeyframe);
                    std::pair<bool, bool> atEnd = {m_currentKeyframe.first == m_inFrame, m_currentKeyframe.first == m_keyframes.lastKey()};
                    Q_EMIT selectedKf(m_currentKeyframe, speeds, atEnd);
                    if (m_currentKeyframeOriginal.first > -1) {
                        m_moveKeyframeMode = BottomMove;
                        m_previousSelection = m_selectedKeyframes;
                        if (m_moveNext) {
                            QMapIterator<int, int> i(m_keyframes);
                            while (i.hasNext()) {
                                i.next();
                                if (i.key() > m_currentKeyframeOriginal.first) {
                                    m_selectedKeyframes.insert(i.key(), i.value());
                                }
                            }
                        }
                        if (KdenliveSettings::keyframeseek()) {
                            m_bottomPosition = m_currentKeyframeOriginal.first - m_inFrame;
                            int topPos = GenTime(m_remapProps.anim_get_double("time_map", m_currentKeyframeOriginal.first)).frames(pCore->getCurrentFps());
                            m_position = topPos - m_inFrame;
                            bool isLast = m_currentKeyframe.first == m_keyframes.firstKey() || m_currentKeyframe.first == m_keyframes.lastKey();
                            Q_EMIT atKeyframe(true, isLast);
                        } else {
                            update();
                        }
                    } else {
                        update();
                    }
                    return;
                }
            }
            // no keyframe next to mouse
            // m_selectedKeyframes.clear();
            // m_currentKeyframe = m_currentKeyframeOriginal = {-1,-1};
            m_moveKeyframeMode = CursorMoveBottom;
            pos = qMin(pos, m_keyframes.lastKey() - m_inFrame);
            if (pos != m_bottomPosition) {
                m_bottomPosition = pos;
                bool isKfr = m_keyframes.contains(m_bottomPosition + m_inFrame);
                if (isKfr) {
                    bool isLast = m_bottomPosition + m_inFrame == m_keyframes.firstKey() || m_bottomPosition + m_inFrame == m_keyframes.lastKey();
                    Q_EMIT atKeyframe(isKfr, isLast);
                } else {
                    Q_EMIT atKeyframe(false, false);
                }
                // slotUpdatePosition();
                Q_EMIT seekToPos(-1, pos);
                update();
            }
            return;
        }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    } else if (event->button() == Qt::RightButton && event->y() > m_bottomView) {
#else
    } else if (event->button() == Qt::RightButton && event->position().y() > m_bottomView) {
#endif
        // Right click on zoom, switch between no zoom and last zoom status
        if (m_zoomHandle == QPointF(0, 1)) {
            if (!m_lastZoomHandle.isNull()) {
                m_zoomHandle = m_lastZoomHandle;
                int maxWidth = width() - (2 * m_offset);
                m_zoomStart = m_zoomHandle.x() * maxWidth;
                m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
                update();
                return;
            }
        } else {
            m_lastZoomHandle = m_zoomHandle;
            m_zoomHandle = QPointF(0, 1);
            int maxWidth = width() - (2 * m_offset);
            m_zoomStart = m_zoomHandle.x() * maxWidth;
            m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
            update();
            return;
        }
    }
    if (pos != m_position) {
        // Q_EMIT seekToPos(pos);
        update();
    }
}

void RemapView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::AltModifier) {
        // Alt modifier seems to invert x/y axis
        if (event->angleDelta().x() > 0) {
            goPrev();
        } else {
            goNext();
        }
        return;
    }
    if (event->modifiers() & Qt::ControlModifier) {
        int maxWidth = width() - 2 * m_offset;
        double scaledPos = m_position * m_scale;
        double zoomRange = (m_zoomHandle.y() - m_zoomHandle.x()) * maxWidth;
        if (event->angleDelta().y() > 0) {
            zoomRange /= 1.5;
        } else {
            zoomRange *= 1.5;
        }
        if (zoomRange < 5) {
            // Don't allow too small zoombar
            return;
        }
        double length = (scaledPos - zoomRange / 2) / maxWidth;
        m_zoomHandle.setX(qMax(0., length));
        if (length < 0) {
            m_zoomHandle.setY(qMin(1.0, (scaledPos + zoomRange / 2) / maxWidth - length));
        } else {
            m_zoomHandle.setY(qMin(1.0, (scaledPos + zoomRange / 2) / maxWidth));
        }
        m_zoomStart = m_zoomHandle.x() * maxWidth;
        m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
        update();
        return;
    }
    if (event->position().y() < m_bottomView) {
        int change = event->angleDelta().y() > 0 ? -1 : 1;
        int pos = qBound(0, m_position + change, m_duration - 1);
        Q_EMIT seekToPos(pos + m_inFrame, -1);
    } else {
        // Wheel on zoom bar, scroll
        double pos = m_zoomHandle.x();
        double zoomWidth = m_zoomHandle.y() - pos;
        int maxWidth = width() - 2 * m_offset;
        if (event->angleDelta().y() > 0) {
            if (zoomWidth / 2 > pos) {
                pos = 0;
            } else {
                pos -= zoomWidth / 2;
            }
        } else {
            if (pos + zoomWidth + zoomWidth / 2 > 1.) {
                pos = 1. - zoomWidth;
            } else {
                pos += zoomWidth / 2;
            }
        }
        m_zoomHandle = QPointF(pos, pos + zoomWidth);
        m_zoomStart = m_zoomHandle.x() * maxWidth;
        m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
        update();
    }
}

void RemapView::slotSetPosition(int pos)
{
    if (pos != m_position + m_inFrame) {
        m_position = pos - m_inFrame;
        // int offset = pCore->getItemIn(m_model->getOwnerId());
        // Q_EMIT atKeyframe(m_keyframes.contains(pos));
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

void RemapView::slotSetBottomPosition(int pos)
{
    if (pos < 0 || pos + m_inFrame > m_keyframes.lastKey()) {
        pos = -1;
    }
    if (pos != m_bottomPosition) {
        m_bottomPosition = pos;
        if (m_bottomPosition > -1) {
            bool isKfr = m_keyframes.contains(m_bottomPosition + m_inFrame);
            if (isKfr) {
                bool isLast = m_bottomPosition + m_inFrame == m_keyframes.firstKey() || m_bottomPosition + m_inFrame == m_keyframes.lastKey();
                Q_EMIT atKeyframe(isKfr, isLast);
            } else {
                Q_EMIT atKeyframe(false, false);
            }
        } else {
            Q_EMIT atKeyframe(false, false);
        }
        update();
    }
}

void RemapView::goNext()
{
    // Seek to next keyframe
    QMapIterator<int, int> i(m_keyframes);
    while (i.hasNext()) {
        i.next();
        if (i.key() > m_bottomPosition + m_inFrame) {
            m_currentKeyframe = {i.key(), i.value()};
            m_selectedKeyframes = {m_currentKeyframe};
            slotSetPosition(i.key());
            m_bottomPosition = m_currentKeyframe.first - m_inFrame;
            Q_EMIT seekToPos(i.value(), m_bottomPosition);
            std::pair<double, double> speeds = getSpeed(m_currentKeyframe);
            std::pair<bool, bool> atEnd = {m_currentKeyframe.first == m_inFrame, m_currentKeyframe.first == m_keyframes.lastKey()};
            Q_EMIT selectedKf(m_currentKeyframe, speeds, atEnd);
            bool isLast = m_currentKeyframe.first == m_keyframes.firstKey() || m_currentKeyframe.first == m_keyframes.lastKey();
            Q_EMIT atKeyframe(true, isLast);
            break;
        }
    }
}

void RemapView::goPrev()
{
    // insert keyframe at interpolated position
    bool previousFound = false;
    QMap<int, int>::iterator it = m_keyframes.begin();
    while (it.key() < m_bottomPosition + m_inFrame && it != m_keyframes.end()) {
        it++;
    }
    if (it != m_keyframes.end()) {
        if (it != m_keyframes.begin()) {
            it--;
        }
        m_currentKeyframe = {it.key(), it.value()};
        m_selectedKeyframes = {m_currentKeyframe};
        slotSetPosition(m_currentKeyframe.second);
        m_bottomPosition = m_currentKeyframe.first - m_inFrame;
        Q_EMIT seekToPos(m_currentKeyframe.second, m_bottomPosition);
        std::pair<double, double> speeds = getSpeed(m_currentKeyframe);
        std::pair<bool, bool> atEnd = {m_currentKeyframe.first == m_inFrame, m_currentKeyframe.first == m_keyframes.lastKey()};
        Q_EMIT selectedKf(m_currentKeyframe, speeds, atEnd);
        bool isLast = m_currentKeyframe.first == m_keyframes.firstKey() || m_currentKeyframe.first == m_keyframes.lastKey();
        Q_EMIT atKeyframe(true, isLast);
        previousFound = true;
    }

    if (!previousFound && !m_keyframes.isEmpty()) {
        // We are after the last keyframe
        m_currentKeyframe = {m_keyframes.lastKey(), m_keyframes.value(m_keyframes.lastKey())};
        m_selectedKeyframes = {m_currentKeyframe};
        slotSetPosition(m_currentKeyframe.second);
        m_bottomPosition = m_currentKeyframe.first - m_inFrame;
        Q_EMIT seekToPos(m_currentKeyframe.second, m_bottomPosition);
        std::pair<double, double> speeds = getSpeed(m_currentKeyframe);
        std::pair<bool, bool> atEnd = {m_currentKeyframe.first == m_inFrame, m_currentKeyframe.first == m_keyframes.lastKey()};
        Q_EMIT selectedKf(m_currentKeyframe, speeds, atEnd);
    }
}

void RemapView::updateBeforeSpeed(double speed)
{
    QMutexLocker lock(&m_kfrMutex);
    QMap<int, int>::iterator it = m_keyframes.find(m_currentKeyframe.first);
    QMap<int, int> updatedKfrs;
    QList<int> toDelete;
    if (*it != m_keyframes.first() && it != m_keyframes.end()) {
        m_keyframesOrigin = m_keyframes;
        it--;
        double updatedLength = qFuzzyIsNull(speed) ? 0 : (m_currentKeyframe.second - it.value()) * 100. / speed;
        double offset = it.key() + updatedLength - m_currentKeyframe.first;
        int offsetInt = int(offset);
        int lengthInt = int(updatedLength);
        if (offsetInt == 0) {
            if (offset < 0) {
                offsetInt = -1;
            } else {
                offsetInt = 1;
            }
            lengthInt += offsetInt;
        }
        m_keyframes.remove(m_currentKeyframe.first);
        m_currentKeyframe.first = it.key() + lengthInt;
        m_keyframes.insert(m_currentKeyframe.first, m_currentKeyframe.second);
        m_bottomPosition = m_currentKeyframe.first;
        m_selectedKeyframes.clear();
        m_selectedKeyframes.insert(m_currentKeyframe.first, m_currentKeyframe.second);
        it = m_keyframes.find(m_currentKeyframe.first);
        if (*it != m_keyframes.last()) {
            it++;
            // Update all keyframes after that so that we don't alter the speeds
            while (m_moveNext && it != m_keyframes.end()) {
                toDelete << it.key();
                updatedKfrs.insert(it.key() + offsetInt, it.value());
                it++;
            }
        }
        for (int p : qAsConst(toDelete)) {
            m_keyframes.remove(p);
        }
        QMapIterator<int, int> i(updatedKfrs);
        while (i.hasNext()) {
            i.next();
            m_keyframes.insert(i.key(), i.value());
        }
        int maxWidth = width() - (2 * m_offset);
        m_scale = maxWidth / double(qMax(1, remapMax()));
        m_zoomStart = m_zoomHandle.x() * maxWidth;
        m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
        Q_EMIT updateKeyframesWithUndo(m_keyframes, m_keyframesOrigin);
        update();
    }
}

void RemapView::updateAfterSpeed(double speed)
{
    QMutexLocker lock(&m_kfrMutex);
    m_keyframesOrigin = m_keyframes;
    QMap<int, int>::iterator it = m_keyframes.find(m_currentKeyframe.first);
    if (*it != m_keyframes.last()) {
        it++;
        QMap<int, int> updatedKfrs;
        QList<int> toDelete;
        double updatedLength = (it.value() - m_currentKeyframe.second) * 100. / speed;
        double offset = m_currentKeyframe.first + updatedLength - it.key();
        int offsetInt = int(offset);
        int lengthInt = int(updatedLength);
        if (offsetInt == 0) {
            if (offset < 0) {
                offsetInt = -1;
            } else {
                offsetInt = 1;
            }
            lengthInt += offsetInt;
        }
        if (m_moveNext) {
            while (it != m_keyframes.end()) {
                toDelete << it.key();
                updatedKfrs.insert(it.key() + offsetInt, it.value());
                it++;
            }
        } else {
            m_keyframes.insert(m_currentKeyframe.first + lengthInt, it.value());
            m_keyframes.remove(it.key());
        }
        // Update all keyframes after that so that we don't alter the speeds
        for (int p : qAsConst(toDelete)) {
            m_keyframes.remove(p);
        }
        QMapIterator<int, int> i(updatedKfrs);
        while (i.hasNext()) {
            i.next();
            m_keyframes.insert(i.key(), i.value());
        }
        int maxWidth = width() - (2 * m_offset);
        m_scale = maxWidth / double(qMax(1, remapMax()));
        m_zoomStart = m_zoomHandle.x() * maxWidth;
        m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
        Q_EMIT updateKeyframesWithUndo(m_keyframes, m_keyframesOrigin);
        update();
    }
}

const QString RemapView::getKeyframesData(QMap<int, int> keyframes) const
{
    QStringList result;
    if (keyframes.isEmpty()) {
        keyframes = m_keyframes;
    }
    QMapIterator<int, int> i(keyframes);
    int offset = 0;
    Mlt::Properties props;
    props.set("_profile", pCore->getProjectProfile().get_profile(), 0);
    while (i.hasNext()) {
        i.next();
        if (i.key() == keyframes.lastKey()) {
            // HACK: we always set last keyframe 1 frame after in MLT to ensure we have a correct last frame
            offset = 1;
        }
        result << QString("%1=%2").arg(props.frames_to_time(i.key() + offset, mlt_time_clock)).arg(GenTime(i.value(), pCore->getCurrentFps()).seconds());
    }
    return result.join(QLatin1Char(';'));
}

void RemapView::reloadProducer()
{
    if (!m_clip || !m_clip->clipUrl().endsWith(QLatin1String(".mlt"))) {
        qDebug() << "==== this is not a playlist clip, aborting";
        return;
    }
    QMutexLocker lock(&pCore->xmlMutex);
    Mlt::Consumer c(pCore->getProjectProfile(), "xml", m_clip->clipUrl().toUtf8().constData());
    QScopedPointer<Mlt::Service> serv(m_clip->originalProducer()->producer());
    if (serv == nullptr) {
        return;
    }
    qDebug() << "==== GOR PLAYLIST SERVICE: " << serv->type() << " / " << serv->consumer()->type() << ", SAVING TO " << m_clip->clipUrl();
    Mlt::Multitrack s2(*serv.data());
    qDebug() << "==== MULTITRACK: " << s2.count();
    Mlt::Tractor s(pCore->getProjectProfile());
    s.set_track(*s2.track(0), 0);
    qDebug() << "==== GOT TRACKS: " << s.count();
    int ignore = s.get_int("ignore_points");
    if (ignore) {
        s.set("ignore_points", 0);
    }
    c.connect(s);
    c.set("time_format", "frames");
    c.set("no_meta", 1);
    c.set("no_root", 1);
    // c.set("no_profile", 1);
    c.set("root", "/");
    c.set("store", "kdenlive");
    c.run();
    if (ignore) {
        s.set("ignore_points", ignore);
    }
}

std::pair<double, double> RemapView::getSpeed(std::pair<int, int> kf)
{
    std::pair<double, double> speeds = {-1, -1};
    QMap<int, int>::iterator it = m_keyframes.find(kf.first);
    if (it == m_keyframes.end()) {
        // Not a keyframe
        return speeds;
    }
    if (*it != m_keyframes.first()) {
        it--;
        speeds.first = (double)(kf.second - it.value()) / (kf.first - it.key());
        it++;
    }
    if (*it != m_keyframes.last()) {
        it++;
        speeds.second = (double)(kf.second - it.value()) / (kf.first - it.key());
    }
    return speeds;
}

void RemapView::addKeyframe()
{
    // insert or remove keyframe at interpolated position
    m_keyframesOrigin = m_keyframes;
    if (m_keyframes.contains(m_bottomPosition + m_inFrame)) {
        m_keyframes.remove(m_bottomPosition + m_inFrame);
        if (m_currentKeyframe.first == m_bottomPosition + m_inFrame) {
            m_currentKeyframe = m_currentKeyframeOriginal = {-1, -1};
            Q_EMIT selectedKf(m_currentKeyframe, {-1, -1});
        }
        Q_EMIT atKeyframe(false, false);
        Q_EMIT updateKeyframesWithUndo(m_keyframes, m_keyframesOrigin);
        update();
        return;
    }
    QMapIterator<int, int> i(m_keyframes);
    std::pair<int, int> newKeyframe = {-1, -1};
    std::pair<int, int> previous = {-1, -1};
    newKeyframe.first = m_bottomPosition + m_inFrame;
    while (i.hasNext()) {
        i.next();
        if (i.key() > m_bottomPosition + m_inFrame) {
            if (i.key() == m_keyframes.firstKey()) {
                // This is the first keyframe
                double ratio = (double)(m_bottomPosition + m_inFrame) / i.key();
                newKeyframe.second = i.value() * ratio;
                break;
            } else if (previous.first > -1) {
                std::pair<int, int> current = {i.key(), i.value()};
                double ratio = (double)(m_bottomPosition + m_inFrame - previous.first) / (current.first - previous.first);
                qDebug() << "=== RATIO: " << ratio;
                newKeyframe.second = previous.second + (qAbs(current.second - previous.second) * ratio);
                break;
            }
        }
        previous = {i.key(), i.value()};
    }
    if (newKeyframe.second == -1) {
        // We are after the last keyframe
        if (m_keyframes.isEmpty()) {
            newKeyframe.second = m_position + m_inFrame;
        } else {
            double ratio = (double)(m_position + m_inFrame - m_keyframes.lastKey()) / (m_duration - m_keyframes.lastKey());
            newKeyframe.second = m_keyframes.value(m_keyframes.lastKey()) + (qAbs(m_duration - m_keyframes.value(m_keyframes.lastKey())) * ratio);
        }
    }
    m_keyframes.insert(newKeyframe.first, newKeyframe.second);
    m_currentKeyframe = newKeyframe;
    m_selectedKeyframes = {m_currentKeyframe};
    std::pair<double, double> speeds = getSpeed(m_currentKeyframe);
    std::pair<bool, bool> atEnd = {m_currentKeyframe.first == m_inFrame, m_currentKeyframe.first == m_keyframes.lastKey()};
    Q_EMIT selectedKf(m_currentKeyframe, speeds, atEnd);
    bool isLast = m_currentKeyframe.first == m_keyframes.firstKey() || m_currentKeyframe.first == m_keyframes.lastKey();
    Q_EMIT atKeyframe(true, isLast);
    Q_EMIT updateKeyframesWithUndo(m_keyframes, m_keyframesOrigin);
    update();
}

void RemapView::toggleMoveNext(bool moveNext)
{
    m_moveNext = moveNext;
    // Reset keyframe selection
    m_selectedKeyframes.clear();
}

void RemapView::refreshOnDurationChanged(int remapDuration)
{
    if (remapDuration != m_duration) {
        m_duration = qMax(remapDuration, remapMax());
        int maxWidth = width() - (2 * m_offset);
        m_scale = maxWidth / double(qMax(1, remapMax()));
        m_zoomStart = m_zoomHandle.x() * maxWidth;
        m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
    }
}

void RemapView::resizeEvent(QResizeEvent *event)
{
    int maxWidth = width() - (2 * m_offset);
    m_scale = maxWidth / double(qMax(1, remapMax()));
    m_zoomStart = m_zoomHandle.x() * maxWidth;
    m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
    QWidget::resizeEvent(event);
    update();
}

void RemapView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPalette pal = palette();
    KColorScheme scheme(pal.currentColorGroup(), KColorScheme::Window);
    m_colSelected = palette().highlight().color();
    m_colKeyframe = scheme.foreground(KColorScheme::NormalText).color();
    QColor bg = scheme.background(KColorScheme::AlternateBackground).color();
    QStylePainter p(this);
    int maxWidth = width() - (2 * m_offset);
    int zoomEnd = qCeil(m_zoomHandle.y() * maxWidth);
    // Top timeline
    p.fillRect(m_offset, 0, maxWidth + 1, m_centerPos, bg);
    // Bottom timeline
    p.fillRect(m_offset, m_bottomView - m_centerPos, maxWidth + 1, m_centerPos, bg);
    /* ticks */
    double fps = pCore->getCurrentFps();
    int maxLength = remapMax();
    if (maxLength == 0) {
        return;
    }
    int displayedLength = int(maxLength / m_zoomFactor / fps);
    double factor = 1;
    if (displayedLength < 2) {
        // 1 frame tick
    } else if (displayedLength < 30) {
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
    for (int i = 0; i < maxWidth / frameSize; i++) {
        int scaledTick = int(m_offset + (i * frameSize) + tickOffset);
        if (scaledTick >= maxWidth + m_offset) {
            break;
        }
        p.drawLine(QPointF(scaledTick, m_lineHeight + 1), QPointF(scaledTick, m_lineHeight - 3));
        p.drawLine(QPointF(scaledTick, m_bottomView - m_lineHeight + 1), QPointF(scaledTick, m_bottomView - m_lineHeight - 3));
    }

    /*
     * Time-"lines"
     * We have a top timeline for the source (clip monitor) and a bottom timeline for the output (project monitor)
     */
    p.setPen(m_colKeyframe);
    // Top timeline
    // qDebug()<<"=== MAX KFR WIDTH: "<<maxWidth<<", DURATION SCALED: "<<(m_duration * m_scale)<<", POS: "<<(m_position * m_scale);
    int maxPos = maxWidth + m_offset - 1;
    p.drawLine(m_offset, m_lineHeight, maxPos, m_lineHeight);
    p.drawLine(m_offset, m_lineHeight - m_lineHeight / 4, m_offset, m_lineHeight + m_lineHeight / 4);
    p.drawLine(maxPos, m_lineHeight - m_lineHeight / 4, maxPos, m_lineHeight + m_lineHeight / 4);
    // Bottom timeline
    p.drawLine(m_offset, m_bottomView - m_lineHeight, maxPos, m_bottomView - m_lineHeight);
    p.drawLine(m_offset, m_bottomView - m_lineHeight - m_lineHeight / 4, m_offset, m_bottomView - m_lineHeight + m_lineHeight / 4);
    p.drawLine(maxPos, m_bottomView - m_lineHeight - m_lineHeight / 4, maxPos, m_bottomView - m_lineHeight + m_lineHeight / 4);

    /*
     * Original clip in/out
     */
    p.setPen(palette().mid().color());
    double inPos = (double)(m_originalRange.first - m_inFrame) * m_scale;
    double outPos = (double)(m_originalRange.second - m_inFrame) * m_scale;
    inPos -= m_zoomStart;
    inPos *= m_zoomFactor;
    outPos -= m_zoomStart;
    outPos *= m_zoomFactor;
    if (inPos >= 0) {
        inPos += m_offset;
        p.drawLine(inPos, m_lineHeight, inPos, m_bottomView - m_lineHeight);
    }
    if (outPos <= maxWidth) {
        outPos += m_offset;
        p.drawLine(outPos, m_lineHeight, outPos, m_bottomView - m_lineHeight);
    }

    /*
     * Keyframes
     */
    QMapIterator<int, int> i(m_keyframes);
    while (i.hasNext()) {
        i.next();
        double kfOutPos = (double)(i.key() - m_inFrame) * m_scale;
        double kfInPos = (double)(i.value() - m_inFrame) * m_scale;
        if ((kfInPos < m_zoomStart && kfOutPos < m_zoomStart) || (qFloor(kfInPos) > zoomEnd && qFloor(kfOutPos) > zoomEnd)) {
            continue;
        }
        if (m_currentKeyframe.first == i.key()) {
            p.setPen(Qt::red);
            p.setBrush(Qt::darkRed);
        } else if (m_selectedKeyframes.contains(i.key())) {
            p.setPen(m_colSelected);
            p.setBrush(m_colSelected);
        } else {
            p.setPen(m_colKeyframe);
            p.setBrush(m_colKeyframe);
        }
        kfInPos -= m_zoomStart;
        kfInPos *= m_zoomFactor;
        kfInPos += m_offset;
        kfOutPos -= m_zoomStart;
        kfOutPos *= m_zoomFactor;
        kfOutPos += m_offset;

        p.drawLine(kfInPos, m_lineHeight + m_lineHeight * 0.75, kfOutPos, m_bottomView - m_lineHeight * 1.75);
        p.drawLine(kfInPos, m_lineHeight, kfInPos, m_lineHeight + m_lineHeight / 2);
        p.drawLine(kfOutPos, m_bottomView - m_lineHeight, kfOutPos, m_bottomView - m_lineHeight * 1.5);
        p.drawEllipse(QRectF(kfInPos - m_lineHeight / 4.0, m_lineHeight + m_lineHeight / 2, m_lineHeight / 2, m_lineHeight / 2));
        p.drawEllipse(QRectF(kfOutPos - m_lineHeight / 4.0, m_bottomView - 2 * m_lineHeight, m_lineHeight / 2, m_lineHeight / 2));
    }

    /*
     * current position cursor
     */
    p.setPen(m_colSelected);
    // Top seek cursor
    if (m_position >= 0 && m_position < m_duration) {
        p.setBrush(m_colSelected);
        double scaledPos = m_position * m_scale;
        scaledPos -= m_zoomStart;
        scaledPos *= m_zoomFactor;
        scaledPos += m_offset;
        if (scaledPos >= m_offset && qFloor(scaledPos) <= m_offset + maxWidth) {
            QPolygonF topCursor;
            topCursor << QPointF(-int(m_lineHeight / 3), -m_lineHeight * 0.5) << QPointF(int(m_lineHeight / 3), -m_lineHeight * 0.5) << QPointF(0, 0);
            topCursor.translate(scaledPos, m_lineHeight);
            p.drawPolygon(topCursor);
        }
    }

    if (m_bottomPosition >= 0 && m_bottomPosition < m_duration) {
        p.setBrush(m_colSelected);
        double scaledPos = -1;
        if (!m_keyframes.isEmpty()) {
            int topPos = GenTime(m_remapProps.anim_get_double("time_map", m_bottomPosition + m_inFrame)).frames(pCore->getCurrentFps()) - m_inFrame;
            scaledPos = topPos * m_scale;
            scaledPos -= m_zoomStart;
            scaledPos *= m_zoomFactor;
            scaledPos += m_offset;
        }
        double scaledPos2 = m_bottomPosition * m_scale;
        scaledPos2 -= m_zoomStart;
        scaledPos2 *= m_zoomFactor;
        scaledPos2 += m_offset;
        if (scaledPos2 >= m_offset && qFloor(scaledPos2) <= m_offset + maxWidth) {
            QPolygonF bottomCursor;
            bottomCursor << QPointF(-int(m_lineHeight / 3), m_lineHeight * 0.5) << QPointF(int(m_lineHeight / 3), m_lineHeight * 0.5) << QPointF(0, 0);
            bottomCursor.translate(scaledPos2, m_bottomView - m_lineHeight);
            p.setBrush(m_colSelected);
            p.drawPolygon(bottomCursor);
        }
        if (scaledPos > -1) {
            p.drawLine(scaledPos, m_lineHeight * 1.75, scaledPos2, m_bottomView - (m_lineHeight * 1.75));
            p.drawLine(scaledPos, m_lineHeight, scaledPos, m_lineHeight * 1.75);
        }
        p.drawLine(scaledPos2, m_bottomView - m_lineHeight, scaledPos2, m_bottomView - m_lineHeight * 1.75);
    }

    // Zoom bar
    p.setPen(Qt::NoPen);
    p.setBrush(palette().mid());
    p.drawRoundedRect(0, m_bottomView + 2, width() - 2 * 0, m_zoomHeight, m_lineHeight / 3, m_lineHeight / 3);
    p.setBrush(palette().highlight());
    p.drawRoundedRect(int((width()) * m_zoomHandle.x()), m_bottomView + 2, int((width()) * (m_zoomHandle.y() - m_zoomHandle.x())), m_zoomHeight,
                      m_lineHeight / 3, m_lineHeight / 3);
}

TimeRemap::TimeRemap(QWidget *parent)
    : QWidget(parent)
    , m_cid(-1)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    warningMessage->hide();
    QAction *ac = new QAction(i18n("Transcode"), this);
    warningMessage->addAction(ac);
    connect(ac, &QAction::triggered, this, [&]() {
        QMetaObject::invokeMethod(pCore->bin(), "requestTranscoding", Qt::QueuedConnection, Q_ARG(QString, QString()), Q_ARG(QString, m_binId), Q_ARG(int, 0),
                                  Q_ARG(bool, false));
    });
    m_view = new RemapView(this);
    speedBefore->setKeyboardTracking(false);
    speedAfter->setKeyboardTracking(false);
    remapLayout->addWidget(m_view);
    connect(m_view, &RemapView::selectedKf, this, [this](std::pair<int, int> selection, std::pair<double, double> speeds, std::pair<bool, bool> atEnd) {
        info_frame->setEnabled(selection.first > -1);
        QSignalBlocker bk(m_in);
        QSignalBlocker bk2(m_out);
        m_in->setValue(selection.second - m_view->m_inFrame);
        m_out->setValue(selection.first - m_view->m_inFrame);
        QSignalBlocker bk3(speedBefore);
        QSignalBlocker bk4(speedAfter);
        speedBefore->setEnabled(!atEnd.first);
        speedBefore->setValue(100. * speeds.first);
        speedAfter->setEnabled(!atEnd.second);
        speedAfter->setValue(100. * speeds.second);
    });
    connect(m_view, &RemapView::updateSpeeds, this, [this](std::pair<double, double> speeds) {
        QSignalBlocker bk3(speedBefore);
        QSignalBlocker bk4(speedAfter);
        speedBefore->setEnabled(speeds.first > 0);
        speedBefore->setValue(100. * speeds.first);
        speedAfter->setEnabled(speeds.second > 0);
        speedAfter->setValue(100. * speeds.second);
    });
    button_add->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-add")));
    button_add->setToolTip(i18n("Add keyframe"));
    button_add->setWhatsThis(xi18nc("@info:whatsthis", "Inserts a keyframe at the current playhead position/frame."));
    button_next->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-next")));
    button_next->setToolTip(i18n("Go to next keyframe"));
    button_next->setWhatsThis(xi18nc("@info:whatsthis", "Moves the playhead to the next keyframe to the right."));
    button_prev->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-previous")));
    button_prev->setToolTip(i18n("Go to previous keyframe"));
    button_prev->setWhatsThis(xi18nc("@info:whatsthis", "Moves the playhead to the next keyframe to the left."));
    connect(m_view, &RemapView::updateKeyframes, this, &TimeRemap::updateKeyframes);
    connect(m_view, &RemapView::updateKeyframesWithUndo, this, &TimeRemap::updateKeyframesWithUndo);
    connect(m_in, &TimecodeDisplay::timeCodeUpdated, this, [this]() { m_view->updateInPos(m_in->getValue() + m_view->m_inFrame); });
    button_center->setToolTip(i18n("Move selected keyframe to cursor"));
    button_center->setWhatsThis(xi18nc("@info:whatsthis", "Moves the selected keyframes to the current playhead position/frame."));
    button_center_top->setToolTip(i18n("Move selected keyframe to cursor"));
    button_center_top->setWhatsThis(xi18nc("@info:whatsthis", "Moves the selected keyframes to the current playhead position/frame."));
    connect(m_out, &TimecodeDisplay::timeCodeUpdated, this, [this]() { m_view->updateOutPos(m_out->getValue() + m_view->m_inFrame); });
    connect(button_center, &QToolButton::clicked, m_view, &RemapView::centerCurrentKeyframe);
    connect(button_center_top, &QToolButton::clicked, m_view, &RemapView::centerCurrentTopKeyframe);
    connect(m_view, &RemapView::atKeyframe, button_add, [&](bool atKeyframe, bool last) {
        if (atKeyframe) {
            button_add->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-remove")));
            button_add->setToolTip(i18n("Delete keyframe"));
            button_add->setWhatsThis(xi18nc("@info:whatsthis", "Deletes the keyframe at the current position of the playhead."));
        } else {
            button_add->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-add")));
            button_add->setToolTip(i18n("Add keyframe"));
            button_add->setWhatsThis(xi18nc("@info:whatsthis", "Inserts a keyframe at the current playhead position/frame."));
        }
        button_add->setEnabled(!atKeyframe || !last);
    });
    connect(speedBefore, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [&](double speed) { m_view->updateBeforeSpeed(speed); });
    connect(speedAfter, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [&](double speed) { m_view->updateAfterSpeed(speed); });
    connect(button_del, &QToolButton::clicked, this, [this]() {
        if (m_cid > -1) {
            std::shared_ptr<TimelineItemModel> model = pCore->currentDoc()->getTimeline(m_uuid);
            model->requestClipTimeRemap(m_cid, false);
            selectedClip(-1, QUuid());
        }
    });
    connect(button_add, &QToolButton::clicked, m_view, &RemapView::addKeyframe);
    connect(button_next, &QToolButton::clicked, m_view, &RemapView::goNext);
    connect(button_prev, &QToolButton::clicked, m_view, &RemapView::goPrev);
    connect(move_next, &QCheckBox::toggled, m_view, &RemapView::toggleMoveNext);
    connect(pitch_compensate, &QCheckBox::toggled, this, &TimeRemap::switchRemapParam);
    connect(frame_blending, &QCheckBox::toggled, this, &TimeRemap::switchRemapParam);
    connect(m_view, &RemapView::updateMaxDuration, this, [this]() {
        m_out->setRange(0, INT_MAX);
        // m_in->setRange(0, duration - 1);
    });
    remap_box->setEnabled(false);
}

const QString &TimeRemap::currentClip() const
{
    return m_binId;
}

void TimeRemap::checkClipUpdate(const QModelIndex &topLeft, const QModelIndex &, const QVector<int> &roles)
{
    int id = int(topLeft.internalId());
    if (m_cid != id || !roles.contains(TimelineModel::FinalMoveRole)) {
        return;
    }
    // Don't resize view if we are moving a keyframe
    if (!m_view->movingKeyframe()) {
        ObjectId oid(KdenliveObjectType::TimelineClip, m_cid, m_uuid);
        int newDuration = pCore->getItemDuration(oid);
        // Check if the keyframes were modified by an external resize operation
        std::shared_ptr<TimelineItemModel> model = pCore->currentDoc()->getTimeline(m_uuid);
        std::shared_ptr<ClipModel> clip = model->getClipPtr(m_cid);
        QMap<QString, QString> values = clip->getRemapValues();
        if (values.value(QLatin1String("time_map")) == m_view->getKeyframesData()) {
            // Resize was triggered by our keyframe move, nothing to do
            return;
        }
        // Reload keyframes
        m_lastLength = newDuration;
        int min = pCore->getItemIn(oid);
        m_remapLink->set("time_map", values.value(QLatin1String("time_map")).toUtf8().constData());
        m_view->m_remapProps.inherit(*m_remapLink.get());
        // This is a fake query to force the animation to be parsed
        (void)m_view->m_remapProps.anim_get_double("time_map", 0);
        m_view->m_startPos = pCore->getItemPosition(oid);
        m_in->setRange(0, m_view->m_maxLength - min);
        m_out->setRange(0, INT_MAX);
        m_view->loadKeyframes(values.value(QLatin1String("time_map")));
        m_view->update();
    }
}

void TimeRemap::selectedClip(int cid, const QUuid uuid)
{
    if (cid == -1 && cid == m_cid) {
        warningMessage->hide();
        return;
    }
    QObject::disconnect(m_seekConnection1);
    QObject::disconnect(m_seekConnection3);
    disconnect(pCore->getMonitor(Kdenlive::ClipMonitor), &Monitor::seekRemap, m_view, &RemapView::slotSetPosition);
    disconnect(pCore->getMonitor(Kdenlive::ProjectMonitor), &Monitor::seekPosition, this, &TimeRemap::monitorSeek);
    if (!m_uuid.isNull()) {
        std::shared_ptr<TimelineItemModel> previousModel = pCore->currentDoc()->getTimeline(m_uuid);
        disconnect(previousModel.get(), &TimelineItemModel::dataChanged, this, &TimeRemap::checkClipUpdate);
    }
    m_cid = cid;
    m_uuid = uuid;
    m_remapLink.reset();
    m_splitRemap.reset();
    if (cid == -1 || uuid.isNull()) {
        m_binId.clear();
        m_view->setDuration(nullptr, -1);
        remap_box->setEnabled(false);
        return;
    }
    m_view->m_remapProps.set("_profile", pCore->getProjectProfile().get_profile(), 0);
    connect(pCore->getMonitor(Kdenlive::ClipMonitor), &Monitor::seekRemap, m_view, &RemapView::slotSetPosition, Qt::UniqueConnection);
    std::shared_ptr<TimelineItemModel> model = pCore->currentDoc()->getTimeline(uuid);
    m_binId = model->getClipBinId(cid);
    std::shared_ptr<Mlt::Producer> prod = model->getClipProducer(cid);
    // Check for B Frames and warn
    if (prod->parent().get_int("meta.media.has_b_frames") == 1) {
        m_view->setDuration(nullptr, -1);
        remap_box->setEnabled(false);
        warningMessage->setText(i18n("Time remap does not work on clip with B frames."));
        warningMessage->animatedShow();
        return;
    } else {
        warningMessage->hide();
        remap_box->setEnabled(true);
    }
    m_splitId = model->m_groups->getSplitPartner(cid);
    ObjectId oid(KdenliveObjectType::TimelineClip, cid, m_uuid);
    m_lastLength = pCore->getItemDuration(oid);
    m_view->m_startPos = pCore->getItemPosition(oid);
    model->requestClipTimeRemap(cid);
    connect(model.get(), &TimelineItemModel::dataChanged, this, &TimeRemap::checkClipUpdate);
    m_view->m_maxLength = prod->get_length();
    m_in->setRange(0, m_view->m_maxLength - prod->get_in());
    // m_in->setRange(0, m_lastLength - 1);
    m_out->setRange(0, INT_MAX);
    m_view->setDuration(prod, m_lastLength, prod->parent().get_length());
    bool remapFound = false;
    if (prod->parent().type() == mlt_service_chain_type) {
        Mlt::Chain fromChain(prod->parent());
        int count = fromChain.link_count();
        QScopedPointer<Mlt::Link> fromLink;
        for (int i = 0; i < count; i++) {
            fromLink.reset(fromChain.link(i));
            if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
                if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                    // Found a timeremap effect, read params
                    if (!fromLink->property_exists("time_map")) {
                        fromLink->set("time_map", fromLink->get("map"));
                    }
                    QString mapData(fromLink->get("time_map"));
                    m_remapLink.reset(fromChain.link(i));
                    m_view->m_remapProps.inherit(*m_remapLink.get());
                    // This is a fake query to force the animation to be parsed
                    (void)m_view->m_remapProps.anim_get_double("time_map", 0, m_lastLength + prod->get_in());
                    m_view->loadKeyframes(mapData);
                    if (mapData.isEmpty()) {
                        // We are just adding the remap effect, set default params
                        if (model->clipIsAudio(cid)) {
                            fromLink->set("pitch", 1);
                        }
                        fromLink->set("image_mode", "nearest");
                    }
                    remapFound = true;
                    break;
                }
            }
        }
        if (remapFound && m_splitId > -1) {
            std::shared_ptr<Mlt::Producer> prod2 = model->getClipProducer(m_splitId);
            if (prod2->parent().type() == mlt_service_chain_type) {
                Mlt::Chain fromChain2(prod2->parent());
                count = fromChain2.link_count();
                for (int j = 0; j < count; j++) {
                    QScopedPointer<Mlt::Link> fromLink2(fromChain2.link(j));
                    if (fromLink2 && fromLink2->is_valid() && fromLink2->get("mlt_service")) {
                        if (fromLink2->get("mlt_service") == QLatin1String("timeremap")) {
                            m_splitRemap.reset(fromChain2.link(j));
                            break;
                        }
                    }
                }
            }
        }
        if (remapFound) {
            QSignalBlocker bk(pitch_compensate);
            QSignalBlocker bk2(frame_blending);
            pitch_compensate->setChecked(m_remapLink->get_int("pitch") == 1 || (m_splitRemap && m_splitRemap->get_int("pitch")));
            frame_blending->setChecked(m_remapLink->get("image_mode") != QLatin1String("nearest"));
            remap_box->setEnabled(true);
        }
    } else {
        qDebug() << "/// PRODUCER IS NOT A CHAIN!!!!";
    }
    if (!m_binId.isEmpty() && pCore->getMonitor(Kdenlive::ClipMonitor)->activeClipId() == m_binId) {
        connect(pCore->getMonitor(Kdenlive::ClipMonitor), &Monitor::seekPosition, pCore->getMonitor(Kdenlive::ClipMonitor), &Monitor::seekRemap,
                Qt::UniqueConnection);
    }
    m_seekConnection1 = connect(m_view, &RemapView::seekToPos, this, [this](int topPos, int bottomPos) {
        if (topPos > -1) {
            if (pCore->getMonitor(Kdenlive::ClipMonitor)->activeClipId() != m_binId) {
                int min = pCore->getItemIn(ObjectId(KdenliveObjectType::TimelineClip, m_cid, m_uuid));
                int lastLength = pCore->getItemDuration(ObjectId(KdenliveObjectType::TimelineClip, m_cid, m_uuid));
                int max = min + lastLength;
                pCore->selectBinClip(m_binId, true, min, {min, max});
            }
            pCore->getMonitor(Kdenlive::ClipMonitor)->requestSeekIfVisible(topPos);
        }
        if (bottomPos > -1) {
            pCore->getMonitor(Kdenlive::ProjectMonitor)->requestSeek(bottomPos + m_view->m_startPos);
        }
    });
    connect(pCore->getMonitor(Kdenlive::ProjectMonitor), &Monitor::seekPosition, this, &TimeRemap::monitorSeek, Qt::UniqueConnection);
}

void TimeRemap::monitorSeek(int pos)
{
    m_view->slotSetBottomPosition(pos - m_view->m_startPos);
}

void TimeRemap::setClip(std::shared_ptr<ProjectClip> clip, int in, int out)
{
    if (m_cid > -1 && clip == nullptr) {
        return;
    }
    QObject::disconnect(m_seekConnection1);
    QObject::disconnect(m_seekConnection2);
    QObject::disconnect(m_seekConnection3);
    m_cid = -1;
    m_splitId = -1;
    m_uuid = QUuid();
    m_binId.clear();
    m_remapLink.reset();
    m_splitRemap.reset();
    if (clip == nullptr || !clip->statusReady() || clip->clipType() != ClipType::Playlist) {
        m_view->setDuration(nullptr, -1);
        remap_box->setEnabled(false);
        return;
    }
    bool keyframesLoaded = false;
    int min = in == -1 ? 0 : in;
    int max = out == -1 ? clip->getFramePlaytime() : out;
    m_in->setRange(0, max - min);
    m_out->setRange(min, INT_MAX);
    m_view->m_startPos = 0;
    m_view->setBinClipDuration(clip, max - min);
    if (clip->clipType() == ClipType::Playlist) {
        std::unique_ptr<Mlt::Service> service(clip->originalProducer()->producer());
        qDebug() << "==== producer type: " << service->type();
        if (service->type() == mlt_service_multitrack_type) {
            Mlt::Multitrack multi(*service.get());
            for (int i = 0; i < multi.count(); i++) {
                std::unique_ptr<Mlt::Producer> track(multi.track(i));
                qDebug() << "==== GOT TRACK TYPE: " << track->type();
                switch (track->type()) {
                case mlt_service_chain_type: {
                    Mlt::Chain fromChain(*track.get());
                    int count = fromChain.link_count();
                    for (int j = 0; j < count; j++) {
                        QScopedPointer<Mlt::Link> fromLink(fromChain.link(j));
                        if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
                            if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                                // Found a timeremap effect, read params
                                if (!fromLink->property_exists("time_map")) {
                                    fromLink->set("time_map", fromLink->get("map"));
                                }
                                m_remapLink.reset(fromChain.link(j));
                                m_view->m_remapProps.inherit(*m_remapLink.get());
                                // This is a fake query to force the animation to be parsed
                                (void)m_view->m_remapProps.anim_get_double("time_map", 0);
                                QString mapData(fromLink->get("time_map"));
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
                    int count = local_playlist.count();
                    qDebug() << "==== PLAYLIST COUNT: " << count;
                    if (count == 1) {
                        std::unique_ptr<Mlt::Producer> playlistEntry(local_playlist.get_clip(0));
                        Mlt::Producer prod = playlistEntry->parent();
                        qDebug() << "==== GOT PROD TYPE: " << prod.type() << " = " << prod.get("mlt_service") << " = " << prod.get("resource");
                        if (prod.type() == mlt_service_chain_type) {
                            Mlt::Chain fromChain(prod);
                            int linkCount = fromChain.link_count();
                            for (int j = 0; j < linkCount; j++) {
                                QScopedPointer<Mlt::Link> fromLink(fromChain.link(j));
                                if (fromLink && fromLink->is_valid() && fromLink->get("mlt_service")) {
                                    if (fromLink->get("mlt_service") == QLatin1String("timeremap")) {
                                        // Found a timeremap effect, read params
                                        if (!fromLink->property_exists("time_map")) {
                                            fromLink->set("time_map", fromLink->get("map"));
                                        }
                                        m_remapLink.reset(fromChain.link(j));
                                        m_view->m_remapProps.inherit(*m_remapLink.get());
                                        (void)m_view->m_remapProps.anim_get_double("time_map", 0);
                                        QString mapData(fromLink->get("time_map"));
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
                    qDebug() << "=== UNHANDLED TRACK TYPE";
                    break;
                }
            }
        }
    }
    if (!keyframesLoaded) {
        m_view->loadKeyframes(QString());
    }
    m_seekConnection1 = connect(m_view, &RemapView::seekToPos, pCore->getMonitor(Kdenlive::ClipMonitor), &Monitor::requestSeek, Qt::UniqueConnection);
    m_seekConnection2 = connect(pCore->getMonitor(Kdenlive::ClipMonitor), &Monitor::seekPosition, this, [&](int pos) { m_view->slotSetPosition(pos); });
    remap_box->setEnabled(m_remapLink != nullptr);
}

void TimeRemap::updateKeyframes()
{
    QString kfData = m_view->getKeyframesData();
    if (m_remapLink) {
        m_remapLink->set("time_map", kfData.toUtf8().constData());
        m_view->m_remapProps.inherit(*m_remapLink.get());
        (void)m_view->m_remapProps.anim_get_double("time_map", 0);
        if (m_splitRemap) {
            m_splitRemap->set("time_map", kfData.toUtf8().constData());
        }
        if (m_cid == -1) {
            // This is a playlist clip
            m_view->timer.start();
        }
    }
}

void TimeRemap::updateKeyframesWithUndo(const QMap<int, int> &updatedKeyframes, const QMap<int, int> &previousKeyframes)
{
    if (m_remapLink == nullptr) {
        return;
    }
    bool usePitch = pitch_compensate->isChecked();
    bool useBlend = frame_blending->isChecked();
    bool hadPitch = m_remapLink->get_int("pitch") == 1;
    bool splitHadPitch = m_splitRemap && m_splitRemap->get_int("pitch") == 1;
    bool hadBlend = m_remapLink->get("image_mode") != QLatin1String("nearest");
    std::shared_ptr<TimelineItemModel> model = pCore->currentDoc()->getTimeline(m_uuid);
    bool masterIsAudio = model->clipIsAudio(m_cid);
    bool splitIsAudio = model->clipIsAudio(m_splitId);
    ObjectId oid(KdenliveObjectType::TimelineClip, m_cid, m_uuid);
    bool durationChanged = updatedKeyframes.isEmpty() ? false : updatedKeyframes.lastKey() - pCore->getItemIn(oid) + 1 != pCore->getItemDuration(oid);
    int lastFrame = pCore->getItemDuration(oid) + pCore->getItemIn(oid);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    if (durationChanged) {
        // Resize first so that serialization doesn't cut keyframes
        int length = updatedKeyframes.lastKey() - m_view->m_inFrame + 1;
        std::shared_ptr<TimelineItemModel> model = pCore->currentDoc()->getTimeline(m_uuid);
        model->requestItemResize(m_cid, length, true, true, undo, redo);
        if (m_splitId > 0) {
            model->requestItemResize(m_splitId, length, true, true, undo, redo);
        }
    }

    Fun local_undo = [this, link = m_remapLink, splitLink = m_splitRemap, previousKeyframes, cid = m_cid, oldIn = m_view->m_oldInFrame, hadPitch, splitHadPitch,
                      masterIsAudio, splitIsAudio, hadBlend]() {
        QString oldKfData;
        bool keyframesChanged = false;
        if (!previousKeyframes.isEmpty()) {
            oldKfData = m_view->getKeyframesData(previousKeyframes);
            keyframesChanged = true;
        }
        if (keyframesChanged) {
            link->set("time_map", oldKfData.toUtf8().constData());
        }
        if (masterIsAudio) {
            link->set("pitch", hadPitch ? 1 : 0);
        }
        link->set("image_mode", hadBlend ? "blend" : "nearest");
        if (splitLink) {
            if (keyframesChanged) {
                splitLink->set("time_map", oldKfData.toUtf8().constData());
            }
            if (splitIsAudio) {
                splitLink->set("pitch", splitHadPitch ? 1 : 0);
            }
            splitLink->set("image_mode", hadBlend ? "blend" : "nearest");
        }
        if (cid == m_cid) {
            QSignalBlocker bk(pitch_compensate);
            QSignalBlocker bk2(frame_blending);
            pitch_compensate->setChecked(hadPitch);
            frame_blending->setChecked(hadBlend);
            if (keyframesChanged) {
                m_lastLength = previousKeyframes.lastKey() - oldIn;
                // This clip is currently displayed in remap view
                link->set("time_map", oldKfData.toUtf8().constData());
                m_view->m_remapProps.inherit(*link.get());
                (void)m_view->m_remapProps.anim_get_double("time_map", 0);
                m_view->loadKeyframes(oldKfData);
                update();
            }
        }
        return true;
    };

    Fun local_redo = [this, link = m_remapLink, splitLink = m_splitRemap, updatedKeyframes, cid = m_cid, usePitch, masterIsAudio, splitIsAudio,
                      in = m_view->m_inFrame, useBlend]() {
        QString newKfData;
        bool keyframesChanged = false;
        if (!updatedKeyframes.isEmpty()) {
            newKfData = m_view->getKeyframesData(updatedKeyframes);
            keyframesChanged = true;
        }
        if (keyframesChanged) {
            link->set("time_map", newKfData.toUtf8().constData());
        }
        if (masterIsAudio) {
            link->set("pitch", usePitch ? 1 : 0);
        }
        link->set("image_mode", useBlend ? "blend" : "nearest");
        if (splitLink) {
            if (keyframesChanged) {
                splitLink->set("time_map", newKfData.toUtf8().constData());
            }
            if (splitIsAudio) {
                splitLink->set("pitch", usePitch ? 1 : 0);
            }
            splitLink->set("image_mode", useBlend ? "blend" : "nearest");
        }
        if (cid == m_cid) {
            QSignalBlocker bk(pitch_compensate);
            QSignalBlocker bk2(frame_blending);
            pitch_compensate->setChecked(usePitch);
            frame_blending->setChecked(useBlend);
            if (keyframesChanged) {
                // This clip is currently displayed in remap view
                m_lastLength = updatedKeyframes.lastKey() - in;
                link->set("time_map", newKfData.toUtf8().constData());
                m_view->m_remapProps.inherit(*link.get());
                (void)m_view->m_remapProps.anim_get_double("time_map", 0);
                m_view->loadKeyframes(newKfData);
                update();
            }
        }
        return true;
    };
    local_redo();
    UPDATE_UNDO_REDO_NOLOCK(redo, undo, local_undo, local_redo);
    pCore->pushUndo(local_undo, local_redo, i18n("Edit Timeremap keyframes"));
}

void TimeRemap::switchRemapParam()
{
    updateKeyframesWithUndo(QMap<int, int>(), QMap<int, int>());
}

bool TimeRemap::isInRange() const
{
    return m_cid != -1 && m_view->isInRange();
}

TimeRemap::~TimeRemap()
{
    // delete m_previewTimer;
}
