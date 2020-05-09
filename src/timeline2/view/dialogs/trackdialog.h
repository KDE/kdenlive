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

#ifndef TRACKDIALOG2_H
#define TRACKDIALOG2_H

#include "timeline2/model/timelineitemmodel.hpp"
#include "ui_addtrack_ui.h"

class TrackDialog : public QDialog, public Ui::AddTrack_UI
{
    Q_OBJECT

public:
    explicit TrackDialog(const std::shared_ptr<TimelineItemModel> &model, int trackIndex = -1, QWidget *parent = nullptr, bool deleteMode = false);
    /** @brief: returns the selected position in MLT
     */
    int selectedTrackPosition() const;
    /** @brief: returns the selected track's trackId
     */
    int selectedTrackId() const;

    /** @brief: returns true if we want to insert an audio track
     */
    bool addAudioTrack() const;
    /** @brief: returns true if we want to insert an audio record track
     */
    bool addRecTrack() const;
    /** @brief: returns true if we want to insert an audio and video track
     */
    bool addAVTrack() const;
    /** @brief: returns the newly created track name
     */
    const QString trackName() const;
    /** @brief: returns the number of tracks to be inserted
     */
    int tracksCount() const;

private:
    int m_trackIndex;
    std::shared_ptr<TimelineItemModel> m_model;
    bool m_deleteMode;
    QMap<int, int> m_positionByIndex;

private slots:
    /** @brief: Fill track list combo
     */
    void buildCombo();
};

#endif
