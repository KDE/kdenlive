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

#include "klocalizedstring.h"
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStandardPaths>
#include <QVBoxLayout>

EncodingProfilesDialog::EncodingProfilesDialog(int profileType, QWidget *parent)
    : QDialog(parent)
    , m_configGroup(nullptr)
{
    setupUi(this);
    setWindowTitle(i18n("Manage Encoding Profiles"));
    profile_type->addItem(i18n("Proxy clips"), 0);
    profile_type->addItem(i18n("Timeline preview"), 1);
    profile_type->addItem(i18n("Video4Linux capture"), 2);
    profile_type->addItem(i18n("Screen capture"), 3);
    profile_type->addItem(i18n("Decklink capture"), 4);

    button_add->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    button_edit->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
    button_delete->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
    button_download->setIcon(QIcon::fromTheme(QStringLiteral("download")));

    m_configFile = new KConfig(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    profile_type->setCurrentIndex(profileType);
    connect(profile_type, static_cast<void (KComboBox::*)(int)>(&KComboBox::currentIndexChanged), this, &EncodingProfilesDialog::slotLoadProfiles);
    connect(profile_list, &QListWidget::currentRowChanged, this, &EncodingProfilesDialog::slotShowParams);
    connect(button_delete, &QAbstractButton::clicked, this, &EncodingProfilesDialog::slotDeleteProfile);
    connect(button_add, &QAbstractButton::clicked, this, &EncodingProfilesDialog::slotAddProfile);
    connect(button_edit, &QAbstractButton::clicked, this, &EncodingProfilesDialog::slotEditProfile);
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
        group = QStringLiteral("proxy");
        break;
    case 2:
        group = QStringLiteral("video4linux");
        break;
    case 3:
        group = QStringLiteral("screengrab");
        break;
    case 4:
        group = QStringLiteral("decklink");
        break;
    case 1:
    default:
        group = QStringLiteral("timelinepreview");
        break;
    }

    delete m_configGroup;
    m_configGroup = new KConfigGroup(m_configFile, group);
    QMap<QString, QString> values = m_configGroup->entryMap();
    QMapIterator<QString, QString> i(values);
    while (i.hasNext()) {
        i.next();
        auto *item = new QListWidgetItem(i.key(), profile_list);
        item->setData(Qt::UserRole, i.value());
        // cout << i.key() << ": " << i.value() << endl;
    }
    profile_list->blockSignals(false);
    profile_list->setCurrentRow(0);
    const bool multiProfile(profile_list->count() > 0);
    button_delete->setEnabled(multiProfile);
    button_edit->setEnabled(multiProfile);
}

void EncodingProfilesDialog::slotShowParams()
{
    profile_parameters->clear();
    QListWidgetItem *item = profile_list->currentItem();
    if (!item) {
        return;
    }
    profile_parameters->setPlainText(item->data(Qt::UserRole).toString().section(QLatin1Char(';'), 0, 0));
}

void EncodingProfilesDialog::slotDeleteProfile()
{
    QListWidgetItem *item = profile_list->currentItem();
    if (!item) {
        return;
    }
    QString profile = item->text();
    m_configGroup->deleteEntry(profile);
    slotLoadProfiles();
}

void EncodingProfilesDialog::slotAddProfile()
{
    QPointer<QDialog> d = new QDialog(this);
    auto *l = new QVBoxLayout;
    l->addWidget(new QLabel(i18n("Profile name:")));
    auto *pname = new QLineEdit;
    l->addWidget(pname);
    l->addWidget(new QLabel(i18n("Parameters:")));
    auto *pparams = new QPlainTextEdit;
    l->addWidget(pparams);
    l->addWidget(new QLabel(i18n("File extension:")));
    auto *pext = new QLineEdit;
    l->addWidget(pext);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(box, &QDialogButtonBox::accepted, d.data(), &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, d.data(), &QDialog::reject);
    l->addWidget(box);
    d->setLayout(l);

    QListWidgetItem *item = profile_list->currentItem();
    if (item) {
        QString profilestr = item->data(Qt::UserRole).toString();
        pparams->setPlainText(profilestr.section(QLatin1Char(';'), 0, 0));
        pext->setText(profilestr.section(QLatin1Char(';'), 1, 1));
    }
    if (d->exec() == QDialog::Accepted) {
        m_configGroup->writeEntry(pname->text(), pparams->toPlainText() + QLatin1Char(';') + pext->text());
        slotLoadProfiles();
    }
    delete d;
}

void EncodingProfilesDialog::slotEditProfile()
{
    QPointer<QDialog> d = new QDialog(this);
    auto *l = new QVBoxLayout;
    l->addWidget(new QLabel(i18n("Profile name:")));
    auto *pname = new QLineEdit;
    l->addWidget(pname);
    l->addWidget(new QLabel(i18n("Parameters:")));
    auto *pparams = new QPlainTextEdit;
    l->addWidget(pparams);
    l->addWidget(new QLabel(i18n("File extension:")));
    auto *pext = new QLineEdit;
    l->addWidget(pext);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    connect(box, &QDialogButtonBox::accepted, d.data(), &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, d.data(), &QDialog::reject);
    l->addWidget(box);
    d->setLayout(l);

    QListWidgetItem *item = profile_list->currentItem();
    if (item) {
        pname->setText(item->text());
        QString profilestr = item->data(Qt::UserRole).toString();
        pparams->setPlainText(profilestr.section(QLatin1Char(';'), 0, 0));
        pext->setText(profilestr.section(QLatin1Char(';'), 1, 1));
        pparams->setFocus();
    }
    if (d->exec() == QDialog::Accepted) {
        m_configGroup->writeEntry(pname->text(), pparams->toPlainText().simplified() + QLatin1Char(';') + pext->text());
        slotLoadProfiles();
    }
    delete d;
}
