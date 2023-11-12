/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ui_timeremap_ui.h"

#include "definitions.h"
#include "utils/timecode.h"

#include <QMutex>
#include <QTimer>
#include <QUuid>
#include <QWidget>

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
    void setDuration(std::shared_ptr<Mlt::Producer> service, int duration, int sourceDuration = 0);
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
    Mlt::Properties m_remapProps;
    /** @brief The position of the clip in timeline, used to seek to correct place */
    int m_startPos;
    /** @brief The in frame of the clip in timeline, used to correctly offset keyframes */
    int m_inFrame;
    int m_oldInFrame;
    /** @brief the original in/out of the clip when opening the remap widget, used to snap to ends */
    std::pair<int,int> m_originalRange;

public Q_SLOTS:
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
    int m_sourceDuration;
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
    std::pair<int, int> m_currentKeyframe{-1, -1};
    std::pair<int, int> m_currentKeyframeOriginal{-1, -1};
    MOVEMODE m_moveKeyframeMode;
    double m_clickOffset;
    int m_clickPoint;
    bool m_moveNext;
    int m_clickEnd;
    int m_offset;
    QMutex m_kfrMutex;
    QMap <int,int>m_selectedKeyframes;
    QMap<int,int>m_previousSelection;
    std::pair<int,int> getClosestKeyframe(int pos, bool bottomKeyframe = false) const;
    std::pair<double,double> getSpeed(std::pair<int,int>kf);

Q_SIGNALS:
    void seekToPos(int,int);
    void selectedKf(std::pair<int,int>, std::pair<double,double>, std::pair<bool,bool>atEnd = {true,true});
    void updateSpeeds(std::pair<double,double>);
    /** When the cursor position changes inform parent if we are on a keyframe or not. */
    void atKeyframe(bool isKeyframe, bool last);
    void updateKeyframes(bool resize);
    void updateKeyframesWithUndo(QMap<int,int>updatedKeyframes, QMap<int,int>previousKeyframes);
    void updateMaxDuration();
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
    void selectedClip(int cid, const QUuid uuid);
    void setClip(std::shared_ptr<ProjectClip> clip, int in = -1, int out = -1);
    /** @brief the bin id of the clip currently active in remap widget */
    const QString &currentClip() const;
    /** @brief Returns true if timeline cursor is inside the remapped clip */
    bool isInRange() const;

private Q_SLOTS:
    void updateKeyframes();
    void updateKeyframesWithUndo(const QMap<int,int>&updatedKeyframes, const QMap<int,int>&previousKeyframes);
    void checkClipUpdate(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int>& roles);
    void switchRemapParam();
    void monitorSeek(int pos);

private:
    std::shared_ptr<Mlt::Link> m_remapLink;
    std::shared_ptr<Mlt::Link> m_splitRemap;
    RemapView *m_view;
    int m_lastLength;
    int m_cid;
    QUuid m_uuid;
    int m_splitId;
    QString m_binId;
    QMetaObject::Connection m_seekConnection1;
    QMetaObject::Connection m_seekConnection2;
    QMetaObject::Connection m_seekConnection3;

};
