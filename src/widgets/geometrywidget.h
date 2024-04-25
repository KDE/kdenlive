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

/** @class GeometryWidget
    @brief A widget for modifying numbers by dragging, using the mouse wheel or entering them with the keyboard.
 */
class GeometryWidget : public QWidget
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
    explicit GeometryWidget(Monitor *monitor, QPair<int, int> range, const QRect &rect, double opacity, const QSize frameSize, bool useRatioLock,
                            bool useOpacity, QWidget *parent = nullptr);
    void setValue(const QRect r, double opacity = 1);
    void connectMonitor(bool activate);

private:
    int m_min;
    int m_max;
    bool m_active;
    Monitor *m_monitor;
    DragValue *m_spinX;
    DragValue *m_spinY;
    DragValue *m_spinWidth;
    DragValue *m_spinHeight;
    DragValue *m_spinSize;
    DragValue *m_opacity;
    double m_opacityFactor;
    QSize m_defaultSize;
    QSize m_sourceSize;
    QAction *m_originalSize;
    QAction *m_lockRatio;
    const QString getValue() const;
    void adjustSizeValue();

public Q_SLOTS:
    void slotUpdateGeometryRect(const QRect r);
    void slotSetRange(QPair<int, int>);

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
    void valueChanged(const QString val, int ix);
    void updateMonitorGeometry(const QRect r);
};
