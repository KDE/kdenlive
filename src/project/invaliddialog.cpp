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
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

// KDE
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>

InvalidDialog::InvalidDialog(const QString &caption, const QString &message, bool infoOnly, QWidget *parent)
    : QDialog(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    setWindowTitle(caption);
    // Info only means users can only click on ok
    QDialogButtonBox *buttonBox;
    QPushButton *okButton;
    if (infoOnly) {
        buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
        okButton = buttonBox->button(QDialogButtonBox::Ok);
    } else {
        buttonBox = new QDialogButtonBox(QDialogButtonBox::No | QDialogButtonBox::Yes);
        okButton = buttonBox->button(QDialogButtonBox::Yes);
    }
    okButton->setDefault(true);
    okButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &InvalidDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &InvalidDialog::reject);

    m_clipList = new QListWidget(this);
    mainLayout->addWidget(new QLabel(message));
    mainLayout->addWidget(m_clipList);
    mainLayout->addWidget(buttonBox);
}

InvalidDialog::~InvalidDialog()
{
    delete m_clipList;
}

void InvalidDialog::addClip(const QString &id, const QString &path)
{
    auto *item = new QListWidgetItem(path);
    item->setData(Qt::UserRole, id);
    m_clipList->addItem(item);
}

QStringList InvalidDialog::getIds() const
{
    QStringList ids;
    ids.reserve(m_clipList->count());
    for (int i = 0; i < m_clipList->count(); ++i) {
        ids << m_clipList->item(i)->data(Qt::UserRole).toString();
    }
    return ids;
}
