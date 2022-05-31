/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "encodingprofilesdialog.h"

#include "kdenlivesettings.h"

#include "klocalizedstring.h"
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStandardPaths>
#include <QVBoxLayout>

QString EncodingProfilesManager::configGroupName(ProfileType type)
{
    QString groupName;
    switch (type) {
    case ProfileType::ProxyClips:
        groupName = QStringLiteral("proxy");
        break;
    case ProfileType::V4LCapture:
        groupName = QStringLiteral("video4linux");
        break;
    case ProfileType::ScreenCapture:
        groupName = QStringLiteral("screengrab");
        break;
    case ProfileType::DecklinkCapture:
        groupName = QStringLiteral("decklink");
        break;
    case ProfileType::TimelinePreview:
    default:
        groupName = QStringLiteral("timelinepreview");
        break;
    }
    return groupName;
}

EncodingProfilesDialog::EncodingProfilesDialog(EncodingProfilesManager::ProfileType profileType, QWidget *parent)
    : QDialog(parent)
    , m_configGroup(nullptr)
{
    setupUi(this);
    setWindowTitle(i18nc("@title:window", "Manage Encoding Profiles"));
    profile_type->addItem(i18n("Proxy Clips"), EncodingProfilesManager::ProxyClips);
    profile_type->addItem(i18n("Timeline Preview"), EncodingProfilesManager::TimelinePreview);
    profile_type->addItem(i18n("Video4Linux Capture"), EncodingProfilesManager::V4LCapture);
    profile_type->addItem(i18n("Screen Capture"), EncodingProfilesManager::ScreenCapture);
    profile_type->addItem(i18n("Decklink Capture"), EncodingProfilesManager::DecklinkCapture);

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

    delete m_configGroup;
    m_configGroup =
        new KConfigGroup(m_configFile, EncodingProfilesManager::configGroupName(EncodingProfilesManager::ProfileType(profile_type->currentIndex())));
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

EncodingProfilesChooser::EncodingProfilesChooser(QWidget *parent, EncodingProfilesManager::ProfileType type, bool showAutoItem, const QString &configName)
    : QWidget(parent)
    , m_type(type)
    , m_showAutoItem(showAutoItem)
{
    QGridLayout *grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    setLayout(grid);
    m_profilesCombo = new QComboBox();
    if (!configName.isEmpty()) {
        m_profilesCombo->setObjectName(QStringLiteral("kcfg_%1").arg(configName));
    }

    QToolButton *buttonConfigure = new QToolButton();
    buttonConfigure->setIcon(QIcon::fromTheme(QStringLiteral("configure")));
    buttonConfigure->setText(i18n("Configure profiles"));
    buttonConfigure->setToolTip(i18n("Manage Encoding Profiles"));
    QToolButton *buttonInfo = new QToolButton();
    buttonInfo->setCheckable(true);
    buttonInfo->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));
    buttonConfigure->setToolTip(i18n("Show Profile Parameters"));

    m_info = new QPlainTextEdit();
    m_info->setVisible(false);
    m_info->setReadOnly(true);
    m_info->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 3);

    grid->addWidget(m_profilesCombo, 0, 0);
    grid->addWidget(buttonConfigure, 0, 1);
    grid->addWidget(buttonInfo, 0, 2);
    grid->addWidget(m_info, 1, 0, -1, -1);

    connect(buttonConfigure, &QAbstractButton::clicked, this, &EncodingProfilesChooser::slotManageEncodingProfile);
    connect(buttonInfo, &QAbstractButton::clicked, m_info, &QWidget::setVisible);
    connect(m_profilesCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &EncodingProfilesChooser::slotUpdateProfile);
    loadEncodingProfiles();
    if (!configName.isEmpty()) {
        KConfigGroup resourceConfig(KSharedConfig::openConfig(), "project");
        int ix = resourceConfig.readEntry(configName).toInt();
        m_profilesCombo->setCurrentIndex(ix);
        slotUpdateProfile(ix);
    }
}

void EncodingProfilesChooser::slotManageEncodingProfile()
{
    QPointer<EncodingProfilesDialog> dia = new EncodingProfilesDialog(m_type);
    dia->exec();
    delete dia;
    loadEncodingProfiles();
}

void EncodingProfilesChooser::loadEncodingProfiles()
{

    m_profilesCombo->blockSignals(true);
    QString currentItem = m_profilesCombo->currentText();
    m_profilesCombo->clear();
    if (m_showAutoItem) {
        m_profilesCombo->addItem(i18n("Automatic"));
    }

    KConfig conf(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, EncodingProfilesManager::configGroupName(m_type));
    QMap<QString, QString> values = group.entryMap();
    QMapIterator<QString, QString> i(values);
    while (i.hasNext()) {
        i.next();
        if (!i.key().isEmpty()) {
            m_profilesCombo->addItem(i.key(), i.value());
        }
    }
    if (!currentItem.isEmpty()) {
        int ix = m_profilesCombo->findText(currentItem);
        m_profilesCombo->setCurrentIndex(ix);
        slotUpdateProfile(ix);
    }
    m_profilesCombo->blockSignals(false);
}

QString EncodingProfilesChooser::currentExtension()
{
    QString profilestr = m_profilesCombo->currentData().toString();
    if (profilestr.isEmpty()) {
        return {};
    }
    return profilestr.section(QLatin1Char(';'), 1, 1);
}

QString EncodingProfilesChooser::currentParams()
{
    QString profilestr = m_profilesCombo->currentData().toString();
    if (profilestr.isEmpty()) {
        return {};
    }
    return profilestr.section(QLatin1Char(';'), 0, 0);
}

void EncodingProfilesChooser::slotUpdateProfile(int ix)
{
    m_info->clear();
    QString profilestr = m_profilesCombo->itemData(ix).toString();
    if (!profilestr.isEmpty()) {
        m_info->setPlainText(profilestr.section(QLatin1Char(';'), 0, 0));
    }
}
