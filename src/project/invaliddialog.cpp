/*
 *   SPDX-FileCopyrightText: 2007 Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *               2013 by Jean-Nicolas Artaud (jeannicolasartaud@gmail.com) *
 *                                                                         *
SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
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
