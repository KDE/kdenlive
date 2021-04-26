/***************************************************************************
 *   Copyright (C) 2017 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "trackdialog.h"

#include "kdenlivesettings.h"

#include <QIcon>
#include <utility>

TrackDialog::TrackDialog(std::shared_ptr<TimelineItemModel> model, int trackIndex, QWidget *parent, bool deleteMode, int activeTrackId)
    : QDialog(parent)
    , m_trackIndex(trackIndex)
    , m_model(std::move(model))
    , m_deleteMode(deleteMode)
    , m_activeTrack(activeTrackId)
{
    setWindowTitle(deleteMode ? i18n("Delete Track(s)") : i18n("Add Track"));
    // setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    if (m_trackIndex > -1 && m_model->isAudioTrack(m_trackIndex)) {
        audio_track->setChecked(true);
        before_select->setCurrentIndex(1);
    }
    if (deleteMode) {
        tracks_count->setVisible(false);
        track_name->setVisible(false);
        video_track->setVisible(false);
        audio_track->setVisible(false);
        av_track->setVisible(false);
        arec_track->setVisible(false);
        name_label->setVisible(false);
        before_select->setVisible(false);
        comboTracks->setVisible(false);
        label->setText(i18n("Select tracks to be deleted :"));
        this->adjustSize();
    }
    else {
        deleteTracks->setVisible(false);
    }
    buildCombo();
    connect(audio_track, &QRadioButton::toggled, this, &TrackDialog::buildCombo);
    connect(arec_track, &QRadioButton::toggled, this, &TrackDialog::buildCombo);
    connect(tracks_count, QOverload<int>::of(&QSpinBox::valueChanged), this, [this] (int count) {
        tracks_count->setSuffix(i18ncp("Spinbox suffix", " track", " tracks", count));
        track_name->setEnabled(count == 1);
    });
}

void TrackDialog::buildCombo()
{
    QIcon videoIcon = QIcon::fromTheme(QStringLiteral("kdenlive-show-video"));
    QIcon audioIcon = QIcon::fromTheme(QStringLiteral("kdenlive-show-audio"));
    m_positionByIndex.clear();
    comboTracks->clear();
    deleteTracks->clear();
    bool audioMode = audio_track->isChecked() || arec_track->isChecked();
    for (int i = m_model->getTracksCount() - 1; i >= 0; i--) {
        int tid = m_model->getTrackIndexFromPosition(i);
        bool audioTrack = m_model->isAudioTrack(tid);
        if (!m_deleteMode && audioMode != audioTrack) {
            continue;
        }
        const QString trackName = m_model->getTrackFullName(tid);
        if (m_deleteMode) {
            auto *track = new QListWidgetItem(audioTrack ? audioIcon : videoIcon,trackName);
            m_idByTrackname.insert(trackName,tid);
            track->setFlags(track->flags() | Qt::ItemIsUserCheckable);
            track->setCheckState(Qt::Unchecked);
            if(m_activeTrack == tid) {
                track->setCheckState(Qt::Checked);
            }
            deleteTracks->addItem(track);
        }
        else {
            comboTracks->addItem(audioTrack ? audioIcon : videoIcon, trackName.isEmpty() ? QString::number(i) : trackName, tid);
        }
        // Track index in in MLT, so add + 1 to compensate black track
        m_positionByIndex.insert(tid, i + 1);
    }
    if (m_trackIndex > -1) {
        int ix = qMax(0, comboTracks->findData(m_trackIndex));
        comboTracks->setCurrentIndex(ix);
    }
    if (m_deleteMode) {
        deleteTracks->setMinimumWidth(deleteTracks->sizeHintForColumn(0));
    }
}

int TrackDialog::selectedTrackPosition() const
{
    bool audioMode = audio_track->isChecked() || arec_track->isChecked();
    if (comboTracks->count() > 0) {
        int position = m_positionByIndex.value(comboTracks->currentData().toInt());
        if (audioMode && KdenliveSettings::audiotracksbelow() == 0) {
            // In mixed track modes, indexes are sorted differently so above/under have different meaning
            if (before_select->currentIndex() == 0) {
                position--;
            }
        } else if (before_select->currentIndex() == 1) {
            position--;
        }
        return position;
    }
    return audioMode ? 0 : -1;
}

int TrackDialog::selectedTrackId() const
{
    if (comboTracks->count() > 0) {
        return comboTracks->currentData().toInt();
    }
    return -1;
}

QList<int> TrackDialog::toDeleteTrackIds()
{
    QList<int> todeleteIds;
    for (int i = deleteTracks->count() - 1; i >= 0; i--)
    {
        QListWidgetItem *listitem = deleteTracks->item(i);
        if (listitem->checkState() == Qt::Checked) {
            todeleteIds.append(m_idByTrackname[listitem->text()]);
        }
    }
    m_idByTrackname.clear();
    return todeleteIds;
}

bool TrackDialog::addAVTrack() const
{
    return av_track->isChecked();
}

bool TrackDialog::addRecTrack() const
{
    return arec_track->isChecked();
}

bool TrackDialog::addAudioTrack() const
{
    return audio_track->isChecked() || arec_track->isChecked();
}
const QString TrackDialog::trackName() const
{
    return track_name->text();
}

int TrackDialog::tracksCount() const
{
    return tracks_count->value();
}
