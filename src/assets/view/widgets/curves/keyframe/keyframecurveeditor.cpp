/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>
    SPDX-FileCopyrightText: 2024 aisuneko icecat <iceneko@protonmail.ch>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "keyframecurveeditor.h"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "macros.hpp"
#include <complex>

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QtMath>

KeyframeCurveEditor::KeyframeCurveEditor(std::shared_ptr<KeyframeModelList> model, int duration, double min, double max, double factor,
                                         const QPersistentModelIndex &paramindex, int rectindex, QWidget *parent)
    : AbstractCurveWidget(parent)
    , m_model(std::move(model))
    , m_paramindex(paramindex)
    , m_duration(duration)
    , m_position(0)
    , m_rectindex(rectindex)
    , m_hoverKeyframe(-1)
    , m_moveKeyframeMode(false)
    , m_keyframeZonePress(false)
    , m_clickPoint(-1)
    , m_clickEnd(-1)
    , m_minLimit(min)
    , m_maxLimit(max)
    , m_factor(factor)
    , m_paddingfactor(0.1)
{
    // loadSplineFromModel();
    m_minLimit /= factor;
    m_maxLimit /= factor;
}

KeyframeCurveEditor::~KeyframeCurveEditor() = default;
void KeyframeCurveEditor::slotOnFocus()
{
    loadSplineFromModel();
    connect(m_model.get(), &KeyframeModelList::modelChanged, this, &KeyframeCurveEditor::slotModelChanged, Qt::UniqueConnection);
    connect(m_model.get(), &KeyframeModelList::modelDisplayChanged, this, &KeyframeCurveEditor::slotModelDisplayChanged);
    update();
}
void KeyframeCurveEditor::slotLoseFocus()
{
    disconnect(m_model.get(), &KeyframeModelList::modelChanged, this, &KeyframeCurveEditor::slotModelChanged);
    disconnect(m_model.get(), &KeyframeModelList::modelDisplayChanged, this, &KeyframeCurveEditor::slotModelDisplayChanged);
}
void KeyframeCurveEditor::slotModelChanged()
{
    if (m_state != State_t::DRAG) {
        int offset = pCore->getItemIn(m_model->getOwnerId());
        Q_EMIT atKeyframe(m_model->hasKeyframe(m_position + offset), m_model->singleKeyframe());
    }
    loadSplineFromModel();
    update();
}
void KeyframeCurveEditor::slotModelDisplayChanged()
{
    int offset = pCore->getItemIn(m_model->getOwnerId());
    Q_EMIT atKeyframe(m_model->hasKeyframe(m_position + offset), m_model->singleKeyframe());
    update();
}
void KeyframeCurveEditor::slotSetPosition(int pos, bool isInRange)
{
    if (!isInRange) {
        m_position = -1;
        update();
        return;
    }
    if (pos != m_position) {
        m_position = pos;
        update();
    }
}

void KeyframeCurveEditor::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (m_curve.count() != m_model->count()) {
        qDebug() << "CURVES COUNT: " << m_curve.count() << ", MODEL: " << m_model->count();
        return;
    }
    QPainter p(this);
    paintBackground(&p);
    int offset = pCore->getItemIn(m_model->getOwnerId());
    if (m_duration == 0) {
        // Nothing to draw, for example empty track
        return;
    }

    /*
     * Points + Handles
     */

    BPoint point(m_curve.getPoint(0, m_wWidth, m_wHeight, true));
    BPoint newPoint;
    // QPolygonF handle = QPolygonF() << QPointF(0, -3) << QPointF(3, 0) << QPointF(0, 3) << QPointF(-3, 0);
    QPointF firstPoint = getPointFromModel(offset, offset);
    QPointF nextPoint;
    p.setPen(QPen(Qt::gray, 1, Qt::SolidLine));
    p.setBrush(QBrush(QColor(Qt::gray), Qt::SolidPattern));
    for (int i = 1; i < m_wWidth; ++i) {
        // TODO: re-enable handles after adding custom curve keyframe type in MLT
        // if (i == m_currentPointIndex) {
        // selected point: fill p and handles
        // p.setBrush(QBrush(QColor(Qt::red), Qt::SolidPattern));

        // connect p and handles with lines
        // if (i != 0) {
        //     p.drawLine(QLineF(point.h1.x(), point.h1.y(), point.p.x(), point.p.y()));
        // }
        // if (i != max) {
        //     p.drawLine(QLineF(point.p.x(), point.p.y(), point.h2.x(), point.h2.y()));
        // }
        // }

        // Draw normal interpolated curve in gray
        nextPoint = getPointFromModel(i * m_duration / m_wWidth + offset, offset);
        // p.drawLine(qMax(0, i - 1), firstPoint.y(), i, nextPoint.y());
        p.drawLine(firstPoint.x(), firstPoint.y(), nextPoint.x(), nextPoint.y());
        firstPoint = nextPoint;

        // TODO: re-enable handles after adding custom curve keyframe type in MLT
        // if (i != 0 && (i == m_currentPointIndex || m_showAllHandles)) {
        //     p.drawConvexPolygon(handle.translated(point.h1.x(), point.h1.y()));
        // }
        // if (i != max && (i == m_currentPointIndex || m_showAllHandles)) {
        //     p.drawConvexPolygon(handle.translated(point.h2.x(), point.h2.y()));
        // }

        // if (i == m_currentPointIndex) {
        //     point = m_curve.getPoint(i, m_wWidth, m_wHeight, true);
        //     p.setBrush(QBrush(Qt::NoBrush));
        // }
    }

    // Draw keyframe points separately
    double fps = pCore->getCurrentFps();
    // double normalizedOffset = offset / m_duration * m_wWidth;
    QMutexLocker lock(&m_curveMutex);
    for (int ix = 0; ix < m_model->count(); ix++) {
        int pos = m_model->getPosAtIndex(ix).frames(fps) - offset;
        if (pos < 0 || ix >= m_curve.count()) {
            continue;
        }
        point = m_curve.getPoint(ix, m_wWidth, m_wHeight, true);
        if (pos == m_position) {
            p.setBrush(QBrush(QColor(Qt::darkRed), Qt::SolidPattern));
        } else {
            p.setBrush(QBrush(QColor(Qt::red), Qt::SolidPattern));
        }
        p.drawEllipse(QRectF(pos * m_wWidth / m_duration - 3, point.p.y() - 3, 6, 6));
    }
}

const QPointF KeyframeCurveEditor::getPointFromModel(int framePos, int offset)
{
    double val;
    if (m_rectindex == -1) {
        val = m_model->getInterpolatedValue(framePos, m_paramindex).toDouble();
    } else {
        val = m_model->getInterpolatedValue(framePos, m_paramindex).toString().split(QLatin1Char(' ')).at(m_rectindex).toDouble();
    }
    double normalizedx = (double)(framePos - offset) / m_duration * m_wWidth;
    double normalizedy = 0.5; // center the curve when all values are the same
    if (m_interval) {
        normalizedy = ((val - m_minVal) / m_interval) * (1 - 2 * m_paddingfactor) + m_paddingfactor;
    }
    return QPointF(normalizedx, (1. - normalizedy) * m_wHeight);
}

void KeyframeCurveEditor::mousePressEvent(QMouseEvent *event)
{
    Q_EMIT activateEffect();
    int wWidth = width() - 1;
    int wHeight = height() - 1;
    int offset = pCore->getItemIn(m_model->getOwnerId());
    int offsetX = int(1 / 8. * m_zoomLevel * wWidth);
    int offsetY = int(1 / 8. * m_zoomLevel * wHeight);
    wWidth -= 2 * offsetX;
    wHeight -= 2 * offsetY;

    double x = ((event->pos().x() - offsetX) * m_duration / double(wWidth) + offset) / (offset + m_duration);
    double y = 1.0 - (event->pos().y() - offsetY) / double(wHeight);

    BPoint::PointType selectedPoint;
    int closestPointIndex = nearestPointInRange(QPointF(x, y), wWidth, wHeight, &selectedPoint);
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (closestPointIndex < 0) {
        Q_EMIT seekToPos(x * (offset + m_duration) - offset);
        update();
        return;
    } else {
        m_currentPointIndex = closestPointIndex;
        m_currentPointType = selectedPoint;
    }
    m_clickKeyframe = m_model->getPosAtIndex(m_currentPointIndex).frames(pCore->getCurrentFps());
    m_clickValue = m_model->getInterpolatedValue(m_clickKeyframe, m_paramindex);
    m_model->setSelectedKeyframes({m_clickKeyframe});
    m_model->setActiveKeyframe(m_clickKeyframe);
    BPoint point = m_curve.getPoint(m_currentPointIndex);

    m_grabPOriginal = point;
    m_grabOffsetX = point[int(m_currentPointType)].x() - x;
    m_grabOffsetY = point[int(m_currentPointType)].y() - y;

    point[int(m_currentPointType)] = QPointF(x + m_grabOffsetX, y + m_grabOffsetY);

    m_state = State_t::DRAG;
    if ((m_clickKeyframe != m_position) || KdenliveSettings::keyframeseek()) {
        Q_EMIT seekToPos(m_clickKeyframe - offset);
    }
    update();
}

void KeyframeCurveEditor::mouseMoveEvent(QMouseEvent *event)
{
    int wWidth = width() - 1;
    int wHeight = height() - 1;
    int offsetX = int(1 / 8. * m_zoomLevel * wWidth);
    int offsetY = int(1 / 8. * m_zoomLevel * wHeight);
    int offset = pCore->getItemIn(m_model->getOwnerId());
    wWidth -= 2 * offsetX;
    wHeight -= 2 * offsetY;

    //  X is in the 0-1 range
    double x = ((event->pos().x() - offsetX) * m_duration / double(wWidth) + offset) / (offset + m_duration);
    double y = 1.0 - (event->pos().y() - offsetY) / double(wHeight);

    if (m_state == State_t::NORMAL) {
        // If no point is selected set the cursor shape if on top
        BPoint::PointType type;
        int nearestPointIndex = nearestPointInRange(QPointF(x, y), wWidth, wHeight, &type);

        if (nearestPointIndex < 0) {
            setCursor(Qt::ArrowCursor);
        } else {
            setCursor(Qt::CrossCursor);
        }
        if (event->buttons() & Qt::LeftButton) {
            int offset = pCore->getItemIn(m_model->getOwnerId());
            Q_EMIT seekToPos(x * (offset + m_duration) - offset);
        }
    } else {
        // Else, drag the selected point

        x += m_grabOffsetX;
        y += m_grabOffsetY;

        double leftX = 0.;
        double rightX = 1.;
        double fps = pCore->getCurrentFps();
        BPoint point = m_curve.getPoint(m_currentPointIndex);
        int newPos = m_model->getPosAtIndex(m_currentPointIndex).frames(fps);
        bool ok;
        switch (m_currentPointType) {
        case BPoint::PointType::H1:
            rightX = point.p.x();
            if (m_currentPointIndex == 0) {
                leftX = -4;
            } else {
                leftX = m_curve.getPoint(m_currentPointIndex - 1).p.x();
            }

            x = qBound(leftX, x, rightX);
            point.setH1(QPointF(x, y));
            break;

        case BPoint::PointType::P: {
            if (m_currentPointIndex == 0) {
                rightX = 0.0;
            }
            // else if (m_currentPointIndex == m_curve.count() - 1) {
            //    leftX = 1.0;
            // }

            x = qBound(leftX, x, rightX);
            // y = qBound(0., y, 1.);

            // handles might have changed because we neared another point
            // try to restore
            point.h1 = m_grabPOriginal.h1;
            point.h2 = m_grabPOriginal.h2;
            // and move by same offset
            // (using update handle in point.setP won't work because the offset between new and old point is very small)
            point.h1 += QPointF(x, y) - m_grabPOriginal.p;
            point.h2 += QPointF(x, y) - m_grabPOriginal.p;

            newPos = x * (offset + m_duration);
            int keyframePos = m_model->getPosAtIndex(m_currentPointIndex).frames(pCore->getCurrentFps());
            if (newPos != keyframePos && !(event->modifiers() & Qt::ControlModifier)) {
                GenTime currentPos(keyframePos, fps);
                GenTime updatedPos(newPos, fps);
                GenTime prevPos = m_model->getPrevKeyframe(currentPos, &ok).first;
                if (!ok) {
                    prevPos = GenTime(offset, fps);
                }
                GenTime nextPos = m_model->getNextKeyframe(currentPos, &ok).first;
                if (!ok) {
                    nextPos = GenTime(m_duration - 1 + offset, fps);
                }
                if (m_currentPointIndex == 0 || updatedPos <= prevPos || (updatedPos >= nextPos && m_currentPointIndex < m_model->count() - 1)) {
                    // Don't allow moving first keyframe or moving over another keyframe
                    qDebug() << ":::: ABORTING OUT OF RANGE: " << updatedPos.frames(25) << ", NEXT: " << nextPos.frames(25);
                    break;
                }
                if (!m_model->moveKeyframe(currentPos, updatedPos, false)) {
                    qDebug() << "=== FAILED KF MOVE!!!";
                    Q_ASSERT(false);
                } else {
                    Q_EMIT seekToPos(newPos - offset);
                }
            }
            if (qAbs(y - point.p.y()) > 0.01) {
                updateKeyframeData(valueFromCanvasPos(y));
            }
            break;
        }
        case BPoint::PointType::H2:
            leftX = point.p.x();
            if (m_currentPointIndex == m_curve.count() - 1) {
                rightX = 5;
            } else {
                rightX = m_curve.getPoint(m_currentPointIndex + 1).p.x();
            }

            x = qBound(leftX, x, rightX);
            point.setH2(QPointF(x, y));
        };

        // int index = m_currentPointIndex;
        // m_currentPointIndex = m_curve.setPoint(m_currentPointIndex, point);

        // if (m_currentPointType == BPoint::PointType::P) {
        //     // we might have changed the handles of other points
        //     // try to restore
        //     if (index == m_currentPointIndex) {
        //         if (m_currentPointIndex > 0) {
        //             m_curve.setPoint(m_currentPointIndex - 1, m_grabPPrevious);
        //         }
        //         if (m_currentPointIndex < m_curve.count() - 1) {
        //             m_curve.setPoint(m_currentPointIndex + 1, m_grabPNext);
        //         }
        //     } else {
        //         if (m_currentPointIndex < index) {
        //             m_curve.setPoint(index, m_grabPPrevious);
        //             m_grabPNext = m_grabPPrevious;
        //             if (m_currentPointIndex > 0) {
        //                 m_grabPPrevious = m_curve.getPoint(m_currentPointIndex - 1);
        //             }
        //         } else {
        //             m_curve.setPoint(index, m_grabPNext);
        //             m_grabPPrevious = m_grabPNext;
        //             if (m_currentPointIndex < m_curve.count() - 1) {
        //                 m_grabPNext = m_curve.getPoint(m_currentPointIndex + 1);
        //             }
        //         }
        //     }
        // }

        Q_EMIT currentPoint(point, isCurrentPointExtremal());
        if (KdenliveSettings::dragvalue_directupdate()) {
            Q_EMIT modified();
        }
        // update();
    }
}
int KeyframeCurveEditor::seekPosOnCanvas(double xpos)
{
    return qBound(0, int(xpos * m_duration), m_duration - 1);
}
void KeyframeCurveEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        Q_EMIT activateEffect();
        int wWidth = width() - 1;
        int wHeight = height() - 1;
        int offset = pCore->getItemIn(m_model->getOwnerId());
        int offsetX = int(1 / 8. * m_zoomLevel * wWidth);
        int offsetY = int(1 / 8. * m_zoomLevel * wHeight);
        int fps = pCore->getCurrentFps();
        wWidth -= 2 * offsetX;
        wHeight -= 2 * offsetY;

        double x = ((event->pos().x() - offsetX) * m_duration / double(wWidth) + offset) / (offset + m_duration);
        double y = 1.0 - (event->pos().y() - offsetY) / double(wHeight);

        BPoint::PointType selectedPoint;
        int closestPointIndex = nearestPointInRange(QPointF(x, y), wWidth, wHeight, &selectedPoint);
        if (selectedPoint == BPoint::PointType::P && m_model->hasKeyframe(closestPointIndex)) {
            bool ok;
            GenTime position(closestPointIndex, fps);
            auto keyframe = m_model->getKeyframe(position, &ok);
            if (ok && (keyframe.first.frames(pCore->getCurrentFps()) != offset)) {
                if (ok) {
                    m_model->removeKeyframe(keyframe.first);
                    m_currentPointIndex--;
                }
                if (keyframe.first.frames(pCore->getCurrentFps()) == m_position + offset) {
                    Q_EMIT atKeyframe(false, m_model->singleKeyframe());
                }
            }
        } else {
            int pos = x * (offset + m_duration);
            if (!m_model->hasKeyframe(pos)) {
                GenTime position(pos, pCore->getCurrentFps());
                m_model->addKeyframe(position, KeyframeType::KeyframeEnum(KdenliveSettings::defaultkeyframeinterp()));
                m_currentPointIndex = m_model->getIndexForPos(position);
                y = qBound(0., y, 1.);
                updateKeyframeData(valueFromCanvasPos(y));
            }
        }
    } else {
        QWidget::mouseDoubleClickEvent(event);
    }
}

void KeyframeCurveEditor::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_state != State_t::DRAG) {
        return QWidget::mouseReleaseEvent(event);
    }

    // Create the undo command. First reset keyframe position to before the move
    GenTime newKeyframePos = m_model->getPosAtIndex(m_currentPointIndex);
    const QVariant newKeyframeValue = m_model->getInterpolatedValue(newKeyframePos, m_paramindex);
    GenTime originalPos(m_clickKeyframe, pCore->getCurrentFps());
    m_model->moveKeyframe(newKeyframePos, originalPos, false);
    m_model->getKeyModel(m_paramindex)->directUpdateKeyframe(originalPos, m_clickValue, false);

    // We are now in the state before the move, create the command
    QUndoCommand *parentCommand = new QUndoCommand();
    parentCommand->setText(i18n("Move Keyframe"));
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    m_model->moveKeyframeWithUndo(originalPos, newKeyframePos, undo, redo);
    new FunctionalUndoCommand(undo, redo, QString(), parentCommand);
    m_model->updateKeyframe(newKeyframePos, newKeyframeValue, m_currentPointIndex, m_paramindex, parentCommand);
    pCore->pushUndo(parentCommand);
    setCursor(Qt::ArrowCursor);
    m_state = State_t::NORMAL;

    loadSplineFromModel();
    update();
}
int KeyframeCurveEditor::nearestPointInRange(const QPointF &p, int wWidth, int wHeight, BPoint::PointType *sel)
{

    auto nearest = m_curve.closestPoint(p);
    int nearestIndex = nearest.first;
    if (nearestIndex < 0) {
        return -1;
    }
    BPoint::PointType pointType;
    BPoint point = m_curve.getPoint(nearestIndex);
    if (isHandleDisabled(point)) {
        pointType = BPoint::PointType::P;
    } else {
        pointType = nearest.second;
    }

    if (nearestIndex == m_currentPointIndex || pointType == BPoint::PointType::P) {
        // a point was found and it is not a hidden handle
        double dx = (p.x() - point[int(pointType)].x()) * wWidth;
        double dy = (p.y() - point[int(pointType)].y()) * wHeight;
        if (dx * dx + dy * dy <= m_grabRadius * m_grabRadius) {
            *sel = pointType;
            return nearestIndex;
        }
    }

    return -1;
}

QList<BPoint> KeyframeCurveEditor::getPoints() const
{
    return m_curve.getPoints();
}

bool KeyframeCurveEditor::isHandleDisabled(BPoint point)
{
    // warning: this is a fuzzy comparison
    return point.h1 == point.p && point.p == point.h2;
}
void KeyframeCurveEditor::loadSplineFromModel()
{
    auto curve = CubicBezierSpline(true);
    int keyframesCount = m_model->count();
    if (m_state != State_t::DRAG) {
        updateInterval();
    }
    int offset = pCore->getItemIn(m_model->getOwnerId());
    for (int i = 0; i < keyframesCount; i++) {
        GenTime pos = m_model->getPosAtIndex(i);
        double val;
        if (m_rectindex == -1) {
            val = m_model->getInterpolatedValue(pos, m_paramindex).toDouble();
        } else {
            val = m_model->getInterpolatedValue(pos, m_paramindex).toString().split(QLatin1Char(' ')).at(m_rectindex).toDouble();
        }
        // if(m_model->selectedKeyframes().contains(i)) continue;
        double normalizedx = double(pos.frames(pCore->getCurrentFps())) / (offset + m_duration);
        double normalizedy = 0.5; // center the curve when all values are the same
        if (m_interval) {
            normalizedy = ((val - m_minVal) / m_interval) * (1 - 2 * m_paddingfactor) + m_paddingfactor;
        }
        QPointF p = QPointF(normalizedx, normalizedy);
        curve.addPoint(BPoint(p, p, p)); // disable handles
    }
    QMutexLocker lock(&m_curveMutex);
    m_curve = curve;
}
void KeyframeCurveEditor::paintBackground(QPainter *p)
{
    /*
     * Zoom
     */
    m_wWidth = width() - 1;
    m_wHeight = height() - 1;
    int offsetX = int(1 / 8. * m_zoomLevel * m_wWidth);
    int offsetY = int(1 / 8. * m_zoomLevel * m_wHeight);
    m_wWidth -= 2 * offsetX;
    m_wHeight -= 2 * offsetY;

    p->translate(offsetX, offsetY);

    /*
     * Background
     */
    QRect bgrect = rect().translated(-offsetX, -offsetY);
    p->fillRect(bgrect, palette().window());
    if (!m_pixmap.isNull()) {
        if (m_pixmapIsDirty || !m_pixmapCache) {
            m_pixmapCache = std::make_shared<QPixmap>(m_wWidth + 1, m_wHeight + 1);
            QPainter cachePainter(m_pixmapCache.get());

            cachePainter.scale(1.0 * (m_wWidth + 1) / m_pixmap.width(), 1.0 * (m_wHeight + 1) / m_pixmap.height());
            cachePainter.drawPixmap(0, 0, m_pixmap);
            m_pixmapIsDirty = false;
        }
        p->drawPixmap(0, 0, *m_pixmapCache);
    }

    // select color of the grid, depending on whether we have a palette or not
    QPen gridPen;
    if (!m_pixmap.isNull()) {
        gridPen = QPen(palette().mid().color(), 1, Qt::SolidLine);
    } else {
        int h, s, l, a;
        auto bg = palette().color(QPalette::Window);
        bg.getHsl(&h, &s, &l, &a);
        l = (l > 128) ? l - 30 : l + 30;
        bg.setHsl(h, s, l, a);
        gridPen = QPen(bg, 1, Qt::SolidLine);
    }
    p->setPen(gridPen);

    /*
     * Borders
     */
    p->drawRect(0, 0, m_wWidth, m_wHeight);

    /*
     * Grid
     */
    double scale = m_wWidth / double(qMax(1, m_duration));
    int step = (1. / m_paddingfactor);
    for (int i = 0; i <= step; i++) {
        p->drawLine(QLineF(0, i * m_paddingfactor * m_wHeight, m_wWidth, i * m_paddingfactor * m_wHeight));
    }
    p->drawLine(QLineF(m_position * scale, 0, m_position * scale, m_wHeight));

    /*
     * Interval Value Text
     */
    QFont font = p->font();
    font.setPixelSize(8);
    p->setPen(QPen(palette().color(QPalette::Text), 1, Qt::SolidLine));
    QString realMinVal = m_model->getKeyModel(m_paramindex)->realValueFromInternal(qMax(m_minLimit, m_minVal - getPadding()));
    QString realMaxVal = m_model->getKeyModel(m_paramindex)->realValueFromInternal(qMin(m_maxLimit, m_maxVal + getPadding()));
    p->drawText(bgrect.bottomLeft(), realMinVal);
    p->drawText(bgrect.topLeft() + QPoint(0, font.pixelSize() + 2), realMaxVal);

    p->setRenderHint(QPainter::Antialiasing);
}

void KeyframeCurveEditor::setDuration(int duration)
{
    m_duration = duration;
    loadSplineFromModel();
    update();
}
void KeyframeCurveEditor::updateInterval()
{
    int offset = pCore->getItemIn(m_model->getOwnerId());
    QList<double> val;
    for (int i = 0; i < m_model->count(); i++) {
        int pos = m_model->getPosAtIndex(i).frames(pCore->getCurrentFps());
        if (pos < offset) {
            continue;
        }
        if (pos > m_duration + offset) {
            break;
        }
        if (m_rectindex >= 0) {
            val.append(m_model->getInterpolatedValue(pos, m_paramindex).toString().split(QLatin1Char(' ')).at(m_rectindex).toDouble());
        } else {
            val.append(m_model->getInterpolatedValue(pos, m_paramindex).toDouble());
        }
    }
    if (!val.isEmpty()) {
        m_minVal = *std::min_element(val.begin(), val.end());
        m_maxVal = *std::max_element(val.begin(), val.end());
        m_interval = qAbs(m_maxVal - m_minVal);
    }
}
double KeyframeCurveEditor::getPadding()
{
    double padding = qAbs(m_maxLimit - m_minLimit);
    // use the current profile's width/height for AnimatedRect params
    switch (m_rectindex) {
    case 0: // x
    case 2: // width
        padding = pCore->getCurrentFrameSize().width() / 2;
        break;
    case 1: // y
    case 3: // height
        padding = pCore->getCurrentFrameSize().height() / 2;
        break;
    default:
        break;
    }
    return padding * m_paddingfactor;
}
double KeyframeCurveEditor::valueFromCanvasPos(double ypos)
{
    double padding = getPadding();
    double value;
    if (m_interval) {
        if (ypos <= m_paddingfactor) {
            value = m_minVal + ((ypos - m_paddingfactor) / m_paddingfactor) * padding;
        } else if (ypos > m_paddingfactor && ypos < (1 - m_paddingfactor)) {
            value = ((ypos - m_paddingfactor) / (1 - 2 * m_paddingfactor)) * m_interval + m_minVal;
        } else {
            value = m_maxVal + ((ypos + m_paddingfactor - 1) / m_paddingfactor) * padding;
        }
    } else {
        value = ((ypos - 0.5) / 0.5) * padding + m_minVal;
    }
    return qBound(m_minLimit, value, m_maxLimit);
}

void KeyframeCurveEditor::updateKeyframeData(double val)
{
    Q_EMIT activateEffect();
    int fps = pCore->getCurrentFps();
    if (val > m_maxLimit || val < m_minLimit) return;
    // val = qBound(m_minLimit, val, m_maxLimit);
    int decimal = qRound(qLn(m_factor) / qLn(10));
    if (decimal < 1) {
        val = static_cast<double>(qRound(val));
    } else {
        val *= qPow(10, decimal);
        val = qRound(val);
        val /= qPow(10, decimal);
    }
    int keyframePos = m_model->getPosAtIndex(m_currentPointIndex).frames(fps);
    QVariant data(val);
    if (m_rectindex >= 0) {
        auto rectdata = m_model->getInterpolatedValue(keyframePos, m_paramindex).toString().split(QLatin1Char(' '));
        rectdata[m_rectindex] = QStringLiteral("%1").arg(val);
        data = QVariant(rectdata.join(QLatin1Char(' ')));
    }
    m_model->updateKeyframe(GenTime(keyframePos, fps), data, m_currentPointIndex, m_paramindex);
}
