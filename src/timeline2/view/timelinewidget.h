/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef TIMELINEWIDGET_H
#define TIMELINEWIDGET_H

#include "mltcontroller/bincontroller.h"
#include "timeline2/model/timelinemodel.hpp"
#include <QQuickWidget>

class BinController;

class TimelineWidget : public QQuickWidget
{
    Q_OBJECT
    Q_PROPERTY(QList<int> selection READ selection WRITE setSelection NOTIFY selectionChanged)
    Q_PROPERTY(double scaleFactor READ scaleFactor WRITE setScaleFactor NOTIFY scaleFactorChanged)
    Q_PROPERTY(int duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(int position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(bool snap READ snap NOTIFY snapChanged)
    Q_PROPERTY(bool ripple READ ripple NOTIFY rippleChanged)
    Q_PROPERTY(bool scrub READ scrub NOTIFY scrubChanged)

public:
    TimelineWidget(BinController *binController, std::weak_ptr<DocUndoStack> undoStack, QWidget *parent = Q_NULLPTR);
    void setSelection(QList<int> selection = QList<int>(), int trackIndex = -1, bool isMultitrack = false);
    QList<int> selection() const;
    Q_INVOKABLE bool isMultitrackSelected() const { return m_selection.isMultitrackSelected; }
    Q_INVOKABLE int selectedTrack() const { return m_selection.selectedTrack; }
    Q_INVOKABLE double scaleFactor() const;
    Q_INVOKABLE void setScaleFactor(double scale);
    Q_INVOKABLE bool moveClip(int toTrack, int clipIndex, int position, bool test_only = false);
    Q_INVOKABLE bool allowMoveClip(int toTrack, int clipIndex, int position);
    Q_INVOKABLE bool trimClip(int clipIndex, int delta, bool right);
    Q_INVOKABLE int duration() const;
    Q_INVOKABLE int position() const { return m_position; }
    Q_INVOKABLE void setPosition(int);
    Q_INVOKABLE bool snap();
    Q_INVOKABLE bool ripple();
    Q_INVOKABLE bool scrub();
    Q_INVOKABLE QString timecode(int frames);
    Q_INVOKABLE void insertClip(int track, int position, QString xml);

public slots:
    void selectMultitrack();
    void onSeeked(int position);

private:
    std::shared_ptr<TimelineModel> m_model;
    BinController *m_binController;
    struct Selection {
        QList<int> selectedClips;
        int selectedTrack;
        bool isMultitrackSelected;
    };
    int m_position;
    Selection m_selection;
    Selection m_savedSelection;
    void emitSelectedFromSelection();

signals:
    void selectionChanged();
    void selected(Mlt::Producer* producer);
    void trackHeightChanged();
    void scaleFactorChanged();
    void durationChanged();
    void positionChanged();
    void snapChanged();
    void rippleChanged();
    void scrubChanged();
    void seeked(int position);
};

#endif


