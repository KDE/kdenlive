/*
    SPDX-FileCopyrightText: 2011 Till Theato <root@ttill.de>
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "keyframeview.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "core.h"
#include "kdenlivesettings.h"

#include <QApplication>
#include <QMouseEvent>
#include <QStylePainter>
#include <QtMath>

#include <KColorScheme>
#include <QFontDatabase>
#include <utility>

KeyframeView::KeyframeView(std::shared_ptr<KeyframeModelList> model, int duration, QWidget *parent)
    : QWidget(parent)
    , m_model(std::move(model))
    , m_duration(duration)
    , m_position(0)
    , m_currentKeyframeOriginal(-1)
    , m_hoverKeyframe(-1)
    , m_scale(1)
    , m_zoomFactor(1)
    , m_zoomStart(0)
    , m_moveKeyframeMode(false)
    , m_keyframeZonePress(false)
    , m_clickPoint(-1)
    , m_clickEnd(-1)
    , m_zoomHandle(0, 1)
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
    m_lineHeight = int(m_size / 2.1);
    m_zoomHeight = m_size * 3 / 4;
    m_offset = m_size / 4;
    setFixedHeight(m_size);
    setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
    connect(m_model.get(), &KeyframeModelList::modelChanged, this, &KeyframeView::slotModelChanged);
    connect(m_model.get(), &KeyframeModelList::modelDisplayChanged, this, &KeyframeView::slotModelDisplayChanged);
    m_centerConnection = connect(this, &KeyframeView::updateKeyframeOriginal, this, [&](int pos) {
        m_currentKeyframeOriginal = pos;
        update();
    });
}

KeyframeView::~KeyframeView()
{
    QObject::disconnect(m_centerConnection);
}

void KeyframeView::slotModelChanged()
{
    int offset = pCore->getItemIn(m_model->getOwnerId());
    Q_EMIT atKeyframe(m_model->hasKeyframe(m_position + offset), m_model->singleKeyframe());
    Q_EMIT modified();
    update();
}

void KeyframeView::slotModelDisplayChanged()
{
    int offset = pCore->getItemIn(m_model->getOwnerId());
    Q_EMIT atKeyframe(m_model->hasKeyframe(m_position + offset), m_model->singleKeyframe());
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
        Q_EMIT atKeyframe(m_model->hasKeyframe(pos + offset), m_model->singleKeyframe());
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

void KeyframeView::initKeyframePos()
{
    int offset = pCore->getItemIn(m_model->getOwnerId());
    Q_EMIT atKeyframe(m_model->hasKeyframe(m_position + offset), m_model->singleKeyframe());
}

const QVector<int> KeyframeView::selectedKeyframesIndexes()
{
    return m_model->selectedKeyframes();
}

void KeyframeView::slotDuplicateKeyframe()
{
    int offset = pCore->getItemIn(m_model->getOwnerId());
    if (m_model->activeKeyframe() > -1 && !m_model->hasKeyframe(m_position + offset)) {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        int delta = offset + m_position - m_model->getPosAtIndex(m_model->activeKeyframe()).frames(pCore->getCurrentFps());
        for (int &kf : m_model->selectedKeyframes()) {
            int kfrPos = m_model->getPosAtIndex(kf).frames(pCore->getCurrentFps());
            m_model->duplicateKeyframeWithUndo(GenTime(kfrPos, pCore->getCurrentFps()), GenTime(kfrPos + delta, pCore->getCurrentFps()), undo, redo);
        }
        pCore->pushUndo(undo, redo, i18n("Duplicate keyframe"));
    }
}

bool KeyframeView::slotAddKeyframe(int pos)
{
    if (pos < 0) {
        pos = m_position;
    }
    int offset = pCore->getItemIn(m_model->getOwnerId());
    return m_model->addKeyframe(GenTime(pos + offset, pCore->getCurrentFps()), KeyframeType(KdenliveSettings::defaultkeyframeinterp()));
}

const QString KeyframeView::getAssetId()
{
    return m_model->getAssetId();
}

void KeyframeView::slotAddRemove()
{
    Q_EMIT activateEffect();
    int offset = pCore->getItemIn(m_model->getOwnerId());
    if (m_model->hasKeyframe(m_position + offset)) {
        if (m_model->selectedKeyframes().contains(m_position)) {
            // Delete all selected keyframes
            slotRemoveKeyframe(m_model->selectedKeyframes());
        } else {
            slotRemoveKeyframe({m_position});
        }
    } else {
        if (slotAddKeyframe(m_position)) {
            GenTime position(m_position + offset, pCore->getCurrentFps());
            int currentIx = m_model->getIndexForPos(position);
            if (currentIx > -1) {
                m_model->setSelectedKeyframes({currentIx});
                m_model->setActiveKeyframe(currentIx);
            }
        }
    }
}

void KeyframeView::slotEditType(int type, const QPersistentModelIndex &index)
{
    int offset = pCore->getItemIn(m_model->getOwnerId());
    if (m_model->hasKeyframe(m_position + offset)) {
        m_model->updateKeyframeType(GenTime(m_position + offset, pCore->getCurrentFps()), type, index);
    }
}

void KeyframeView::slotRemoveKeyframe(const QVector<int> &positions)
{
    if (m_model->singleKeyframe()) {
        // Don't allow zero keyframe
        pCore->displayMessage(i18n("Cannot remove the last keyframe"), MessageType::ErrorMessage, 500);
        return;
    }
    int offset = pCore->getItemIn(m_model->getOwnerId());
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    for (int pos : positions) {
        if (pos == 0) {
            // Don't allow moving first keyframe
            continue;
        }
        m_model->removeKeyframeWithUndo(GenTime(pos + offset, pCore->getCurrentFps()), undo, redo);
    }
    pCore->pushUndo(undo, redo, i18np("Remove keyframe", "Remove keyframes", positions.size()));
}

void KeyframeView::setDuration(int duration)
{
    m_duration = duration;
    int offset = pCore->getItemIn(m_model->getOwnerId());
    Q_EMIT atKeyframe(m_model->hasKeyframe(m_position + offset), m_model->singleKeyframe());
    // Unselect keyframes that are outside range if any
    QVector<int> toDelete;
    int kfrIx = 0;
    for (auto &p : m_model->selectedKeyframes()) {
        int kfPos = m_model->getPosAtIndex(p).frames(pCore->getCurrentFps());
        if (kfPos < offset || kfPos >= offset + m_duration) {
            toDelete << kfrIx;
        }
        kfrIx++;
    }
    for (auto &p : toDelete) {
        m_model->removeFromSelected(p);
    }
    update();
}

void KeyframeView::slotGoToNext()
{
    Q_EMIT activateEffect();
    if (m_position == m_duration - 1) {
        return;
    }

    bool ok;
    int offset = pCore->getItemIn(m_model->getOwnerId());
    auto next = m_model->getNextKeyframe(GenTime(m_position + offset, pCore->getCurrentFps()), &ok);

    if (ok) {
        Q_EMIT seekToPos(qMin(int(next.first.frames(pCore->getCurrentFps())) - offset, m_duration - 1));
    } else {
        // no keyframe after current position
        Q_EMIT seekToPos(m_duration - 1);
    }
}

void KeyframeView::slotGoToPrev()
{
    Q_EMIT activateEffect();
    if (m_position == 0) {
        return;
    }

    bool ok;
    int offset = pCore->getItemIn(m_model->getOwnerId());
    auto prev = m_model->getPrevKeyframe(GenTime(m_position + offset, pCore->getCurrentFps()), &ok);

    if (ok) {
        Q_EMIT seekToPos(qMax(0, int(prev.first.frames(pCore->getCurrentFps())) - offset));
    } else {
        // no keyframe after current position
        Q_EMIT seekToPos(m_duration - 1);
    }
}

void KeyframeView::slotCenterKeyframe()
{
    if (m_currentKeyframeOriginal == -1 || m_currentKeyframeOriginal == m_position || m_currentKeyframeOriginal == 0) {
        return;
    }
    int offset = pCore->getItemIn(m_model->getOwnerId());
    if (!m_model->hasKeyframe(m_currentKeyframeOriginal)) {
        return;
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    int delta = m_position - (m_currentKeyframeOriginal - offset);
    int sourcePosition = m_currentKeyframeOriginal;
    QVector<int> updatedSelection;
    for (int &kf : m_model->selectedKeyframes()) {
        if (kf == 0) {
            // Don't allow moving first keyframe
            updatedSelection << 0;
            continue;
        }
        int kfPos = m_model->getPosAtIndex(kf).frames(pCore->getCurrentFps());
        GenTime initPos(kfPos, pCore->getCurrentFps());
        GenTime targetPos(kfPos + delta, pCore->getCurrentFps());
        m_model->moveKeyframeWithUndo(initPos, targetPos, undo, redo);
        break;
    }
    Fun local_redo = [this, position = m_position]() {
        Q_EMIT updateKeyframeOriginal(position);
        return true;
    };
    Fun local_undo = [this, sourcePosition]() {
        Q_EMIT updateKeyframeOriginal(sourcePosition);
        return true;
    };
    local_redo();
    PUSH_LAMBDA(local_redo, redo);
    PUSH_FRONT_LAMBDA(local_undo, undo);
    pCore->pushUndo(undo, redo, i18nc("@action", "Move keyframe"));
}

void KeyframeView::mousePressEvent(QMouseEvent *event)
{
    Q_EMIT activateEffect();
    int offset = pCore->getItemIn(m_model->getOwnerId());
    double zoomStart = m_zoomHandle.x() * (width() - 2 * m_offset);
    double zoomEnd = m_zoomHandle.y() * (width() - 2 * m_offset);
    double zoomFactor = (width() - 2 * m_offset) / (zoomEnd - zoomStart);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    int pos = int(((event->x() - m_offset) / zoomFactor + zoomStart) / m_scale);
#else
    int pos = int(((event->position().x() - m_offset) / zoomFactor + zoomStart) / m_scale);
#endif
    pos = qBound(0, pos, m_duration - 1);
    m_moveKeyframeMode = false;
    m_keyframeZonePress = false;
    if (event->button() == Qt::LeftButton) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        if (event->y() < m_lineHeight) {
#else
        if (event->position().y() < m_lineHeight) {
#endif
            // mouse click in keyframes area
            bool ok;
            GenTime position(pos + offset, pCore->getCurrentFps());
            if (event->modifiers() & Qt::ShiftModifier) {
                m_clickPoint = pos;
                return;
            }
            m_keyframeZonePress = true;
            auto keyframe = m_model->getClosestKeyframe(position, &ok);
            if (ok && qAbs(keyframe.first.frames(pCore->getCurrentFps()) - pos - offset) * m_scale * m_zoomFactor < QApplication::startDragDistance()) {
                int currentIx = m_model->getIndexForPos(keyframe.first);
                m_currentKeyframeOriginal = keyframe.first.frames(pCore->getCurrentFps());
                if (event->modifiers() & Qt::ControlModifier) {
                    if (m_model->selectedKeyframes().contains(currentIx)) {
                        m_model->removeFromSelected(currentIx);
                        if (m_model->activeKeyframe() == currentIx) {
                            m_model->setActiveKeyframe(-1);
                        }
                        m_currentKeyframeOriginal = -1;
                    } else {
                        m_model->appendSelectedKeyframe(currentIx);
                        m_model->setActiveKeyframe(currentIx);
                    }
                } else if (!m_model->selectedKeyframes().contains(currentIx)) {
                    m_model->setSelectedKeyframes({currentIx});
                    m_model->setActiveKeyframe(currentIx);
                } else {
                    m_model->setActiveKeyframe(currentIx);
                }
                // Select and seek to keyframe
                if (m_currentKeyframeOriginal > -1) {
                    if (KdenliveSettings::keyframeseek()) {
                        Q_EMIT seekToPos(m_currentKeyframeOriginal - offset);
                    }
                }
                return;
            }
            // no keyframe next to mouse
            m_model->setSelectedKeyframes({});
            m_model->setActiveKeyframe(-1);
            m_currentKeyframeOriginal = -1;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        } else if (event->y() > m_zoomHeight + 2) {
#else
        } else if (event->position().y() > m_zoomHeight + 2) {
#endif
            // click on zoom area
            if (m_hoverZoom) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                m_clickOffset = (double(event->x()) - m_offset) / (width() - 2 * m_offset);
#else
                m_clickOffset = (double(event->position().x()) - m_offset) / (width() - 2 * m_offset);
#endif
            }
            // When not zoomed, allow seek by clicking on zoombar
            if (qFuzzyCompare(m_zoomFactor, 1.) && pos != m_position && !m_hoverZoomIn && !m_hoverZoomOut) {
                Q_EMIT seekToPos(pos);
            }
            return;
        }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    } else if (event->button() == Qt::RightButton && event->y() > m_zoomHeight + 2) {
#else
    } else if (event->button() == Qt::RightButton && event->position().y() > m_zoomHeight + 2) {
#endif
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
        Q_EMIT seekToPos(pos);
        update();
    }
}

void KeyframeView::mouseMoveEvent(QMouseEvent *event)
{
    int offset = pCore->getItemIn(m_model->getOwnerId());
    double zoomStart = m_zoomHandle.x() * (width() - 2 * m_offset);
    double zoomEnd = m_zoomHandle.y() * (width() - 2 * m_offset);
    double zoomFactor = (width() - 2 * m_offset) / (zoomEnd - zoomStart);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    int pos = int(((double(event->x()) - m_offset) / zoomFactor + zoomStart) / m_scale);
#else
    int pos = int(((double(event->position().x()) - m_offset) / zoomFactor + zoomStart) / m_scale);
#endif
    pos = qBound(0, pos, m_duration - 1);
    GenTime position(pos + offset, pCore->getCurrentFps());
    if ((event->buttons() & Qt::LeftButton) != 0u) {
        if (m_hoverZoomIn || m_hoverZoomOut || m_hoverZoom) {
            // Moving zoom handles
            if (m_hoverZoomIn) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                m_zoomHandle.setX(qMin(qMax(0., double(event->x() - m_offset) / (width() - 2 * m_offset)), m_zoomHandle.y() - 0.015));
#else
                m_zoomHandle.setX(qMin(qMax(0., double(event->position().x() - m_offset) / (width() - 2 * m_offset)), m_zoomHandle.y() - 0.015));
#endif
                update();
                return;
            }
            if (m_hoverZoomOut) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                m_zoomHandle.setY(qMax(qMin(1., double(event->x() - m_offset) / (width() - 2 * m_offset)), m_zoomHandle.x() + 0.015));
#else
                m_zoomHandle.setY(qMax(qMin(1., double(event->position().x() - m_offset) / (width() - 2 * m_offset)), m_zoomHandle.x() + 0.015));
#endif
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
                update();
            }
            return;
        }
        if (m_model->activeKeyframe() == pos) {
            return;
        }
        if (m_model->activeKeyframe() > 0 && m_currentKeyframeOriginal > -1 && m_clickPoint == -1 &&
            (m_moveKeyframeMode ||
             (qAbs(pos - (m_currentKeyframeOriginal - offset)) * m_scale * m_zoomFactor < QApplication::startDragDistance() && m_keyframeZonePress))) {
            m_moveKeyframeMode = true;
            if (!m_model->hasKeyframe(pos + offset)) {
                int delta = pos - (m_model->getPosAtIndex(m_model->activeKeyframe()).frames(pCore->getCurrentFps()) - offset);
                // Check that the move is possible
                for (int &kf : m_model->selectedKeyframes()) {
                    int updatedPos = m_model->getPosAtIndex(kf).frames(pCore->getCurrentFps()) + delta;
                    if (m_model->hasKeyframe(updatedPos)) {
                        // Don't allow moving over another keyframe
                        return;
                    }
                }
                for (int &kf : m_model->selectedKeyframes()) {
                    if (kf == 0) {
                        // Don't allow moving first keyframe
                        continue;
                    }
                    int kfPos = m_model->getPosAtIndex(kf).frames(pCore->getCurrentFps());
                    GenTime currentPos(kfPos, pCore->getCurrentFps());
                    GenTime updatedPos(kfPos + delta, pCore->getCurrentFps());
                    if (!m_model->moveKeyframe(currentPos, updatedPos, false)) {
                        qDebug() << "=== FAILED KF MOVE!!!";
                        Q_ASSERT(false);
                    }
                    // We only move first keyframe, the other are moved in the model command
                    break;
                }
            }
        }
        // Rubberband selection
        if (m_clickPoint >= 0) {
            m_clickEnd = pos;
            int min = qMin(m_clickPoint, m_clickEnd);
            int max = qMax(m_clickPoint, m_clickEnd);
            min = qMax(1, min);
            m_model->setSelectedKeyframes({});
            m_model->setActiveKeyframe(-1);
            m_currentKeyframeOriginal = -1;
            double fps = pCore->getCurrentFps();
            int kfrIx = 0;
            for (const auto &keyframe : *m_model.get()) {
                int kfPos = keyframe.first.frames(fps) - offset;
                if (kfPos > min && kfPos <= max) {
                    m_model->appendSelectedKeyframe(kfrIx);
                }
                kfrIx++;
            }
            if (!m_model->selectedKeyframes().isEmpty()) {
                m_model->setActiveKeyframe(m_model->selectedKeyframes().first());
                m_currentKeyframeOriginal = m_model->getPosAtIndex(m_model->selectedKeyframes().first()).frames(pCore->getCurrentFps());
            }
            update();
            return;
        }

        if (!m_moveKeyframeMode || KdenliveSettings::keyframeseek()) {
            if (pos != m_position) {
                Q_EMIT seekToPos(pos);
            }
        }
        return;
    }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (event->y() < m_lineHeight) {
#else
    if (event->position().y() < m_lineHeight) {
#endif
        bool ok;
        auto keyframe = m_model->getClosestKeyframe(position, &ok);
        if (ok && qAbs(((position.frames(pCore->getCurrentFps()) - keyframe.first.frames(pCore->getCurrentFps())) * m_scale) * m_zoomFactor) <
                      QApplication::startDragDistance()) {
            m_hoverKeyframe = keyframe.first.frames(pCore->getCurrentFps()) - offset;
            setCursor(Qt::PointingHandCursor);
            setToolTip(KeyframeModel::getKeyframeTypes().value(keyframe.second));
            m_hoverZoomIn = false;
            m_hoverZoomOut = false;
            m_hoverZoom = false;
            update();
            return;
        }
    }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (event->y() > m_zoomHeight + 2) {
        // Moving in zoom area
        if (qAbs(event->x() - m_offset - (m_zoomHandle.x() * (width() - 2 * m_offset))) < QApplication::startDragDistance()) {
#else
    if (event->position().y() > m_zoomHeight + 2) {
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
    m_clickPoint = -1;
    if (m_clickEnd >= 0) {
        m_clickEnd = -1;
        update();
    }
    if (m_moveKeyframeMode && m_model->activeKeyframe() >= 0 &&
        m_currentKeyframeOriginal != m_model->getPosAtIndex(m_model->activeKeyframe()).frames(pCore->getCurrentFps())) {
        int delta = m_model->getPosAtIndex(m_model->activeKeyframe()).frames(pCore->getCurrentFps()) - m_currentKeyframeOriginal;
        // Move back all keyframes to their initial positions
        for (int &kf : m_model->selectedKeyframes()) {
            if (kf == 0) {
                // Don't allow moving first keyframe
                continue;
            }
            int kfPos = m_model->getPosAtIndex(kf).frames(pCore->getCurrentFps());
            GenTime initPos(kfPos - delta, pCore->getCurrentFps());
            GenTime targetPos(kfPos, pCore->getCurrentFps());
            m_model->moveKeyframe(targetPos, initPos, false, true);
            break;
        }
        // Move all keyframes to their new positions
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        for (int &kf : m_model->selectedKeyframes()) {
            if (kf == 0) {
                // Don't allow moving first keyframe
                continue;
            }
            int kfPos = m_model->getPosAtIndex(kf).frames(pCore->getCurrentFps());
            GenTime initPos(kfPos, pCore->getCurrentFps());
            GenTime targetPos(kfPos + delta, pCore->getCurrentFps());
            m_model->moveKeyframeWithUndo(initPos, targetPos, undo, redo);
            break;
        }
        m_currentKeyframeOriginal = m_model->getPosAtIndex(m_model->activeKeyframe()).frames(pCore->getCurrentFps());
        pCore->pushUndo(undo, redo, i18np("Move keyframe", "Move keyframes", m_model->selectedKeyframes().size()));
        qDebug() << "RELEASING keyframe move" << delta;
    }
    m_moveKeyframeMode = false;
    m_keyframeZonePress = false;
}

void KeyframeView::mouseDoubleClickEvent(QMouseEvent *event)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    if (event->button() == Qt::LeftButton && event->y() < m_lineHeight) {
#else
    if (event->button() == Qt::LeftButton && event->position().y() < m_lineHeight) {
#endif
        int offset = pCore->getItemIn(m_model->getOwnerId());
        double zoomStart = m_zoomHandle.x() * (width() - 2 * m_offset);
        double zoomEnd = m_zoomHandle.y() * (width() - 2 * m_offset);
        double zoomFactor = (width() - 2 * m_offset) / (zoomEnd - zoomStart);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        int pos = int(((event->x() - m_offset) / zoomFactor + zoomStart) / m_scale);
#else
        int pos = int(((event->position().x() - m_offset) / zoomFactor + zoomStart) / m_scale);
#endif
        pos = qBound(0, pos, m_duration - 1);
        GenTime position(pos + offset, pCore->getCurrentFps());
        bool ok;
        auto keyframe = m_model->getClosestKeyframe(position, &ok);
        if (ok && qAbs(keyframe.first.frames(pCore->getCurrentFps()) - pos - offset) * m_scale * m_zoomFactor < QApplication::startDragDistance()) {
            if (keyframe.first.frames(pCore->getCurrentFps()) != offset) {
                m_model->removeKeyframe(keyframe.first);
                m_currentKeyframeOriginal = -1;
                if (keyframe.first.frames(pCore->getCurrentFps()) == m_position + offset) {
                    Q_EMIT atKeyframe(false, m_model->singleKeyframe());
                }
            }
            return;
        }

        // add new keyframe
        m_model->addKeyframe(position, KeyframeType(KdenliveSettings::defaultkeyframeinterp()));
    } else {
        QWidget::mouseDoubleClickEvent(event);
    }
}

void KeyframeView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::AltModifier) {
        // Alt modifier seems to invert x/y axis
        if (event->angleDelta().x() > 0) {
            slotGoToPrev();
        } else {
            slotGoToNext();
        }
        event->setAccepted(true);
        return;
    }
    if (event->modifiers() & Qt::ControlModifier) {
        int maxWidth = width() - 2 * m_offset;
        m_zoomStart = m_zoomHandle.x() * maxWidth;
        m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
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
        update();
        event->setAccepted(true);
        return;
    }
    int change = event->angleDelta().y() > 0 ? -1 : 1;
    int pos = qBound(0, m_position + change, m_duration - 1);
    Q_EMIT seekToPos(pos);
    event->setAccepted(true);
}

void KeyframeView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QStylePainter p(this);
    int maxWidth = width() - 2 * m_offset;
    if (m_duration > 1) {
        m_scale = maxWidth / double(m_duration - 1);
    } else {
        m_scale = maxWidth;
    }
    int headOffset = m_lineHeight / 2;
    int offset = pCore->getItemIn(m_model->getOwnerId());
    m_zoomStart = m_zoomHandle.x() * maxWidth;
    m_zoomFactor = maxWidth / (m_zoomHandle.y() * maxWidth - m_zoomStart);
    int zoomEnd = qCeil(m_zoomHandle.y() * maxWidth);
    /* ticks */
    double fps = pCore->getCurrentFps();
    int displayedLength = int(m_duration / m_zoomFactor / fps);
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
    }

    /*
     * keyframes
     */
    int kfrIx = 0;
    QVector<int> selecteds = m_model->selectedKeyframes();
    for (const auto &keyframe : *m_model.get()) {
        int pos = keyframe.first.frames(fps) - offset;
        if (pos < 0) continue;
        double scaledPos = pos * m_scale;
        if (scaledPos < m_zoomStart || qFloor(scaledPos) > zoomEnd) {
            kfrIx++;
            continue;
        }
        if (kfrIx == m_model->activeKeyframe()) {
            p.setBrush(Qt::red);
        } else if (selecteds.contains(kfrIx)) {
            p.setBrush(Qt::darkRed);
        } else if (pos == m_hoverKeyframe) {
            p.setBrush(m_colSelected);
        } else {
            p.setBrush(m_colKeyframe);
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
        kfrIx++;
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
        if (scaledPos >= m_zoomStart && qFloor(scaledPos) <= zoomEnd) {
            scaledPos -= m_zoomStart;
            scaledPos *= m_zoomFactor;
            scaledPos += m_offset;
            QPolygon pa(3);
            int cursorwidth = int((m_zoomHeight - m_lineHeight) / 1.8);
            QPolygonF position = QPolygonF() << QPointF(-cursorwidth, m_zoomHeight - 3) << QPointF(cursorwidth, m_zoomHeight - 3)
                                             << QPointF(0, m_lineHeight + 1);
            position.translate(scaledPos, 0);
            p.setBrush(m_colKeyframe);
            p.drawPolygon(position);
        }
    }
    // Rubberband
    if (m_clickEnd >= 0) {
        int min = int((qMin(m_clickPoint, m_clickEnd) * m_scale - m_zoomStart) * m_zoomFactor + m_offset);
        int max = int((qMax(m_clickPoint, m_clickEnd) * m_scale - m_zoomStart) * m_zoomFactor + m_offset);
        p.setOpacity(0.5);
        p.fillRect(QRect(min, 0, max - min, m_lineHeight), palette().highlight());
        p.setOpacity(1);
    }

    // Zoom bar
    p.setPen(Qt::NoPen);
    p.setBrush(palette().mid());
    p.drawRoundedRect(m_offset, m_zoomHeight + 3, width() - 2 * m_offset, m_size - m_zoomHeight - 3, m_lineHeight / 5, m_lineHeight / 5);
    p.setBrush(palette().highlight());
    p.drawRoundedRect(int(m_offset + (width() - m_offset) * m_zoomHandle.x()), m_zoomHeight + 3,
                      int((width() - 2 * m_offset) * (m_zoomHandle.y() - m_zoomHandle.x())), m_size - m_zoomHeight - 3, m_lineHeight / 5, m_lineHeight / 5);
}

void KeyframeView::copyCurrentValue(const QModelIndex &ix, const QString &paramName)
{
    int offset = pCore->getItemIn(m_model->getOwnerId());
    const QString val = m_model->getInterpolatedValue(m_position + offset, ix).toString();
    QString newVal;
    const QStringList vals = val.split(QLatin1Char(' '));
    qDebug() << "=== COPYING VALS: " << val << " AT POS: " << m_position << ", PARAM NAME_ " << paramName;
    auto *parentCommand = new QUndoCommand();
    bool multiParams = paramName.contains(QLatin1Char(' '));
    for (int &kfrIx : m_model->selectedKeyframes()) {
        QString oldValue = m_model->getInterpolatedValue(m_model->getPosAtIndex(kfrIx), ix).toString();
        QStringList oldVals = oldValue.split(QLatin1Char(' '));
        bool found = false;
        if (paramName.contains(QLatin1String("spinX"))) {
            oldVals[0] = vals.at(0);
            newVal = oldVals.join(QLatin1Char(' '));
            found = true;
            if (!multiParams) {
                parentCommand->setText(i18n("Update keyframes X position"));
            }
        }
        if (paramName.contains(QLatin1String("spinY"))) {
            oldVals[1] = vals.at(1);
            newVal = oldVals.join(QLatin1Char(' '));
            found = true;
            if (!multiParams) {
                parentCommand->setText(i18n("Update keyframes Y position"));
            }
        }
        if (paramName.contains(QLatin1String("spinW"))) {
            oldVals[2] = vals.at(2);
            newVal = oldVals.join(QLatin1Char(' '));
            found = true;
            if (!multiParams) {
                parentCommand->setText(i18n("Update keyframes width"));
            }
        }
        if (paramName.contains(QLatin1String("spinH"))) {
            oldVals[3] = vals.at(3);
            newVal = oldVals.join(QLatin1Char(' '));
            found = true;
            if (!multiParams) {
                parentCommand->setText(i18n("Update keyframes height"));
            }
        }
        if (paramName.contains(QLatin1String("spinO"))) {
            oldVals[4] = vals.at(4);
            newVal = oldVals.join(QLatin1Char(' '));
            found = true;
            if (!multiParams) {
                parentCommand->setText(i18n("Update keyframes opacity"));
            }
        }
        if (!found) {
            newVal = val;
            parentCommand->setText(i18n("Update keyframes value"));
        } else if (multiParams) {
            parentCommand->setText(i18n("Update keyframes value"));
        }
        bool result = m_model->updateKeyframe(m_model->getPosAtIndex(kfrIx), newVal, -1, ix, parentCommand);
        if (result) {
            pCore->displayMessage(i18n("Keyframe value copied"), InformationMessage);
        }
    }
    pCore->pushUndo(parentCommand);
}
