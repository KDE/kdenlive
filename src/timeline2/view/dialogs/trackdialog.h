/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef TRACKDIALOG2_H
#define TRACKDIALOG2_H

#include "timeline2/model/timelineitemmodel.hpp"
#include "ui_addtrack_ui.h"

class TrackDialog : public QDialog, public Ui::AddTrack_UI
{
    Q_OBJECT

public:
    explicit TrackDialog(std::shared_ptr<TimelineItemModel> model, int trackIndex = -1, QWidget *parent = nullptr, bool deleteMode = false, int activeTrack = -1);
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
    /** @brief: returns all the selected ids
     */
    QList<int> toDeleteTrackIds();
    /** @brief: returns the number of tracks to be inserted
     */
    int tracksCount() const;

private:
    int m_trackIndex;
    std::shared_ptr<TimelineItemModel> m_model;
    bool m_deleteMode;
    int m_activeTrack;
    QMap<int, int> m_positionByIndex;
    QMap<QString,int> m_idByTrackname;

private slots:
    /** @brief: Fill track list combo
     */
    void buildCombo();
};

#endif
