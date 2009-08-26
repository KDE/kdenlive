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


TrackDialog::TrackDialog(KdenliveDoc *doc, QWidget * parent) :
        QDialog(parent),
        m_doc(doc)
{
    //setFont(KGlobalSettings::toolBarFont());
    view.setupUi(this);
    connect(view.track_nb, SIGNAL(valueChanged(int)), this, SLOT(slotUpdateName(int)));
}

TrackDialog::~TrackDialog()
{
}

void TrackDialog::slotUpdateName(int ix)
{
    ix = m_doc->tracksCount() - ix;
    view.track_name->setText(m_doc->trackInfoAt(ix - 1).trackName);
}


#include "trackdialog.moc"


