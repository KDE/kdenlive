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

TrackDialog::TrackDialog(std::shared_ptr<TimelineItemModel> model, int trackIndex, QWidget *parent, bool deleteMode) :
    QDialog(parent)
    , m_audioCount(1)
    , m_videoCount(1)
{
    //setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    QIcon videoIcon = QIcon::fromTheme(QStringLiteral("kdenlive-show-video"));
    QIcon audioIcon = QIcon::fromTheme(QStringLiteral("kdenlive-show-audio"));
    setupUi(this);
    QStringList existingTrackNames;
    for (int i = model->getTracksCount() - 1; i >= 0; i--) {
        int tid = model->getTrackIndexFromPosition(i);
        bool audioTrack = model->getTrackProperty(tid, QStringLiteral("kdenlive:audio_track")) == QLatin1String("1");
        if (audioTrack) {
            m_audioCount++;
        } else {
            m_videoCount++;
        }
        const QString trackName = model->getTrackProperty(tid, QStringLiteral("kdenlive:track_name")).toString();
        existingTrackNames << trackName;
        // Track index in in MLT, so add + 1 to compensate black track
        comboTracks->addItem(audioTrack ? audioIcon : videoIcon,
                             trackName.isEmpty() ? QString::number(i) : trackName, i + 1);
    }
    if (trackIndex > -1) {
        int ix = comboTracks->findData(trackIndex);
        comboTracks->setCurrentIndex(ix);
    }
    trackIndex--;
    if (deleteMode) {
        track_name->setVisible(false);
        video_track->setVisible(false);
        audio_track->setVisible(false);
        name_label->setVisible(false);
        before_select->setVisible(false);
        label->setText(i18n("Delete Track"));
    } else {
        QString proposedName = i18n("Video %1", trackIndex);
        while (existingTrackNames.contains(proposedName)) {
            proposedName = i18n("Video %1", ++trackIndex);
        }
        track_name->setText(proposedName);
    }
    connect(audio_track, &QRadioButton::toggled, this, &TrackDialog::updateName);
}

void TrackDialog::updateName(bool audioTrack)
{
    QString proposedName = i18n(audioTrack ? "Audio %1" : "Video %1", audioTrack ? m_audioCount : m_videoCount);
    track_name->setText(proposedName);
    
}


int TrackDialog::selectedTrack() const
{
    if (comboTracks->count() > 0) {
        int ix = comboTracks->currentData().toInt();
        if (before_select->currentIndex() == 1) {
            ix--;
        }
        return ix;
    }
    return -1;
}

bool TrackDialog::addAudioTrack() const
{
    return !video_track->isChecked();
}
const QString TrackDialog::trackName() const
{
    return track_name->text();
}
