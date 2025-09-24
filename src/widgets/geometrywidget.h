/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QWidget>
#include <kselectaction.h>

class QAction;
class DragValue;
class Monitor;
class QFormLayout;

/** @class GeometryWidget
    @brief A widget for modifying numbers by dragging, using the mouse wheel or entering them with the keyboard.
 */
class GeometryWidget : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Default constructor.
     * @param monitor The monitor attached to this stack
     * @param range The in / out points of the clip, useful to deactivate monitor scene when out of bounds
     * @param rect The default geometry value
     * @param frameSize The frame size of the original source video
     * @param useRatioLock When true, width/height will keep the profile's aspect ratio on resize
     */
    explicit GeometryWidget(Monitor *monitor, QPair<int, int> range, const QRect &rect, bool allowNullRect, double opacity, const QSize frameSize,
                            bool useRatioLock, bool useOpacity, QWidget *parent, QFormLayout *layout);
    void setValue(const QRect r, double opacity = 1, int frame = -1);
    bool connectMonitor(bool activate, bool singleKeyframe = false);
    void setEnabled(bool enable);
    const QRect getRect() const;
    void setRotatable(bool rotatable);

private:
    int m_min;
    int m_max;
    bool m_active;
    bool m_rotatable{false};
    double m_rotation{0};
    Monitor *m_monitor;
    DragValue *m_spinX;
    DragValue *m_spinY;
    DragValue *m_spinWidth;
    DragValue *m_spinHeight;
    DragValue *m_spinSize;
    DragValue *m_opacity{nullptr};
    int m_frameForRect{-1};
    double m_opacityFactor;
    QFormLayout *m_layout;
    QSize m_defaultSize;
    QSize m_sourceSize;
    QAction *m_originalSize;
    QAction *m_lockRatio;
    QList<QWidget *> m_allWidgets;
    const QString getValue() const;
    void adjustSizeValue();
    QRectF rotatedBoundingRect(double x, double y, double w, double h, double angleDeg) const;

public Q_SLOTS:
    void slotUpdateGeometryRect(const QRectF &r);
    void slotSetRange(QPair<int, int>);
    void slotUpdateRotation(double rotation);

private Q_SLOTS:
    void slotAdjustRectKeyframeValue(int ix = -1);
    void slotAdjustRectXKeyframeValue();
    void slotAdjustRectYKeyframeValue();
    void slotAdjustToSource();
    void slotAdjustToFrameSize();
    void slotFitToWidth();
    void slotFitToHeight();
    void slotResize(double value);
    /** @brief Moves the rect to the left frame border (x position = 0). */
    void slotMoveLeft();
    /** @brief Centers the rect horizontally. */
    void slotCenterH();
    /** @brief Moves the rect to the right frame border (x position = frame width - rect width). */
    void slotMoveRight();
    /** @brief Moves the rect to the top frame border (y position = 0). */
    void slotMoveTop();
    /** @brief Centers the rect vertically. */
    void slotCenterV();
    /** @brief Moves the rect to the bottom frame border (y position = frame height - rect height). */
    void slotMoveBottom();
    /** @brief Un/Lock aspect ratio for size in effect parameter. */
    void slotLockRatio();
    void slotAdjustRectHeight();
    void slotAdjustRectWidth();

Q_SIGNALS:
    void valueChanged(const QString val, int ix, int frame);
    void updateMonitorGeometry(const QRect r);
};
