/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef ANIMKEYFRAMERULER_H
#define ANIMKEYFRAMERULER_H

#include <QWidget>

#include "timecode.h"

namespace Mlt
{
}

class AnimKeyframeRuler : public QWidget
{
    Q_OBJECT
public:
    explicit AnimKeyframeRuler(int min, int max, QWidget *parent = nullptr);
    int position() const;
    int frameLength;
    void updateKeyframes(const QVector<int> &keyframes, const QVector<int> &types, int attachToEnd);
    void setActiveKeyframe(int frame);
    int activeKeyframe() const;

protected:
    void paintEvent(QPaintEvent * /*e*/) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent *e) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent *event) Q_DECL_OVERRIDE;

private:
    /** @brief Holds a list of frame positions for the keyframes. */
    QVector <int> m_keyframes;
    /** @brief Holds the keyframe type (linear, discrete, smooth) for each keyframes. */
    QVector <int> m_keyframeTypes;
    /** @brief Holds the keyframe relativity (relative to start, to end, percent) for each keyframes. */
    //QVector <int> m_keyframeRelatives;
    int m_position;
    int m_size;
    double m_scale;
    bool m_movingKeyframe;
    int m_movingKeyframePos;
    int m_movingKeyframeType;
    int m_hoverKeyframe;
    int m_selectedKeyframe;
    QPoint m_dragStart;
    int m_lineHeight;
    QColor m_selected;
    QColor m_keyframe;
    int m_seekPosition;
    int m_attachedToEnd;

public slots:
    void setValue(const int pos);

signals:
    void requestSeek(int);
    void keyframeMoved(int);
    void addKeyframe(int);
    void removeKeyframe(int);
    void moveKeyframe(int, int);
};

#endif
