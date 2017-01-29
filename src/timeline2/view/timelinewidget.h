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

#include "../model/timelinemodel.hpp"
#include <QQuickWidget>


class TimelineWidget : public QQuickWidget
{
    Q_OBJECT
    Q_PROPERTY(QList<int> selection READ selection WRITE setSelection NOTIFY selectionChanged)
    Q_PROPERTY(double scaleFactor READ scaleFactor WRITE setScaleFactor NOTIFY scaleFactorChanged)
    Q_PROPERTY(int duration READ duration)

public:
    TimelineWidget(QWidget *parent = Q_NULLPTR);
    void setSelection(QList<int> selection = QList<int>(), int trackIndex = -1, bool isMultitrack = false);
    QList<int> selection() const;
    Q_INVOKABLE bool isMultitrackSelected() const { return m_selection.isMultitrackSelected; }
    Q_INVOKABLE int selectedTrack() const { return m_selection.selectedTrack; }
    Q_INVOKABLE double scaleFactor() const;
    Q_INVOKABLE void setScaleFactor(double scale);
    int duration() const;

public slots:
    void selectMultitrack();

private:
    std::shared_ptr<TimelineModel> m_model;
    struct Selection {
        QList<int> selectedClips;
        int selectedTrack;
        bool isMultitrackSelected;
    };
    Selection m_selection;
    Selection m_savedSelection;
    void emitSelectedFromSelection();

signals:
    void selectionChanged();
    void selected(Mlt::Producer* producer);
    void trackHeightChanged();
    void scaleFactorChanged();
};

#endif


