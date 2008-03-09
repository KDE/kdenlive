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

#include <QDir>

#include <KStandardDirs>
#include <KDebug>

#include "kdenlivesettings.h"
#include "renderwidget.h"

RenderWidget::RenderWidget(QWidget * parent): QDialog(parent) {
    m_view.setupUi(this);
    connect(m_view.buttonStart, SIGNAL(clicked()), this, SLOT(slotExport()));
    connect(m_view.out_file, SIGNAL(textChanged(const QString &)), this, SLOT(slotUpdateButtons()));
    m_view.buttonStart->setEnabled(false);
}

void RenderWidget::slotUpdateButtons() {
    if (m_view.out_file->url().isEmpty()) m_view.buttonStart->setEnabled(false);
    else m_view.buttonStart->setEnabled(true);
}

void RenderWidget::slotExport() {
    emit doRender(m_view.out_file->url().path(), QStringList(), m_view.zone_only->isChecked(), m_view.play_after->isChecked());
}


#include "renderwidget.moc"


