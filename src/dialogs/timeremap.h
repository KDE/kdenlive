/***************************************************************************
 *   Copyright (C) 2021 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef TIMEREMAP_H
#define TIMEREMAP_H

#include "ui_timeremap_ui.h"

#include "definitions.h"
#include "timecode.h"

#include <QWidget>
#include <QTimer>

#include "mlt++/Mlt.h"

class ProjectClip;
class TimecodeDisplay;


/**
 * @class RemapView
 * @brief A widget to view remap keyframes.
 * @author Jean-Baptiste Mardelle
 */

class RemapView : public QWidget
{
    Q_OBJECT

public:
    friend class TimeRemap;
    explicit RemapView(QWidget *parent = nullptr);
    void setBinClipDuration(std::shared_ptr<ProjectClip> clip, int duration);
    void setDuration(std::shared_ptr<Mlt::Producer> service, int duration);
    void loadKeyframes(const QString &mapData);
    const QString getKeyframesData(QMap<int,int> keyframes = {}) const;
    int position() const;
    int remapDuration() const;
    int remapMax() const;
    bool movingKeyframe() const;
    void refreshOnDurationChanged(int remapDuration);
    /** @brief Returns true if timeline cursor is inside the remapped clip */
    bool isInRange() const;
    QTimer timer;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    std::shared_ptr<Mlt::Link> m_remapLink;
    /** @brief The position of the clip in timeline, used to seek to correct place */
    int m_startPos;
    /** @brief The in frame of the clip in timeline, used to correctly offset keyframes */
    int m_inFrame;
    int m_oldInFrame;
    /** @brief the original in/out of the clip when opening the remap widget, used to snap to ends */
    std::pair<int,int> m_originalRange;

public slots:
    void updateInPos(int pos);
    void updateOutPos(int pos);
    void slotSetPosition(int pos);
    void slotSetBottomPosition(int pos);
    void addKeyframe();
    void goNext();
    void goPrev();
    void updateBeforeSpeed(double speed);
    void updateAfterSpeed(double speed);
    void toggleMoveNext(bool moveNext);
    void reloadProducer();
    void centerCurrentKeyframe();
    void centerCurrentTopKeyframe();

private:
    enum MOVEMODE {NoMove, TopMove, BottomMove, CursorMove, CursorMoveBottom};
    int m_duration;
    int m_lastMaxDuration;
    int m_position;
    /** @brief the maximum duration of the parent (bin) clip */
    int m_maxLength;
    int m_bottomPosition;
    double m_scale;
    QColor m_colSelected;
    QColor m_colKeyframe;
    int m_zoomHeight;
    int m_centerPos;
    int m_lineHeight;
    double m_zoomFactor;
    double m_zoomStart;
    /** @brief The zoom factor (start, end - between 0 and 1) */
    QPointF m_zoomHandle;
    /** @brief the keyframes for the remap effect. first value is output, second is source time */
    QMap<int, int>m_keyframes;
    QMap<int, int>m_keyframesOrigin;
    std::shared_ptr<ProjectClip> m_clip;
    std::shared_ptr<Mlt::Producer> m_service;
    QPointF m_lastZoomHandle;
    /** @brief Mouse is the zoom left handle */
    bool m_hoverZoomIn;
    /** @brief Mouse is the zoom right handle */
    bool m_hoverZoomOut;
    /** @brief Mouse is over the zoom bar */
    bool m_hoverZoom;
    int m_bottomView;
    std::pair<int, int> m_currentKeyframe;
    std::pair<int,int> m_currentKeyframeOriginal;
    MOVEMODE m_moveKeyframeMode;
    double m_clickOffset;
    int m_clickPoint;
    bool m_moveNext;
    int m_clickEnd;
    int m_offset;
    QMap <int,int>m_selectedKeyframes;
    QMap<int,int>m_previousSelection;
    std::pair<int,int> getClosestKeyframe(int pos, bool bottomKeyframe = false) const;
    std::pair<double,double> getSpeed(std::pair<int,int>kf);

signals:
    void seekToPos(int,int);
    void selectedKf(std::pair<int,int>, std::pair<double,double>, std::pair<bool,bool>atEnd = {true,true});
    void updateSpeeds(std::pair<double,double>);
    /** When the cursor position changes inform parent if we are on a keyframe or not. */
    void atKeyframe(bool isKeyframe, bool last);
    void updateKeyframes(bool resize);
    void updateKeyframesWithUndo(QMap<int,int>updatedKeyframes, QMap<int,int>previousKeyframes);
    void updateMaxDuration(int duration);
};

 /**
 * @class TimeRemap
 * @brief A dialog for editing time remap effect.
 * @author Jean-Baptiste Mardelle
 */
class TimeRemap : public QWidget, public Ui::TimeRemap_UI
{
    Q_OBJECT

public:
    explicit TimeRemap(QWidget *parent = nullptr);
    ~TimeRemap() override;
    void selectedClip(int cid);
    void setClip(std::shared_ptr<ProjectClip> clip, int in = -1, int out = -1);
    /** @brief the bin id of the clip currently active in remap widget */
    const QString &currentClip() const;
    /** @brief Returns true if timeline cursor is inside the remapped clip */
    bool isInRange() const;

private slots:
    void updateKeyframes(bool resize = true);
    void updateKeyframesWithUndo(QMap<int,int>updatedKeyframes, QMap<int,int>previousKeyframes);
    void checkClipUpdate(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int>& roles);
    void switchRemapParam();

private:
    std::shared_ptr<Mlt::Link> m_splitRemap;
    TimecodeDisplay *m_in;
    TimecodeDisplay *m_out;
    RemapView *m_view;
    int m_lastLength;
    int m_cid;
    int m_splitId;
    QString m_binId;
    QMetaObject::Connection m_seekConnection1;
    QMetaObject::Connection m_seekConnection2;
    QMetaObject::Connection m_seekConnection3;

};

#endif
