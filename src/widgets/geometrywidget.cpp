/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "geometrywidget.h"
#include "core.h"
#include "doublewidget.h"
#include "dragvalue.h"
#include "monitor/monitor.h"

#include <KLocalizedString>
#include <QFormLayout>
#include <QLabel>
#include <QStyle>
#include <QtMath>
#include <cmath>

GeometryWidget::GeometryWidget(Monitor *monitor, QPair<int, int> range, const QRect &rect, double opacity, const QSize frameSize, bool useRatioLock,
                               bool useOpacity, QWidget *parent, QFormLayout *layout)
    : QObject(parent)
    , m_min(range.first)
    , m_max(range.second)
    , m_active(false)
    , m_monitor(monitor)
    , m_opacityFactor(100.)
    , m_layout(layout)
{
    Q_UNUSED(useRatioLock)
    m_defaultSize = pCore->getCurrentFrameSize();
    m_sourceSize = (frameSize.isValid() && !frameSize.isNull()) ? frameSize : m_defaultSize;

    // auto *positionLayout = new QHBoxLayout;
    m_spinX = new DragValue(i18nc("x axis position", "Position X"), 0, 0, -99000, 99000, -1, QString(), false, false, parent, true);
    connect(m_spinX, &DragValue::customValueChanged, this, &GeometryWidget::slotAdjustRectXKeyframeValue);
    m_spinX->setObjectName("spinX");
    m_allWidgets << m_spinX;

    m_spinY = new DragValue(i18nc("y axis position", "Y"), 0, 0, -99000, 99000, -1, QString(), false, false, parent, true);
    connect(m_spinY, &DragValue::customValueChanged, this, &GeometryWidget::slotAdjustRectYKeyframeValue);
    m_spinY->setObjectName("spinY");
    m_allWidgets << m_spinY;

    int maxLabelWidth = 0;
    QHBoxLayout *poslayout = new QHBoxLayout;
    poslayout->addWidget(m_spinX);
    QLabel *label = m_spinY->createLabel();
    int posLabel = label->sizeHint().width();
    maxLabelWidth = posLabel;
    poslayout->addWidget(label);
    poslayout->addSpacing(layout->horizontalSpacing() / 3);
    poslayout->addWidget(m_spinY);
    poslayout->addStretch(10);
    m_allWidgets << label;

    m_spinWidth = new DragValue(i18nc("Image Size (Width)", "Size W"), m_defaultSize.width(), 0, 1, 99000, -1, QString(), false, false, parent, true);
    connect(m_spinWidth, &DragValue::customValueChanged, this, &GeometryWidget::slotAdjustRectWidth);
    m_spinWidth->setObjectName("spinW");
    m_allWidgets << m_spinWidth;

    // Lock ratio stuff
    m_lockRatio = new QAction(QIcon::fromTheme(QStringLiteral("link")), i18n("Lock aspect ratio"), parent);
    m_lockRatio->setCheckable(true);
    connect(m_lockRatio, &QAction::triggered, this, &GeometryWidget::slotLockRatio);
    auto *ratioButton = new QToolButton;
    ratioButton->setDefaultAction(m_lockRatio);
    ratioButton->setAutoRaise(true);
    ratioButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_allWidgets << ratioButton;

    m_spinHeight = new DragValue(i18nc("Image Height", "H"), m_defaultSize.height(), 0, 1, 99000, -1, QString(), false, false, parent, true);
    connect(m_spinHeight, &DragValue::customValueChanged, this, &GeometryWidget::slotAdjustRectHeight);
    m_spinHeight->setObjectName("spinH");
    QHBoxLayout *sizelayout = new QHBoxLayout;
    sizelayout->addWidget(m_spinWidth);
    sizelayout->addWidget(ratioButton);
    label = m_spinHeight->createLabel();
    int sizeLabel = label->sizeHint().width() + ratioButton->sizeHint().width();
    maxLabelWidth = qMax(maxLabelWidth, sizeLabel);
    sizelayout->addWidget(label);
    sizelayout->addSpacing(layout->horizontalSpacing() / 3);
    sizelayout->addWidget(m_spinHeight);
    sizelayout->addStretch(10);
    m_allWidgets << m_spinHeight;
    m_allWidgets << label;

    // Build buttons
    m_originalSize = new QAction(QIcon::fromTheme(QStringLiteral("zoom-original")), i18n("Adjust to original size"), parent);
    connect(m_originalSize, &QAction::triggered, this, &GeometryWidget::slotAdjustToSource);
    m_originalSize->setCheckable(true);
    QAction *adjustSize = new QAction(QIcon::fromTheme(QStringLiteral("zoom-fit-best")), i18n("Adjust and center in frame"), parent);
    connect(adjustSize, &QAction::triggered, this, &GeometryWidget::slotAdjustToFrameSize);
    QAction *fitToWidth = new QAction(QIcon::fromTheme(QStringLiteral("zoom-fit-width")), i18n("Fit to width"), parent);
    connect(fitToWidth, &QAction::triggered, this, &GeometryWidget::slotFitToWidth);
    QAction *fitToHeight = new QAction(QIcon::fromTheme(QStringLiteral("zoom-fit-height")), i18n("Fit to height"), parent);
    connect(fitToHeight, &QAction::triggered, this, &GeometryWidget::slotFitToHeight);

    QAction *alignleft = new QAction(QIcon::fromTheme(QStringLiteral("align-horizontal-left")), i18n("Align left"), parent);
    connect(alignleft, &QAction::triggered, this, &GeometryWidget::slotMoveLeft);
    QAction *alignhcenter = new QAction(QIcon::fromTheme(QStringLiteral("align-horizontal-center")), i18n("Center horizontally"), parent);
    connect(alignhcenter, &QAction::triggered, this, &GeometryWidget::slotCenterH);
    QAction *alignright = new QAction(QIcon::fromTheme(QStringLiteral("align-horizontal-right")), i18n("Align right"), parent);
    connect(alignright, &QAction::triggered, this, &GeometryWidget::slotMoveRight);
    QAction *aligntop = new QAction(QIcon::fromTheme(QStringLiteral("align-vertical-top")), i18n("Align top"), parent);
    connect(aligntop, &QAction::triggered, this, &GeometryWidget::slotMoveTop);
    QAction *alignvcenter = new QAction(QIcon::fromTheme(QStringLiteral("align-vertical-center")), i18n("Center vertically"), parent);
    connect(alignvcenter, &QAction::triggered, this, &GeometryWidget::slotCenterV);
    QAction *alignbottom = new QAction(QIcon::fromTheme(QStringLiteral("align-vertical-bottom")), i18n("Align bottom"), parent);
    connect(alignbottom, &QAction::triggered, this, &GeometryWidget::slotMoveBottom);

    QToolBar *tbAlign = new QToolBar(parent);
    int size = parent->style()->pixelMetric(QStyle::PM_SmallIconSize);
    tbAlign->setIconSize(QSize(size, size));
    // tbAlign->setStyleSheet(QStringLiteral("QToolBar { padding: 0; } QToolBar QToolButton { padding: 0; margin: 0; }"));
    tbAlign->setFixedHeight(m_spinX->height());
    tbAlign->setToolButtonStyle(Qt::ToolButtonIconOnly);
    tbAlign->addAction(alignleft);
    tbAlign->addAction(alignhcenter);
    tbAlign->addAction(alignright);
    tbAlign->addAction(aligntop);
    tbAlign->addAction(alignvcenter);
    tbAlign->addAction(alignbottom);
    m_allWidgets << tbAlign;

    tbAlign->addAction(m_originalSize);
    tbAlign->addAction(adjustSize);
    tbAlign->addAction(fitToWidth);
    tbAlign->addAction(fitToHeight);
    tbAlign->setMinimumWidth(m_spinX->height());

    QHBoxLayout *scaleLayout = new QHBoxLayout;
    m_spinSize = new DragValue(i18n("Scale"), 100, 2, 1, 99000, -1, i18n("%"), false, false, parent, true);
    m_spinSize->setStep(5);
    m_spinSize->setObjectName("spinS");
    connect(m_spinSize, &DragValue::customValueChanged, this, &GeometryWidget::slotResize);
    scaleLayout->addWidget(m_spinSize);
    m_allWidgets << m_spinSize;
    int opacityLabel = 0;

    if (useOpacity) {
        m_opacity = new DragValue(i18n("Opacity"), 100, 0, 0, 100, -1, i18n("%"), false, false, parent, true);
        m_opacity->setValue((int)(opacity * m_opacityFactor));
        connect(m_opacity, &DragValue::customValueChanged, this, [&]() { Q_EMIT valueChanged(getValue(), 4, m_frameForRect); });
        m_opacity->setObjectName("spinO");
        label = m_opacity->createLabel();
        opacityLabel = label->sizeHint().width();
        maxLabelWidth = qMax(maxLabelWidth, opacityLabel);
        scaleLayout->addWidget(label);
        scaleLayout->addSpacing(layout->horizontalSpacing() / 3);
        scaleLayout->addWidget(m_opacity);
        m_allWidgets << m_opacity;
        m_allWidgets << label;
    }
    scaleLayout->addStretch(10);
    maxLabelWidth += layout->horizontalSpacing() / 3;
    posLabel += m_spinX->sizeHint().width();
    sizeLabel += m_spinWidth->sizeHint().width();
    if (opacityLabel > 0) {
        opacityLabel += m_spinSize->sizeHint().width();
    }
    int offset = qMax(m_spinX->sizeHint().width(), m_spinWidth->sizeHint().width());
    offset = qMax(offset, m_spinSize->sizeHint().width());
    maxLabelWidth += offset;
    int diff = maxLabelWidth - posLabel;
    if (diff > 0) {
        poslayout->insertSpacing(1, diff);
    }
    label = m_spinX->createLabel();
    layout->addRow(label, poslayout);
    m_allWidgets << label;
    diff = maxLabelWidth - sizeLabel;
    if (diff > 0) {
        sizelayout->insertSpacing(1, diff);
    }
    label = m_spinWidth->createLabel();
    layout->addRow(label, sizelayout);
    m_allWidgets << label;
    label = new QLabel(i18n("Align"));
    tbAlign->setMinimumHeight(tbAlign->sizeHint().height());
    layout->addRow(label, tbAlign);
    m_allWidgets << label;
    if (opacityLabel > 0) {
        diff = maxLabelWidth - opacityLabel;
        if (diff > 0) {
            scaleLayout->insertSpacing(1, diff);
        }
    }
    label = m_spinSize->createLabel();
    layout->addRow(label, scaleLayout);
    m_allWidgets << label;

    slotUpdateGeometryRect(rect);
    slotAdjustRectKeyframeValue();
}

void GeometryWidget::slotAdjustToSource()
{
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinWidth->setValue(qRound(m_sourceSize.width() / pCore->getCurrentSar()), false);
    m_spinHeight->setValue(m_sourceSize.height(), false);
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    slotAdjustRectKeyframeValue();
    if (m_lockRatio->isChecked()) {
        m_monitor->setEffectSceneProperty(QStringLiteral("lockratio"), m_originalSize->isChecked() ? (double)m_sourceSize.width() / m_sourceSize.height()
                                                                                                   : (double)m_defaultSize.width() / m_defaultSize.height());
    }
}
void GeometryWidget::slotAdjustToFrameSize()
{
    double monitorDar = pCore->getCurrentDar();
    double sourceDar = (m_sourceSize.width() / (double)m_sourceSize.height()) * pCore->getCurrentSar();
    QSignalBlocker bk1(m_spinWidth);
    QSignalBlocker bk2(m_spinHeight);
    double aspect = sourceDar;
    double angleRad = qDegreesToRadians(m_rotation);
    double c = std::abs(std::cos(angleRad));
    double s = std::abs(std::sin(angleRad));
    double W = m_defaultSize.width();
    double H = m_defaultSize.height();
    double h, w;
    if (m_rotatable && m_rotation != 0) {
        // Fit the rotated bounding box inside the frame
        double h1 = W / (aspect * c + s);
        double h2 = H / (aspect * s + c);
        h = std::min(h1, h2);
        w = aspect * h;
    } else {
        if (sourceDar > monitorDar) {
            // Fit to width
            double factor = (double)m_defaultSize.width() / m_sourceSize.width() * pCore->getCurrentSar();
            h = qRound(m_sourceSize.height() * factor);
            w = m_defaultSize.width();
        } else {
            // Fit to height
            double factor = (double)m_defaultSize.height() / m_sourceSize.height();
            h = m_defaultSize.height();
            w = qRound(m_sourceSize.width() / pCore->getCurrentSar() * factor);
        }
    }
    m_spinWidth->setValue(w);
    m_spinHeight->setValue(h);
    // Center
    QSignalBlocker bk3(m_spinX);
    QSignalBlocker bk4(m_spinY);
    if (m_rotatable && m_rotation != 0) {
        double x = (W - w) / 2;
        double y = (H - h) / 2;
        QRectF bbox = rotatedBoundingRect(x, y, w, h, m_rotation);
        double dx = (W / 2.0) - (bbox.left() + bbox.width() / 2.0);
        double dy = (H / 2.0) - (bbox.top() + bbox.height() / 2.0);
        m_spinX->setValue(x + dx);
        m_spinY->setValue(y + dy);
    } else {
        m_spinX->setValue((m_defaultSize.width() - w) / 2);
        m_spinY->setValue((m_defaultSize.height() - h) / 2);
    }
    slotAdjustRectKeyframeValue();
}

void GeometryWidget::slotFitToWidth()
{
    double sourceDar = (m_sourceSize.width() / (double)m_sourceSize.height()) * pCore->getCurrentSar();
    double W = m_defaultSize.width();
    double w, h;
    if (m_rotatable && m_rotation != 0) {
        double angleRad = qDegreesToRadians(m_rotation);
        double c = std::abs(std::cos(angleRad));
        double s = std::abs(std::sin(angleRad));
        h = W / (sourceDar * c + s);
        w = sourceDar * h;
    } else {
        double factor = (double)m_defaultSize.width() / m_sourceSize.width() * pCore->getCurrentSar();
        h = qRound(m_sourceSize.height() * factor);
        w = m_defaultSize.width();
    }
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinHeight->setValue(h);
    m_spinWidth->setValue(w);
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    slotAdjustRectKeyframeValue();
}
void GeometryWidget::slotFitToHeight()
{
    double sourceDar = (m_sourceSize.width() / (double)m_sourceSize.height()) * pCore->getCurrentSar();
    double H = m_defaultSize.height();
    double w, h;
    if (m_rotatable && m_rotation != 0) {
        double angleRad = qDegreesToRadians(m_rotation);
        double c = std::abs(std::cos(angleRad));
        double s = std::abs(std::sin(angleRad));
        h = H / (sourceDar * s + c);
        w = sourceDar * h;
    } else {
        double factor = (double)m_defaultSize.height() / m_sourceSize.height();
        h = m_defaultSize.height();
        w = qRound(m_sourceSize.width() / pCore->getCurrentSar() * factor);
    }
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinHeight->setValue(h);
    m_spinWidth->setValue(w);
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    slotAdjustRectKeyframeValue();
}
void GeometryWidget::slotResize(double value)
{
    QSignalBlocker bkh(m_spinHeight);
    QSignalBlocker bkw(m_spinWidth);
    QSignalBlocker bkx(m_spinX);
    QSignalBlocker bky(m_spinY);
    int w = (m_originalSize->isChecked() ? m_sourceSize.width() : m_defaultSize.width()) * value / 100.0;
    int h = (m_originalSize->isChecked() ? m_sourceSize.height() : m_defaultSize.height()) * value / 100.0;
    int delta_x = (m_spinWidth->value() - w) / 2;
    int delta_y = (m_spinHeight->value() - h) / 2;
    m_spinWidth->setValue(w);
    m_spinHeight->setValue(h);
    m_spinX->setValue(m_spinX->value() + delta_x);
    m_spinY->setValue(m_spinY->value() + delta_y);
    slotAdjustRectKeyframeValue();
}

/** @brief Moves the rect to the left frame border (x position = 0). */
void GeometryWidget::slotMoveLeft()
{
    if (m_rotatable && m_rotation != 0) {
        double x = m_spinX->value();
        QRectF bbox = rotatedBoundingRect(x, m_spinY->value(), m_spinWidth->value(), m_spinHeight->value(), m_rotation);
        double dx = 0 - bbox.left();
        m_spinX->setValue(x + dx);
    } else {
        m_spinX->setValue(0);
    }
}
/** @brief Centers the rect horizontally. */
void GeometryWidget::slotCenterH()
{
    if (m_rotatable && m_rotation != 0) {
        double x = m_spinX->value();
        QRectF bbox = rotatedBoundingRect(x, m_spinY->value(), m_spinWidth->value(), m_spinHeight->value(), m_rotation);
        double dx = (m_defaultSize.width() / 2.0) - (bbox.left() + bbox.width() / 2.0);
        m_spinX->setValue(x + dx);
    } else {
        m_spinX->setValue((m_defaultSize.width() - m_spinWidth->value()) / 2);
    }
}
/** @brief Moves the rect to the right frame border (x position = frame width - rect width). */
void GeometryWidget::slotMoveRight()
{
    if (m_rotatable && m_rotation != 0) {
        double x = m_spinX->value();
        QRectF bbox = rotatedBoundingRect(x, m_spinY->value(), m_spinWidth->value(), m_spinHeight->value(), m_rotation);
        double dx = m_defaultSize.width() - bbox.right();
        m_spinX->setValue(x + dx);
    } else {
        m_spinX->setValue(m_defaultSize.width() - m_spinWidth->value());
    }
}

// Helper: returns the bounding rect of a rotated rectangle
QRectF GeometryWidget::rotatedBoundingRect(double x, double y, double w, double h, double angleDeg) const
{
    QPointF center(x + w / 2.0, y + h / 2.0);
    double angleRad = qDegreesToRadians(angleDeg);
    QVector<QPointF> corners = {QPointF(x, y), QPointF(x + w, y), QPointF(x + w, y + h), QPointF(x, y + h)};
    for (QPointF &pt : corners) {
        QPointF rel = pt - center;
        double xr = rel.x() * qCos(angleRad) - rel.y() * qSin(angleRad);
        double yr = rel.x() * qSin(angleRad) + rel.y() * qCos(angleRad);
        pt = QPointF(xr, yr) + center;
    }
    double minX = corners[0].x(), maxX = corners[0].x();
    double minY = corners[0].y(), maxY = corners[0].y();
    for (const QPointF &pt : corners) {
        minX = qMin(minX, pt.x());
        maxX = qMax(maxX, pt.x());
        minY = qMin(minY, pt.y());
        maxY = qMax(maxY, pt.y());
    }
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

/** @brief Moves the rect to the top frame border (y position = 0). */
void GeometryWidget::slotMoveTop()
{
    if (m_rotatable && m_rotation != 0) {
        // Move so that the top of the bounding box is at y=0
        double y = m_spinY->value();
        QRectF bbox = rotatedBoundingRect(m_spinX->value(), y, m_spinWidth->value(), m_spinHeight->value(), m_rotation);
        double dy = 0 - bbox.top();
        m_spinY->setValue(y + dy);
    } else {
        m_spinY->setValue(0);
    }
}

/** @brief Centers the rect vertically. */
void GeometryWidget::slotCenterV()
{
    if (m_rotatable && m_rotation != 0) {
        double y = m_spinY->value();
        QRectF bbox = rotatedBoundingRect(m_spinX->value(), y, m_spinWidth->value(), m_spinHeight->value(), m_rotation);
        double dy = (m_defaultSize.height() / 2.0) - (bbox.top() + bbox.height() / 2.0);
        m_spinY->setValue(y + dy);
    } else {
        m_spinY->setValue((m_defaultSize.height() - m_spinHeight->value()) / 2);
    }
}

/** @brief Moves the rect to the bottom frame border (y position = frame height - rect height). */
void GeometryWidget::slotMoveBottom()
{
    if (m_rotatable && m_rotation != 0) {
        double y = m_spinY->value();
        QRectF bbox = rotatedBoundingRect(m_spinX->value(), y, m_spinWidth->value(), m_spinHeight->value(), m_rotation);
        double dy = m_defaultSize.height() - bbox.bottom();
        m_spinY->setValue(y + dy);
    } else {
        m_spinY->setValue(m_defaultSize.height() - m_spinHeight->value());
    }
}

/** @brief Un/Lock aspect ratio for size in effect parameter. */
void GeometryWidget::slotLockRatio()
{
    auto *lockRatio = qobject_cast<QAction *>(QObject::sender());
    if (lockRatio->isChecked()) {
        m_monitor->setEffectSceneProperty(QStringLiteral("lockratio"), m_originalSize->isChecked() ? (double)m_sourceSize.width() / m_sourceSize.height()
                                                                                                   : (double)m_defaultSize.width() / m_defaultSize.height());
    } else {
        m_monitor->setEffectSceneProperty(QStringLiteral("lockratio"), -1);
    }
}
void GeometryWidget::slotAdjustRectHeight()
{
    int ix = 3;
    if (m_lockRatio->isChecked()) {
        ix = -1;
        m_spinWidth->blockSignals(true);
        if (m_originalSize->isChecked()) {
            m_spinWidth->setValue(qRound(m_spinHeight->value() * m_sourceSize.width() / m_sourceSize.height()));
        } else {
            m_spinWidth->setValue(qRound(m_spinHeight->value() * m_defaultSize.width() / m_defaultSize.height()));
        }
        m_spinWidth->blockSignals(false);
    }
    adjustSizeValue();
    slotAdjustRectKeyframeValue(ix);
}

void GeometryWidget::slotAdjustRectWidth()
{
    int ix = 2;
    if (m_lockRatio->isChecked()) {
        ix = -1;
        m_spinHeight->blockSignals(true);
        if (m_originalSize->isChecked()) {
            m_spinHeight->setValue(qRound(m_spinWidth->value() * m_sourceSize.height() / m_sourceSize.width()));
        } else {
            m_spinHeight->setValue(qRound(m_spinWidth->value() * m_defaultSize.height() / m_defaultSize.width()));
        }
        m_spinHeight->blockSignals(false);
    }
    adjustSizeValue();
    slotAdjustRectKeyframeValue(ix);
}

void GeometryWidget::adjustSizeValue()
{
    double size;
    if ((double)m_spinWidth->value() / m_spinHeight->value() < pCore->getCurrentDar()) {
        if (m_originalSize->isChecked()) {
            size = m_spinWidth->value() * 100.0 / m_sourceSize.width();
        } else {
            size = m_spinWidth->value() * 100.0 / m_defaultSize.width();
        }
    } else {
        if (m_originalSize->isChecked()) {
            size = m_spinHeight->value() * 100.0 / m_sourceSize.height();
        } else {
            size = m_spinHeight->value() * 100.0 / m_defaultSize.height();
        }
    }
    m_spinSize->blockSignals(true);
    m_spinSize->setValue(size);
    m_spinSize->blockSignals(false);
}

void GeometryWidget::slotAdjustRectKeyframeValue(int ix)
{
    QRect rect(m_spinX->value(), m_spinY->value(), m_spinWidth->value(), m_spinHeight->value());
    Q_EMIT updateMonitorGeometry(rect);
    Q_EMIT valueChanged(getValue(), ix, m_frameForRect);
}

void GeometryWidget::slotAdjustRectXKeyframeValue()
{
    QRect rect(m_spinX->value(), m_spinY->value(), m_spinWidth->value(), m_spinHeight->value());
    Q_EMIT updateMonitorGeometry(rect);
    Q_EMIT valueChanged(getValue(), 0, m_frameForRect);
}

void GeometryWidget::slotAdjustRectYKeyframeValue()
{
    QRect rect(m_spinX->value(), m_spinY->value(), m_spinWidth->value(), m_spinHeight->value());
    Q_EMIT updateMonitorGeometry(rect);
    Q_EMIT valueChanged(getValue(), 1, m_frameForRect);
}

void GeometryWidget::slotUpdateGeometryRect(const QRectF &r)
{
    if (!r.isValid()) {
        return;
    }
    m_spinX->blockSignals(true);
    m_spinY->blockSignals(true);
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinX->setValue(qRound(r.x()));
    m_spinY->setValue(qRound(r.y()));
    m_spinWidth->setValue(qRound(r.width()));
    m_spinHeight->setValue(qRound(r.height()));
    m_spinX->blockSignals(false);
    m_spinY->blockSignals(false);
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    // Q_EMIT updateMonitorGeometry(r);
    adjustSizeValue();
    if (m_spinX->isEnabled()) {
        Q_EMIT valueChanged(getValue(), -1, m_frameForRect);
    }
}

void GeometryWidget::setValue(const QRect r, double opacity, int frame)
{
    if (!r.isValid()) {
        return;
    }
    m_frameForRect = frame;
    m_spinX->blockSignals(true);
    m_spinY->blockSignals(true);
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinX->setValue(r.x());
    m_spinY->setValue(r.y());
    m_spinWidth->setValue(r.width());
    m_spinHeight->setValue(r.height());
    if (m_opacity) {
        m_opacity->blockSignals(true);
        if (opacity < 0) {
            opacity = 100 / m_opacityFactor;
        }
        m_opacity->setValue(qRound(opacity * m_opacityFactor));
        m_opacity->blockSignals(false);
    }
    m_spinX->blockSignals(false);
    m_spinY->blockSignals(false);
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    adjustSizeValue();
}

const QString GeometryWidget::getValue() const
{
    if (m_opacity) {
        return QStringLiteral("%1 %2 %3 %4 %5")
            .arg(m_spinX->value())
            .arg(m_spinY->value())
            .arg(m_spinWidth->value())
            .arg(m_spinHeight->value())
            .arg(QString::number(m_opacity->value() / m_opacityFactor, 'f'));
    }
    return QStringLiteral("%1 %2 %3 %4").arg(m_spinX->value()).arg(m_spinY->value()).arg(m_spinWidth->value()).arg(m_spinHeight->value());
}

const QRect GeometryWidget::getRect() const
{
    return QRect(m_spinX->value(), m_spinY->value(), m_spinWidth->value(), m_spinHeight->value());
}

void GeometryWidget::connectMonitor(bool activate, bool singleKeyframe)
{
    if (m_active == activate) {
        return;
    }
    m_active = activate;
    setEnabled(activate || singleKeyframe);
    if (activate) {
        connect(m_monitor, &Monitor::effectChanged, this, &GeometryWidget::slotUpdateGeometryRect, Qt::UniqueConnection);
        connect(m_monitor, &Monitor::effectRotationChanged, this, &GeometryWidget::slotUpdateRotation, Qt::UniqueConnection);
        QRect rect(m_spinX->value(), m_spinY->value(), m_spinWidth->value(), m_spinHeight->value());
        Q_EMIT updateMonitorGeometry(rect);
    } else {
        m_monitor->setEffectKeyframe(false, true);
        disconnect(m_monitor, &Monitor::effectChanged, this, &GeometryWidget::slotUpdateGeometryRect);
        disconnect(m_monitor, &Monitor::effectRotationChanged, this, &GeometryWidget::slotUpdateRotation);
    }
}

void GeometryWidget::slotSetRange(QPair<int, int> range)
{
    m_min = range.first;
    m_max = range.second;
}

void GeometryWidget::setEnabled(bool enable)
{
    for (auto &w : m_allWidgets) {
        w->setEnabled(enable);
    }
}

void GeometryWidget::setRotatable(bool rotatable)
{
    m_rotatable = rotatable;
}

void GeometryWidget::slotUpdateRotation(double rotation)
{
    if (!m_rotatable) {
        return;
    }
    m_rotation = rotation;
}
