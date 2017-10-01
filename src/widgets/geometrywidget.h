/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef GEOMETRYWIDGET2_H
#define GEOMETRYWIDGET2_H

#include <QDoubleSpinBox>
#include <QLayout>
#include <QSpinBox>
#include <QWidget>
#include <kselectaction.h>

class QAction;
class QMenu;
class KSelectAction;
class DragValue;

/**
 * @brief A widget for modifying numbers by dragging, using the mouse wheel or entering them with the keyboard.
 */

class GeometryWidget : public QWidget
{
    Q_OBJECT

public:
    /**
    * @brief Default constructor.
    * @param label The label that will be displayed in the progress bar
    * @param defaultValue The default value
    * @param decimals The number of decimals for the parameter. 0 means it is an integer
    * @param min The minimum value
    * @param max The maximum value
    * @param id Used to identify this widget. If this parameter is set, "Show in Timeline" will be available in context menu.
    * @param suffix The suffix that will be displayed in the spinbox (for example '%')
    * @param showSlider If disabled, user can still drag on the label but no progress bar is shown
    */
    explicit GeometryWidget(const QRect &rect, bool useRatioLock, QWidget *parent = nullptr);

private:
    DragValue *m_spinX;
    DragValue *m_spinY;
    DragValue *m_spinWidth;
    DragValue *m_spinHeight;
    DragValue *m_spinSize;
    QSize m_defaultSize;
    QAction *m_originalSize;
    QAction *m_lockRatio;
    const QString getValue() const;

public slots:
    void slotUpdateGeometryRect(const QRect r);

private slots:
    void slotAdjustRectKeyframeValue();
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

signals:
    void valueChanged(const QString val);

};

#endif
