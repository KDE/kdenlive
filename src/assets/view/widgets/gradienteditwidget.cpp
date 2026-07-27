/*
    SPDX-FileCopyrightText: 2024 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "gradienteditwidget.hpp"
#include "assets/model/assetparametermodel.hpp"

#include <QColorDialog>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOption>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>

// --- GradientData ---

void GradientData::enforceMinStops()
{
    if (stops.size() < 1) stops.append({0.0, Qt::black});
    if (stops.size() < 2) stops.append({1.0, Qt::white});
}

void GradientData::sortStops()
{
    std::sort(stops.begin(), stops.end(), [](const QPair<qreal, QColor> &a, const QPair<qreal, QColor> &b) { return a.first < b.first; });
}

int GradientData::hitTest(const QVector<QPair<qreal, QColor>> &s, qreal pos, qreal tolerance) const
{
    for (int i = 0; i < s.size(); i++) {
        if (qAbs(s.at(i).first - pos) <= tolerance) {
            return i;
        }
    }
    return -1;
}

QColor GradientData::interpolateColor(qreal pos) const
{
    if (stops.isEmpty()) return Qt::black;
    if (stops.size() == 1) return stops.at(0).second;

    // Find surrounding stops
    int lo = 0, hi = stops.size() - 1;
    for (int i = 0; i < stops.size() - 1; i++) {
        if (stops.at(i).first <= pos && stops.at(i + 1).first >= pos) {
            lo = i;
            hi = i + 1;
            break;
        }
    }

    const qreal span = stops.at(hi).first - stops.at(lo).first;
    if (qFuzzyIsNull(span)) return stops.at(lo).second;

    const qreal t = (pos - stops.at(lo).first) / span;
    const QColor &c0 = stops.at(lo).second;
    const QColor &c1 = stops.at(hi).second;

    return QColor::fromRgbF(c0.redF() + t * (c1.redF() - c0.redF()), c0.greenF() + t * (c1.greenF() - c0.greenF()), c0.blueF() + t * (c1.blueF() - c0.blueF()),
                            c0.alphaF() + t * (c1.alphaF() - c0.alphaF()));
}

int GradientData::addStop(qreal pos)
{
    if (stops.size() >= GradientMaxStops) return -1;

    // Enforce minimum distance from existing stops
    for (const auto &stop : stops) {
        if (qAbs(stop.first - pos) < GradientMinDistance) {
            pos = stop.first + (pos >= stop.first ? GradientMinDistance : -GradientMinDistance);
        }
    }
    pos = qBound(0.0, pos, 1.0);

    const QColor color = interpolateColor(pos);
    stops.append({pos, color});
    sortStops();

    for (int i = 0; i < stops.size(); i++) {
        if (qFuzzyCompare(stops.at(i).first, pos)) {
            return i;
        }
    }
    return stops.size() - 1;
}

bool GradientData::removeStop(int index)
{
    if (stops.size() <= 2) return false;
    if (index < 0 || index >= stops.size()) return false;
    stops.removeAt(index);
    return true;
}

QString GradientData::toString() const
{
    QStringList parts;
    parts.reserve(stops.size());
    for (const auto &stop : stops) {
        parts << QStringLiteral("%1 %2").arg(stop.second.name(QColor::HexArgb)).arg(stop.first, 0, 'f', 4);
    }
    return parts.join(QLatin1Char('|'));
}

void GradientData::fromString(const QString &str)
{
    stops.clear();
    const QStringList parts = str.split(QLatin1Char('|'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QStringList tokens = part.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (tokens.size() >= 2) {
            const QColor color(tokens.at(0));
            if (color.isValid()) {
                stops.append({qBound(0.0, tokens.at(1).toDouble(), 1.0), color});
            }
        }
    }
    enforceMinStops();
    sortStops();
}

// --- GradientEditWidget ---

GradientEditWidget::GradientEditWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    setFixedHeight(BarHeight + HandleRadius * 3 + 4);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(false);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);

    m_data.fromString(m_model->data(m_index, AssetParameterModel::ValueRole).toString());
}

void GradientEditWidget::slotRefresh()
{
    m_data.fromString(m_model->data(m_index, AssetParameterModel::ValueRole).toString());
    m_selectedIndex = qBound(-1, m_selectedIndex, m_data.stops.size() - 1);
    update();
}

// --- geometry helpers ---

qreal GradientEditWidget::xToPos(int x) const
{
    const int left = HandleRadius;
    const int right = width() - HandleRadius;
    if (right <= left) return 0.0;
    return qBound(0.0, static_cast<qreal>(x - left) / (right - left), 1.0);
}

int GradientEditWidget::posToX(qreal pos) const
{
    const int left = HandleRadius;
    const int right = width() - HandleRadius;
    return left + qRound(pos * (right - left));
}

int GradientEditWidget::hitTest(int x) const
{
    for (int i = 0; i < m_data.stops.size(); i++) {
        if (qAbs(posToX(m_data.stops.at(i).first) - x) <= HandleRadius + 2) {
            return i;
        }
    }
    return -1;
}

// --- painting ---

namespace {
// Tiles a light/dark gray checkerboard into rect, used as a backdrop so
// translucent gradient areas remain visually distinguishable from opaque ones.
void paintAlphaCheckerboard(QPainter &p, const QRect &rect)
{
    static constexpr int TileSize = 8;
    const QColor light(205, 205, 205);
    const QColor dark(155, 155, 155);

    p.save();
    p.setClipRect(rect);
    p.setPen(Qt::NoPen);
    for (int y = rect.top(); y < rect.bottom(); y += TileSize) {
        const int row = (y - rect.top()) / TileSize;
        for (int x = rect.left(); x < rect.right(); x += TileSize) {
            const int col = (x - rect.left()) / TileSize;
            p.setBrush(((row + col) % 2 == 0) ? light : dark);
            p.drawRect(QRect(x, y, TileSize, TileSize));
        }
    }
    p.restore();
}
} // namespace

void GradientEditWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int by = HandleRadius + 2;
    const QRect barRect(HandleRadius, by, width() - 2 * HandleRadius, BarHeight);

    // Native styles (e.g. Breeze) fill the PE_Frame interior with an opaque
    // background, so the frame must be drawn first and the gradient painted
    // inside its border
    QStyleOptionFrame frameOption;
    frameOption.initFrom(this);
    frameOption.rect = barRect;
    frameOption.state |= QStyle::State_Sunken;
    frameOption.frameShape = QFrame::Panel;
    frameOption.lineWidth = 1;
    style()->drawPrimitive(QStyle::PE_Frame, &frameOption, &p, this);

    const int frameWidth = qMax(1, style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &frameOption, this));
    const QRect innerRect = barRect.adjusted(frameWidth, frameWidth, -frameWidth, -frameWidth);

    const bool hasTranslucentStop =
        std::any_of(m_data.stops.cbegin(), m_data.stops.cend(), [](const QPair<qreal, QColor> &stop) { return stop.second.alpha() < 255; });
    if (hasTranslucentStop) {
        paintAlphaCheckerboard(p, innerRect);
    }

    QLinearGradient grad(barRect.left(), 0, barRect.right(), 0);
    for (const auto &stop : m_data.stops) {
        grad.setColorAt(stop.first, stop.second);
    }
    p.fillRect(innerRect, grad);

    const int hy = by + BarHeight;
    for (int i = 0; i < m_data.stops.size(); i++) {
        const int hx = posToX(m_data.stops.at(i).first);
        QPolygon tri;
        tri << QPoint(hx, hy + 1) << QPoint(hx - HandleRadius, hy + HandleRadius * 2) << QPoint(hx + HandleRadius, hy + HandleRadius * 2);

        const bool selected = (i == m_selectedIndex);
        // Outline with the theme's text color so dark stops stay visible on dark themes
        QColor outline = palette().color(QPalette::WindowText);
        outline.setAlphaF(0.5);
        p.setBrush(m_data.stops.at(i).second);
        p.setPen(QPen(selected ? palette().highlight().color() : outline, selected ? 2 : 1));
        p.drawPolygon(tri);
    }
}

// --- mouse events ---

bool GradientEditWidget::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *helpEvent = static_cast<QHelpEvent *>(event);
        const int hit = (helpEvent->pos().y() > HandleRadius + 2 + BarHeight) ? hitTest(helpEvent->pos().x()) : -1;
        if (hit >= 0) {
            const QColor &c = m_data.stops.at(hit).second;
            QToolTip::showText(helpEvent->globalPos(), QStringLiteral("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha()), this);
        } else {
            QToolTip::hideText();
        }
        return true;
    }
    return AbstractParamWidget::event(event);
}

void GradientEditWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        const int hit = hitTest(event->pos().x());
        if (hit >= 0) {
            if (m_data.removeStop(hit)) {
                m_selectedIndex = qBound(-1, m_selectedIndex, m_data.stops.size() - 1);
                update();
                Q_EMIT valueChanged(m_index, m_data.toString(), true);
            }
        }
        return;
    }

    if (event->button() != Qt::LeftButton) return;

    const int hit = hitTest(event->pos().x());
    if (hit >= 0) {
        m_dragIndex = hit;
        m_selectedIndex = hit;
        update();
    } else {
        // Add new stop at click position
        const qreal pos = xToPos(event->pos().x());
        const int newIdx = m_data.addStop(pos);
        if (newIdx >= 0) {
            m_selectedIndex = newIdx;
            m_dragIndex = newIdx;
            update();
            Q_EMIT valueChanged(m_index, m_data.toString(), true);
        }
    }
}

void GradientEditWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragIndex < 0 || !(event->buttons() & Qt::LeftButton)) return;

    qreal newPos = xToPos(event->pos().x());

    // Snap away from neighbors, enforce min distance
    for (int i = 0; i < m_data.stops.size(); i++) {
        if (i == m_dragIndex) continue;
        const qreal nb = m_data.stops.at(i).first;
        if (qAbs(newPos - nb) < GradientMinDistance) {
            newPos = (newPos < nb) ? nb - GradientMinDistance : nb + GradientMinDistance;
        }
    }
    newPos = qBound(0.0, newPos, 1.0);
    m_data.stops[m_dragIndex].first = newPos;

    m_data.sortStops();
    // Re-find drag index after sort
    for (int i = 0; i < m_data.stops.size(); i++) {
        if (qFuzzyCompare(m_data.stops.at(i).first, newPos)) {
            m_dragIndex = i;
            m_selectedIndex = i;
            break;
        }
    }

    update();
    Q_EMIT valueChanged(m_index, m_data.toString(), false);
}

void GradientEditWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_dragIndex >= 0) {
        m_dragIndex = -1;
        Q_EMIT valueChanged(m_index, m_data.toString(), true);
    }
}

void GradientEditWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    const int hit = hitTest(event->pos().x());
    if (hit < 0) return;

    m_selectedIndex = hit;
    const QColor chosen = QColorDialog::getColor(m_data.stops.at(hit).second, this, tr("Select Stop Color"), QColorDialog::ShowAlphaChannel);
    if (chosen.isValid()) {
        m_data.stops[hit].second = chosen;
        update();
        Q_EMIT valueChanged(m_index, m_data.toString(), true);
    }
}
