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


#include "trackdialog.h"
#include "kdenlivedoc.h"
#include "kdenlivesettings.h"

#include <KDebug>
#include <KIcon>


TrackDialog::TrackDialog(KdenliveDoc *doc, QWidget * parent) :
        QDialog(parent)
{
    //setFont(KGlobalSettings::toolBarFont());
    KIcon videoIcon("kdenlive-show-video");
    KIcon audioIcon("kdenlive-show-audio");
    setupUi(this);
    for (int i = 0; i < doc->tracksCount(); ++i) {
        TrackInfo info = doc->trackInfoAt(doc->tracksCount() - i - 1);
        comboTracks->addItem(info.type == VIDEOTRACK ? videoIcon : audioIcon,
                             info.trackName.isEmpty() ? QString::number(i) : info.trackName + " (" + QString::number(i) + ')');
    }
}

#include "trackdialog.moc"


