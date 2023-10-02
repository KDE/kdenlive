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
#include <QButtonGroup>
#include <QFontDatabase>
#include <QUuid>

ClipJobManager::ClipJobManager(AbstractTask::JOBTYPE type, QWidget *parent)
    : QDialog(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    setWindowTitle(i18n("Manage Bin Clip Jobs"));
    connect(job_list, &QListWidget::currentRowChanged, this, &ClipJobManager::displayJob);
    loadJobs();
    if (type != AbstractTask::NOJOBTYPE) {
        for (int i = 0; i < job_list->count(); i++) {
            QListWidgetItem *item = job_list->item(i);
            if (item && item->data(Qt::UserRole + 1).toInt() == (int)type) {
                job_list->setCurrentRow(i);
                break;
            }
        }
    } else {
        job_list->setCurrentRow(0);
    }

    job_list->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    connect(button_add, &QToolButton::clicked, this, &ClipJobManager::addJob);
    connect(button_delete, &QToolButton::clicked, this, &ClipJobManager::deleteJob);

    job_params->setToolTip(i18n("Arguments for the command line script"));
    job_params->setWhatsThis(
        xi18nc("@info:whatsthis",
               "<b>{&#x25;1}</b> will be replaced by the first parameter, <b>{&#x25;2}</b> by the second, , <b>{&#x25;3}</b> by the output file path."));
    destination_pattern->setToolTip(i18n("File extension for the output file"));
    destination_pattern->setWhatsThis(
        xi18nc("@info:whatsthis",
               "The output file name will be the same as the source bin clip filename, with the modified extension. It will be appended at the end of the "
               "arguments or inserted in place of <b>{&#x25;3}</b> if found. If output filename already exists, a -0001 pattern will be appended."));

    param1_isfile->setToolTip(i18n("The first parameter that will be injected in the script arguments"));
    param1_isfile->setWhatsThis(xi18nc("@info:whatsthis",
                                       "<b>Source Clip Path</b> will put the selected Bin Clip path<br/><b>Request File Path</b> will ask for a file path on "
                                       "execution<br/><b>Request Option in List</b> will ask for a choice in the list below on execution."));

    param1_list->setToolTip(i18n("A newline separated list of possible values that will be offered on execution"));
    param1_list->setWhatsThis(
        xi18nc("@info:whatsthis",
               "When the parameter is set to <b>Request Option in List</b> the user will be asked to choose a value in this list when the job is started."));

    param2_isfile->setToolTip(i18n("The second parameter that will be injected in the script arguments"));
    param2_isfile->setWhatsThis(xi18nc("@info:whatsthis",
                                       "<b>Source Clip Path</b> will put the selected Bin Clip path<br/><b>Request File Path</b> will ask for a file path on "
                                       "execution<br/><b>Request Option in List</b> will ask for a choice in the list below on execution."));

    param2_list->setToolTip(i18n("A newline separated list of possible values that will be offered on execution"));
    param2_list->setWhatsThis(
        xi18nc("@info:whatsthis",
               "When the parameter is set to <b>Request Option in List</b> the user will be asked to choose a value in this list when the job is started."));

    connect(job_list, &QListWidget::itemChanged, this, &ClipJobManager::updateName);

    QButtonGroup *bg = new QButtonGroup(this);
    bg->setExclusive(false);
    bg->addButton(enable_video);
    bg->addButton(enable_audio);
    bg->addButton(enable_image);

    // Mark preset as dirty if anything changes
    connect(url_binary, &KUrlRequester::textChanged, this, [this]() {
        checkScript();
        setDirty();
    });
    connect(job_params, &QPlainTextEdit::textChanged, this, &ClipJobManager::setDirty);
    connect(destination_pattern, &QLineEdit::textChanged, this, &ClipJobManager::setDirty);
    connect(radio_replace, &QRadioButton::toggled, this, &ClipJobManager::setDirty);
    connect(combo_folder, &QComboBox::currentIndexChanged, this, &ClipJobManager::setDirty);
    connect(folder_name, &QLineEdit::textChanged, this, &ClipJobManager::setDirty);
    connect(param1_list, &QPlainTextEdit::textChanged, this, &ClipJobManager::setDirty);
    connect(param2_list, &QPlainTextEdit::textChanged, this, &ClipJobManager::setDirty);
    connect(taskDescription, &QPlainTextEdit::textChanged, this, &ClipJobManager::setDirty);
    connect(param1_islist, &QAbstractButton::toggled, this, &ClipJobManager::setDirty);
    connect(param2_islist, &QAbstractButton::toggled, this, &ClipJobManager::setDirty);
    connect(param1_name, &QLineEdit::textChanged, this, &ClipJobManager::setDirty);
    connect(param2_name, &QLineEdit::textChanged, this, &ClipJobManager::setDirty);
    connect(bg, &QButtonGroup::idToggled, this, &ClipJobManager::setDirty);
    info_message->setVisible(false);
    checkScript();
    connect(buttonBox->button(QDialogButtonBox::Close), &QPushButton::clicked, this, &ClipJobManager::validate);
}

ClipJobManager::~ClipJobManager() {}

void ClipJobManager::setDirty()
{
    m_dirty = job_list->currentItem()->data(Qt::UserRole).toString();
}

void ClipJobManager::validate()
{
    // ensure changes to the current preset get saved
    displayJob(-1);
    writePresetsToConfig();
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
    item->setData(Qt::UserRole + 1, AbstractTask::STABILIZEJOB);
    item = new QListWidgetItem(i18n("Duplicate Clip with Speed Change…"), job_list, QListWidgetItem::Type);
    item->setData(Qt::UserRole, QLatin1String("timewarp"));
    item->setData(Qt::UserRole + 1, AbstractTask::SPEEDJOB);

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
    KConfigGroup groupEnabled(&conf, "EnabledTypes");
    m_enableType = groupEnabled.entryMap();
    KConfigGroup param1Type(&conf, "Param1Type");
    m_param1Type = param1Type.entryMap();
    KConfigGroup param1List(&conf, "Param1List");
    m_param1List = param1List.entryMap();
    KConfigGroup param2Type(&conf, "Param2Type");
    m_param2Type = param2Type.entryMap();
    KConfigGroup param2List(&conf, "Param2List");
    m_param2List = param2List.entryMap();
    KConfigGroup param1Name(&conf, "Param1Name");
    m_param1Name = param1Name.entryMap();
    KConfigGroup param2Name(&conf, "Param2Name");
    m_param2Name = param2Name.entryMap();
    KConfigGroup descName(&conf, "Description");
    m_description = descName.entryMap();
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
    QListWidgetItem *item = job_list->item(row);
    QString jobId = item->data(Qt::UserRole).toString();
    bool customJob = item->type() == QListWidgetItem::UserType;
    param_box->setEnabled(customJob);
    button_delete->setEnabled(customJob);
    if (customJob && !m_ids.contains(jobId)) {
        // This is a new job, set some default values
        url_binary->setUrl(QUrl::fromLocalFile(KdenliveSettings::ffmpegpath()));
        job_params->setPlainText(QStringLiteral("-i {source} -codec:a copy -codec:v copy {output}"));
    } else {
        url_binary->setText(m_binaries.value(jobId));
        job_params->setPlainText(m_params.value(jobId));
    }
    destination_pattern->setText(m_output.value(jobId));
    folder_name->setText(m_folderNames.value(jobId));
    folder_name->setText(m_folderNames.value(jobId));
    if (m_folderUse.contains(jobId) || m_dirty == jobId) {
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
    enable_video->setChecked(m_enableType.value(jobId).contains(QLatin1String("v")));
    enable_audio->setChecked(m_enableType.value(jobId).contains(QLatin1String("a")));
    enable_image->setChecked(m_enableType.value(jobId).contains(QLatin1String("i")));

    if (m_param1Type.value(jobId) == QLatin1String("list")) {
        param1_islist->setChecked(true);
    } else {
        param1_isfile->setChecked(true);
    }
    if (m_param2Type.value(jobId) == QLatin1String("list")) {
        param2_islist->setChecked(true);
    } else {
        param2_isfile->setChecked(true);
    }
    param1_name->setText(m_param1Name.value(jobId));
    param2_name->setText(m_param2Name.value(jobId));
    taskDescription->setPlainText(m_description.value(jobId));

    QStringList option1 = m_param1List.value(jobId).split(QLatin1String("  "));
    param1_list->setPlainText(option1.join(QLatin1Char('\n')));
    QStringList option2 = m_param2List.value(jobId).split(QLatin1String("  "));
    param2_list->setPlainText(option2.join(QLatin1Char('\n')));
}

void ClipJobManager::checkScript()
{
    if (url_binary->text().isEmpty() || !url_binary->isEnabled()) {
        script_message->setVisible(false);
        return;
    }
    QFileInfo info(url_binary->text());
    if (!info.exists()) {
        script_message->setText(i18n("Missing executable"));
        script_message->setVisible(true);
    } else if (!info.isExecutable()) {
        script_message->setText(i18n("Your script or application %1 is not executable, change permissions", info.fileName()));
        script_message->setVisible(true);
    } else {
        script_message->setVisible(false);
    }
}

QMap<QString, QString> ClipJobManager::getClipJobNames()
{
    KConfig conf(QStringLiteral("clipjobsettings.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, "Ids");
    QMap<QString, QString> ids = group.entryMap();
    KConfigGroup group2(&conf, "EnabledTypes");
    QMap<QString, QString> id2s = group2.entryMap();
    // Append supported type after job key
    QMapIterator<QString, QString> i(ids);
    QMap<QString, QString> newIds;
    while (i.hasNext()) {
        i.next();
        if (!i.value().isEmpty()) {
            newIds.insert(QString("%1;%2").arg(i.key(), id2s.value(i.key())), i.value());
        }
    }
    // Add the 3 internal jobs
    if (EffectsRepository::get()->exists(QLatin1String("vidstab"))) {
        newIds.insert(QStringLiteral("stabilize;v"), i18n("Stabilize"));
    }
    newIds.insert(QStringLiteral("scenesplit;v"), i18n("Automatic Scene Split…"));
    if (KdenliveSettings::producerslist().contains(QLatin1String("timewarp"))) {
        newIds.insert(QStringLiteral("timewarp;av"), i18n("Duplicate Clip with Speed Change…"));
    }
    return newIds;
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

QMap<QString, QString> ClipJobManager::getJobParameters(const QString &jobId)
{
    QMap<QString, QString> result;
    KConfig conf(QStringLiteral("clipjobsettings.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup idGroup(&conf, "Ids");
    result.insert(QLatin1String("description"), idGroup.readEntry(jobId, i18n("Job description")));
    KConfigGroup group(&conf, "Binary");
    result.insert(QLatin1String("binary"), group.readEntry(jobId, QString()));
    KConfigGroup paramGroup(&conf, "Parameters");
    result.insert(QLatin1String("parameters"), paramGroup.readEntry(jobId, QString()));
    KConfigGroup outGroup(&conf, "Output");
    result.insert(QLatin1String("output"), outGroup.readEntry(jobId, QString()));
    KConfigGroup p1Group(&conf, "Param1Type");
    result.insert(QLatin1String("param1type"), p1Group.readEntry(jobId, QString()));
    KConfigGroup p2Group(&conf, "Param2Type");
    result.insert(QLatin1String("param2type"), p2Group.readEntry(jobId, QString()));
    KConfigGroup p1List(&conf, "Param1List");
    result.insert(QLatin1String("param1list"), p1List.readEntry(jobId, QString()));
    KConfigGroup p2List(&conf, "Param2List");
    result.insert(QLatin1String("param2list"), p2List.readEntry(jobId, QString()));
    KConfigGroup p1Name(&conf, "Param1Name");
    result.insert(QLatin1String("param1name"), p1Name.readEntry(jobId, QString()));
    KConfigGroup p2Name(&conf, "Param2Name");
    result.insert(QLatin1String("param2name"), p2Name.readEntry(jobId, QString()));
    KConfigGroup descName(&conf, "Description");
    result.insert(QLatin1String("details"), descName.readEntry(jobId, QString()));
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
    m_description.insert(m_dirty, taskDescription->toPlainText().simplified());
    QString enabledTypes;
    if (enable_video->isChecked()) {
        enabledTypes.append(QLatin1String("v"));
    }
    if (enable_audio->isChecked()) {
        enabledTypes.append(QLatin1String("a"));
    }
    if (enable_image->isChecked()) {
        enabledTypes.append(QLatin1String("i"));
    }
    m_enableType.insert(m_dirty, enabledTypes);
    m_param1Type.insert(m_dirty, param1_islist->isChecked() ? QLatin1String("list") : QLatin1String("file"));
    m_param2Type.insert(m_dirty, param2_islist->isChecked() ? QLatin1String("list") : QLatin1String("file"));
    QStringList option1 = param1_list->toPlainText().split(QLatin1Char('\n'));
    m_param1List.insert(m_dirty, option1.join(QLatin1String("  ")));
    QStringList option2 = param2_list->toPlainText().split(QLatin1Char('\n'));
    m_param2List.insert(m_dirty, option2.join(QLatin1String("  ")));
    m_param1Name.insert(m_dirty, param1_name->text());
    m_param2Name.insert(m_dirty, param2_name->text());
    m_description.insert(m_dirty, taskDescription->toPlainText());
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

void ClipJobManager::writePresetsToConfig()
{
    KConfig conf(QStringLiteral("clipjobsettings.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    writeGroup(conf, QStringLiteral("Ids"), m_ids);
    writeGroup(conf, QStringLiteral("Binary"), m_binaries);
    writeGroup(conf, QStringLiteral("Parameters"), m_params);
    writeGroup(conf, QStringLiteral("Output"), m_output);
    writeGroup(conf, QStringLiteral("FolderName"), m_folderNames);
    writeGroup(conf, QStringLiteral("FolderUse"), m_folderUse);
    writeGroup(conf, QStringLiteral("EnabledTypes"), m_enableType);
    writeGroup(conf, QStringLiteral("Param1Type"), m_param1Type);
    writeGroup(conf, QStringLiteral("Param2Type"), m_param2Type);
    writeGroup(conf, QStringLiteral("Param1List"), m_param1List);
    writeGroup(conf, QStringLiteral("Param2List"), m_param2List);
    writeGroup(conf, QStringLiteral("Param1Name"), m_param1Name);
    writeGroup(conf, QStringLiteral("Param2Name"), m_param2Name);
    writeGroup(conf, QStringLiteral("Description"), m_description);
}

void ClipJobManager::addJob()
{
    const QString uuid = QUuid::createUuid().toString();
    QString jobName = i18n("My Clip Job");
    bool newName = false;
    int j = 1;
    while (newName == false) {
        int i = 0;
        for (; i < job_list->count(); i++) {
            QListWidgetItem *it = job_list->item(i);
            if (it->text() == jobName) {
                jobName = i18n("My Clip Job %1", j);
                j++;
                break;
            }
        }
        if (i == job_list->count()) {
            // All the list was parsed so this is a unique name
            newName = true;
        }
    }
    QListWidgetItem *item = new QListWidgetItem(jobName, job_list, QListWidgetItem::UserType);
    item->setData(Qt::UserRole, uuid);
    m_folderUse.insert(uuid, QStringLiteral("replace"));
    m_enableType.insert(uuid, QStringLiteral("v"));
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
            m_ids.remove(jobId);
            m_binaries.remove(jobId);
            m_params.remove(jobId);
            m_output.remove(jobId);
            m_folderNames.remove(jobId);
            m_folderUse.remove(jobId);
            m_enableType.remove(jobId);
            m_param1Type.remove(jobId);
            m_param1List.remove(jobId);
            m_param2Type.remove(jobId);
            m_param2List.remove(jobId);
            m_param1Name.remove(jobId);
            m_param2Name.remove(jobId);
            m_description.remove(jobId);
            m_dirty.clear();
            job_list->setCurrentRow(0);
        }
    }
}

void ClipJobManager::updateName(QListWidgetItem *item)
{
    setDirty();
    m_ids.insert(m_dirty, item->text());
}
