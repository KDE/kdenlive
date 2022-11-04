/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "encodingprofilesdialog.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"

#include "klocalizedstring.h"
#include <KMessageWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QStandardItemModel>
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

EncodingProfilesChooser::EncodingProfilesChooser(QWidget *parent, EncodingProfilesManager::ProfileType type, bool showAutoItem, const QString &configName,
                                                 bool native)
    : QWidget(parent)
    , m_type(type)
    , m_showAutoItem(showAutoItem)
{
    QVBoxLayout *verticalLayout = new QVBoxLayout(this);
    verticalLayout->setContentsMargins(0, 0, 0, 0);
    m_profilesCombo = new QComboBox(this);
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
    m_info->setReadOnly(true);
    m_info->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 4);
    QHBoxLayout *hor = new QHBoxLayout;
    hor->addWidget(m_profilesCombo);
    hor->addWidget(buttonConfigure);
    hor->addWidget(buttonInfo);

    // Message widget
    m_messageWidget = new KMessageWidget(this);
    m_messageWidget->setWordWrap(true);
    m_messageWidget->setCloseButtonVisible(true);

    verticalLayout->addLayout(hor);
    verticalLayout->addWidget(m_info);
    verticalLayout->addWidget(m_messageWidget);
    m_info->setVisible(false);
    m_messageWidget->hide();
    connect(buttonConfigure, &QAbstractButton::clicked, this, &EncodingProfilesChooser::slotManageEncodingProfile);
    connect(buttonInfo, &QAbstractButton::clicked, m_info, &QWidget::setVisible);
    connect(m_profilesCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &EncodingProfilesChooser::slotUpdateProfile);
    if (native) {
        loadEncodingProfiles();
        if (!configName.isEmpty()) {
            KConfigGroup resourceConfig(KSharedConfig::openConfig(), "project");
            int ix = resourceConfig.readEntry(configName).toInt();
            m_profilesCombo->setCurrentIndex(ix);
            slotUpdateProfile(ix);
        }
    }
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
}

void EncodingProfilesChooser::slotManageEncodingProfile()
{
    QPointer<EncodingProfilesDialog> dia = new EncodingProfilesDialog(m_type);
    dia->exec();
    delete dia;
    loadEncodingProfiles();
    filterPreviewProfiles(pCore->getCurrentProfilePath());
}

void EncodingProfilesChooser::loadEncodingProfiles()
{
    QSignalBlocker bk(m_profilesCombo);
    QString currentItem = m_profilesCombo->currentText();
    m_profilesCombo->clear();
    if (m_showAutoItem) {
        if (m_type == EncodingProfilesManager::TimelinePreview && (KdenliveSettings::nvencEnabled() || KdenliveSettings::vaapiEnabled())) {
            m_profilesCombo->addItem(QIcon::fromTheme(QStringLiteral("speedometer")), i18n("Automatic"));
        } else {
            m_profilesCombo->addItem(i18n("Automatic"));
        }
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

void EncodingProfilesChooser::hideMessage()
{
    m_messageWidget->hide();
}

void EncodingProfilesChooser::filterPreviewProfiles(const QString & /*profile*/) {}

EncodingTimelinePreviewProfilesChooser::EncodingTimelinePreviewProfilesChooser(QWidget *parent, bool showAutoItem, const QString &defaultValue,
                                                                               bool selectFromConfig)
    : EncodingProfilesChooser(parent, EncodingProfilesManager::TimelinePreview, showAutoItem, QString(), false)
{
    loadEncodingProfiles();
    if (selectFromConfig) {
        KConfigGroup resourceConfig(KSharedConfig::openConfig(), "project");
        int ix = resourceConfig.readEntry(defaultValue).toInt();
        m_profilesCombo->setCurrentIndex(ix);
        slotUpdateProfile(ix);
    } else if (!defaultValue.isEmpty()) {
        int ix = m_profilesCombo->findData(defaultValue);
        if (ix == -1) {
            m_profilesCombo->addItem(i18n("Current Settings"), defaultValue);
            ix = m_profilesCombo->findData(defaultValue);
        }
        if (ix > -1) {
            m_profilesCombo->setCurrentIndex(ix);
            slotUpdateProfile(ix);
        }
    }
    connect(m_profilesCombo, &KComboBox::currentIndexChanged, m_messageWidget, &KMessageWidget::hide);
    connect(m_profilesCombo, &KComboBox::currentIndexChanged, this, &EncodingTimelinePreviewProfilesChooser::currentIndexChanged);
}

void EncodingTimelinePreviewProfilesChooser::loadEncodingProfiles()
{
    QSignalBlocker bk(m_profilesCombo);
    QString currentItem = m_profilesCombo->currentText();
    m_profilesCombo->clear();
    if (m_showAutoItem) {
        if (KdenliveSettings::nvencEnabled() || KdenliveSettings::vaapiEnabled()) {
            m_profilesCombo->addItem(QIcon::fromTheme(QStringLiteral("speedometer")), i18n("Automatic"));
        } else {
            m_profilesCombo->addItem(i18n("Automatic"));
        }
    }

    KConfig conf(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::AppDataLocation);
    KConfigGroup group(&conf, EncodingProfilesManager::configGroupName(m_type));
    QMap<QString, QString> values = group.entryMap();
    QMapIterator<QString, QString> i(values);
    while (i.hasNext()) {
        i.next();
        if (!i.key().isEmpty()) {
            // We filter out incompatible profiles
            if (i.value().contains(QLatin1String("nvenc"))) {
                if (KdenliveSettings::nvencEnabled()) {
                    m_profilesCombo->addItem(QIcon::fromTheme(QStringLiteral("speedometer")), i.key(), i.value());
                }
                continue;
            }
            if (i.value().contains(QLatin1String("vaapi"))) {
                if (KdenliveSettings::vaapiEnabled()) {
                    m_profilesCombo->addItem(QIcon::fromTheme(QStringLiteral("speedometer")), i.key(), i.value());
                }
                continue;
            }
            m_profilesCombo->addItem(i.key(), i.value());
        }
    }
    if (!currentItem.isEmpty()) {
        int ix = m_profilesCombo->findText(currentItem);
        m_profilesCombo->setCurrentIndex(ix);
        slotUpdateProfile(ix);
    }
}

void EncodingTimelinePreviewProfilesChooser::filterPreviewProfiles(const QString &profile)
{
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(m_profilesCombo->model());
    Q_ASSERT(model != nullptr);
    int max = m_profilesCombo->count();
    int current = m_profilesCombo->currentIndex();
    double projectFps = ProfileRepository::get()->getProfile(profile)->fps();
    for (int i = 0; i < max; i++) {
        QString itemData = m_profilesCombo->itemData(i).toString();
        double fps = 0.;
        if (itemData.startsWith(QStringLiteral("r="))) {
            QString fpsString = itemData.section(QLatin1Char('='), 1).section(QLatin1Char(' '), 0, 0);
            fps = fpsString.toDouble();
        } else if (itemData.contains(QStringLiteral(" r="))) {
            QString fpsString = itemData.section(QLatin1String(" r="), 1).section(QLatin1Char(' '), 0, 0);
            // This profile has a hardcoded framerate, chack if same as project
            fps = fpsString.toDouble();
        }
        QStandardItem *item = model->item(i);
        if (fps > 0. && qAbs(fps - projectFps) > 0.01) {
            // Fps does not match, disable
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            continue;
        }
        item->setFlags(item->flags() | Qt::ItemIsEnabled);
    }
    QStandardItem *item = model->item(current);
    if (!(item->flags() & Qt::ItemIsEnabled)) {
        // Currently selected profile is not usable, switch back to automatic
        for (int i = 0; i < max; i++) {
            if (m_profilesCombo->itemData(i).isNull()) {
                m_profilesCombo->setCurrentIndex(i);
                m_messageWidget->setText(i18n("Selected Timeline preview profile is not compatible with the project framerate,\nreverting to Automatic."));
                m_messageWidget->animatedShow();
                break;
            }
        }
    }
}
