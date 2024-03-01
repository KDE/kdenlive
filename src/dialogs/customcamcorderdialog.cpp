/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "customcamcorderdialog.h"

#include "kdenlive_debug.h"

#include <QDir>
#include <QFontDatabase>
#include <QInputDialog>
#include <QListWidgetItem>
#include <QPushButton>
#include <QStandardPaths>
#include <QToolButton>

#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSharedConfig>

CustomCamcorderDialog::CustomCamcorderDialog(QWidget *parent)
    : QDialog(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setWindowTitle(i18n("Edit Custom Camcorder Proxy Profiles"));
    loadEntries();

    connect(profiles_list, &QListWidget::currentRowChanged, this, &CustomCamcorderDialog::updateParams);
    if (profiles_list->count() > 0) {
        profiles_list->setCurrentRow(0);
    }
    proxy_message->setText(i18n("You can input several profiles in a preset, separated by a semicolon"));
    connect(button_add, &QToolButton::clicked, this, &CustomCamcorderDialog::slotAddProfile);
    connect(button_remove, &QToolButton::clicked, this, &CustomCamcorderDialog::slotRemoveProfile);
    connect(button_reset, &QToolButton::clicked, this, &CustomCamcorderDialog::slotResetProfiles);
    connect(buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &CustomCamcorderDialog::slotSaveProfiles);
    connect(le_relPathOrigToProxy, &QLineEdit::textChanged, this, &CustomCamcorderDialog::validateInput);
    connect(le_prefix_proxy, &QLineEdit::textChanged, this, &CustomCamcorderDialog::validateInput);
    connect(le_suffix_proxy, &QLineEdit::textChanged, this, &CustomCamcorderDialog::validateInput);
    connect(le_relPathProxyToOrig, &QLineEdit::textChanged, this, &CustomCamcorderDialog::validateInput);
    connect(le_prefix_clip, &QLineEdit::textChanged, this, &CustomCamcorderDialog::validateInput);
    connect(le_suffix_clip, &QLineEdit::textChanged, this, &CustomCamcorderDialog::validateInput);
}

CustomCamcorderDialog::~CustomCamcorderDialog() {}

void CustomCamcorderDialog::slotSaveProfiles()
{
    // Sync data
    auto cfg = KSharedConfig::openConfig(QStringLiteral("externalproxies.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(cfg, "proxy");
    group.deleteGroup();
    for (int ix = 0; ix < profiles_list->count(); ix++) {
        auto item = profiles_list->item(ix);
        if (item->data(Qt::UserRole + 2).toInt() > 0) {
            // Broken profile
            continue;
        }
        group.writeEntry(item->text(), item->data(Qt::UserRole).toString());
    }
    cfg->sync();
}

void CustomCamcorderDialog::loadEntries()
{
    // Load system profiles
    KConfigGroup group(KSharedConfig::openConfig(QStringLiteral("externalproxies.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation), "proxy");
    QMap<QString, QString> values = group.entryMap();
    QMapIterator<QString, QString> k(values);
    profiles_list->clear();
    while (k.hasNext()) {
        k.next();
        if (!k.key().isEmpty()) {
            if (k.value().contains(QLatin1Char(';'))) {
                auto *item = new QListWidgetItem(k.key(), profiles_list);
                item->setData(Qt::UserRole, k.value());
            }
        }
    }
}

void CustomCamcorderDialog::updateParams(int row)
{
    if (row == -1) {
        return;
    }
    QString profile = profiles_list->item(row)->data(Qt::UserRole).toString();
    auto params = profile.split(";");
    QString val1, val2, val3, val4, val5, val6;
    int count = 0;
    while (params.count() >= 6) {
        if (count > 0) {
            val1.append(QLatin1Char(';'));
            val2.append(QLatin1Char(';'));
            val3.append(QLatin1Char(';'));
            val4.append(QLatin1Char(';'));
            val5.append(QLatin1Char(';'));
            val6.append(QLatin1Char(';'));
        }
        val1.append(params.at(0));
        val2.append(params.at(1));
        val3.append(params.at(2));
        val4.append(params.at(3));
        val5.append(params.at(4));
        val6.append(params.at(5));
        params = params.mid(6);
        count++;
    }
    le_relPathOrigToProxy->setText(val1);
    le_prefix_proxy->setText(val2);
    le_suffix_proxy->setText(val3);
    le_relPathProxyToOrig->setText(val4);
    le_prefix_clip->setText(val5);
    le_suffix_clip->setText(val6);
    validateInput();
}

void CustomCamcorderDialog::slotAddProfile()
{
    bool ok;
    QString profileName;
    QStringList existingProfiles;
    for (int ix = 0; ix < profiles_list->count(); ix++) {
        existingProfiles << profiles_list->item(ix)->text();
    }
    while (profileName.isEmpty() || existingProfiles.contains(profileName)) {
        profileName = QInputDialog::getText(this, i18nc("@title:window", "Enter Preset Name"),
                                            existingProfiles.contains(profileName) ? i18n("Preset already exists, enter a new name:")
                                                                                   : i18n("Enter the name of this preset:"),
                                            QLineEdit::Normal, profileName, &ok);
        if (!ok) return;
    }
    QString currentData;
    auto item = profiles_list->currentItem();
    if (item) {
        currentData = item->data(Qt::UserRole).toString();
    }
    auto newItem = new QListWidgetItem(profileName, profiles_list);
    newItem->setData(Qt::UserRole, currentData);
    newItem->setData(Qt::UserRole + 1, 1);
    profiles_list->setCurrentItem(newItem);
}

void CustomCamcorderDialog::slotRemoveProfile()
{
    auto item = profiles_list->currentItem();
    if (item) {
        delete item;
    }
}

void CustomCamcorderDialog::slotResetProfiles()
{
    if (KMessageBox::warningContinueCancel(this, i18n("This will delete all custom camcorder profiles and reset values to the default settings")) !=
        KMessageBox::Continue) {
        return;
    }
    const QString settingsFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!settingsFolder.isEmpty()) {
        QDir dir(settingsFolder);
        if (dir.exists(QStringLiteral("externalproxies.rc"))) {
            dir.remove(QStringLiteral("externalproxies.rc"));
            loadEntries();
        }
    }
}

void CustomCamcorderDialog::validateInput()
{
    bool multiProfileError = false;
    int profilesCount = le_relPathOrigToProxy->text().count(QLatin1Char(';'));
    if (le_prefix_proxy->text().count(QLatin1Char(';')) != profilesCount) {
        multiProfileError = true;
    } else if (le_suffix_proxy->text().count(QLatin1Char(';')) != profilesCount) {
        multiProfileError = true;
    } else if (le_relPathProxyToOrig->text().count(QLatin1Char(';')) != profilesCount) {
        multiProfileError = true;
    } else if (le_prefix_clip->text().count(QLatin1Char(';')) != profilesCount) {
        multiProfileError = true;
    } else if (le_suffix_clip->text().count(QLatin1Char(';')) != profilesCount) {
        multiProfileError = true;
    }
    if (multiProfileError) {
        proxy_message->setMessageType(KMessageWidget::Warning);
        proxy_message->setText(i18n(
            "You are using multiple presets in one profile, ensure you have the same count of semicolon in each parameter. This profile will not be saved."));
        auto brokenItem = profiles_list->currentItem();
        if (brokenItem) {
            brokenItem->setData(Qt::UserRole + 2, 1);
        }
        return;
    }
    proxy_message->setMessageType(KMessageWidget::Information);
    proxy_message->setText(i18n("You can input several profiles in a preset, separated by a semicolon"));
    auto currentItem = profiles_list->currentItem();
    if (!currentItem) {
        return;
    }
    if (currentItem->data(Qt::UserRole + 2).toInt() > 0) {
        currentItem->setData(Qt::UserRole + 2, QVariant());
    }
    QStringList profileData;
    profileData << le_relPathOrigToProxy->text().simplified();
    profileData << le_prefix_proxy->text().simplified();
    profileData << le_suffix_proxy->text().simplified();
    profileData << le_relPathProxyToOrig->text().simplified();
    profileData << le_prefix_clip->text().simplified();
    profileData << le_suffix_clip->text().simplified();
    currentItem->setData(Qt::UserRole, profileData.join(QLatin1Char(';')));
}
