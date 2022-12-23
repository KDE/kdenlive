/*
    SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "clipjobmanager.h"

#include "core.h"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"

#include "kdenlive_debug.h"
#include <KLocalizedString>
#include <KMessageBox>
#include <QFontDatabase>
#include <QUuid>

ClipJobManager::ClipJobManager(QWidget *parent)
    : QDialog(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setWindowTitle(i18n("Manage Bin Clip Jobs"));
    connect(job_list, &QListWidget::currentRowChanged, this, &ClipJobManager::displayJob);
    loadJobs();
    job_list->setCurrentRow(0);
    job_list->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    connect(button_add, &QToolButton::clicked, this, &ClipJobManager::addJob);
    connect(button_delete, &QToolButton::clicked, this, &ClipJobManager::deleteJob);

    connect(job_list, &QListWidget::itemChanged, this, &ClipJobManager::updateName);

    // Mark preset as dirty if anything changes
    connect(url_binary, &KUrlRequester::textChanged, this, &ClipJobManager::setDirty);
    connect(job_params, &QPlainTextEdit::textChanged, this, &ClipJobManager::setDirty);
    connect(destination_pattern, &QLineEdit::textChanged, this, &ClipJobManager::setDirty);
    connect(radio_replace, &QRadioButton::toggled, this, &ClipJobManager::setDirty);
    connect(combo_folder, &QComboBox::currentIndexChanged, this, &ClipJobManager::setDirty);
    connect(folder_name, &QLineEdit::textChanged, this, &ClipJobManager::setDirty);
    connect(buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &ClipJobManager::validate);
}

ClipJobManager::~ClipJobManager() {}

void ClipJobManager::setDirty()
{
    m_dirty = job_list->currentItem()->data(Qt::UserRole).toString();
}

void ClipJobManager::validate()
{
    /*if (!m_dirty.isEmpty()) {
        saveCurrentPreset();
    }*/
    saveAllPresets();
    accept();
}

void ClipJobManager::loadJobs()
{
    QSignalBlocker bk(job_list);
    job_list->clear();
    // Add jobs
    KConfig conf(QStringLiteral("clipjobsettings.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "Ids");
    m_ids = group.entryMap();
    QListWidgetItem *item;

    // Add builtin jobs
    item = new QListWidgetItem(i18n("Stabilize"), job_list, QListWidgetItem::Type);
    item->setData(Qt::UserRole, QLatin1String("stabilize"));
    item = new QListWidgetItem(i18n("Automatic Scene Split…"), job_list, QListWidgetItem::Type);
    item->setData(Qt::UserRole, QLatin1String("scenesplit"));
    item = new QListWidgetItem(i18n("Duplicate Clip with Speed Change…"), job_list, QListWidgetItem::Type);
    item->setData(Qt::UserRole, QLatin1String("timewarp"));

    QMapIterator<QString, QString> k(m_ids);
    while (k.hasNext()) {
        k.next();
        if (!k.value().isEmpty()) {
            item = new QListWidgetItem(k.value(), job_list, QListWidgetItem::UserType);
            item->setData(Qt::UserRole, k.key());
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
        }
    }
    // Read all data
    KConfigGroup groupParams(&conf, "Parameters");
    m_params = groupParams.entryMap();
    KConfigGroup groupFolder(&conf, "FolderName");
    m_folderNames = groupFolder.entryMap();
    KConfigGroup groupUse(&conf, "FolderUse");
    m_folderUse = groupUse.entryMap();
    KConfigGroup groupOutput(&conf, "Output");
    m_output = groupOutput.entryMap();
    KConfigGroup groupBinary(&conf, "Binary");
    m_binaries = groupBinary.entryMap();
}

void ClipJobManager::displayJob(int row)
{
    if (!m_dirty.isEmpty()) {
        saveCurrentPreset();
    }
    if (row == -1) {
        param_box->setEnabled(false);
        return;
    }
    param_box->setEnabled(true);
    QListWidgetItem *item = job_list->item(row);
    QString jobId = item->data(Qt::UserRole).toString();
    bool customJob = item->type() == QListWidgetItem::UserType;
    url_binary->setEnabled(customJob);
    job_params->setEnabled(customJob);
    button_delete->setEnabled(customJob);
    if (customJob && !m_ids.contains(jobId)) {
        // This is a new job, set some default values
        url_binary->setUrl(QUrl::fromLocalFile(KdenliveSettings::ffmpegpath()));
    } else {
        url_binary->setText(m_binaries.value(jobId));
    }
    job_params->setPlainText(m_params.value(jobId));
    destination_pattern->setText(m_output.value(jobId));
    folder_name->setText(m_folderNames.value(jobId));
    folder_name->setText(m_folderNames.value(jobId));
    if (m_folderUse.contains(jobId)) {
        folder_box->setEnabled(true);
        if (m_folderUse.value(jobId) == QLatin1String("replace")) {
            radio_replace->setChecked(true);
        } else if (m_folderUse.value(jobId) == QLatin1String("rootfolder")) {
            radio_folder->setChecked(true);
            combo_folder->setCurrentIndex(0);
        } else if (m_folderUse.value(jobId) == QLatin1String("subfolder")) {
            radio_folder->setChecked(true);
            combo_folder->setCurrentIndex(1);
        } else {
            folder_box->setEnabled(false);
        }
    } else {
        folder_box->setEnabled(false);
    }
}

QMap<QString, QString> ClipJobManager::getClipJobNames()
{
    KConfig conf(QStringLiteral("clipjobsettings.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "Ids");
    QMap<QString, QString> ids = group.entryMap();
    // Add the 3 internal jobs
    if (EffectsRepository::get()->exists(QLatin1String("vidstab"))) {
        ids.insert(QStringLiteral("stabilize"), i18n("Stabilize"));
    }
    ids.insert(QStringLiteral("scenesplit"), i18n("Automatic Scene Split…"));
    if (KdenliveSettings::producerslist().contains(QLatin1String("timewarp"))) {
        ids.insert(QStringLiteral("timewarp"), i18n("Duplicate Clip with Speed Change…"));
    }
    return ids;
}

std::pair<ClipJobManager::JobCompletionAction, QString> ClipJobManager::getJobAction(const QString &jobId)
{
    KConfig conf(QStringLiteral("clipjobsettings.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "FolderUse");
    KConfigGroup nameGroup(&conf, "FolderName");
    QString useValue = group.readEntry(jobId, QString());
    if (useValue == QLatin1String("replace")) {
        return {JobCompletionAction::ReplaceOriginal, QString()};
    }
    if (useValue == QLatin1String("rootfolder")) {
        return {JobCompletionAction::RootFolder, nameGroup.readEntry(jobId, QString())};
    }
    if (useValue == QLatin1String("subfolder")) {
        return {JobCompletionAction::SubFolder, nameGroup.readEntry(jobId, QString())};
    }
    return {JobCompletionAction::NoAction, nameGroup.readEntry(jobId, QString())};
}

QStringList ClipJobManager::getJobParameters(const QString &jobId)
{
    QStringList result;
    KConfig conf(QStringLiteral("clipjobsettings.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup idGroup(&conf, "Ids");
    result << idGroup.readEntry(jobId, i18n("Job description"));
    KConfigGroup group(&conf, "Binary");
    result << group.readEntry(jobId, QString());
    KConfigGroup paramGroup(&conf, "Parameters");
    result << paramGroup.readEntry(jobId, QString());
    KConfigGroup outGroup(&conf, "Output");
    result << outGroup.readEntry(jobId, QString());
    return result;
}

void ClipJobManager::saveCurrentPreset()
{
    m_folderNames.insert(m_dirty, folder_name->text().simplified());
    if (radio_replace->isChecked()) {
        m_folderUse.insert(m_dirty, QStringLiteral("replace"));
    } else if (combo_folder->currentIndex() == 0) {
        m_folderUse.insert(m_dirty, QStringLiteral("rootfolder"));
    } else {
        m_folderUse.insert(m_dirty, QStringLiteral("subfolder"));
    }
    m_binaries.insert(m_dirty, url_binary->text().simplified());
    m_params.insert(m_dirty, job_params->toPlainText().simplified());
    m_output.insert(m_dirty, destination_pattern->text().simplified());
    m_dirty.clear();
}

void ClipJobManager::writeGroup(KConfig &conf, const QString &groupName, QMap<QString, QString> values)
{
    KConfigGroup idGroup(&conf, groupName);
    QMapIterator<QString, QString> i(values);
    while (i.hasNext()) {
        i.next();
        idGroup.writeEntry(i.key(), i.value());
    }
}

void ClipJobManager::saveAllPresets()
{
    KConfig conf(QStringLiteral("clipjobsettings.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    writeGroup(conf, QStringLiteral("Ids"), m_ids);
    writeGroup(conf, QStringLiteral("Binary"), m_binaries);
    writeGroup(conf, QStringLiteral("Parameters"), m_params);
    writeGroup(conf, QStringLiteral("Output"), m_output);
    writeGroup(conf, QStringLiteral("FolderName"), m_folderNames);
    writeGroup(conf, QStringLiteral("FolderUse"), m_folderUse);
}

void ClipJobManager::addJob()
{
    const QString uuid = QUuid::createUuid().toString();
    QListWidgetItem *item = new QListWidgetItem(i18n("My Clip Job"), job_list, QListWidgetItem::UserType);
    item->setData(Qt::UserRole, uuid);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
    job_list->setCurrentItem(item);
    KConfig conf(QStringLiteral("clipjobsettings.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup idGroup(&conf, "Ids");
    idGroup.writeEntry(uuid, item->text());
    m_dirty = uuid;
}

void ClipJobManager::deleteJob()
{
    QListWidgetItem *item = job_list->currentItem();
    if (item && item->type() == QListWidgetItem::UserType) {
        if (KMessageBox::warningContinueCancel(this, i18n("Permanently delete this Clip Job:\n%1 ?", item->text())) == KMessageBox::Continue) {
            const QString jobId = item->data(Qt::UserRole).toString();
            KConfig conf(QStringLiteral("clipjobsettings.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
            QStringList groupIds = {QStringLiteral("Ids"),    QStringLiteral("Binary"),     QStringLiteral("Parameters"),
                                    QStringLiteral("Output"), QStringLiteral("FolderName"), QStringLiteral("FolderUse")};
            for (auto &gName : groupIds) {
                KConfigGroup group(&conf, gName);
                group.deleteEntry(jobId);
            }
            delete item;
            m_dirty.clear();
            job_list->setCurrentRow(0);
        }
    }
}

void ClipJobManager::updateName(QListWidgetItem *item)
{
    KConfig conf(QStringLiteral("clipjobsettings.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup idGroup(&conf, "Ids");
    idGroup.writeEntry(item->data(Qt::UserRole).toString(), item->text());
}
