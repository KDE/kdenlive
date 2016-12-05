/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

/**
 * @class CustomRuler
 * @author Jean-Baptiste Mardelle
 * @brief Manages the timeline ruler.
 */

#ifndef CUSTOMRULER_H
#define CUSTOMRULER_H

#include <QWidget>
#include <QPair>

#include "timeline/customtrackview.h"
#include "timecode.h"

enum RULER_MOVE { RULER_CURSOR = 0, RULER_START = 1, RULER_MIDDLE = 2, RULER_END = 3 };
enum MOUSE_MOVE { NO_MOVE = 0, HORIZONTAL_MOVE = 1, VERTICAL_MOVE = 2 };

class CustomRuler : public QWidget
{
    Q_OBJECT

public:
    CustomRuler(const Timecode &tc, const QList<QAction *> &rulerActions, CustomTrackView *parent);
    void setPixelPerMark(int rate, bool force = false);
    static const int comboScale[];
    int outPoint() const;
    int inPoint() const;
    void setDuration(int d);
    void setZone(const QPoint &p);
    int offset() const;
    void updateProjectFps(const Timecode &t);
    void updateFrameSize();
    void activateZone();
    bool updatePreview(int frame, bool rendered = true, bool refresh = false);
    /** @brief Returns a list of rendered timeline preview chunks */
    const QPair <QStringList, QStringList> previewChunks() const;
    /** @brief Returns a list of clean timeline preview chunks (that have been created) */
    const QList<int> getProcessedChunks() const;
    /** @brief Returns a list of dirty timeline preview chunks (that need to be generated) */
    const QList<int> getDirtyChunks() const;
    void clearChunks();
    QList<int> addChunks(QList<int> chunks, bool add);
    /** @brief Returns true if a timeline preview zone has already be defined */
    bool hasPreviewRange() const;
    /** @brief Refresh timeline preview range */
    void updatePreviewDisplay(int start, int end);
    bool isUnderPreview(int start, int end);
    void hidePreview(bool hide);

protected:
    void paintEvent(QPaintEvent * /*e*/) Q_DECL_OVERRIDE;
    void wheelEvent(QWheelEvent *e) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QMouseEvent *event) Q_DECL_OVERRIDE;

private:
    Timecode m_timecode;
    CustomTrackView *m_view;
    int m_zoneStart;
    int m_zoneEnd;
    int m_duration;
    double m_textSpacing;
    double m_factor;
    double m_scale;
    int m_offset;
    bool m_hidePreview;
    /** @brief the position of the seek point */
    int m_headPosition;
    QColor m_zoneBG;
    RULER_MOVE m_moveCursor;
    QMenu *m_contextMenu;
    QAction *m_editGuide;
    QAction *m_deleteGuide;
    int m_clickedGuide;
    /** Used for zooming through vertical move */
    QPoint m_clickPoint;
    int m_rate;
    int m_startRate;
    MOUSE_MOVE m_mouseMove;
    QMenu *m_goMenu;
    QList<int> m_renderingPreviews;
    QList<int> m_dirtyRenderingPreviews;

public slots:
    void slotMoveRuler(int newPos);
    void slotCursorMoved(int oldpos, int newpos);
    void updateRuler(int pos);

private slots:
    void slotEditGuide();
    void slotDeleteGuide();
    void slotGoToGuide(QAction *act);

signals:
    void zoneMoved(int, int);
    void adjustZoom(int);
    void mousePosition(int);
    void seekCursorPos(int);
    void resizeRuler(int);
};

#endif
