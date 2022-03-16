/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "renderwidget.h"
#include "bin/bin.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "dialogs/renderpresetdialog.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "project/projectmanager.h"
#include "utils/timecode.h"
#include "xml/xml.hpp"

#include "renderpresets/tree/renderpresettreemodel.hpp"
#include "renderpresets/renderpresetrepository.hpp"
#include "renderpresets/renderpresetmodel.hpp"

#include "klocalizedstring.h"
#include <KColorScheme>
#include <KIO/DesktopExecParser>
#include <KIO/OpenFileManagerWindowJob>
#include <KMessageBox>
#include <KNotification>
#include <KRun>
#include <kio_version.h>
#include <knotifications_version.h>

#include "kdenlive_debug.h"
#include <QDBusConnectionInterface>
#include <QDir>
#include <QDomDocument>
#include <QFileIconProvider>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QThread>
#include <QTreeWidgetItem>
#include <qglobal.h>
#include <qstring.h>

#ifdef KF5_USE_PURPOSE
#include <Purpose/AlternativesModel>
#include <PurposeWidgets/Menu>
#endif

#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

// Render job roles
enum {
    ParametersRole = Qt::UserRole + 1,
    StartTimeRole,
    ProgressRole,
    ExtraInfoRole = ProgressRole + 2, // vpinon: don't understand why, else spurious message displayed
    LastTimeRole,
    LastFrameRole,
    OpenBrowserRole
};

// Running job status
enum JOBSTATUS { WAITINGJOB = 0, STARTINGJOB, RUNNINGJOB, FINISHEDJOB, FAILEDJOB, ABORTEDJOB };

RenderJobItem::RenderJobItem(QTreeWidget *parent, const QStringList &strings, int type)
    : QTreeWidgetItem(parent, strings, type)
    , m_status(-1)
{
    setSizeHint(1, QSize(parent->columnWidth(1), parent->fontMetrics().height() * 3));
    setStatus(WAITINGJOB);
}

void RenderJobItem::setStatus(int status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
    switch (status) {
    case WAITINGJOB:
        setIcon(0, QIcon::fromTheme(QStringLiteral("media-playback-pause")));
        setData(1, Qt::UserRole, i18n("Waiting…"));
        break;
    case FINISHEDJOB:
        setData(1, Qt::UserRole, i18n("Rendering finished"));
        setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-ok")));
        setData(1, ProgressRole, 100);
        break;
    case FAILEDJOB:
        setData(1, Qt::UserRole, i18n("Rendering crashed"));
        setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-close")));
        setData(1, ProgressRole, 100);
        break;
    case ABORTEDJOB:
        setData(1, Qt::UserRole, i18n("Rendering aborted"));
        setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-cancel")));
        setData(1, ProgressRole, 100);
        break;
    default:
        break;
    }
}

int RenderJobItem::status() const
{
    return m_status;
}

void RenderJobItem::setMetadata(const QString &data)
{
    m_data = data;
}

const QString RenderJobItem::metadata() const
{
    return m_data;
}

RenderWidget::RenderWidget(bool enableProxy, QWidget *parent)
    : QDialog(parent)
    , m_blockProcessing(false)
{
    m_view.setupUi(this);
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    QSize iconSize(size, size);

    // ===== "Render Project" tab =====
    m_view.buttonDelete->setIconSize(iconSize);
    m_view.buttonEdit->setIconSize(iconSize);
    m_view.buttonNew->setIconSize(iconSize);
    m_view.buttonSaveAs->setIconSize(iconSize);
    m_view.buttonDownload->setIconSize(iconSize);

    m_view.buttonRender->setEnabled(false);
    m_view.buttonGenerateScript->setEnabled(false);

    connect(m_view.profileTree, &QTreeView::doubleClicked, this, [&](const QModelIndex &index){
        if (m_treeModel->parent(index) == QModelIndex()) {
            // This is a top level item - group - don't edit
            return;
        }
        slotEditPreset();
    });
    connect(m_view.buttonNew, &QAbstractButton::clicked, this, &RenderWidget::slotNewPreset);
    connect(m_view.buttonEdit, &QAbstractButton::clicked, this, &RenderWidget::slotEditPreset);
    connect(m_view.buttonDelete, &QAbstractButton::clicked, this, [this](){
        RenderPresetRepository::get()->deletePreset(m_currentProfile);
        m_currentProfile = QString();
        parseProfiles();
        refreshView();
    });
    connect(m_view.buttonSaveAs, &QAbstractButton::clicked, this, &RenderWidget::slotSavePresetAs);
    connect(m_view.buttonDownload, &QAbstractButton::clicked, this, [&](){
        if (pCore->getNewStuff(QStringLiteral(":data/kdenlive_renderprofiles.knsrc")) > 0) {
            parseProfiles();
        }
    });
    m_view.optionsGroup->setVisible(m_view.options->isChecked());
    connect(m_view.options, &QAbstractButton::toggled, m_view.optionsGroup, &QWidget::setVisible);

    connect(m_view.out_file, &KUrlRequester::textChanged, this, static_cast<void (RenderWidget::*)()>(&RenderWidget::slotUpdateButtons));
    connect(m_view.out_file, &KUrlRequester::urlSelected, this, static_cast<void (RenderWidget::*)(const QUrl &)>(&RenderWidget::slotUpdateButtons));

    connect(m_view.render_multi, &QAbstractButton::clicked, this, &RenderWidget::slotRenderModeChanged);
    connect(m_view.render_guide, &QAbstractButton::clicked, this, &RenderWidget::slotRenderModeChanged);
    connect(m_view.render_zone, &QAbstractButton::clicked, this, &RenderWidget::slotRenderModeChanged);
    connect(m_view.render_full, &QAbstractButton::clicked, this, &RenderWidget::slotRenderModeChanged);

    connect(m_view.guide_end, static_cast<void (KComboBox::*)(int)>(&KComboBox::activated), this, &RenderWidget::slotCheckStartGuidePosition);
    connect(m_view.guide_start, static_cast<void (KComboBox::*)(int)>(&KComboBox::activated), this, &RenderWidget::slotCheckEndGuidePosition);


    // === "More Options" widget ===
    setRescaleEnabled(false);
    m_view.guide_zone_box->setVisible(false);
    m_view.guide_multi_box->setVisible(false);
    m_view.error_box->setVisible(false);
    m_view.tc_type->addItem(i18n("None"));
    m_view.tc_type->addItem(i18n("Timecode"), QStringLiteral("#timecode#"));
    m_view.tc_type->addItem(i18n("Timecode Non Drop Frame"), QStringLiteral("#smtpe_ndf#"));
    m_view.tc_type->addItem(i18n("Frame Number"), QStringLiteral("#frame#"));
    m_view.checkTwoPass->setEnabled(false);
    m_view.proxy_render->setHidden(!enableProxy);
    connect(m_view.proxy_render, &QCheckBox::toggled, this, [&](bool enabled){
        errorMessage(ProxyWarning, enabled ? i18n("Rendering using low quality proxy") : QString());
    });

    connect(m_view.quality, &QAbstractSlider::valueChanged, this, &RenderWidget::refreshParams);
    connect(m_view.qualityGroup, &QGroupBox::toggled, this, &RenderWidget::refreshParams);
    connect(m_view.speed, &QAbstractSlider::valueChanged, this, &RenderWidget::adjustSpeed);

    m_view.encoder_threads->setMaximum(QThread::idealThreadCount());
    m_view.encoder_threads->setValue(KdenliveSettings::encodethreads());
    connect(m_view.encoder_threads, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &KdenliveSettings::setEncodethreads);

    connect(m_view.video_box, &QGroupBox::toggled, this, &RenderWidget::refreshParams);
    connect(m_view.audio_box, &QGroupBox::toggled, this, &RenderWidget::refreshParams);
    m_view.rescale_keep->setChecked(KdenliveSettings::rescalekeepratio());
    connect(m_view.rescale, &QAbstractButton::toggled, this, &RenderWidget::setRescaleEnabled);
    connect(m_view.rescale, &QAbstractButton::toggled, this, &RenderWidget::refreshParams);
    connect(m_view.rescale_width, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderWidget::slotUpdateRescaleWidth);
    connect(m_view.rescale_height, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderWidget::slotUpdateRescaleHeight);
    connect(m_view.rescale_keep, &QAbstractButton::clicked, this, &RenderWidget::slotSwitchAspectRatio);
    m_view.parallel_process->setChecked(KdenliveSettings::parallelrender());
    connect(m_view.parallel_process, &QCheckBox::stateChanged, [&](int state) {
        KdenliveSettings::setParallelrender(state == Qt::Checked);
        refreshParams();
    });
    if (KdenliveSettings::gpu_accel()) {
        // Disable parallel rendering for movit
        m_view.parallel_process->setEnabled(false);
    }
    connect(m_view.export_meta, &QCheckBox::stateChanged, this, &RenderWidget::refreshParams);
    connect(m_view.checkTwoPass, &QCheckBox::stateChanged, this, &RenderWidget::refreshParams);

    connect(m_view.buttonRender, &QAbstractButton::clicked, this, [&](){ slotPrepareExport(); });
    connect(m_view.buttonGenerateScript, &QAbstractButton::clicked, this, [&](){ slotPrepareExport(true); });

    m_view.infoMessage->hide();
    m_view.jobInfo->hide();
    parseProfiles();

    // ===== "Job Queue" tab =====
    parseScriptFiles();
    m_view.running_jobs->setUniformRowHeights(false);
    m_view.running_jobs->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view.running_jobs, &QTreeWidget::customContextMenuRequested, this, &RenderWidget::prepareJobContextMenu);
    m_view.scripts_list->setUniformRowHeights(false);
    connect(m_view.start_script, &QAbstractButton::clicked, this, &RenderWidget::slotStartScript);
    connect(m_view.delete_script, &QAbstractButton::clicked, this, &RenderWidget::slotDeleteScript);
    connect(m_view.scripts_list, &QTreeWidget::itemSelectionChanged, this, &RenderWidget::slotCheckScript);
    connect(m_view.running_jobs, &QTreeWidget::itemSelectionChanged, this, &RenderWidget::slotCheckJob);
    connect(m_view.running_jobs, &QTreeWidget::itemDoubleClicked, this, &RenderWidget::slotPlayRendering);

    connect(m_view.abort_job, &QAbstractButton::clicked, this, &RenderWidget::slotAbortCurrentJob);
    connect(m_view.start_job, &QAbstractButton::clicked, this, &RenderWidget::slotStartCurrentJob);
    connect(m_view.clean_up, &QAbstractButton::clicked, this, &RenderWidget::slotCleanUpJobs);
    connect(m_view.hide_log, &QAbstractButton::clicked, this, &RenderWidget::slotHideLog);

    connect(m_view.buttonClose, &QAbstractButton::clicked, this, &QWidget::hide);
    connect(m_view.buttonClose2, &QAbstractButton::clicked, this, &QWidget::hide);
    connect(m_view.buttonClose3, &QAbstractButton::clicked, this, &QWidget::hide);

    m_jobsDelegate = new RenderViewDelegate(this);
    m_view.running_jobs->setHeaderLabels(QStringList() << QString() << i18n("File"));
    m_view.running_jobs->setItemDelegate(m_jobsDelegate);

    QHeaderView *header = m_view.running_jobs->header();
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    header->resizeSection(0, size + 4);
    header->setSectionResizeMode(1, QHeaderView::Interactive);

    // ===== "Scripts" tab =====
    m_view.scripts_list->setHeaderLabels(QStringList() << QString() << i18n("Stored Playlists"));
    m_scriptsDelegate = new RenderViewDelegate(this);
    m_view.scripts_list->setItemDelegate(m_scriptsDelegate);
    header = m_view.scripts_list->header();
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    header->resizeSection(0, size + 4);

// Find path for Kdenlive renderer
#ifdef Q_OS_WIN
    m_renderer = QCoreApplication::applicationDirPath() + QStringLiteral("/kdenlive_render.exe");
#else
    m_renderer = QCoreApplication::applicationDirPath() + QStringLiteral("/kdenlive_render");
#endif
    if (!QFile::exists(m_renderer)) {
        m_renderer = QStandardPaths::findExecutable(QStringLiteral("kdenlive_render"));
        if (m_renderer.isEmpty()) {
            KMessageBox::sorry(this, i18n("Could not find the kdenlive_render application, something is wrong with your installation. Rendering will not work"));
        }
    }

    QDBusConnectionInterface *interface = QDBusConnection::sessionBus().interface();
    if ((interface == nullptr) ||
        (!interface->isServiceRegistered(QStringLiteral("org.kde.ksmserver")) && !interface->isServiceRegistered(QStringLiteral("org.gnome.SessionManager")))) {
        m_view.shutdown->setEnabled(false);
    }

#ifdef KF5_USE_PURPOSE
    m_shareMenu = new Purpose::Menu();
    m_view.shareButton->setMenu(m_shareMenu);
    m_view.shareButton->setIcon(QIcon::fromTheme(QStringLiteral("document-share")));
    connect(m_shareMenu, &Purpose::Menu::finished, this, &RenderWidget::slotShareActionFinished);
#else
    m_view.shareButton->setEnabled(false);
#endif

    loadConfig();
    refreshView();
    focusItem();
    adjustSize();
}

void RenderWidget::slotShareActionFinished(const QJsonObject &output, int error, const QString &message)
{
#ifdef KF5_USE_PURPOSE
    m_view.jobInfo->hide();
    if (error) {
        KMessageBox::error(this, i18n("There was a problem sharing the document: %1", message), i18n("Share"));
    } else {
        const QString url = output["url"].toString();
        if (url.isEmpty()) {
            m_view.jobInfo->setMessageType(KMessageWidget::Positive);
            m_view.jobInfo->setText(i18n("Document shared successfully"));
            m_view.jobInfo->show();
        } else {
            KMessageBox::information(this, i18n("You can find the shared document at: <a href=\"%1\">%1</a>", url), i18n("Share"), QString(),
                                     KMessageBox::Notify | KMessageBox::AllowLink);
        }
    }
#else
    Q_UNUSED(output);
    Q_UNUSED(error);
    Q_UNUSED(message);
#endif
}

QSize RenderWidget::sizeHint() const
{
    // Make sure the widget has minimum size on opening
    return {200, 200};
}

RenderWidget::~RenderWidget()
{
    saveConfig();
    m_view.running_jobs->blockSignals(true);
    m_view.scripts_list->blockSignals(true);
    m_view.running_jobs->clear();
    m_view.scripts_list->clear();
    delete m_jobsDelegate;
    delete m_scriptsDelegate;
}

void RenderWidget::saveConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup resourceConfig(config, "RenderWidget");
    resourceConfig.writeEntry(QStringLiteral("showoptions"), m_view.options->isChecked());
    config->sync();
}

void RenderWidget::loadConfig()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup resourceConfig(config, "RenderWidget");
    m_view.options->setChecked(resourceConfig.readEntry("showoptions", false));
}

void RenderWidget::updateDocumentPath()
{
    if (m_view.out_file->url().isEmpty()) {
        return;
    }
    const QString fileName = m_view.out_file->url().fileName();
    m_view.out_file->setUrl(QUrl::fromLocalFile(QDir(pCore->currentDoc()->projectDataFolder()).absoluteFilePath(fileName)));
    parseScriptFiles();
}

void RenderWidget::slotRenderModeChanged()
{
    m_view.guide_zone_box->setVisible(m_view.render_guide->isChecked());
    m_view.guide_multi_box->setVisible(m_view.render_multi->isChecked());
    m_view.buttonGenerateScript->setVisible(m_view.render_multi->isChecked());
}

void RenderWidget::slotUpdateRescaleWidth(int val)
{
    KdenliveSettings::setDefaultrescalewidth(val);
    if (!m_view.rescale_keep->isChecked()) {
        refreshParams();
        return;
    }
    m_view.rescale_height->blockSignals(true);
    std::unique_ptr<ProfileModel> &profile = pCore->getCurrentProfile();
    m_view.rescale_height->setValue(val * profile->height() / profile->width());
    KdenliveSettings::setDefaultrescaleheight(m_view.rescale_height->value());
    m_view.rescale_height->blockSignals(false);
    refreshParams();
}

void RenderWidget::slotUpdateRescaleHeight(int val)
{
    KdenliveSettings::setDefaultrescaleheight(val);
    if (!m_view.rescale_keep->isChecked()) {
        refreshParams();
        return;
    }
    m_view.rescale_width->blockSignals(true);
    std::unique_ptr<ProfileModel> &profile = pCore->getCurrentProfile();
    m_view.rescale_width->setValue(val * profile->width() / profile->height());
    KdenliveSettings::setDefaultrescaleheight(m_view.rescale_width->value());
    m_view.rescale_width->blockSignals(false);
    refreshParams();
}

void RenderWidget::slotSwitchAspectRatio()
{
    KdenliveSettings::setRescalekeepratio(m_view.rescale_keep->isChecked());
    if (m_view.rescale_keep->isChecked()) {
        slotUpdateRescaleWidth(m_view.rescale_width->value());
    }
}

void RenderWidget::slotCheckStartGuidePosition()
{
    if (m_view.guide_start->currentIndex() > m_view.guide_end->currentIndex()) {
        m_view.guide_start->setCurrentIndex(m_view.guide_end->currentIndex());
    }
}

void RenderWidget::slotCheckEndGuidePosition()
{
    if (m_view.guide_end->currentIndex() < m_view.guide_start->currentIndex()) {
        m_view.guide_end->setCurrentIndex(m_view.guide_start->currentIndex());
    }
}

void RenderWidget::setGuides(std::weak_ptr<MarkerListModel> guidesModel)
{
    m_guidesModel = std::move(guidesModel);
    reloadGuides();
    if (auto ptr = m_guidesModel.lock()) {
        connect(ptr.get(), &MarkerListModel::modelChanged, this, &RenderWidget::reloadGuides);
    }
}

void RenderWidget::reloadGuides()
{
    double projectDuration = GenTime(pCore->projectDuration() - 1, pCore->getCurrentFps()).ms() / 1000;
    QVariant startData = m_view.guide_start->currentData();
    QVariant endData = m_view.guide_end->currentData();
    m_view.guide_start->clear();
    m_view.guide_end->clear();
    m_view.guideCategoryCombo->clear();

    if (auto ptr = m_guidesModel.lock()) {
        QList<CommentedTime> markers = ptr->getAllMarkers();
        double fps = pCore->getCurrentFps();
        m_view.render_guide->setEnabled(!markers.isEmpty());
        m_view.render_multi->setEnabled(!markers.isEmpty());
        if (!markers.isEmpty()) {
            m_view.guide_start->addItem(i18n("Beginning"), 0);
            for (const auto &marker : qAsConst(markers)) {
                GenTime pos = marker.time();
                const QString guidePos = Timecode::getStringTimecode(pos.frames(fps), fps);
                m_view.guide_start->addItem(marker.comment() + QLatin1Char('/') + guidePos, pos.seconds());
                m_view.guide_end->addItem(marker.comment() + QLatin1Char('/') + guidePos, pos.seconds());
            }
            m_view.guide_end->addItem(i18n("End"), projectDuration);
            if (!startData.isNull()) {
                int ix = qMax(0, m_view.guide_start->findData(startData));
                m_view.guide_start->setCurrentIndex(ix);
            }
            if (!endData.isNull()) {
                int ix = qMax(m_view.guide_start->currentIndex() + 1, m_view.guide_end->findData(endData));
                m_view.guide_end->setCurrentIndex(ix);
            }

            // Set up guide categories
            static std::array<QColor, 9> markerTypes = ptr->markerTypes;
            QPixmap pixmap(32,32);
            m_view.guideCategoryCombo->addItem(i18n("All Categories"), -1);
            for (uint i = 0; i < markerTypes.size(); ++i) {
                if (!ptr->getAllMarkers(i).isEmpty()) {
                    pixmap.fill(markerTypes[size_t(i)]);
                    QIcon colorIcon(pixmap);
                    m_view.guideCategoryCombo->addItem(colorIcon, i18n("Category %1", i), i);
                }
            }
        } else {
            if (m_view.render_guide->isChecked() || m_view.render_multi->isChecked()) {
                m_view.render_full->setChecked(true);
            }
        }
    } else {
        m_view.render_guide->setEnabled(false);
        m_view.render_multi->setEnabled(false);
        if (m_view.render_guide->isChecked() || m_view.render_multi->isChecked()) {
            m_view.render_full->setChecked(true);
        }
    }
    slotRenderModeChanged();
}

void RenderWidget::slotUpdateButtons(const QUrl &url)
{
    if (!RenderPresetRepository::get()->presetExists(m_currentProfile)) {
        m_view.buttonSaveAs->setEnabled(false);
        m_view.buttonDelete->setEnabled(false);
        m_view.buttonEdit->setEnabled(false);
        m_view.buttonRender->setEnabled(false);
        m_view.buttonGenerateScript->setEnabled(false);
    } else {
        std::unique_ptr<RenderPresetModel> &profile = RenderPresetRepository::get()->getPreset(m_currentProfile);
        m_view.buttonRender->setEnabled(!m_view.out_file->url().isEmpty() && profile->error().isEmpty());
        m_view.buttonGenerateScript->setEnabled(!m_view.out_file->url().isEmpty() && profile->error().isEmpty());
        m_view.buttonSaveAs->setEnabled(true);
        m_view.buttonDelete->setEnabled(profile->editable());
        m_view.buttonEdit->setEnabled(profile->editable());
    }
    if (url.isValid()) {
        if (!RenderPresetRepository::get()->presetExists(m_currentProfile)) {
            m_view.buttonRender->setEnabled(false);
            m_view.buttonGenerateScript->setEnabled(false);
            return;
        }
        std::unique_ptr<RenderPresetModel> &profile = RenderPresetRepository::get()->getPreset(m_currentProfile);
        m_view.out_file->setUrl(filenameWithExtension(url, profile->extension()));
    }
}


void RenderWidget::slotUpdateButtons()
{
    slotUpdateButtons(QUrl());
}

void RenderWidget::slotSavePresetAs()
{
    if (RenderPresetRepository::get()->presetExists(m_currentProfile)) {
        std::unique_ptr<RenderPresetModel> &profile = RenderPresetRepository::get()->getPreset(m_currentProfile);

        QPointer<RenderPresetDialog> dialog = new RenderPresetDialog(this, profile.get(), RenderPresetDialog::SaveAs);
        if (dialog->exec() == QDialog::Accepted) {
            parseProfiles(dialog->saveName());
        }
        delete dialog;
    } else {
        slotNewPreset();
    }
}

void RenderWidget::slotNewPreset()
{
    QPointer<RenderPresetDialog> dialog = new RenderPresetDialog(this);
    if (dialog->exec() == QDialog::Accepted) {
        parseProfiles(dialog->saveName());
    }
    delete dialog;
}

void RenderWidget::slotEditPreset()
{
    if (!RenderPresetRepository::get()->presetExists(m_currentProfile)) {
        return;
    }
    std::unique_ptr<RenderPresetModel> &profile = RenderPresetRepository::get()->getPreset(m_currentProfile);

    if (!profile->editable()) {
        return;
    }

    QPointer<RenderPresetDialog> dialog = new RenderPresetDialog(this, profile.get(), RenderPresetDialog::Edit);
    if (dialog->exec() == QDialog::Accepted) {      
        parseProfiles(dialog->saveName());
    }
    delete dialog;
}

void RenderWidget::focusItem(const QString &preset)
{
    QItemSelectionModel *selection = m_view.profileTree->selectionModel();
    QModelIndex index = m_treeModel->findPreset(preset.isEmpty() ? QStringLiteral("MP4-H264/AAC") : preset);
    if (!index.isValid() && RenderPresetRepository::get()->presetExists(KdenliveSettings::renderProfile())) {
        index = m_treeModel->findPreset(KdenliveSettings::renderProfile());
    }
    if (index.isValid()) {
        selection->select(index, QItemSelectionModel::Select);
        // expand corresponding category
        auto parent = m_treeModel->parent(index);
        m_view.profileTree->expand(parent);
        m_view.profileTree->scrollTo(index, QAbstractItemView::PositionAtCenter);
    }
}

void RenderWidget::slotPrepareExport(bool delayedRendering, const QString &scriptPath)
{
    Q_UNUSED(scriptPath)
    if (pCore->projectDuration() < 2) {
        // Empty project, don't attempt to render
        m_view.infoMessage->setMessageType(KMessageWidget::Warning);
        m_view.infoMessage->setText(i18n("Add a clip to timeline before rendering"));
        m_view.infoMessage->animatedShow();
        return;
    }
    if (!QFile::exists(KdenliveSettings::rendererpath())) {
        m_view.infoMessage->setMessageType(KMessageWidget::Warning);
        m_view.infoMessage->setText(i18n("Cannot find the melt program required for rendering (part of Mlt)"));
        m_view.infoMessage->animatedShow();
        return;
    }
    m_view.infoMessage->hide();
    if (QFile::exists(m_view.out_file->url().toLocalFile())) {
        if (KMessageBox::warningYesNo(this, i18n("Output file already exists. Do you want to overwrite it?")) != KMessageBox::Yes) {
            return;
        }
    }
    // mantisbt 1051
    QDir dir(m_view.out_file->url().adjusted(QUrl::RemoveFilename).toLocalFile());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        KMessageBox::sorry(this, i18n("The directory %1, could not be created.\nPlease make sure you have the required permissions.",
                                      m_view.out_file->url().adjusted(QUrl::RemoveFilename).toLocalFile()));
        return;
    }

    prepareRendering(delayedRendering);
}

void RenderWidget::prepareRendering(bool delayedRendering)
{
    saveRenderProfile();

    KdenliveDoc *project = pCore->currentDoc();
    QString overlayData = m_view.tc_type->currentData().toString();
    QString playlistContent = pCore->projectManager()->projectSceneList(project->url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile(), overlayData);

    // Set playlist audio volume to 100%
    QDomDocument doc;
    doc.setContent(playlistContent);
    QDomElement tractor = doc.documentElement().firstChildElement(QStringLiteral("tractor"));
    if (!tractor.isNull()) {
        QDomNodeList props = tractor.elementsByTagName(QStringLiteral("property"));
        for (int i = 0; i < props.count(); ++i) {
            if (props.at(i).toElement().attribute(QStringLiteral("name")) == QLatin1String("meta.volume")) {
                props.at(i).firstChild().setNodeValue(QStringLiteral("1"));
                break;
            }
        }
    }

    // Add autoclose to playlists.
    QDomNodeList playlists = doc.elementsByTagName(QStringLiteral("playlist"));
    for (int i = 0; i < playlists.length(); ++i) {
        playlists.item(i).toElement().setAttribute(QStringLiteral("autoclose"), 1);
    }

    // Do we want proxy rendering
    if (project->useProxy() && !m_view.proxy_render->isChecked()) {
        pCore->currentDoc()->useOriginals(doc);
    }

    QString outputFile = m_view.out_file->url().toLocalFile();
    // in/out points
    int in = 0;
    // Remove last black frame
    int out = pCore->projectDuration() - 2;
    Monitor *pMon = pCore->getMonitor(Kdenlive::ProjectMonitor);
    double fps = pCore->getCurrentProfile()->fps();
    if (m_view.render_zone->isChecked()) {
        in = pMon->getZoneStart();
        out = pMon->getZoneEnd() - 1;
        generateRenderFiles(doc, in, out, outputFile, delayedRendering);
    } else if (m_view.render_guide->isChecked()) {
        double guideStart = m_view.guide_start->itemData(m_view.guide_start->currentIndex()).toDouble();
        double guideEnd = m_view.guide_end->itemData(m_view.guide_end->currentIndex()).toDouble();
        in = int(GenTime(guideStart).frames(fps));
        // End rendering at frame before last guide
        out = int(GenTime(guideEnd).frames(fps)) - 1;
        generateRenderFiles(doc, in, out, outputFile, delayedRendering);
    } else if (m_view.render_multi->isChecked()) {
        if (auto ptr = m_guidesModel.lock()) {
            int category = m_view.guideCategoryCombo->currentData().toInt();
            QList<CommentedTime> markers = ptr->getAllMarkers(category);
            if (!markers.isEmpty()) {
                bool beginParsed = false;
                QStringList names;
                for (int i = 0; i < markers.count(); i++) {
                    QString name;
                    int absoluteOut = pCore->projectDuration() - 2;
                    in = 0;
                    out = absoluteOut;
                    if (!beginParsed && i == 0 && markers.at(i).time().frames(fps) != 0) {
                        i -= 1;
                        beginParsed = true;
                        name = i18n("begin");
                    }
                    if (i >= 0) {
                        name = markers.at(i).comment();
                        in = markers.at(i).time().frames(fps);
                    }
                    int j = 0;
                    QString newName = name;
                    // if name alrady exist, add a suffix
                    while (names.contains(newName)) {
                        newName = QStringLiteral("%1_%2").arg(name).arg(j);
                        j++;
                    }
                    names.append(newName);
                    name = newName;
                    if (in < absoluteOut) {
                        if (i+1 < markers.count()) {
                            out = qMin(markers.at(i+1).time().frames(fps) - 1, absoluteOut);
                        }
                        QString filename = outputFile.section(QLatin1Char('.'), 0, -2) + QStringLiteral("-%1.").arg(name) + outputFile.section(QLatin1Char('.'), -1);
                        QDomDocument docCopy = doc.cloneNode(true).toDocument();
                        generateRenderFiles(docCopy, in, out, filename, false);
                    }
                }
            }
        }
    } else {
        generateRenderFiles(doc, in, out, outputFile, delayedRendering);
    }
}

QString RenderWidget::generatePlaylistFile(bool delayedRendering)
{
    if (delayedRendering) {
        bool ok;
        QString filename = QFileInfo(pCore->currentDoc()->url().toLocalFile()).fileName();
        const QString fileExtension = QStringLiteral(".mlt");
        if (filename.isEmpty()) {
            filename = i18n("export");
        } else {
            filename = filename.section(QLatin1Char('.'), 0, -2);
        }

        QDir projectFolder(pCore->currentDoc()->projectDataFolder());
        projectFolder.mkpath(QStringLiteral("kdenlive-renderqueue"));
        projectFolder.cd(QStringLiteral("kdenlive-renderqueue"));
        int ix = 1;
        QString newFilename = filename;
        // if name alrady exist, add a suffix
        while (projectFolder.exists(newFilename + fileExtension)) {
            newFilename = QStringLiteral("%1-%2").arg(filename).arg(ix);
            ix++;
        }
        filename = QInputDialog::getText(this, i18nc("@title:window", "Delayed Rendering"), i18n("Select a name for this rendering."), QLineEdit::Normal, newFilename, &ok);
        if (!ok) {
            return {};
        }
        if (!filename.endsWith(fileExtension)) {
            filename.append(fileExtension);
        }
        if (projectFolder.exists(newFilename)) {
            if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", filename)) == KMessageBox::No) {
                return {};
            }
        }
        return projectFolder.absoluteFilePath(filename);
    }
    // No delayed rendering, we can use a temp file
    QTemporaryFile tmp(QDir::temp().absoluteFilePath(QStringLiteral("kdenlive-XXXXXX.mlt")));
    if (!tmp.open()) {
        // Something went wrong
        return {};
    }
    tmp.close();
    return tmp.fileName();
}

void RenderWidget::generateRenderFiles(QDomDocument doc, int in, int out, QString outputFile, bool delayedRendering)
{
    QString playlistPath = generatePlaylistFile(delayedRendering);
    QString extension = outputFile.section(QLatin1Char('.'), -1);

    if (playlistPath.isEmpty()) {
        return;
    }

    QString renderArgs = m_view.advanced_params->toPlainText().simplified();
    QDomElement consumer = doc.createElement(QStringLiteral("consumer"));
    consumer.setAttribute(QStringLiteral("in"), in);
    consumer.setAttribute(QStringLiteral("out"), out);
    consumer.setAttribute(QStringLiteral("mlt_service"), QStringLiteral("avformat"));

    QDomNodeList profiles = doc.elementsByTagName(QStringLiteral("profile"));
    if (profiles.isEmpty()) {
        doc.documentElement().insertAfter(consumer, doc.documentElement());
    } else {
        doc.documentElement().insertAfter(consumer, profiles.at(profiles.length() - 1));
    }

    // insert params from preset
    QStringList args = renderArgs.split(QLatin1Char(' '));
    for (auto &param : args) {
        if (param.contains(QLatin1Char('='))) {
            QString paramName = param.section(QLatin1Char('='), 0, 0);
            QString paramValue = param.section(QLatin1Char('='), 1);
            consumer.setAttribute(paramName, paramValue);
        }
    }

    // If we use a pix_fmt with alpha channel (ie. transparent),
    // we need to remove the black background track
    if (renderArgs.contains(QLatin1String("pix_fmt=argb"))
     || renderArgs.contains(QLatin1String("pix_fmt=abgr"))
     || renderArgs.contains(QLatin1String("pix_fmt=bgra"))
     || renderArgs.contains(QLatin1String("pix_fmt=gbra"))
     || renderArgs.contains(QLatin1String("pix_fmt=rgba"))
     || renderArgs.contains(QLatin1String("pix_fmt=yuva"))
     || renderArgs.contains(QLatin1String("pix_fmt=ya"  ))
     || renderArgs.contains(QLatin1String("pix_fmt=ayuv"))) {
        auto prods = doc.elementsByTagName(QStringLiteral("producer"));
        for (int i = 0; i < prods.count(); ++i) {
            auto prod = prods.at(i).toElement();
            if (prod.attribute(QStringLiteral("id")) == QStringLiteral("black_track")) {
                Xml::setXmlProperty(prod, QStringLiteral("resource"), QStringLiteral("transparent"));
                break;
            }
        }
    }

    QMap<QString, QString> renderFiles;
    if (renderArgs.contains("=stills/"))
    {
        // Image sequence, ensure we have a %0xd at file end.
        // Format string for counter
        QRegularExpression rx(QRegularExpression::anchoredPattern(QStringLiteral(".*%[0-9]*d.*")));
        if (!rx.match(outputFile).hasMatch()) {
            outputFile = outputFile.section(QLatin1Char('.'), 0, -2) + QStringLiteral("_%05d.") + extension;
        }
    }

    if (m_view.stemAudioExport->isChecked() && m_view.stemAudioExport->isEnabled()) {
        if (delayedRendering) {
            if (KMessageBox::warningContinueCancel(this, i18n("Script rendering and multi track audio export can not be used together.\n"
                                                        "Script will be saved without multi tracke export."))
                                                   == KMessageBox::Cancel) { return; };
        }
        int audioCount = 0;
        QDomNodeList orginalTractors = doc.elementsByTagName(QStringLiteral("tractor"));
        // process in reversed order to make file naming fit to UI
        for (int i = orginalTractors.size(); i >= 0; i--) {
            bool isAudio = Xml::getXmlProperty(orginalTractors.at(i).toElement(), QStringLiteral("kdenlive:audio_track")).toInt() == 1;
            QString trackName = Xml::getXmlProperty(orginalTractors.at(i).toElement(), QStringLiteral("kdenlive:track_name"));
            if (isAudio) {
                // setup filenames
                QString appendix = QString("_Audio_%1%2%3.").arg(audioCount + 1).arg(trackName.isEmpty() ? QString() : QStringLiteral("-")).arg(trackName.replace(QStringLiteral(" "), QStringLiteral("_")));
                QString playlistFile = playlistPath.section(QLatin1Char('.'), 0, -2) + appendix + playlistPath.section(QLatin1Char('.'), -1);
                QString targetFile = outputFile.section(QLatin1Char('.'), 0, -2) + appendix + extension;
                renderFiles.insert(playlistFile, targetFile);

                // init doc copy
                QDomDocument docCopy = doc.cloneNode(true).toDocument();
                QDomElement copyConsumer = docCopy.elementsByTagName(QStringLiteral("consumer")).at(0).toElement();
                copyConsumer.setAttribute(QStringLiteral("target"), targetFile);

                QDomNodeList tracktors = docCopy.elementsByTagName(QStringLiteral("tractor"));
                // the last tractor is the main tracktor, don't process it (-1)
                for (int j = 0; j < tracktors.size() - 1; j++) {
                    QDomNodeList tracks = tracktors.at(j).toElement().elementsByTagName(QStringLiteral("track"));
                    for (int l = 0; l < tracks.size(); l++) {
                        if (i != j) {
                            tracks.at(l).toElement().setAttribute(QStringLiteral("hide"), QStringLiteral("both"));
                        }
                    }
                }

                QFile file(playlistFile);
                if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    pCore->displayMessage(i18n("Cannot write to file %1", playlistFile), ErrorMessage);
                    return;
                }
                file.write(docCopy.toString().toUtf8());
                if (file.error() != QFile::NoError) {
                    pCore->displayMessage(i18n("Cannot write to file %1", playlistFile), ErrorMessage);
                    file.close();
                    return;
                }
                file.close();
                audioCount++;
            }

        }
    }

    QDomDocument clone;
    int passes = m_view.checkTwoPass->isChecked() ? 2 : 1;
    if (passes == 2) {
        // We will generate 2 files, one for each pass.
        clone = doc.cloneNode(true).toDocument();
    }

    for (int i = 0; i < passes; i++) {
        // Append consumer settings
        QDomDocument final = i > 0 ? clone : doc;
        QDomNodeList consList = final.elementsByTagName(QStringLiteral("consumer"));
        QDomElement finalConsumer = consList.at(0).toElement();
        finalConsumer.setAttribute(QStringLiteral("target"), outputFile);
        int pass = passes == 2 ? i + 1 : 0;
        QString playlistName = playlistPath;
        if (pass == 2) {
            playlistName = playlistName.section(QLatin1Char('.'), 0, -2) + QStringLiteral("-pass%1.").arg(2) + extension;
        }
        renderFiles.insert(playlistName, outputFile);

        // Prepare rendering args
        QString logFile = QStringLiteral("%1_2pass.log").arg(outputFile);
        if (renderArgs.contains(QStringLiteral("libx265"))) {
            if (pass == 1 || pass == 2) {
                QString x265params = finalConsumer.attribute("x265-params");
                x265params = QString("pass=%1:stats=%2:%3").arg(pass).arg(logFile.replace(":", "\\:"), x265params);
                finalConsumer.setAttribute("x265-params", x265params);
            }
        } else {
            if (pass == 1 || pass == 2) {
                finalConsumer.setAttribute("pass", pass);
                finalConsumer.setAttribute("passlogfile", logFile);
            }
            if (pass == 1) {
                finalConsumer.setAttribute("fastfirstpass", 1);
                finalConsumer.removeAttribute("acodec");
                finalConsumer.setAttribute("an", 1);
            } else {
                finalConsumer.removeAttribute("fastfirstpass");
            }
        }
        QFile file(playlistName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            pCore->displayMessage(i18n("Cannot write to file %1", playlistName), ErrorMessage);
            return;
        }
        file.write(final.toString().toUtf8());
        if (file.error() != QFile::NoError) {
            pCore->displayMessage(i18n("Cannot write to file %1", playlistName), ErrorMessage);
            file.close();
            return;
        }
        file.close();
    }

    // Create jobs
    if (delayedRendering) {
        parseScriptFiles();
        return;
    }
    QList<RenderJobItem *> jobList;
    QMap<QString, QString>::const_iterator i = renderFiles.constBegin();
    while (i != renderFiles.constEnd()) {
        RenderJobItem *renderItem = createRenderJob(i.key(), i.value(), in, out);
        if (renderItem != nullptr) {
            jobList << renderItem;
        }
        ++i;
    }
    if (jobList.count() > 0) {
        m_view.running_jobs->setCurrentItem(jobList.at(0));
    }
    m_view.tabWidget->setCurrentIndex(Tabs::JobsTab);
    // check render status
    checkRenderStatus();
}

RenderJobItem *RenderWidget::createRenderJob(const QString &playlist, const QString &outputFile, int in, int out)
{
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(outputFile, Qt::MatchExactly, 1);
    RenderJobItem *renderItem = nullptr;
    if (!existing.isEmpty()) {
        renderItem = static_cast<RenderJobItem *>(existing.at(0));
        if (renderItem->status() == RUNNINGJOB || renderItem->status() == WAITINGJOB || renderItem->status() == STARTINGJOB) {
            // There is an existing job that is still pending
            KMessageBox::information(this,
                i18n("There is already a job writing file:<br /><b>%1</b><br />Abort the job if you want to overwrite it…", outputFile),
                i18n("Already running"));
            // focus the running job
            m_view.running_jobs->setCurrentItem(renderItem);
            return nullptr;
        }
        // There is an existing job that already finished
        delete renderItem;
        renderItem = nullptr;
    }
    renderItem = new RenderJobItem(m_view.running_jobs, QStringList() << QString() << outputFile);

    QDateTime t = QDateTime::currentDateTime();
    renderItem->setData(1, StartTimeRole, t);
    renderItem->setData(1, LastTimeRole, t);
    renderItem->setData(1, LastFrameRole, in);
    QStringList argsJob = {KdenliveSettings::rendererpath(), playlist, outputFile,
                           QStringLiteral("-pid:%1").arg(QCoreApplication::applicationPid()), QStringLiteral("-out"), QString::number(out)};
    renderItem->setData(1, ParametersRole, argsJob);
    qDebug() << "* CREATED JOB WITH ARGS: " << argsJob;
    renderItem->setData(1, OpenBrowserRole, m_view.open_browser->isChecked());
    if (!m_view.audio_box->isChecked()) {
        renderItem->setData(1, ExtraInfoRole, i18n("Video without audio track"));
    } else if (!m_view.video_box->isChecked()) {
        renderItem->setData(1, ExtraInfoRole, i18n("Audio without video track"));
    } else {
        renderItem->setData(1, ExtraInfoRole, QString());
    }
    return renderItem;
}

void RenderWidget::checkRenderStatus()
{
    // check if we have a job waiting to render
    if (m_blockProcessing) {
        return;
    }

    // Make sure no other rendering is running
    if (runningJobsCount() > 0) {
        return;
    }

    auto *item = static_cast<RenderJobItem *>(m_view.running_jobs->topLevelItem(0));

    bool waitingJob = false;

    // Find first waiting job
    while (item != nullptr) {
        if (item->status() == WAITINGJOB) {
            QDateTime t = QDateTime::currentDateTime();
            item->setData(1, StartTimeRole, t);
            item->setData(1, LastTimeRole, t);
            waitingJob = true;
            startRendering(item);
            // Check for 2 pass encoding
            QStringList jobData = item->data(1, ParametersRole).toStringList();
            if (jobData.size() > 2 && jobData.at(1).endsWith(QStringLiteral("-pass2.mlt"))) {
                // Find and remove 1st pass job
                QTreeWidgetItem *above = m_view.running_jobs->itemAbove(item);
                QString firstPassName = jobData.at(1).section(QLatin1Char('-'), 0, -2) + QStringLiteral(".mlt");
                while (above) {
                    QStringList aboveData = above->data(1, ParametersRole).toStringList();
                    qDebug() << "// GOT  JOB: " << aboveData.at(1);
                    if (aboveData.size() > 2 && aboveData.at(1) == firstPassName) {
                        delete above;
                        break;
                    }
                    above = m_view.running_jobs->itemAbove(above);
                }
            }
            item->setStatus(STARTINGJOB);
            break;
        }
        item = static_cast<RenderJobItem *>(m_view.running_jobs->itemBelow(item));
    }
    if (!waitingJob && m_view.shutdown->isChecked()) {
        emit shutdown();
    }
}

void RenderWidget::startRendering(RenderJobItem *item)
{
    auto rendererArgs = item->data(1, ParametersRole).toStringList();
    qDebug() << "starting kdenlive_render process using: " << m_renderer;
    if (!QProcess::startDetached(m_renderer, rendererArgs)) {
        item->setStatus(FAILEDJOB);
    } else {
        KNotification::event(QStringLiteral("RenderStarted"), i18n("Rendering <i>%1</i> started", item->text(1)), QPixmap(), this);
    }
}

int RenderWidget::waitingJobsCount() const
{
    int count = 0;
    auto *item = static_cast<RenderJobItem *>(m_view.running_jobs->topLevelItem(0));
    while (item != nullptr) {
        if (item->status() == WAITINGJOB) {
            count++;
        }
        item = static_cast<RenderJobItem *>(m_view.running_jobs->itemBelow(item));
    }
    return count;
}

int RenderWidget::runningJobsCount() const
{
    int count = 0;
    auto *item = static_cast<RenderJobItem *>(m_view.running_jobs->topLevelItem(0));
    while (item != nullptr) {
        if (item->status() == RUNNINGJOB || item->status() == STARTINGJOB) {
            count++;
        }
        item = static_cast<RenderJobItem *>(m_view.running_jobs->itemBelow(item));
    }
    return count;
}

void RenderWidget::adjustViewToProfile()
{
    m_view.rescale_width->setValue(KdenliveSettings::defaultrescalewidth());
    if (!m_view.rescale_keep->isChecked()) {
        m_view.rescale_height->blockSignals(true);
        m_view.rescale_height->setValue(KdenliveSettings::defaultrescaleheight());
        m_view.rescale_height->blockSignals(false);
    }
    refreshView();
}

void RenderWidget::refreshView()
{
    focusItem();
    loadProfile();
}

QUrl RenderWidget::filenameWithExtension(QUrl url, const QString &extension)
{
    if (!url.isValid()) {
        url = QUrl::fromLocalFile(pCore->currentDoc()->projectDataFolder() + QDir::separator());
    }
    QString directory = url.adjusted(QUrl::RemoveFilename).toLocalFile();

    QString ext;
    if (extension.startsWith(QLatin1Char('.'))) {
        ext = extension;
    } else {
        ext = '.' + extension;
    }

    QString filename = url.fileName();
    if (filename.isEmpty()) {
        filename = pCore->currentDoc()->url().fileName();
    }
    if (filename.isEmpty()) {
        filename = i18n("untitled");
    }

    int pos = filename.lastIndexOf('.');
    if (pos == 0) {
        filename.append(ext);
    } else {
        if (!filename.endsWith(ext, Qt::CaseInsensitive)) {
            filename = filename.left(pos) + ext;
        }
    }

    return QUrl::fromLocalFile(directory + filename);
}

void RenderWidget::slotChangeSelection(const QModelIndex &current, const QModelIndex &previous)
{
    if (m_treeModel->parent(current) == QModelIndex()) {
        // in that case, we have selected a category, which we don't want
        QItemSelectionModel *selection = m_view.profileTree->selectionModel();
        selection->select(previous, QItemSelectionModel::Select);
        // expand corresponding category
        auto parent = m_treeModel->parent(previous);
        m_view.profileTree->expand(parent);
        m_view.profileTree->scrollTo(previous, QAbstractItemView::PositionAtCenter);
        return;
    }
    m_currentProfile = m_treeModel->getPreset(current);
    KdenliveSettings::setRenderProfile(m_currentProfile);
    loadProfile();
}

void RenderWidget::loadProfile()
{
    slotUpdateButtons();
    // Format not available (e.g. codec not installed); Disable start
    if (!RenderPresetRepository::get()->presetExists(m_currentProfile)) {
        if (m_currentProfile.isEmpty()) {
            errorMessage(PresetError, i18n("No preset selected"));
        } else {
            errorMessage(PresetError, i18n("No matching preset"));
        }
        m_view.advanced_params->clear();
        m_view.buttonRender->setEnabled(false);
        m_view.buttonGenerateScript->setEnabled(false);
        m_view.optionsGroup->setEnabled(false);
        return;
    }
    std::unique_ptr<RenderPresetModel> &profile = RenderPresetRepository::get()->getPreset(m_currentProfile);
    QString params = profile->params();

    if (profile->extension().isEmpty()) {
        errorMessage(PresetError, i18n("Invalid preset"));
    }
    QString error = profile->error();
    if (error.isEmpty()) {
        error = profile->warning();
    }
    errorMessage(PresetError, error);

    QUrl url = filenameWithExtension(m_view.out_file->url(), profile->extension());
    m_view.out_file->setUrl(url);
    m_view.out_file->setFilter("*." + profile->extension());

    m_view.buttonDelete->setEnabled(profile->editable());
    m_view.buttonEdit->setEnabled(profile->editable());

    if (!profile->speeds().isEmpty()) {
        int speed = profile->speeds().count() - 1;
        m_view.speed->setEnabled(true);
        m_view.speed->setMaximum(speed);
        m_view.speed->setValue(speed * 3 / 4); // default to intermediate speed
    } else {
        m_view.speed->setEnabled(false);
    }
    adjustSpeed(m_view.speed->value());
    bool passes = profile->hasParam(QStringLiteral("passes"));
    m_view.checkTwoPass->setEnabled(passes);
    m_view.checkTwoPass->setChecked(passes && params.contains(QStringLiteral("passes=2")));

    m_view.encoder_threads->setEnabled(!params.contains(QStringLiteral("threads=")));

    m_view.video_box->setChecked(profile->getParam(QStringLiteral("vn")) != QStringLiteral("1"));
    m_view.audio_box->setChecked(profile->getParam(QStringLiteral("an")) != QStringLiteral("1"));

    m_view.buttonRender->setEnabled(error.isEmpty());
    m_view.buttonGenerateScript->setEnabled(error.isEmpty());
    m_view.optionsGroup->setEnabled(true);
    refreshParams();
}

void RenderWidget::refreshParams()
{
    if (!RenderPresetRepository::get()->presetExists(m_currentProfile)) {
        return;
    }
    std::unique_ptr<RenderPresetModel> &profile = RenderPresetRepository::get()->getPreset(m_currentProfile);

    if (profile->hasFixedSize()) {
        // profile has a fixed size, do not allow resize
        m_view.rescale->setEnabled(false);
        setRescaleEnabled(false);
    } else {
        m_view.rescale->setEnabled(m_view.video_box->isChecked());
        setRescaleEnabled(m_view.video_box->isChecked() && m_view.rescale->isChecked());
    }

    QStringList removeParams = {
        QStringLiteral("an"),
        QStringLiteral("audio_off"),
        QStringLiteral("vn"),
        QStringLiteral("video_off"),
        QStringLiteral("real_time")
    };

    QStringList newParams;

    // Audio Channels: don't override, only set if it is not yet set
    if (!profile->hasParam(QStringLiteral("channels"))) {
        newParams.append(QStringLiteral("channels=%1").arg(pCore->audioChannels()));
    }

    // Check for movit
    if (KdenliveSettings::gpu_accel()) {
        newParams.append(QStringLiteral("glsl.=1"));
    }

    // In case of libx265 add x265-param
    if (profile->getParam(QStringLiteral("vcodec")).toLower() == QStringLiteral("libx265")) {
        newParams.append(QStringLiteral("x265-param=%1").arg(profile->x265Params()));
    }

    // Rescale
    bool rescale = m_view.rescale->isEnabled() && m_view.rescale->isChecked();
    if (rescale) {
        removeParams.append({QStringLiteral("s"), QStringLiteral("width"), QStringLiteral("height")});
        newParams.append(QStringLiteral("s=%1x%2").arg(m_view.rescale_width->value()).arg(m_view.rescale_height->value()));
    }

    // Preview rendering
    bool previewRes = m_view.render_at_preview_res->isChecked() && pCore->getMonitorProfile().height() != pCore->getCurrentFrameSize().height();
    if (previewRes) {
        removeParams.append(QStringLiteral("scale"));
        newParams.append(QStringLiteral("scale=%1").arg(double(pCore->getMonitorProfile().height()) / pCore->getCurrentFrameSize().height()));
    }

    // disable audio if requested
    if (!m_view.audio_box->isChecked()) {
        newParams.append(QStringLiteral("an=1 audio_off=1"));
    }

    if (!m_view.video_box->isChecked()) {
        newParams.append(QStringLiteral("vn=1 video_off=1"));
    }

    if (!(m_view.video_box->isChecked() || m_view.audio_box->isChecked())) {
        errorMessage(OptionsError, i18n("Rendering without audio and video does not work. Please enable at least one of both."));
        m_view.buttonRender->setEnabled(false);
    } else {
        errorMessage(OptionsError, QString());
        m_view.buttonRender->setEnabled(profile->error().isEmpty());
    }

    // Parallel Processing
    int threadCount = QThread::idealThreadCount();
    if (threadCount < 2 || !m_view.parallel_process->isChecked() || !m_view.parallel_process->isEnabled()) {
        threadCount = 1;
    } else {
        threadCount = qMin(4, threadCount - 1);
    }
    newParams.append(QStringLiteral("real_time=%1").arg(-threadCount));

    // Adjust encoding speed
    if (m_view.speed->isEnabled()) {
        QStringList speeds = profile->speeds();
        if (m_view.speed->value() < speeds.count()) {
            const QString &speedValue = speeds.at(m_view.speed->value());
            if (speedValue.contains(QLatin1Char('='))) {
                newParams.append(speedValue);
            }
        }
    }

    QString params = profile->params(removeParams);

    // Set the thread counts
    if (!params.contains(QStringLiteral("threads="))) {
        newParams.append(QStringLiteral("threads=%1").arg(KdenliveSettings::encodethreads()));
    }

    // Project metadata
    if (m_view.export_meta->isChecked()) {
        QMap<QString, QString> metadata = pCore->currentDoc()->metadata();
        QMap<QString, QString>::const_iterator i = metadata.constBegin();
        while (i != metadata.constEnd()) {
            newParams.append(QStringLiteral("%1=%2").arg(i.key(), i.value()));
            ++i;
        }
    }

    if (params.contains(QStringLiteral("%quality")) || params.contains(QStringLiteral("%audioquality"))) {
        m_view.qualityGroup->setEnabled(true);
    } else {
        m_view.qualityGroup->setEnabled(false);
    }

    double percent = double(m_view.quality->value()) / double(m_view.quality->maximum());
    m_view.qualityPercent->setText(QStringLiteral("%1%").arg(qRound(percent * 100)));
    int min = profile->videoQualities().first().toInt();
    int max = profile->videoQualities().last().toInt();
    int val = profile->defaultVQuality().toInt();
    if (m_view.qualityGroup->isChecked()) {
        if (min < max) {
            int range = max - min;
            val = min + int(range * percent);
        } else {
            int range = min - max;
            val = min - int(range * percent);
        }
    }
    params.replace(QStringLiteral("%quality"), QString::number(val));
    // TODO check if this is finally correct
    params.replace(QStringLiteral("%bitrate+'k'"), QStringLiteral("%1k").arg(profile->defaultVBitrate()));
    params.replace(QStringLiteral("%bitrate"), QStringLiteral("%1").arg(profile->defaultVBitrate()));

    val = profile->defaultABitrate().toInt() * 1000;
    if (m_view.qualityGroup->isChecked()) {
        val *= percent;
    }
    // cvbr = Constrained Variable Bit Rate
    params.replace(QStringLiteral("%cvbr"), QString::number(val));

    min = profile->audioQualities().first().toInt();
    max = profile->audioQualities().last().toInt();
    val = profile->defaultAQuality().toInt();
    if (m_view.qualityGroup->isChecked()) {
        if (min < max) {
            int range = max - min;
            val = min + int(range * percent);
        } else {
            int range = min - max;
            val = min - int(range * percent);
        }
    }
    params.replace(QStringLiteral("%audioquality"), QString::number(val));
    // TODO check if this is finally correct
    params.replace(QStringLiteral("%audiobitrate+'k'"), QStringLiteral("%1k").arg(profile->defaultABitrate()));
    params.replace(QStringLiteral("%audiobitrate"), QStringLiteral("%1").arg(profile->defaultABitrate()));

    std::unique_ptr<ProfileModel> &projectProfile = pCore->getCurrentProfile();
    if (params.contains(QLatin1String("%dv_standard"))) {
        QString dvstd;
        if (fmod(double(projectProfile->frame_rate_num() / projectProfile->frame_rate_den()), 30.01) > 27) {
            dvstd = QStringLiteral("ntsc");
        } else {
            dvstd = QStringLiteral("pal");
        }
        if (double(projectProfile->display_aspect_num() / projectProfile->display_aspect_den()) > 1.5) {
            dvstd += QLatin1String("_wide");
        }
        params.replace(QLatin1String("%dv_standard"), dvstd);
    }

    params.replace(QLatin1String("%dar"), QStringLiteral("@%1/%2").arg(QString::number(projectProfile->display_aspect_num())).arg(QString::number(projectProfile->display_aspect_den())));
    params.replace(QLatin1String("%passes"), QString::number(static_cast<int>(m_view.checkTwoPass->isChecked()) + 1));

    newParams.prepend(params);

    m_view.advanced_params->setPlainText(newParams.join(QStringLiteral(" ")));
}

void RenderWidget::parseProfiles(const QString &selectedProfile)
{
    m_treeModel.reset();
    m_treeModel = RenderPresetTreeModel::construct(this);
    m_view.profileTree->setModel(m_treeModel.get());
    QItemSelectionModel *selectionModel = m_view.profileTree->selectionModel();
    connect(selectionModel, &QItemSelectionModel::currentRowChanged, this, &RenderWidget::slotChangeSelection);
    connect(selectionModel, &QItemSelectionModel::selectionChanged, this, [&](const QItemSelection &selected, const QItemSelection &deselected) {
        QModelIndex current;
        QModelIndex old;
        if (!selected.indexes().isEmpty()) {
            current = selected.indexes().front();
        }
        if (!deselected.indexes().isEmpty()) {
            old = deselected.indexes().front();
        }
        slotChangeSelection(current, old);
    });
    focusItem(selectedProfile);
}

void RenderWidget::setRenderProgress(const QString &dest, int progress, int frame)
{
    RenderJobItem *item = nullptr;
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(dest, Qt::MatchExactly, 1);
    if (!existing.isEmpty()) {
        item = static_cast<RenderJobItem *>(existing.at(0));
    } else {
        item = new RenderJobItem(m_view.running_jobs, QStringList() << QString() << dest);
        if (progress == 0) {
            item->setStatus(WAITINGJOB);
        }
    }
    item->setData(1, ProgressRole, progress);
    item->setStatus(RUNNINGJOB);
    if (progress == 0) {
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("media-record")));
        slotCheckJob();
    } else {
        QDateTime startTime = item->data(1, StartTimeRole).toDateTime();
        qint64 elapsedTime = startTime.secsTo(QDateTime::currentDateTime());
        int dt = elapsedTime - item->data(1, LastTimeRole).toInt();
        if (dt == 0) {
            return;
        }
        qint64 remaining = elapsedTime * (100 - progress) / progress;
        int days = int(remaining / 86400);
        int remainingSecs = int(remaining % 86400);
        QTime when = QTime(0, 0, 0, 0).addSecs(remainingSecs);
        QString est = i18n("Remaining time ");
        if (days > 0) {
            est.append(i18np("%1 day ", "%1 days ", days));
        }
        est.append(when.toString(QStringLiteral("hh:mm:ss")));
        int speed = (frame - item->data(1, LastFrameRole).toInt()) / dt;
        est.append(i18n(" (frame %1 @ %2 fps)", frame, speed));
        item->setData(1, Qt::UserRole, est);
        item->setData(1, LastTimeRole, elapsedTime);
        item->setData(1, LastFrameRole, frame);
    }
}

void RenderWidget::setRenderStatus(const QString &dest, int status, const QString &error)
{
    RenderJobItem *item = nullptr;
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(dest, Qt::MatchExactly, 1);
    if (!existing.isEmpty()) {
        item = static_cast<RenderJobItem *>(existing.at(0));
    } else {
        item = new RenderJobItem(m_view.running_jobs, QStringList() << QString() << dest);
    }
    if (!item) {
        return;
    }
    if (status == -1) {
        // Job finished successfully
        item->setStatus(FINISHEDJOB);
        QDateTime startTime = item->data(1, StartTimeRole).toDateTime();
        qint64 elapsedTime = startTime.secsTo(QDateTime::currentDateTime());
        int days = static_cast<int>(elapsedTime / 86400);
        int secs = static_cast<int>(elapsedTime % 86400);
        QTime when = QTime(0, 0, 0, 0);
        when = when.addSecs(secs);
        QString est = (days > 0) ? i18np("%1 day ", "%1 days ", days) : QString();
        est.append(when.toString(QStringLiteral("hh:mm:ss")));
        QString t = i18n("Rendering finished in %1", est);
        item->setData(1, Qt::UserRole, t);

#ifdef KF5_USE_PURPOSE
        m_shareMenu->model()->setInputData(QJsonObject{{QStringLiteral("mimeType"), QMimeDatabase().mimeTypeForFile(item->text(1)).name()},
                                                       {QStringLiteral("urls"), QJsonArray({item->text(1)})}});
        m_shareMenu->model()->setPluginType(QStringLiteral("Export"));
        m_shareMenu->reload();
#endif
        QString notif = i18n("Rendering of %1 finished in %2", item->text(1), est);
        KNotification *notify = new KNotification(QStringLiteral("RenderFinished"));
        notify->setText(notif);
        notify->setUrls({QUrl::fromLocalFile(dest)});
        notify->sendEvent();
        QUrl url = QUrl::fromLocalFile(item->text(1));
        bool exists = QFile(url.toLocalFile()).exists();
        if (exists && item->data(1, OpenBrowserRole).toBool()) {
            KIO::highlightInFileManager({url});
        }
    } else if (status == -2) {
        // Rendering crashed
        item->setStatus(FAILEDJOB);
        m_view.error_log->append(i18n("<strong>Rendering of %1 crashed</strong><br />", dest));
        m_view.error_log->append(error);
        m_view.error_log->append(QStringLiteral("<hr />"));
        m_view.error_box->setVisible(true);
    } else if (status == -3) {
        // User aborted job
        item->setStatus(ABORTEDJOB);
    } else {
        delete item;
    }
    slotCheckJob();
    checkRenderStatus();
}

void RenderWidget::slotAbortCurrentJob()
{
    auto *current = static_cast<RenderJobItem *>(m_view.running_jobs->currentItem());
    if (current) {
        if (current->status() == RUNNINGJOB) {
            emit abortProcess(current->text(1));
        } else {
            delete current;
            slotCheckJob();
            checkRenderStatus();
        }
    }
}

void RenderWidget::slotStartCurrentJob()
{
    auto *current = static_cast<RenderJobItem *>(m_view.running_jobs->currentItem());
    if ((current != nullptr) && current->status() == WAITINGJOB) {
        startRendering(current);
    }
    m_view.start_job->setEnabled(false);
}

void RenderWidget::slotCheckJob()
{
    bool activate = false;
    auto *current = static_cast<RenderJobItem *>(m_view.running_jobs->currentItem());
    if (current) {
        if (current->status() == RUNNINGJOB || current->status() == STARTINGJOB) {
            m_view.abort_job->setText(i18n("Abort Job"));
            m_view.start_job->setEnabled(false);
        } else {
            m_view.abort_job->setText(i18n("Remove Job"));
            m_view.start_job->setEnabled(current->status() == WAITINGJOB);
        }
        activate = true;
#ifdef KF5_USE_PURPOSE
        if (current->status() == FINISHEDJOB) {
            m_shareMenu->model()->setInputData(QJsonObject{{QStringLiteral("mimeType"), QMimeDatabase().mimeTypeForFile(current->text(1)).name()},
                                                           {QStringLiteral("urls"), QJsonArray({current->text(1)})}});
            m_shareMenu->model()->setPluginType(QStringLiteral("Export"));
            m_shareMenu->reload();
            m_view.shareButton->setEnabled(true);
        } else {
            m_view.shareButton->setEnabled(false);
        }
#endif
    }
    m_view.abort_job->setEnabled(activate);
}

void RenderWidget::slotCleanUpJobs()
{
    int ix = 0;
    auto *current = static_cast<RenderJobItem *>(m_view.running_jobs->topLevelItem(ix));
    while (current != nullptr) {
        if (current->status() == FINISHEDJOB || current->status() == ABORTEDJOB) {
            delete current;
        } else {
            ix++;
        }
        current = static_cast<RenderJobItem *>(m_view.running_jobs->topLevelItem(ix));
    }
    slotCheckJob();
}

void RenderWidget::parseScriptFiles()
{
    QStringList scriptsFilter;
    scriptsFilter << QStringLiteral("*.mlt");
    m_view.scripts_list->clear();

    // List the project scripts
    QDir projectFolder(pCore->currentDoc()->projectDataFolder());
    if (!projectFolder.exists(QStringLiteral("kdenlive-renderqueue"))) {
        return;
    }
    projectFolder.cd(QStringLiteral("kdenlive-renderqueue"));
    QStringList scriptFiles = projectFolder.entryList(scriptsFilter, QDir::Files);
    if (scriptFiles.isEmpty()) {
        // No scripts, delete directory
        if (projectFolder.dirName() == QStringLiteral("kdenlive-renderqueue") && projectFolder.entryList(scriptsFilter, QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
            projectFolder.removeRecursively();
            return;
        }
    }
    for (int i = 0; i < scriptFiles.size(); ++i) {
        QUrl scriptpath = QUrl::fromLocalFile(projectFolder.absoluteFilePath(scriptFiles.at(i)));
        QFile f(scriptpath.toLocalFile());
        QDomDocument doc;
        doc.setContent(&f, false);
        f.close();
        QDomElement consumer = doc.documentElement().firstChildElement(QStringLiteral("consumer"));
        if (consumer.isNull()) {
            continue;
        }
        QString target = consumer.attribute(QStringLiteral("target"));
        int out = consumer.attribute(QStringLiteral("out"), QStringLiteral("0")).toInt();
        if (target.isEmpty()) {
            continue;
        }
        QTreeWidgetItem *item = new QTreeWidgetItem(m_view.scripts_list, QStringList() << QString() << scriptpath.fileName());
        auto icon = QFileIconProvider().icon(QFileInfo(f));
        item->setIcon(0, icon.isNull() ? QIcon::fromTheme(QStringLiteral("application-x-executable-script")) : icon);
        item->setSizeHint(0, QSize(m_view.scripts_list->columnWidth(0), fontMetrics().height() * 2));
        item->setData(1, Qt::UserRole, QUrl(QUrl::fromEncoded(target.toUtf8())).url(QUrl::PreferLocalFile));
        item->setData(1, Qt::UserRole + 1, scriptpath.toLocalFile());
        item->setData(1, Qt::UserRole + 2, out);
    }
    QTreeWidgetItem *script = m_view.scripts_list->topLevelItem(0);
    if (script) {
        m_view.scripts_list->setCurrentItem(script);
        script->setSelected(true);
    }
}

void RenderWidget::slotCheckScript()
{
    QTreeWidgetItem *current = m_view.scripts_list->currentItem();
    if (current == nullptr) {
        return;
    }
    m_view.start_script->setEnabled(current->data(0, Qt::UserRole).toString().isEmpty());
    m_view.delete_script->setEnabled(true);
    for (int i = 0; i < m_view.scripts_list->topLevelItemCount(); ++i) {
        current = m_view.scripts_list->topLevelItem(i);
        if (current == m_view.scripts_list->currentItem()) {
            current->setSizeHint(1, QSize(m_view.scripts_list->columnWidth(1), fontMetrics().height() * 3));
        } else {
            current->setSizeHint(1, QSize(m_view.scripts_list->columnWidth(1), fontMetrics().height() * 2));
        }
    }
}

void RenderWidget::slotStartScript()
{
    auto *item = static_cast<RenderJobItem *>(m_view.scripts_list->currentItem());
    if (item) {
        QString destination = item->data(1, Qt::UserRole).toString();
        int out = item->data(1, Qt::UserRole + 2).toInt();
        if (QFile::exists(destination)) {
            if (KMessageBox::warningYesNo(this, i18n("Output file already exists. Do you want to overwrite it?")) != KMessageBox::Yes) {
                return;
            }
        }
        QString path = item->data(1, Qt::UserRole + 1).toString();
        // Insert new job in queue
        RenderJobItem *renderItem = nullptr;
        QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(destination, Qt::MatchExactly, 1);
        if (!existing.isEmpty()) {
            renderItem = static_cast<RenderJobItem *>(existing.at(0));
            if (renderItem->status() == RUNNINGJOB || renderItem->status() == WAITINGJOB || renderItem->status() == STARTINGJOB) {
                KMessageBox::information(
                    this, i18n("There is already a job writing file:<br /><b>%1</b><br />Abort the job if you want to overwrite it…", destination),
                    i18n("Already running"));
                return;
            }
            delete renderItem;
            renderItem = nullptr;
        }
        if (!renderItem) {
            renderItem = new RenderJobItem(m_view.running_jobs, QStringList() << QString() << destination);
        }
        renderItem->setData(1, ProgressRole, 0);
        renderItem->setStatus(WAITINGJOB);
        renderItem->setIcon(0, QIcon::fromTheme(QStringLiteral("media-playback-pause")));
        renderItem->setData(1, Qt::UserRole, i18n("Waiting…"));
        QDateTime t = QDateTime::currentDateTime();
        renderItem->setData(1, StartTimeRole, t);
        renderItem->setData(1, LastTimeRole, t);
        QStringList argsJob = {KdenliveSettings::rendererpath(), path, destination, QStringLiteral("-pid:%1").arg(QCoreApplication::applicationPid()),QStringLiteral("-out"),QString::number(out)};
        renderItem->setData(1, ParametersRole, argsJob);
        checkRenderStatus();
        m_view.tabWidget->setCurrentIndex(Tabs::JobsTab);
    }
}

void RenderWidget::slotDeleteScript()
{
    QTreeWidgetItem *item = m_view.scripts_list->currentItem();
    if (item) {
        QString path = item->data(1, Qt::UserRole + 1).toString();
        bool success = true;
        success &= static_cast<int>(QFile::remove(path));
        if (!success) {
            qCWarning(KDENLIVE_LOG) << "// Error removing script or playlist: " << path << ", " << path << ".mlt";
        }
        parseScriptFiles();
    }
}

void RenderWidget::slotHideLog()
{
    m_view.error_box->setVisible(false);
}

void RenderWidget::setRenderProfile(const QMap<QString, QString> &props)
{
    int exportAudio = props.value(QStringLiteral("renderexportaudio")).toInt();
    m_view.audio_box->setChecked(exportAudio != 1);
    if (props.contains(QStringLiteral("renderrescale"))) {
        m_view.rescale->setChecked(props.value(QStringLiteral("renderrescale")).toInt() != 0);
    }
    if (props.contains(QStringLiteral("renderrescalewidth"))) {
        m_view.rescale_width->setValue(props.value(QStringLiteral("renderrescalewidth")).toInt());
    } else {
        std::unique_ptr<ProfileModel> &profile = pCore->getCurrentProfile();
        m_view.rescale_width->setValue(profile->width() / 2);
        slotUpdateRescaleWidth(m_view.rescale_width->value());
    }
    if (props.contains(QStringLiteral("renderrescaleheight"))) {
        m_view.rescale_height->setValue(props.value(QStringLiteral("renderrescaleheight")).toInt());
    }
    if (props.contains(QStringLiteral("rendertctype"))) {
        m_view.tc_type->setCurrentIndex(props.value(QStringLiteral("rendertctype")).toInt() + 1);
    }
    if (props.contains(QStringLiteral("rendertcoverlay"))) {
        // for backward compatibility
        if (props.value(QStringLiteral("rendertcoverlay")).toInt() == 0) {
            m_view.tc_type->setCurrentIndex(0);
        }
    }
    if (props.contains(QStringLiteral("renderratio"))) {
        m_view.rescale_keep->setChecked(props.value(QStringLiteral("renderratio")).toInt() != 0);
    }
    if (props.contains(QStringLiteral("renderplay"))) {
        m_view.play_after->setChecked(props.value(QStringLiteral("renderplay")).toInt() != 0);
    }
    if (props.contains(QStringLiteral("rendertwopass"))) {
        m_view.checkTwoPass->setChecked(props.value(QStringLiteral("rendertwopass")).toInt() != 0);
    }

    if (props.contains(QStringLiteral("renderpreview"))) {
        m_view.render_at_preview_res->setChecked(props.value(QStringLiteral("renderpreview")).toInt() != 0);
    } else {
        m_view.render_at_preview_res->setChecked(false);
    }

    int mode = props.value(QStringLiteral("renderguide")).toInt();
    if (mode == 1) {
        m_view.render_zone->setChecked(true);
    } else if (mode == 2) {
        m_view.render_guide->setChecked(true);
        m_view.guide_start->setCurrentIndex(props.value(QStringLiteral("renderstartguide")).toInt());
        m_view.guide_end->setCurrentIndex(props.value(QStringLiteral("renderendguide")).toInt());
    } else if (mode == 3) {
        m_view.render_multi->setChecked(true);
    } else {
        m_view.render_full->setChecked(true);
    }
    slotRenderModeChanged();

    QString url = props.value(QStringLiteral("renderurl"));
    if (url.isEmpty()) {
        if (RenderPresetRepository::get()->presetExists(m_currentProfile)) {
            std::unique_ptr<RenderPresetModel> &profile = RenderPresetRepository::get()->getPreset(m_currentProfile);
            url = filenameWithExtension(QUrl::fromLocalFile(pCore->currentDoc()->projectDataFolder() + QDir::separator()), profile->extension()).toLocalFile();
        }
    } else if (QFileInfo(url).isRelative()) {
        url.prepend(pCore->currentDoc()->documentRoot());
    }
    m_view.out_file->setUrl(QUrl::fromLocalFile(url));

    if (props.contains(QStringLiteral("renderprofile")) || props.contains(QStringLiteral("rendercategory"))) {
        focusItem(props.value(QStringLiteral("renderprofile")));
    }

    if (props.contains(QStringLiteral("renderspeed"))) {
        m_view.speed->setValue(props.value(QStringLiteral("renderspeed")).toInt());
    }
}

void RenderWidget::saveRenderProfile()
{
    // Save rendering profile to document
    QMap<QString, QString> renderProps;
    std::unique_ptr<RenderPresetModel> &preset = RenderPresetRepository::get()->getPreset(m_currentProfile);
    renderProps.insert(QStringLiteral("rendercategory"), preset->groupName());
    renderProps.insert(QStringLiteral("renderprofile"), preset->name());
    renderProps.insert(QStringLiteral("renderurl"), m_view.out_file->url().toLocalFile());
    int mode = 0; // 0 = full project
    if (m_view.render_zone->isChecked()) {
        mode = 1;
    } else if (m_view.render_guide->isChecked()) {
        mode = 2;
    } else if (m_view.render_multi->isChecked()) {
        mode = 3;
    }
    renderProps.insert(QStringLiteral("rendermode"), QString::number(mode));
    renderProps.insert(QStringLiteral("renderstartguide"), QString::number(m_view.guide_start->currentIndex()));
    renderProps.insert(QStringLiteral("renderendguide"), QString::number(m_view.guide_end->currentIndex()));
    int export_audio = 0;

    renderProps.insert(QStringLiteral("renderexportaudio"), QString::number(export_audio));
    renderProps.insert(QStringLiteral("renderrescale"), QString::number(static_cast<int>(m_view.rescale->isChecked())));
    renderProps.insert(QStringLiteral("renderrescalewidth"), QString::number(m_view.rescale_width->value()));
    renderProps.insert(QStringLiteral("renderrescaleheight"), QString::number(m_view.rescale_height->value()));

    // for backward compatibility, remove ones we have a doc version > 1.04
    renderProps.insert(QStringLiteral("rendertcoverlay"), QString::number(static_cast<int>(!m_view.tc_type->currentData().toString().isEmpty())));

    renderProps.insert(QStringLiteral("rendertctype"), QString::number(m_view.tc_type->currentIndex() - 1));
    renderProps.insert(QStringLiteral("renderratio"), QString::number(static_cast<int>(m_view.rescale_keep->isChecked())));
    renderProps.insert(QStringLiteral("renderplay"), QString::number(static_cast<int>(m_view.play_after->isChecked())));
    renderProps.insert(QStringLiteral("rendertwopass"), QString::number(static_cast<int>(m_view.checkTwoPass->isChecked())));
    renderProps.insert(QStringLiteral("renderspeed"), QString::number(m_view.speed->value()));
    renderProps.insert(QStringLiteral("renderpreview"), QString::number(static_cast<int>(m_view.render_at_preview_res->isChecked())));

    emit selectedRenderProfile(renderProps);
}

bool RenderWidget::startWaitingRenderJobs()
{
    m_blockProcessing = true;
#ifdef Q_OS_WIN
    const QLatin1String ScriptFormat(".bat");
#else
    const QLatin1String ScriptFormat(".sh");
#endif
    QTemporaryFile tmp(QDir::temp().absoluteFilePath(QStringLiteral("kdenlive-XXXXXX") + ScriptFormat));
    if (!tmp.open()) {
        // Something went wrong
        return false;
    }
    tmp.close();
    QString autoscriptFile = tmp.fileName();
    QFile file(autoscriptFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(KDENLIVE_LOG) << "//////  ERROR writing to file: " << autoscriptFile;
        KMessageBox::error(nullptr, i18n("Cannot write to file %1", autoscriptFile));
        return false;
    }

    QTextStream outStream(&file);
    outStream.setCodec("UTF-8");
#ifndef Q_OS_WIN
    outStream << "#!/bin/sh\n\n";
#endif
    auto *item = static_cast<RenderJobItem *>(m_view.running_jobs->topLevelItem(0));
    while (item != nullptr) {
        if (item->status() == WAITINGJOB) {
            // Add render process for item
            const QString params = item->data(1, ParametersRole).toStringList().join(QLatin1Char(' '));
            outStream << '\"' << m_renderer << "\" " << params << '\n';
        }
        item = static_cast<RenderJobItem *>(m_view.running_jobs->itemBelow(item));
    }
// erase itself when rendering is finished
#ifndef Q_OS_WIN
    outStream << "rm \"" << autoscriptFile << "\"\n";
#else
    outStream << "del \"" << autoscriptFile << "\"\n";
#endif
    if (file.error() != QFile::NoError) {
        KMessageBox::error(nullptr, i18n("Cannot write to file %1", autoscriptFile));
        file.close();
        m_blockProcessing = false;
        return false;
    }
    file.close();
    QFile::setPermissions(autoscriptFile, file.permissions() | QFile::ExeUser);
    QProcess::startDetached(autoscriptFile, QStringList());
    return true;
}

void RenderWidget::slotPlayRendering(QTreeWidgetItem *item, int)
{
    auto *renderItem = static_cast<RenderJobItem *>(item);
    if (renderItem->status() != FINISHEDJOB) {
        return;
    }
    new KRun(QUrl::fromLocalFile(item->text(1)), this);
}

void RenderWidget::errorMessage(RenderError type, const QString &message)
{
    QString fullMessage;
    m_errorMessages.insert(type, message);
    QMapIterator<int, QString> i(m_errorMessages);
    while (i.hasNext()) {
        i.next();
        if (!i.value().isEmpty()) {
            if (!fullMessage.isEmpty()) {
                fullMessage.append(QLatin1Char('\n'));
            }
            fullMessage.append(i.value());
        }
    }
    if (!fullMessage.isEmpty()) {
        m_view.infoMessage->setMessageType(KMessageWidget::Warning);
        m_view.infoMessage->setText(fullMessage);
        m_view.infoMessage->show();
    } else {
        m_view.infoMessage->hide();
    }
}

void RenderWidget::updateProxyConfig(bool enable)
{
    m_view.proxy_render->setVisible(enable);
}

void RenderWidget::setRescaleEnabled(bool enable)
{
    for (int i = 0; i < m_view.rescale_box->layout()->count(); ++i) {
        if (m_view.rescale_box->itemAt(i)->widget()) {
            m_view.rescale_box->itemAt(i)->widget()->setEnabled(enable);
        }
    }
}

void RenderWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        switch (m_view.tabWidget->currentIndex()) {
        case Tabs::RenderTab:
            if (m_view.start_job->isEnabled()) {
                slotStartCurrentJob();
            }
            break;
        case Tabs::ScriptsTab:
            if (m_view.start_script->isEnabled()) {
                slotStartScript();
            }
            break;
        default:
            if (m_view.buttonRender->isEnabled()) {
                slotPrepareExport();
            }
            break;
        }
    } else if (e->key() == Qt::Key_Delete) {
        // If in Scripts tab, let Del key invoke DeleteScript
        if (m_view.tabWidget->currentIndex() == Tabs::ScriptsTab) {
            if (m_view.delete_script->isEnabled()) {
                slotDeleteScript();
            }
        } else {
            QDialog::keyPressEvent(e);
        }
    } else {
        QDialog::keyPressEvent(e);
    }
}

void RenderWidget::adjustSpeed(int speedIndex)
{
    std::unique_ptr<RenderPresetModel> &profile = RenderPresetRepository::get()->getPreset(m_currentProfile);
    if (profile) {
        QStringList speeds = profile->speeds();
        if (speedIndex < speeds.count()) {
            m_view.speed->setToolTip(i18n("Codec speed parameters:\n%1", speeds.at(speedIndex)));
        }
    }
    refreshParams();
}

void RenderWidget::prepareJobContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *nd = m_view.running_jobs->itemAt(pos);
    RenderJobItem *renderItem = nullptr;
    if (nd) {
        renderItem = static_cast<RenderJobItem *>(nd);
    }
    if (!renderItem) {
        return;
    }
    if (renderItem->status() != FINISHEDJOB) {
        return;
    }
    QMenu menu(this);
    QAction *newAct = new QAction(i18n("Add to current project"), this);
    connect(newAct, &QAction::triggered, [&, renderItem]() {
        pCore->bin()->slotAddClipToProject(QUrl::fromLocalFile(renderItem->text(1)));
    });
    menu.addAction(newAct);
    
    menu.exec(m_view.running_jobs->mapToGlobal(pos));
}

void RenderWidget::resetRenderPath(const QString &path)
{
    QString extension;
    if (RenderPresetRepository::get()->presetExists(m_currentProfile)) {
        std::unique_ptr<RenderPresetModel> &profile = RenderPresetRepository::get()->getPreset(m_currentProfile);
        extension = profile->extension();
    } else {
        extension = m_view.out_file->url().toLocalFile().section(QLatin1Char('.'), -1);
    }
    QString fileName = QDir(pCore->currentDoc()->projectDataFolder()).absoluteFilePath(path + extension);
    QString url = filenameWithExtension(QUrl::fromLocalFile(fileName), extension).toLocalFile();
    if (QFileInfo(url).isRelative()) {
        url.prepend(pCore->currentDoc()->documentRoot());
    }
    m_view.out_file->setUrl(QUrl::fromLocalFile(url));
    QMap<QString, QString> renderProps;
    renderProps.insert(QStringLiteral("renderurl"), url);
    emit selectedRenderProfile(renderProps);
}
