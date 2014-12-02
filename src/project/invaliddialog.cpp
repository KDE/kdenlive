/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *               2013 by Jean-Nicolas Artaud (jeannicolasartaud@gmail.com) *
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

// Self
#include "invaliddialog.h"

// Qt
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>

// KDE
#include <KDialog>

InvalidDialog::InvalidDialog(const QString &caption, const QString &message, bool infoOnly, QWidget *parent)
    : KDialog(parent)
{
    setCaption(caption);
    // Info only means users can only click on ok
    if (infoOnly) {
        setButtons(KDialog::Ok);
    } else {
        setButtons(KDialog::Yes | KDialog::No);
    }

    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *boxLayout = new QVBoxLayout;
    boxLayout->addWidget(new QLabel(message));

    m_clipList = new QListWidget();
    boxLayout->addWidget(m_clipList);
    mainWidget->setLayout(boxLayout);
    setMainWidget(mainWidget);
}

InvalidDialog::~InvalidDialog()
{
    delete m_clipList;
}

void InvalidDialog::addClip(const QString &id, const QString &path)
{
    QListWidgetItem *item = new QListWidgetItem(path);
    item->setData(Qt::UserRole, id);
    m_clipList->addItem(item);
}

QStringList InvalidDialog::getIds() const
{
    QStringList ids;
    for (int i = 0; i < m_clipList->count(); ++i) {
        ids << m_clipList->item(i)->data(Qt::UserRole).toString();
    }
    return ids;
}
