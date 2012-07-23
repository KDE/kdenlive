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

#include "encodingprofilesdialog.h"
#include "kdenlivesettings.h"

#include <KStandardDirs>
#include <KDebug>
#include <KMessageBox>
#include <KIO/NetAccess>

#include <QDir>
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>

EncodingProfilesDialog::EncodingProfilesDialog(int profileType, QWidget * parent) :
    QDialog(parent),
    m_configGroup(NULL)
{
    setupUi(this);
    setWindowTitle(i18n("Manage Encoding Profiles"));
    profile_type->addItem(i18n("Proxy clips"), 0);
    profile_type->addItem(i18n("Video4Linux capture"), 1);
    profile_type->addItem(i18n("Decklink capture"), 2);
    
    button_add->setIcon(KIcon("list-add"));
    button_edit->setIcon(KIcon("document-edit"));
    button_delete->setIcon(KIcon("list-remove"));
    button_download->setIcon(KIcon("download"));
    
    m_configFile = new KConfig("encodingprofiles.rc", KConfig::FullConfig, "appdata");
    profile_type->setCurrentIndex(profileType);
    connect(profile_type, SIGNAL(currentIndexChanged(int)), this, SLOT(slotLoadProfiles()));
    connect(profile_list, SIGNAL(currentRowChanged(int)), this, SLOT(slotShowParams()));
    connect(button_delete, SIGNAL(clicked()), this, SLOT(slotDeleteProfile()));
    connect(button_add, SIGNAL(clicked()), this, SLOT(slotAddProfile()));
    connect(button_edit, SIGNAL(clicked()), this, SLOT(slotEditProfile()));
    profile_parameters->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5);
    slotLoadProfiles();
}

EncodingProfilesDialog::~EncodingProfilesDialog()
{
    delete m_configGroup;
    delete m_configFile;
}

void EncodingProfilesDialog::slotLoadProfiles()
{
    profile_list->blockSignals(true);
    profile_list->clear();
    QString group;
    switch (profile_type->currentIndex()) {
        case 0: 
            group = "proxy";
            break;          
        case 1: 
            group = "video4linux";
            break;
        default:
        case 2: 
            group = "decklink";
            break;
    }


    m_configGroup = new KConfigGroup(m_configFile, group);
    QMap< QString, QString > values = m_configGroup->entryMap();
    QMapIterator<QString, QString> i(values);
    while (i.hasNext()) {
        i.next();
        QListWidgetItem *item = new QListWidgetItem(i.key(), profile_list);
        item->setData(Qt::UserRole, i.value());
        //cout << i.key() << ": " << i.value() << endl;
    }
    profile_list->blockSignals(false);
    profile_list->setCurrentRow(0);
    button_delete->setEnabled(profile_list->count() > 0);
    button_edit->setEnabled(profile_list->count() > 0);
}

void EncodingProfilesDialog::slotShowParams()
{
    profile_parameters->clear();
    QListWidgetItem *item = profile_list->currentItem();
    if (!item) return;
    profile_parameters->setPlainText(item->data(Qt::UserRole).toString().section(';', 0, 0));
}

void EncodingProfilesDialog::slotDeleteProfile()
{
    QListWidgetItem *item = profile_list->currentItem();
    if (!item) return;
    QString profile = item->text();
    m_configGroup->deleteEntry(profile);
    slotLoadProfiles();
}

void EncodingProfilesDialog::slotAddProfile()
{
    QDialog *d = new QDialog(this);
    QVBoxLayout *l = new QVBoxLayout;
    l->addWidget(new QLabel(i18n("Profile name:")));
    QLineEdit *pname = new QLineEdit;
    l->addWidget(pname);
    l->addWidget(new QLabel(i18n("Parameters:")));
    QPlainTextEdit *pparams = new QPlainTextEdit;
    l->addWidget(pparams);
    l->addWidget(new QLabel(i18n("File extension:")));
    QLineEdit *pext = new QLineEdit;
    l->addWidget(pext);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(box, SIGNAL(accepted()), d, SLOT(accept()));
    connect(box, SIGNAL(rejected()), d, SLOT(reject()));
    l->addWidget(box);
    d->setLayout(l);
    
    QListWidgetItem *item = profile_list->currentItem();
    if (item) {
        QString data = item->data(Qt::UserRole).toString();
        pparams->setPlainText(data.section(';', 0, 0));
        pext->setText(data.section(';', 1, 1));
    }
    if (d->exec() == QDialog::Accepted) {
        m_configGroup->writeEntry(pname->text(), pparams->toPlainText() + ';' + pext->text());
        slotLoadProfiles();
    }
    delete d;
}

void EncodingProfilesDialog::slotEditProfile()
{
    QDialog *d = new QDialog(this);
    QVBoxLayout *l = new QVBoxLayout;
    l->addWidget(new QLabel(i18n("Profile name:")));
    QLineEdit *pname = new QLineEdit;
    l->addWidget(pname);
    l->addWidget(new QLabel(i18n("Parameters:")));
    QPlainTextEdit *pparams = new QPlainTextEdit;
    l->addWidget(pparams);
    l->addWidget(new QLabel(i18n("File extension:")));
    QLineEdit *pext = new QLineEdit;
    l->addWidget(pext);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(box, SIGNAL(accepted()), d, SLOT(accept()));
    connect(box, SIGNAL(rejected()), d, SLOT(reject()));
    l->addWidget(box);
    d->setLayout(l);
    
    QListWidgetItem *item = profile_list->currentItem();
    if (item) {
        pname->setText(item->text());
        QString data = item->data(Qt::UserRole).toString();
        pparams->setPlainText(data.section(';', 0, 0));
        pext->setText(data.section(';', 1, 1));
        pparams->setFocus();
    }
    if (d->exec() == QDialog::Accepted) {
        m_configGroup->writeEntry(pname->text(), pparams->toPlainText() + ';' + pext->text());
        slotLoadProfiles();
    }
    delete d;
}

