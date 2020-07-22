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

#include "renderwidget.h"
#include "bin/projectitemmodel.h"
#include "bin/bin.h"
#include "core.h"
#include "dialogs/profilesdialog.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "project/projectmanager.h"
#include "timecode.h"
#include "ui_saveprofile_ui.h"
#include "xml/xml.hpp"

#include "klocalizedstring.h"
#include <KColorScheme>
#include <KIO/DesktopExecParser>
#include <KMessageBox>
#include <KNotification>
#include <KRun>
#include <kio_version.h>
#include <knotifications_version.h>
#include <kns3/downloaddialog.h>

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
#include <QMimeDatabase>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QThread>
#include <QTreeWidgetItem>
#include <qglobal.h>
#include <qstring.h>
#include <QMenu>

#ifdef KF5_USE_PURPOSE
#include <Purpose/AlternativesModel>
#include <PurposeWidgets/Menu>
#endif

#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

// Render profiles roles
enum {
    GroupRole = Qt::UserRole,
    ExtensionRole,
    StandardRole,
    RenderRole,
    ParamsRole,
    EditableRole,
    ExtraRole,
    BitratesRole,
    DefaultBitrateRole,
    AudioBitratesRole,
    DefaultAudioBitrateRole,
    SpeedsRole,
    FieldRole,
    ErrorRole
};

// Render job roles
const int ParametersRole = Qt::UserRole + 1;
const int TimeRole = Qt::UserRole + 2;
const int ProgressRole = Qt::UserRole + 3;
const int ExtraInfoRole = Qt::UserRole + 5;

// Running job status
enum JOBSTATUS { WAITINGJOB = 0, STARTINGJOB, RUNNINGJOB, FINISHEDJOB, FAILEDJOB, ABORTEDJOB };

static QStringList acodecsList;
static QStringList vcodecsList;
static QStringList supportedFormats;

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
        setData(1, Qt::UserRole, i18n("Waiting..."));
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

    setWindowTitle(i18n("Rendering"));
    m_view.buttonDelete->setIconSize(iconSize);
    m_view.buttonEdit->setIconSize(iconSize);
    m_view.buttonSave->setIconSize(iconSize);
    m_view.buttonFavorite->setIconSize(iconSize);
    m_view.buttonDownload->setIconSize(iconSize);

    m_view.buttonDelete->setIcon(QIcon::fromTheme(QStringLiteral("trash-empty")));
    m_view.buttonDelete->setToolTip(i18n("Delete profile"));
    m_view.buttonDelete->setEnabled(false);

    m_view.buttonEdit->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
    m_view.buttonEdit->setToolTip(i18n("Edit profile"));
    m_view.buttonEdit->setEnabled(false);

    m_view.buttonSave->setIcon(QIcon::fromTheme(QStringLiteral("document-new")));
    m_view.buttonSave->setToolTip(i18n("Create new profile"));

    m_view.hide_log->setIcon(QIcon::fromTheme(QStringLiteral("go-down")));

    m_view.buttonFavorite->setIcon(QIcon::fromTheme(QStringLiteral("favorite")));
    m_view.buttonFavorite->setToolTip(i18n("Copy profile to favorites"));

    m_view.buttonDownload->setIcon(QIcon::fromTheme(QStringLiteral("edit-download")));
    m_view.buttonDownload->setToolTip(i18n("Download New Render Profiles..."));

    m_view.out_file->button()->setToolTip(i18n("Select output destination"));
    m_view.advanced_params->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5);

    m_view.optionsGroup->setVisible(m_view.options->isChecked());
    connect(m_view.options, &QAbstractButton::toggled, m_view.optionsGroup, &QWidget::setVisible);
    m_view.videoLabel->setVisible(m_view.options->isChecked());
    connect(m_view.options, &QAbstractButton::toggled, m_view.videoLabel, &QWidget::setVisible);
    m_view.video->setVisible(m_view.options->isChecked());
    connect(m_view.options, &QAbstractButton::toggled, m_view.video, &QWidget::setVisible);
    m_view.audioLabel->setVisible(m_view.options->isChecked());
    connect(m_view.options, &QAbstractButton::toggled, m_view.audioLabel, &QWidget::setVisible);
    m_view.audio->setVisible(m_view.options->isChecked());
    connect(m_view.options, &QAbstractButton::toggled, m_view.audio, &QWidget::setVisible);
    connect(m_view.quality, &QAbstractSlider::valueChanged, this, &RenderWidget::adjustAVQualities);
    connect(m_view.video, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderWidget::adjustQuality);
    connect(m_view.speed, &QAbstractSlider::valueChanged, this, &RenderWidget::adjustSpeed);

    m_view.buttonRender->setEnabled(false);
    m_view.buttonGenerateScript->setEnabled(false);
    setRescaleEnabled(false);
    m_view.guides_box->setVisible(false);
    m_view.open_dvd->setVisible(false);
    m_view.create_chapter->setVisible(false);
    m_view.open_browser->setVisible(false);
    m_view.error_box->setVisible(false);
    m_view.tc_type->setEnabled(false);
    m_view.checkTwoPass->setEnabled(false);
    m_view.proxy_render->setHidden(!enableProxy);
    connect(m_view.proxy_render, &QCheckBox::toggled, this, &RenderWidget::slotProxyWarn);
    KColorScheme scheme(palette().currentColorGroup(), KColorScheme::Window);
    QColor bg = scheme.background(KColorScheme::NegativeBackground).color();
    m_view.errorBox->setStyleSheet(
        QStringLiteral("QGroupBox { background-color: rgb(%1, %2, %3); border-radius: 5px;}; ").arg(bg.red()).arg(bg.green()).arg(bg.blue()));
    int height = QFontInfo(font()).pixelSize();
    m_view.errorIcon->setPixmap(QIcon::fromTheme(QStringLiteral("dialog-warning")).pixmap(height, height));
    m_view.errorBox->setHidden(true);

    m_infoMessage = new KMessageWidget;
    m_view.info->addWidget(m_infoMessage);
    m_infoMessage->setCloseButtonVisible(false);
    m_infoMessage->hide();

    m_jobInfoMessage = new KMessageWidget;
    m_view.jobInfo->addWidget(m_jobInfoMessage);
    m_jobInfoMessage->setCloseButtonVisible(false);
    m_jobInfoMessage->hide();

    m_view.encoder_threads->setMinimum(0);
    m_view.encoder_threads->setMaximum(QThread::idealThreadCount());
    m_view.encoder_threads->setToolTip(i18n("Encoding threads (0 is automatic)"));
    m_view.encoder_threads->setValue(KdenliveSettings::encodethreads());
    connect(m_view.encoder_threads, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderWidget::slotUpdateEncodeThreads);

    m_view.rescale_keep->setChecked(KdenliveSettings::rescalekeepratio());
    connect(m_view.rescale_width, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderWidget::slotUpdateRescaleWidth);
    connect(m_view.rescale_height, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderWidget::slotUpdateRescaleHeight);
    m_view.rescale_keep->setIcon(QIcon::fromTheme(QStringLiteral("edit-link")));
    m_view.rescale_keep->setToolTip(i18n("Preserve aspect ratio"));
    connect(m_view.rescale_keep, &QAbstractButton::clicked, this, &RenderWidget::slotSwitchAspectRatio);

    connect(m_view.buttonRender, SIGNAL(clicked()), this, SLOT(slotPrepareExport()));
    connect(m_view.buttonGenerateScript, &QAbstractButton::clicked, this, &RenderWidget::slotGenerateScript);

    m_view.abort_job->setEnabled(false);
    m_view.start_script->setEnabled(false);
    m_view.delete_script->setEnabled(false);

    connect(m_view.export_audio, &QCheckBox::stateChanged, this, &RenderWidget::slotUpdateAudioLabel);
    m_view.export_audio->setCheckState(Qt::PartiallyChecked);

    checkCodecs();
    parseProfiles();
    parseScriptFiles();
    m_view.running_jobs->setUniformRowHeights(false);
    m_view.running_jobs->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_view.running_jobs, &QTreeWidget::customContextMenuRequested, this, &RenderWidget::prepareMenu);
    m_view.scripts_list->setUniformRowHeights(false);
    connect(m_view.start_script, &QAbstractButton::clicked, this, &RenderWidget::slotStartScript);
    connect(m_view.delete_script, &QAbstractButton::clicked, this, &RenderWidget::slotDeleteScript);
    connect(m_view.scripts_list, &QTreeWidget::itemSelectionChanged, this, &RenderWidget::slotCheckScript);
    connect(m_view.running_jobs, &QTreeWidget::itemSelectionChanged, this, &RenderWidget::slotCheckJob);
    connect(m_view.running_jobs, &QTreeWidget::itemDoubleClicked, this, &RenderWidget::slotPlayRendering);

    connect(m_view.buttonSave, &QAbstractButton::clicked, this, &RenderWidget::slotSaveProfile);
    connect(m_view.buttonEdit, &QAbstractButton::clicked, this, &RenderWidget::slotEditProfile);
    connect(m_view.buttonDelete, &QAbstractButton::clicked, this, &RenderWidget::slotDeleteProfile);
    connect(m_view.buttonFavorite, &QAbstractButton::clicked, this, &RenderWidget::slotCopyToFavorites);
    connect(m_view.buttonDownload, &QAbstractButton::clicked, this, &RenderWidget::slotDownloadNewRenderProfiles);

    connect(m_view.abort_job, &QAbstractButton::clicked, this, &RenderWidget::slotAbortCurrentJob);
    connect(m_view.start_job, &QAbstractButton::clicked, this, &RenderWidget::slotStartCurrentJob);
    connect(m_view.clean_up, &QAbstractButton::clicked, this, &RenderWidget::slotCLeanUpJobs);
    connect(m_view.hide_log, &QAbstractButton::clicked, this, &RenderWidget::slotHideLog);

    connect(m_view.buttonClose, &QAbstractButton::clicked, this, &QWidget::hide);
    connect(m_view.buttonClose2, &QAbstractButton::clicked, this, &QWidget::hide);
    connect(m_view.buttonClose3, &QAbstractButton::clicked, this, &QWidget::hide);
    connect(m_view.rescale, &QAbstractButton::toggled, this, &RenderWidget::setRescaleEnabled);
    connect(m_view.out_file, &KUrlRequester::textChanged, this, static_cast<void (RenderWidget::*)()>(&RenderWidget::slotUpdateButtons));
    connect(m_view.out_file, &KUrlRequester::urlSelected, this, static_cast<void (RenderWidget::*)(const QUrl &)>(&RenderWidget::slotUpdateButtons));

    connect(m_view.formats, &QTreeWidget::currentItemChanged, this, &RenderWidget::refreshParams);
    connect(m_view.formats, &QTreeWidget::itemDoubleClicked, this, &RenderWidget::slotEditItem);

    connect(m_view.render_guide, &QAbstractButton::clicked, this, &RenderWidget::slotUpdateGuideBox);
    connect(m_view.render_zone, &QAbstractButton::clicked, this, &RenderWidget::slotUpdateGuideBox);
    connect(m_view.render_full, &QAbstractButton::clicked, this, &RenderWidget::slotUpdateGuideBox);

    connect(m_view.guide_end, static_cast<void (KComboBox::*)(int)>(&KComboBox::activated), this, &RenderWidget::slotCheckStartGuidePosition);
    connect(m_view.guide_start, static_cast<void (KComboBox::*)(int)>(&KComboBox::activated), this, &RenderWidget::slotCheckEndGuidePosition);

    connect(m_view.tc_overlay, &QAbstractButton::toggled, m_view.tc_type, &QWidget::setEnabled);

    // m_view.splitter->setStretchFactor(1, 5);
    // m_view.splitter->setStretchFactor(0, 2);

    m_view.out_file->setAcceptMode(QFileDialog::AcceptSave);
    m_view.out_file->setMode(KFile::File);
    m_view.out_file->setFocusPolicy(Qt::ClickFocus);

    m_jobsDelegate = new RenderViewDelegate(this);
    m_view.running_jobs->setHeaderLabels(QStringList() << QString() << i18n("File"));
    m_view.running_jobs->setItemDelegate(m_jobsDelegate);

    QHeaderView *header = m_view.running_jobs->header();
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    header->resizeSection(0, size + 4);
    header->setSectionResizeMode(1, QHeaderView::Interactive);

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
            m_renderer = QStringLiteral("kdenlive_render");
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
    m_view.parallel_process->setChecked(KdenliveSettings::parallelrender());
    connect(m_view.parallel_process, &QCheckBox::stateChanged, [](int state) { KdenliveSettings::setParallelrender(state == Qt::Checked); });
    if (KdenliveSettings::gpu_accel()) {
        // Disable parallel rendering for movit
        m_view.parallel_process->setEnabled(false);
    }
    m_view.field_order->setEnabled(false);
    connect(m_view.scanning_list, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) { m_view.field_order->setEnabled(index == 2); });
    refreshView();
    focusFirstVisibleItem();
    adjustSize();
}

void RenderWidget::slotShareActionFinished(const QJsonObject &output, int error, const QString &message)
{
#ifdef KF5_USE_PURPOSE
    m_jobInfoMessage->hide();
    if (error) {
        KMessageBox::error(this, i18n("There was a problem sharing the document: %1", message), i18n("Share"));
    } else {
        const QString url = output["url"].toString();
        if (url.isEmpty()) {
            m_jobInfoMessage->setMessageType(KMessageWidget::Positive);
            m_jobInfoMessage->setText(i18n("Document shared successfully"));
            m_jobInfoMessage->show();
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
    m_view.running_jobs->blockSignals(true);
    m_view.scripts_list->blockSignals(true);
    m_view.running_jobs->clear();
    m_view.scripts_list->clear();
    delete m_jobsDelegate;
    delete m_scriptsDelegate;
    delete m_infoMessage;
    delete m_jobInfoMessage;
}

void RenderWidget::slotEditItem(QTreeWidgetItem *item)
{
    if (item->parent() == nullptr) {
        // This is a top level item - group - don't edit
        return;
    }
    const QString edit = item->data(0, EditableRole).toString();
    if (edit.isEmpty() || !edit.endsWith(QLatin1String("customprofiles.xml"))) {
        slotSaveProfile();
    } else {
        slotEditProfile();
    }
}

void RenderWidget::showInfoPanel()
{
    if (m_view.advanced_params->isVisible()) {
        m_view.advanced_params->setVisible(false);
        KdenliveSettings::setShowrenderparams(false);
    } else {
        m_view.advanced_params->setVisible(true);
        KdenliveSettings::setShowrenderparams(true);
    }
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

void RenderWidget::slotUpdateGuideBox()
{
    m_view.guides_box->setVisible(m_view.render_guide->isChecked());
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
    double projectDuration = GenTime(pCore->projectDuration() - TimelineModel::seekDuration - 2, pCore->getCurrentFps()).ms() / 1000;
    QVariant startData = m_view.guide_start->currentData();
    QVariant endData = m_view.guide_end->currentData();
    m_view.guide_start->clear();
    m_view.guide_end->clear();
    if (auto ptr = m_guidesModel.lock()) {
        QList<CommentedTime> markers = ptr->getAllMarkers();
        double fps = pCore->getCurrentProfile()->fps();
        m_view.render_guide->setEnabled(!markers.isEmpty());
        if (!markers.isEmpty()) {
            m_view.guide_start->addItem(i18n("Beginning"), "0");
            m_view.create_chapter->setEnabled(true);
            for (const auto &marker : qAsConst(markers)) {
                GenTime pos = marker.time();
                const QString guidePos = Timecode::getStringTimecode(pos.frames(fps), fps);
                m_view.guide_start->addItem(marker.comment() + QLatin1Char('/') + guidePos, pos.seconds());
                m_view.guide_end->addItem(marker.comment() + QLatin1Char('/') + guidePos, pos.seconds());
            }
            m_view.guide_end->addItem(i18n("End"), QString::number(projectDuration));
            if (!startData.isNull()) {
                int ix = qMax(0, m_view.guide_start->findData(startData));
                m_view.guide_start->setCurrentIndex(ix);
            }
            if (!endData.isNull()) {
                int ix = qMax(m_view.guide_start->currentIndex() + 1, m_view.guide_end->findData(endData));
                m_view.guide_end->setCurrentIndex(ix);
            }
        }
    } else {
        m_view.render_guide->setEnabled(false);
        m_view.create_chapter->setEnabled(false);
    }
}

/**
 * Will be called when the user selects an output file via the file dialog.
 * File extension will be added automatically.
 */
void RenderWidget::slotUpdateButtons(const QUrl &url)
{
    if (m_view.out_file->url().isEmpty()) {
        m_view.buttonGenerateScript->setEnabled(false);
        m_view.buttonRender->setEnabled(false);
    } else {
        updateButtons(); // This also checks whether the selected format is available
    }
    if (url.isValid()) {
        QTreeWidgetItem *item = m_view.formats->currentItem();
        if ((item == nullptr) || (item->parent() == nullptr)) { // categories have no parent
            m_view.buttonRender->setEnabled(false);
            m_view.buttonGenerateScript->setEnabled(false);
            return;
        }
        const QString extension = item->data(0, ExtensionRole).toString();
        m_view.out_file->setUrl(filenameWithExtension(url, extension));
    }
}

/**
 * Will be called when the user changes the output file path in the text line.
 * File extension must NOT be added, would make editing impossible!
 */
void RenderWidget::slotUpdateButtons()
{
    if (m_view.out_file->url().isEmpty()) {
        m_view.buttonRender->setEnabled(false);
        m_view.buttonGenerateScript->setEnabled(false);
    } else {
        updateButtons(); // This also checks whether the selected format is available
    }
}

void RenderWidget::slotSaveProfile()
{
    Ui::SaveProfile_UI ui;
    QPointer<QDialog> d = new QDialog(this);
    ui.setupUi(d);

    QString customGroup;
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    QStringList arguments = m_view.advanced_params->toPlainText().split(' ', QString::SkipEmptyParts);
#else
    QStringList arguments = m_view.advanced_params->toPlainText().split(' ', Qt::SkipEmptyParts);
#endif
    if (!arguments.isEmpty()) {
        ui.parameters->setText(arguments.join(QLatin1Char(' ')));
    }
    ui.profile_name->setFocus();
    QTreeWidgetItem *item = m_view.formats->currentItem();
    if ((item != nullptr) && (item->parent() != nullptr)) { // not a category
        // Duplicate current item settings
        customGroup = item->parent()->text(0);
        ui.extension->setText(item->data(0, ExtensionRole).toString());
        if (ui.parameters->toPlainText().contains(QStringLiteral("%bitrate")) || ui.parameters->toPlainText().contains(QStringLiteral("%quality"))) {
            if (ui.parameters->toPlainText().contains(QStringLiteral("%quality"))) {
                ui.vbitrates_label->setText(i18n("Qualities"));
                ui.default_vbitrate_label->setText(i18n("Default quality"));
            } else {
                ui.vbitrates_label->setText(i18n("Bitrates"));
                ui.default_vbitrate_label->setText(i18n("Default bitrate"));
            }
            if (item->data(0, BitratesRole).canConvert(QVariant::StringList) && (item->data(0, BitratesRole).toStringList().count() != 0)) {
                QStringList bitrates = item->data(0, BitratesRole).toStringList();
                ui.vbitrates_list->setText(bitrates.join(QLatin1Char(',')));
                if (item->data(0, DefaultBitrateRole).canConvert(QVariant::String)) {
                    ui.default_vbitrate->setValue(item->data(0, DefaultBitrateRole).toInt());
                }
            }
        } else {
            ui.vbitrates->setHidden(true);
        }
        if (ui.parameters->toPlainText().contains(QStringLiteral("%audiobitrate")) || ui.parameters->toPlainText().contains(QStringLiteral("%audioquality"))) {
            if (ui.parameters->toPlainText().contains(QStringLiteral("%audioquality"))) {
                ui.abitrates_label->setText(i18n("Qualities"));
                ui.default_abitrate_label->setText(i18n("Default quality"));
            } else {
                ui.abitrates_label->setText(i18n("Bitrates"));
                ui.default_abitrate_label->setText(i18n("Default bitrate"));
            }
            if ((item != nullptr) && item->data(0, AudioBitratesRole).canConvert(QVariant::StringList) &&
                (item->data(0, AudioBitratesRole).toStringList().count() != 0)) {
                QStringList bitrates = item->data(0, AudioBitratesRole).toStringList();
                ui.abitrates_list->setText(bitrates.join(QLatin1Char(',')));
                if (item->data(0, DefaultAudioBitrateRole).canConvert(QVariant::String)) {
                    ui.default_abitrate->setValue(item->data(0, DefaultAudioBitrateRole).toInt());
                }
            }
        } else {
            ui.abitrates->setHidden(true);
        }

        if (item->data(0, SpeedsRole).canConvert(QVariant::StringList) && (item->data(0, SpeedsRole).toStringList().count() != 0)) {
            QStringList speeds = item->data(0, SpeedsRole).toStringList();
            ui.speeds_list->setText(speeds.join('\n'));
        }
    }

    if (customGroup.isEmpty()) {
        customGroup = i18nc("Group Name", "Custom");
    }
    ui.group_name->setText(customGroup);

    if (d->exec() == QDialog::Accepted && !ui.profile_name->text().simplified().isEmpty()) {
        QString newProfileName = ui.profile_name->text().simplified();
        QString newGroupName = ui.group_name->text().simplified();
        if (newGroupName.isEmpty()) {
            newGroupName = i18nc("Group Name", "Custom");
        }

        QDomDocument doc;
        QDomElement profileElement = doc.createElement(QStringLiteral("profile"));
        profileElement.setAttribute(QStringLiteral("name"), newProfileName);
        profileElement.setAttribute(QStringLiteral("category"), newGroupName);
        profileElement.setAttribute(QStringLiteral("extension"), ui.extension->text().simplified());
        QString args = ui.parameters->toPlainText().simplified();
        profileElement.setAttribute(QStringLiteral("args"), args);
        if (args.contains(QStringLiteral("%bitrate"))) {
            // profile has a variable bitrate
            profileElement.setAttribute(QStringLiteral("defaultbitrate"), QString::number(ui.default_vbitrate->value()));
            profileElement.setAttribute(QStringLiteral("bitrates"), ui.vbitrates_list->text());
        } else if (args.contains(QStringLiteral("%quality"))) {
            profileElement.setAttribute(QStringLiteral("defaultquality"), QString::number(ui.default_vbitrate->value()));
            profileElement.setAttribute(QStringLiteral("qualities"), ui.vbitrates_list->text());
        }

        if (args.contains(QStringLiteral("%audiobitrate"))) {
            // profile has a variable bitrate
            profileElement.setAttribute(QStringLiteral("defaultaudiobitrate"), QString::number(ui.default_abitrate->value()));
            profileElement.setAttribute(QStringLiteral("audiobitrates"), ui.abitrates_list->text());
        } else if (args.contains(QStringLiteral("%audioquality"))) {
            // profile has a variable bitrate
            profileElement.setAttribute(QStringLiteral("defaultaudioquality"), QString::number(ui.default_abitrate->value()));
            profileElement.setAttribute(QStringLiteral("audioqualities"), ui.abitrates_list->text());
        }
        QString speeds_list_str = ui.speeds_list->toPlainText();
        if (!speeds_list_str.isEmpty()) {
            profileElement.setAttribute(QStringLiteral("speeds"), speeds_list_str.replace('\n', ';').simplified());
        }

        doc.appendChild(profileElement);
        saveProfile(doc.documentElement());

        parseProfiles();
    }
    delete d;
}

bool RenderWidget::saveProfile(QDomElement newprofile)
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/export/"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    QDomDocument doc;
    QFile file(dir.absoluteFilePath(QStringLiteral("customprofiles.xml")));
    doc.setContent(&file, false);
    file.close();
    QDomElement documentElement;
    QDomElement profiles = doc.documentElement();
    if (profiles.isNull() || profiles.tagName() != QLatin1String("profiles")) {
        doc.clear();
        profiles = doc.createElement(QStringLiteral("profiles"));
        profiles.setAttribute(QStringLiteral("version"), 1);
        doc.appendChild(profiles);
    }
    int version = profiles.attribute(QStringLiteral("version"), nullptr).toInt();
    if (version < 1) {
        doc.clear();
        profiles = doc.createElement(QStringLiteral("profiles"));
        profiles.setAttribute(QStringLiteral("version"), 1);
        doc.appendChild(profiles);
    }

    QDomNodeList profilelist = doc.elementsByTagName(QStringLiteral("profile"));
    QString newProfileName = newprofile.attribute(QStringLiteral("name"));
    // Check existing profiles
    QStringList existingProfileNames;
    int i = 0;
    while (!profilelist.item(i).isNull()) {
        documentElement = profilelist.item(i).toElement();
        QString profileName = documentElement.attribute(QStringLiteral("name"));
        existingProfileNames << profileName;
        i++;
    }
    // Check if a profile with that same name already exists
    bool ok;
    while (existingProfileNames.contains(newProfileName)) {
        QString updatedProfileName = QInputDialog::getText(this, i18n("Profile already exists"),
                                                           i18n("This profile name already exists. Change the name if you do not want to overwrite it."),
                                                           QLineEdit::Normal, newProfileName, &ok);
        if (!ok) {
            return false;
        }
        if (updatedProfileName == newProfileName) {
            // remove previous profile
            profiles.removeChild(profilelist.item(existingProfileNames.indexOf(newProfileName)));
            break;
        } else {
            newProfileName = updatedProfileName;
            newprofile.setAttribute(QStringLiteral("name"), newProfileName);
        }
    }

    profiles.appendChild(newprofile);

    // QCString save = doc.toString().utf8();

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        KMessageBox::sorry(this, i18n("Unable to write to file %1", dir.absoluteFilePath("customprofiles.xml")));
        return false;
    }
    QTextStream out(&file);
    out << doc.toString();
    if (file.error() != QFile::NoError) {
        KMessageBox::error(this, i18n("Cannot write to file %1", dir.absoluteFilePath("customprofiles.xml")));
        file.close();
        return false;
    }
    file.close();
    return true;
}

void RenderWidget::slotCopyToFavorites()
{
    QTreeWidgetItem *item = m_view.formats->currentItem();
    if ((item == nullptr) || (item->parent() == nullptr)) {
        return;
    }

    QString params = item->data(0, ParamsRole).toString();
    QString extension = item->data(0, ExtensionRole).toString();
    QString currentProfile = item->text(0);
    QDomDocument doc;
    QDomElement profileElement = doc.createElement(QStringLiteral("profile"));
    profileElement.setAttribute(QStringLiteral("name"), currentProfile);
    profileElement.setAttribute(QStringLiteral("category"), i18nc("Category Name", "Custom"));
    profileElement.setAttribute(QStringLiteral("destinationid"), QStringLiteral("favorites"));
    profileElement.setAttribute(QStringLiteral("extension"), extension);
    profileElement.setAttribute(QStringLiteral("args"), params);
    if (params.contains(QStringLiteral("%bitrate"))) {
        // profile has a variable bitrate
        profileElement.setAttribute(QStringLiteral("defaultbitrate"), item->data(0, DefaultBitrateRole).toString());
        profileElement.setAttribute(QStringLiteral("bitrates"), item->data(0, BitratesRole).toStringList().join(QLatin1Char(',')));
    } else if (params.contains(QStringLiteral("%quality"))) {
        profileElement.setAttribute(QStringLiteral("defaultquality"), item->data(0, DefaultBitrateRole).toString());
        profileElement.setAttribute(QStringLiteral("qualities"), item->data(0, BitratesRole).toStringList().join(QLatin1Char(',')));
    }
    if (params.contains(QStringLiteral("%audiobitrate"))) {
        // profile has a variable bitrate
        profileElement.setAttribute(QStringLiteral("defaultaudiobitrate"), item->data(0, DefaultAudioBitrateRole).toString());
        profileElement.setAttribute(QStringLiteral("audiobitrates"), item->data(0, AudioBitratesRole).toStringList().join(QLatin1Char(',')));
    } else if (params.contains(QStringLiteral("%audioquality"))) {
        // profile has a variable bitrate
        profileElement.setAttribute(QStringLiteral("defaultaudioquality"), item->data(0, DefaultAudioBitrateRole).toString());
        profileElement.setAttribute(QStringLiteral("audioqualities"), item->data(0, AudioBitratesRole).toStringList().join(QLatin1Char(',')));
    }
    if (item->data(0, SpeedsRole).canConvert(QVariant::StringList) && (item->data(0, SpeedsRole).toStringList().count() != 0)) {
        // profile has a variable speed
        profileElement.setAttribute(QStringLiteral("speeds"), item->data(0, SpeedsRole).toStringList().join(QLatin1Char(';')));
    }
    doc.appendChild(profileElement);
    if (saveProfile(doc.documentElement())) {
        parseProfiles(profileElement.attribute(QStringLiteral("name")));
    }
}

void RenderWidget::slotDownloadNewRenderProfiles()
{
    if (getNewStuff(QStringLiteral(":data/kdenlive_renderprofiles.knsrc")) > 0) {
        reloadProfiles();
    }
}

int RenderWidget::getNewStuff(const QString &configFile)
{
    KNS3::Entry::List entries;
    QPointer<KNS3::DownloadDialog> dialog = new KNS3::DownloadDialog(configFile);
    if (dialog->exec() != 0) {
        entries = dialog->changedEntries();
    }
    for (const KNS3::Entry &entry : qAsConst(entries)) {
        if (entry.status() == KNS3::Entry::Installed) {
            qCDebug(KDENLIVE_LOG) << "// Installed files: " << entry.installedFiles();
        }
    }
    delete dialog;
    return entries.size();
}

void RenderWidget::slotEditProfile()
{
    QTreeWidgetItem *item = m_view.formats->currentItem();
    if ((item == nullptr) || (item->parent() == nullptr)) {
        return;
    }
    QString params = item->data(0, ParamsRole).toString();

    Ui::SaveProfile_UI ui;
    QPointer<QDialog> d = new QDialog(this);
    ui.setupUi(d);

    QString customGroup = item->parent()->text(0);
    if (customGroup.isEmpty()) {
        customGroup = i18nc("Group Name", "Custom");
    }
    ui.group_name->setText(customGroup);

    ui.profile_name->setText(item->text(0));
    ui.extension->setText(item->data(0, ExtensionRole).toString());
    ui.parameters->setText(params);
    ui.profile_name->setFocus();
    if (params.contains(QStringLiteral("%bitrate")) || ui.parameters->toPlainText().contains(QStringLiteral("%quality"))) {
        if (params.contains(QStringLiteral("%quality"))) {
            ui.vbitrates_label->setText(i18n("Qualities"));
            ui.default_vbitrate_label->setText(i18n("Default quality"));
        } else {
            ui.vbitrates_label->setText(i18n("Bitrates"));
            ui.default_vbitrate_label->setText(i18n("Default bitrate"));
        }
        if (item->data(0, BitratesRole).canConvert(QVariant::StringList) && (item->data(0, BitratesRole).toStringList().count() != 0)) {
            QStringList bitrates = item->data(0, BitratesRole).toStringList();
            ui.vbitrates_list->setText(bitrates.join(QLatin1Char(',')));
            if (item->data(0, DefaultBitrateRole).canConvert(QVariant::String)) {
                ui.default_vbitrate->setValue(item->data(0, DefaultBitrateRole).toInt());
            }
        }
    } else {
        ui.vbitrates->setHidden(true);
    }

    if (params.contains(QStringLiteral("%audiobitrate")) || ui.parameters->toPlainText().contains(QStringLiteral("%audioquality"))) {
        if (params.contains(QStringLiteral("%audioquality"))) {
            ui.abitrates_label->setText(i18n("Qualities"));
            ui.default_abitrate_label->setText(i18n("Default quality"));
        } else {
            ui.abitrates_label->setText(i18n("Bitrates"));
            ui.default_abitrate_label->setText(i18n("Default bitrate"));
        }
        if (item->data(0, AudioBitratesRole).canConvert(QVariant::StringList) && (item->data(0, AudioBitratesRole).toStringList().count() != 0)) {
            QStringList bitrates = item->data(0, AudioBitratesRole).toStringList();
            ui.abitrates_list->setText(bitrates.join(QLatin1Char(',')));
            if (item->data(0, DefaultAudioBitrateRole).canConvert(QVariant::String)) {
                ui.default_abitrate->setValue(item->data(0, DefaultAudioBitrateRole).toInt());
            }
        }
    } else {
        ui.abitrates->setHidden(true);
    }

    if (item->data(0, SpeedsRole).canConvert(QVariant::StringList) && (item->data(0, SpeedsRole).toStringList().count() != 0)) {
        QStringList speeds = item->data(0, SpeedsRole).toStringList();
        ui.speeds_list->setText(speeds.join('\n'));
    }

    d->setWindowTitle(i18n("Edit Profile"));

    if (d->exec() == QDialog::Accepted) {
        slotDeleteProfile(true);
        QString exportFile = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/export/customprofiles.xml");
        QDomDocument doc;
        QFile file(exportFile);
        doc.setContent(&file, false);
        file.close();
        QDomElement documentElement;
        QDomElement profiles = doc.documentElement();

        if (profiles.isNull() || profiles.tagName() != QLatin1String("profiles")) {
            doc.clear();
            profiles = doc.createElement(QStringLiteral("profiles"));
            profiles.setAttribute(QStringLiteral("version"), 1);
            doc.appendChild(profiles);
        }

        int version = profiles.attribute(QStringLiteral("version"), nullptr).toInt();
        if (version < 1) {
            doc.clear();
            profiles = doc.createElement(QStringLiteral("profiles"));
            profiles.setAttribute(QStringLiteral("version"), 1);
            doc.appendChild(profiles);
        }

        QString newProfileName = ui.profile_name->text().simplified();
        QString newGroupName = ui.group_name->text().simplified();
        if (newGroupName.isEmpty()) {
            newGroupName = i18nc("Group Name", "Custom");
        }
        QDomNodeList profilelist = doc.elementsByTagName(QStringLiteral("profile"));
        int i = 0;
        while (!profilelist.item(i).isNull()) {
            // make sure a profile with same name doesn't exist
            documentElement = profilelist.item(i).toElement();
            QString profileName = documentElement.attribute(QStringLiteral("name"));
            if (profileName == newProfileName) {
                // a profile with that same name already exists
                bool ok;
                newProfileName = QInputDialog::getText(this, i18n("Profile already exists"),
                                                       i18n("This profile name already exists. Change the name if you do not want to overwrite it."),
                                                       QLineEdit::Normal, newProfileName, &ok);
                if (!ok) {
                    return;
                }
                if (profileName == newProfileName) {
                    profiles.removeChild(profilelist.item(i));
                    break;
                }
            }
            ++i;
        }

        QDomElement profileElement = doc.createElement(QStringLiteral("profile"));
        profileElement.setAttribute(QStringLiteral("name"), newProfileName);
        profileElement.setAttribute(QStringLiteral("category"), newGroupName);
        profileElement.setAttribute(QStringLiteral("extension"), ui.extension->text().simplified());
        QString args = ui.parameters->toPlainText().simplified();
        profileElement.setAttribute(QStringLiteral("args"), args);
        if (args.contains(QStringLiteral("%bitrate"))) {
            // profile has a variable bitrate
            profileElement.setAttribute(QStringLiteral("defaultbitrate"), QString::number(ui.default_vbitrate->value()));
            profileElement.setAttribute(QStringLiteral("bitrates"), ui.vbitrates_list->text());
        } else if (args.contains(QStringLiteral("%quality"))) {
            profileElement.setAttribute(QStringLiteral("defaultquality"), QString::number(ui.default_vbitrate->value()));
            profileElement.setAttribute(QStringLiteral("qualities"), ui.vbitrates_list->text());
        }
        if (args.contains(QStringLiteral("%audiobitrate"))) {
            // profile has a variable bitrate
            profileElement.setAttribute(QStringLiteral("defaultaudiobitrate"), QString::number(ui.default_abitrate->value()));
            profileElement.setAttribute(QStringLiteral("audiobitrates"), ui.abitrates_list->text());
        } else if (args.contains(QStringLiteral("%audioquality"))) {
            profileElement.setAttribute(QStringLiteral("defaultaudioquality"), QString::number(ui.default_abitrate->value()));
            profileElement.setAttribute(QStringLiteral("audioqualities"), ui.abitrates_list->text());
        }

        QString speeds_list_str = ui.speeds_list->toPlainText();
        if (!speeds_list_str.isEmpty()) {
            // profile has a variable speed
            profileElement.setAttribute(QStringLiteral("speeds"), speeds_list_str.replace('\n', ';').simplified());
        }

        profiles.appendChild(profileElement);

        // QCString save = doc.toString().utf8();
        delete d;
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            KMessageBox::error(this, i18n("Cannot write to file %1", exportFile));
            return;
        }
        QTextStream out(&file);
        out << doc.toString();
        if (file.error() != QFile::NoError) {
            KMessageBox::error(this, i18n("Cannot write to file %1", exportFile));
            file.close();
            return;
        }
        file.close();
        parseProfiles();
    } else {
        delete d;
    }
}

void RenderWidget::slotDeleteProfile(bool dontRefresh)
{
    // TODO: delete a profile installed by KNewStuff the easy way
    /*
    QString edit = m_view.formats->currentItem()->data(EditableRole).toString();
    if (!edit.endsWith(QLatin1String("customprofiles.xml"))) {
        // This is a KNewStuff installed file, process through KNS
        KNS::Engine engine(0);
        if (engine.init("kdenlive_render.knsrc")) {
            KNS::Entry::List entries;
        }
        return;
    }*/
    QTreeWidgetItem *item = m_view.formats->currentItem();
    if ((item == nullptr) || (item->parent() == nullptr)) {
        return;
    }
    QString currentProfile = item->text(0);

    QString exportFile = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/export/customprofiles.xml");
    QDomDocument doc;
    QFile file(exportFile);
    doc.setContent(&file, false);
    file.close();

    QDomElement documentElement;
    QDomNodeList profiles = doc.elementsByTagName(QStringLiteral("profile"));
    int i = 0;
    QString profileName;
    while (!profiles.item(i).isNull()) {
        documentElement = profiles.item(i).toElement();
        profileName = documentElement.attribute(QStringLiteral("name"));
        if (profileName == currentProfile) {
            doc.documentElement().removeChild(profiles.item(i));
            break;
        }
        ++i;
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        KMessageBox::sorry(this, i18n("Unable to write to file %1", exportFile));
        return;
    }
    QTextStream out(&file);
    out << doc.toString();
    if (file.error() != QFile::NoError) {
        KMessageBox::error(this, i18n("Cannot write to file %1", exportFile));
        file.close();
        return;
    }
    file.close();
    if (dontRefresh) {
        return;
    }
    parseProfiles();
    focusFirstVisibleItem();
}

void RenderWidget::updateButtons()
{
    if ((m_view.formats->currentItem() == nullptr) || m_view.formats->currentItem()->isHidden()) {
        m_view.buttonSave->setEnabled(false);
        m_view.buttonDelete->setEnabled(false);
        m_view.buttonEdit->setEnabled(false);
        m_view.buttonRender->setEnabled(false);
        m_view.buttonGenerateScript->setEnabled(false);
    } else {
        m_view.buttonSave->setEnabled(true);
        m_view.buttonRender->setEnabled(m_view.formats->currentItem()->data(0, ErrorRole).isNull());
        m_view.buttonGenerateScript->setEnabled(m_view.formats->currentItem()->data(0, ErrorRole).isNull());
        QString edit = m_view.formats->currentItem()->data(0, EditableRole).toString();
        if (edit.isEmpty() || !edit.endsWith(QLatin1String("customprofiles.xml"))) {
            m_view.buttonDelete->setEnabled(false);
            m_view.buttonEdit->setEnabled(false);
        } else {
            m_view.buttonDelete->setEnabled(true);
            m_view.buttonEdit->setEnabled(true);
        }
    }
}

void RenderWidget::focusFirstVisibleItem(const QString &profile)
{
    QTreeWidgetItem *item = nullptr;
    if (!profile.isEmpty()) {
        QList<QTreeWidgetItem *> items = m_view.formats->findItems(profile, Qt::MatchExactly | Qt::MatchRecursive);
        if (!items.isEmpty()) {
            item = items.constFirst();
        }
    }
    if (!item) {
        // searched profile not found in any category, select 1st available profile
        for (int i = 0; i < m_view.formats->topLevelItemCount(); ++i) {
            item = m_view.formats->topLevelItem(i);
            if (item->childCount() > 0) {
                item = item->child(0);
                break;
            }
        }
    }
    if (item) {
        m_view.formats->setCurrentItem(item);
        item->parent()->setExpanded(true);
        refreshParams();
    }
    updateButtons();
}

void RenderWidget::slotPrepareExport(bool delayedRendering, const QString &scriptPath)
{
    Q_UNUSED(scriptPath)
    if (pCore->projectDuration() < 2) {
        // Empty project, don't attempt to render
        return;
    }
    if (!QFile::exists(KdenliveSettings::rendererpath())) {
        KMessageBox::sorry(this, i18n("Cannot find the melt program required for rendering (part of Mlt)"));
        return;
    }

    if (QFile::exists(m_view.out_file->url().toLocalFile())) {
        if (KMessageBox::warningYesNo(this, i18n("Output file already exists. Do you want to overwrite it?")) != KMessageBox::Yes) {
            return;
        }
    }

    QString chapterFile;
    if (m_view.create_chapter->isChecked()) {
        chapterFile = m_view.out_file->url().toLocalFile() + QStringLiteral(".dvdchapter");
    }

    // mantisbt 1051
    QDir dir(m_view.out_file->url().adjusted(QUrl::RemoveFilename).toLocalFile());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        KMessageBox::sorry(this, i18n("The directory %1, could not be created.\nPlease make sure you have the required permissions.",
                                      m_view.out_file->url().adjusted(QUrl::RemoveFilename).toLocalFile()));
        return;
    }

    prepareRendering(delayedRendering, chapterFile);
}

void RenderWidget::prepareRendering(bool delayedRendering, const QString &chapterFile)
{
    KdenliveDoc *project = pCore->currentDoc();

    // Save rendering profile to document
    QMap<QString, QString> renderProps;
    renderProps.insert(QStringLiteral("rendercategory"), m_view.formats->currentItem()->parent()->text(0));
    renderProps.insert(QStringLiteral("renderprofile"), m_view.formats->currentItem()->text(0));
    renderProps.insert(QStringLiteral("renderurl"), m_view.out_file->url().toLocalFile());
    renderProps.insert(QStringLiteral("renderzone"), QString::number(static_cast<int>(m_view.render_zone->isChecked())));
    renderProps.insert(QStringLiteral("renderguide"), QString::number(static_cast<int>(m_view.render_guide->isChecked())));
    renderProps.insert(QStringLiteral("renderstartguide"), QString::number(m_view.guide_start->currentIndex()));
    renderProps.insert(QStringLiteral("renderendguide"), QString::number(m_view.guide_end->currentIndex()));
    renderProps.insert(QStringLiteral("renderscanning"), QString::number(m_view.scanning_list->currentIndex()));
    renderProps.insert(QStringLiteral("renderfield"), QString::number(m_view.field_order->currentIndex()));
    int export_audio = 0;
    if (m_view.export_audio->checkState() == Qt::Checked) {
        export_audio = 2;
    } else if (m_view.export_audio->checkState() == Qt::Unchecked) {
        export_audio = 1;
    }

    renderProps.insert(QStringLiteral("renderexportaudio"), QString::number(export_audio));
    renderProps.insert(QStringLiteral("renderrescale"), QString::number(static_cast<int>(m_view.rescale->isChecked())));
    renderProps.insert(QStringLiteral("renderrescalewidth"), QString::number(m_view.rescale_width->value()));
    renderProps.insert(QStringLiteral("renderrescaleheight"), QString::number(m_view.rescale_height->value()));
    renderProps.insert(QStringLiteral("rendertcoverlay"), QString::number(static_cast<int>(m_view.tc_overlay->isChecked())));
    renderProps.insert(QStringLiteral("rendertctype"), QString::number(m_view.tc_type->currentIndex()));
    renderProps.insert(QStringLiteral("renderratio"), QString::number(static_cast<int>(m_view.rescale_keep->isChecked())));
    renderProps.insert(QStringLiteral("renderplay"), QString::number(static_cast<int>(m_view.play_after->isChecked())));
    renderProps.insert(QStringLiteral("rendertwopass"), QString::number(static_cast<int>(m_view.checkTwoPass->isChecked())));
    renderProps.insert(QStringLiteral("renderquality"), QString::number(m_view.video->value()));
    renderProps.insert(QStringLiteral("renderaudioquality"), QString::number(m_view.audio->value()));
    renderProps.insert(QStringLiteral("renderspeed"), QString::number(m_view.speed->value()));

    emit selectedRenderProfile(renderProps);

    QString playlistPath;
    QString renderName;

    if (delayedRendering) {
        bool ok;
        renderName = QFileInfo(pCore->currentDoc()->url().toLocalFile()).fileName();
        if (renderName.isEmpty()) {
            renderName = i18n("export") + QStringLiteral(".mlt");
        } else {
            renderName = renderName.section(QLatin1Char('.'), 0, -2);
            renderName.append(QStringLiteral(".mlt"));
        }
        QDir projectFolder(pCore->currentDoc()->projectDataFolder());
        projectFolder.mkpath(QStringLiteral("kdenlive-renderqueue"));
        projectFolder.cd(QStringLiteral("kdenlive-renderqueue"));
        if (projectFolder.exists(renderName)) {
            int ix = 1;
            while (projectFolder.exists(renderName)) {
                if (renderName.contains(QLatin1Char('-'))) {
                    renderName = renderName.section(QLatin1Char('-'), 0, -2);
                } else {
                    renderName = renderName.section(QLatin1Char('.'), 0, -2);
                }
                renderName.append(QString("-%1.mlt").arg(ix));
                ix++;
            }
        }
        renderName = renderName.section(QLatin1Char('.'), 0, -2);
        renderName = QInputDialog::getText(this, i18n("Delayed rendering"), i18n("Select a name for this rendering."), QLineEdit::Normal, renderName, &ok);
        if (!ok) {
            return;
        }
        if (!renderName.endsWith(QStringLiteral(".mlt"))) {
            renderName.append(QStringLiteral(".mlt"));
        }
        if (projectFolder.exists(renderName)) {
            if (KMessageBox::questionYesNo(this, i18n("File %1 already exists.\nDo you want to overwrite it?", renderName)) == KMessageBox::No) {
                return;
            }
        }
        playlistPath = projectFolder.absoluteFilePath(renderName);
    } else {
        QTemporaryFile tmp(QDir::tempPath() + "/kdenlive-XXXXXX.mlt");
        if (!tmp.open()) {
            // Something went wrong
            return;
        }
        tmp.close();
        playlistPath = tmp.fileName();
    }
    int in = 0;
    int out;
    Monitor *pMon = pCore->getMonitor(Kdenlive::ProjectMonitor);
    bool zoneOnly = m_view.render_zone->isChecked();
    if (zoneOnly) {
        in = pMon->getZoneStart();
        out = pMon->getZoneEnd() - 1;
    } else {
        out = pCore->projectDuration() - 2;
    }

    QString playlistContent = pCore->projectManager()->projectSceneList(project->url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toLocalFile());
    if (!chapterFile.isEmpty()) {
        QDomDocument doc;
        QDomElement chapters = doc.createElement(QStringLiteral("chapters"));
        chapters.setAttribute(QStringLiteral("fps"), pCore->getCurrentFps());
        doc.appendChild(chapters);
        const QList<CommentedTime> guidesList = project->getGuideModel()->getAllMarkers();
        for (int i = 0; i < guidesList.count(); i++) {
            const CommentedTime &c = guidesList.at(i);
            int time = c.time().frames(pCore->getCurrentFps());
            if (time >= in && time < out) {
                if (zoneOnly) {
                    time = time - in;
                }
            }
            QDomElement chapter = doc.createElement(QStringLiteral("chapter"));
            chapters.appendChild(chapter);
            chapter.setAttribute(QStringLiteral("title"), c.comment());
            chapter.setAttribute(QStringLiteral("time"), time);
        }
        if (!chapters.childNodes().isEmpty()) {
            if (!project->getGuideModel()->hasMarker(out)) {
                // Always insert a guide in pos 0
                QDomElement chapter = doc.createElement(QStringLiteral("chapter"));
                chapters.insertBefore(chapter, QDomNode());
                chapter.setAttribute(QStringLiteral("title"), i18nc("the first in a list of chapters", "Start"));
                chapter.setAttribute(QStringLiteral("time"), QStringLiteral("0"));
            }
            // save chapters file
            QFile file(chapterFile);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                qCWarning(KDENLIVE_LOG) << "//////  ERROR writing DVD CHAPTER file: " << chapterFile;
            } else {
                file.write(doc.toString().toUtf8());
                if (file.error() != QFile::NoError) {
                    qCWarning(KDENLIVE_LOG) << "//////  ERROR writing DVD CHAPTER file: " << chapterFile;
                }
                file.close();
            }
        }
    }

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
    if (project->useProxy() && !proxyRendering()) {
        QString root = doc.documentElement().attribute(QStringLiteral("root"));
        if (!root.isEmpty() && !root.endsWith(QLatin1Char('/'))) {
            root.append(QLatin1Char('/'));
        }

        // replace proxy clips with originals
        QMap<QString, QString> proxies = pCore->projectItemModel()->getProxies(pCore->currentDoc()->documentRoot());

        QDomNodeList producers = doc.elementsByTagName(QStringLiteral("producer"));
        QString producerResource;
        QString producerService;
        QString suffix;
        QString prefix;
        for (int n = 0; n < producers.length(); ++n) {
            QDomElement e = producers.item(n).toElement();
            producerResource = Xml::getXmlProperty(e, QStringLiteral("resource"));
            producerService = Xml::getXmlProperty(e, QStringLiteral("mlt_service"));
            if (producerResource.isEmpty() || producerService == QLatin1String("color")) {
                continue;
            }
            if (producerService == QLatin1String("timewarp")) {
                // slowmotion producer
                prefix = producerResource.section(QLatin1Char(':'), 0, 0) + QLatin1Char(':');
                producerResource = producerResource.section(QLatin1Char(':'), 1);
            } else {
                prefix.clear();
            }
            if (producerService == QLatin1String("framebuffer")) {
                // slowmotion producer
                suffix = QLatin1Char('?') + producerResource.section(QLatin1Char('?'), 1);
                producerResource = producerResource.section(QLatin1Char('?'), 0, 0);
            } else {
                suffix.clear();
            }
            if (!producerResource.isEmpty()) {
                if (QFileInfo(producerResource).isRelative()) {
                    producerResource.prepend(root);
                }
                if (proxies.contains(producerResource)) {
                    QString replacementResource = proxies.value(producerResource);
                    Xml::setXmlProperty(e, QStringLiteral("resource"), prefix + replacementResource + suffix);
                    if (producerService == QLatin1String("timewarp")) {
                        Xml::setXmlProperty(e, QStringLiteral("warp_resource"), replacementResource);
                    }
                    // We need to delete the "aspect_ratio" property because proxy clips
                    // sometimes have different ratio than original clips
                    Xml::removeXmlProperty(e, QStringLiteral("aspect_ratio"));
                    Xml::removeMetaProperties(e);
                }
            }
        }
    }
    generateRenderFiles(doc, playlistPath, in, out, delayedRendering);
}

void RenderWidget::generateRenderFiles(QDomDocument doc, const QString &playlistPath, int in, int out, bool delayedRendering)
{
    QDomDocument clone;
    KdenliveDoc *project = pCore->currentDoc();
    int passes = m_view.checkTwoPass->isChecked() ? 2 : 1;
    QString renderArgs = m_view.advanced_params->toPlainText().simplified();
    QDomElement consumer = doc.createElement(QStringLiteral("consumer"));
    QDomNodeList profiles = doc.elementsByTagName(QStringLiteral("profile"));
    if (profiles.isEmpty()) {
        doc.documentElement().insertAfter(consumer, doc.documentElement());
    } else {
        doc.documentElement().insertAfter(consumer, profiles.at(profiles.length() - 1));
    }

    std::unique_ptr<ProfileModel> &profile = pCore->getCurrentProfile();
    if (renderArgs.contains(QLatin1String("%dv_standard"))) {
        QString dvstd;
        if (fmod(double(profile->frame_rate_num() / profile->frame_rate_den()), 30.01) > 27) {
            dvstd = QStringLiteral("ntsc");
        } else {
            dvstd = QStringLiteral("pal");
        }
        if (double(profile->display_aspect_num() / profile->display_aspect_den()) > 1.5) {
            dvstd += QLatin1String("_wide");
        }
        renderArgs.replace(QLatin1String("%dv_standard"), dvstd);
    }

    QStringList args = renderArgs.split(QLatin1Char(' '));
    for (auto &param : args) {
        if (param.contains(QLatin1Char('='))) {
            QString paramValue = param.section(QLatin1Char('='), 1);
            if (paramValue.startsWith(QLatin1Char('%'))) {
                if (paramValue.startsWith(QStringLiteral("%bitrate")) || paramValue == QStringLiteral("%quality")) {
                    if (paramValue.contains("+'k'"))
                        paramValue = QString::number(m_view.video->value()) + 'k';
                    else
                        paramValue = QString::number(m_view.video->value());
                }
                if (paramValue.startsWith(QStringLiteral("%audiobitrate")) || paramValue == QStringLiteral("%audioquality")) {
                    if (paramValue.contains("+'k'"))
                        paramValue = QString::number(m_view.audio->value()) + 'k';
                    else
                        paramValue = QString::number(m_view.audio->value());
                }
                if (paramValue == QStringLiteral("%dar"))
                    paramValue = '@' + QString::number(profile->display_aspect_num()) + QLatin1Char('/') + QString::number(profile->display_aspect_den());
                if (paramValue == QStringLiteral("%passes")) paramValue = QString::number(static_cast<int>(m_view.checkTwoPass->isChecked()) + 1);
            }
            consumer.setAttribute(param.section(QLatin1Char('='), 0, 0), paramValue);
        }
    }

    // Check for movit
    if (KdenliveSettings::gpu_accel()) {
        consumer.setAttribute(QStringLiteral("glsl."), 1);
    }

    // in/out points
    if (m_view.render_guide->isChecked()) {
        double fps = profile->fps();
        double guideStart = m_view.guide_start->itemData(m_view.guide_start->currentIndex()).toDouble();
        double guideEnd = m_view.guide_end->itemData(m_view.guide_end->currentIndex()).toDouble();
        consumer.setAttribute(QStringLiteral("in"), int(GenTime(guideStart).frames(fps)));
        consumer.setAttribute(QStringLiteral("out"), int(GenTime(guideEnd).frames(fps)));
    } else {
        consumer.setAttribute(QStringLiteral("in"), in);
        consumer.setAttribute(QStringLiteral("out"), out);
    }
    // Audio Channels
    consumer.setAttribute(QStringLiteral("channels"), pCore->audioChannels());

    // Check if the rendering profile is different from project profile,
    // in which case we need to use the producer_comsumer from MLT
    QString subsize;
    if (renderArgs.startsWith(QLatin1String("s="))) {
        subsize = renderArgs.section(QLatin1Char(' '), 0, 0).toLower();
        subsize = subsize.section(QLatin1Char('='), 1, 1);
    } else if (renderArgs.contains(QStringLiteral(" s="))) {
        subsize = renderArgs.section(QStringLiteral(" s="), 1, 1);
        subsize = subsize.section(QLatin1Char(' '), 0, 0).toLower();
    } else if (m_view.rescale->isChecked() && m_view.rescale->isEnabled()) {
        subsize = QStringLiteral("%1x%2").arg(m_view.rescale_width->value()).arg(m_view.rescale_height->value());
    }
    if (!subsize.isEmpty()) {
        consumer.setAttribute(QStringLiteral("s"), subsize);
    }

    // Project metadata
    if (m_view.export_meta->isChecked()) {
        QMap<QString, QString> metadata = project->metadata();
        QMap<QString, QString>::const_iterator i = metadata.constBegin();
        while (i != metadata.constEnd()) {
            consumer.setAttribute(i.key(), i.value());
            ++i;
        }
    }

    // Adjust encoding speed
    if (m_view.speed->isEnabled() && m_view.formats->currentItem()) {
        QStringList speeds = m_view.formats->currentItem()->data(0, SpeedsRole).toStringList();
        if (m_view.speed->value() < speeds.count()) {
            QString speedValue = speeds.at(m_view.speed->value());
            if (speedValue.contains(QLatin1Char('='))) {
                consumer.setAttribute(speedValue.section(QLatin1Char('='), 0, 0), speedValue.section(QLatin1Char('='), 1));
            }
        }
    }

    // Adjust scanning
    switch (m_view.scanning_list->currentIndex()) {
    case 1:
        consumer.setAttribute(QStringLiteral("progressive"), 1);
        break;
    case 2:
        // Interlaced rendering
        consumer.setAttribute(QStringLiteral("progressive"), 0);
        // Adjust field order
        consumer.setAttribute(QStringLiteral("top_field_first"), m_view.field_order->currentIndex());
        break;
    default:
        // leave as is
        break;
    }

    // check if audio export is selected
    bool exportAudio;
    if (automaticAudioExport()) {
        // TODO check if projact contains audio
        // exportAudio = pCore->projectManager()->currentTimeline()->checkProjectAudio();
        exportAudio = true;
    } else {
        exportAudio = selectedAudioExport();
    }

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

    // disable audio if requested
    if (!exportAudio) {
        consumer.setAttribute(QStringLiteral("an"), 1);
    }

    int threadCount = QThread::idealThreadCount();
    if (threadCount < 2 || !m_view.parallel_process->isChecked() || !m_view.parallel_process->isEnabled()) {
        threadCount = 1;
    } else {
        threadCount = qMin(4, threadCount - 1);
    }

    // Set the thread counts
    if (!renderArgs.contains(QStringLiteral("threads="))) {
        consumer.setAttribute(QStringLiteral("threads"), KdenliveSettings::encodethreads());
    }
    consumer.setAttribute(QStringLiteral("real_time"), -threadCount);

    // check which audio tracks have to be exported
    /*if (stemExport) {
        // TODO refac
        //TODO port to new timeline model
        Timeline *ct = pCore->projectManager()->currentTimeline();
        int allTracksCount = ct->tracksCount();

        // reset tracks count (tracks to be rendered)
        tracksCount = 0;
        // begin with track 1 (track zero is a hidden black track)
        for (int i = 1; i < allTracksCount; i++) {
            Track *track = ct->track(i);
            // add only tracks to render list that are not muted and have audio
            if ((track != nullptr) && !track->info().isMute && track->hasAudio()) {
                QDomDocument docCopy = doc.cloneNode(true).toDocument();
                QString trackName = track->info().trackName;

                // save track name
                trackNames << trackName;
                qCDebug(KDENLIVE_LOG) << "Track-Name: " << trackName;

                // create stem export doc content
                QDomNodeList tracks = docCopy.elementsByTagName(QStringLiteral("track"));
                for (int j = 0; j < allTracksCount; j++) {
                    if (j != i) {
                        // mute other tracks
                        tracks.at(j).toElement().setAttribute(QStringLiteral("hide"), QStringLiteral("both"));
                    }
                }
                docList << docCopy;
                tracksCount++;
            }
        }
    }*/

    if (m_view.checkTwoPass->isChecked()) {
        // We will generate 2 files, one for each pass.
        clone = doc.cloneNode(true).toDocument();
    }
    QStringList playlists;
    QString renderedFile = m_view.out_file->url().toLocalFile();
    if (m_view.advanced_params->toPlainText().simplified().contains("=stills/"))
    {
        // Image sequence, ensure we have a %0xd at file end
        QString extension = renderedFile.section(QLatin1Char('.'), -1);
        // format string for counter
        if (!QRegExp(QStringLiteral(".*%[0-9]*d.*")).exactMatch(renderedFile)) {
            renderedFile = renderedFile.section(QLatin1Char('.'), 0, -2) + QStringLiteral("_%05d.") + extension;
        }
    }
    for (int i = 0; i < passes; i++) {
        // Append consumer settings
        QDomDocument final = i > 0 ? clone : doc;
        QDomNodeList cons = final.elementsByTagName(QStringLiteral("consumer"));
        QDomElement myConsumer = cons.at(0).toElement();
        QString mytarget = renderedFile;
        QString playlistName = playlistPath;
        myConsumer.setAttribute(QStringLiteral("mlt_service"), QStringLiteral("avformat"));
        if (passes == 2 && i == 1) {
            playlistName = playlistName.section(QLatin1Char('.'), 0, -2) + QString("-pass%1.").arg(i + 1) + playlistName.section(QLatin1Char('.'), -1);
        }
        playlists << playlistName;
        myConsumer.setAttribute(QStringLiteral("target"), mytarget);

        // Prepare rendering args
        int pass = passes == 2 ? i + 1 : 0;
        if (renderArgs.contains(QStringLiteral("libx265"))) {
            if (pass == 1 || pass == 2) {
                QString x265params = myConsumer.attribute("x265-params");
                x265params = QString("pass=%1:stats=%2:%3").arg(pass).arg(mytarget.replace(":", "\\:") + "_2pass.log", x265params);
                myConsumer.setAttribute("x265-params", x265params);
            }
        } else {
            if (pass == 1 || pass == 2) {
                myConsumer.setAttribute("pass", pass);
                myConsumer.setAttribute("passlogfile", mytarget + "_2pass.log");
            }
            if (pass == 1) {
                myConsumer.setAttribute("fastfirstpass", 1);
                myConsumer.removeAttribute("acodec");
                myConsumer.setAttribute("an", 1);
            } else {
                myConsumer.removeAttribute("fastfirstpass");
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

    // Create job
    RenderJobItem *renderItem = nullptr;
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(renderedFile, Qt::MatchExactly, 1);
    if (!existing.isEmpty()) {
        renderItem = static_cast<RenderJobItem *>(existing.at(0));
        if (renderItem->status() == RUNNINGJOB || renderItem->status() == WAITINGJOB || renderItem->status() == STARTINGJOB) {
            KMessageBox::information(
                this, i18n("There is already a job writing file:<br /><b>%1</b><br />Abort the job if you want to overwrite it...", renderedFile),
                i18n("Already running"));
            return;
        }
        if (delayedRendering || playlists.size() > 1) {
            delete renderItem;
            renderItem = nullptr;
        } else {
            renderItem->setData(1, ProgressRole, 0);
            renderItem->setStatus(WAITINGJOB);
            renderItem->setIcon(0, QIcon::fromTheme(QStringLiteral("media-playback-pause")));
            renderItem->setData(1, Qt::UserRole, i18n("Waiting..."));
            QStringList argsJob = {KdenliveSettings::rendererpath(), playlistPath, renderedFile,
                                   QStringLiteral("-pid:%1").arg(QCoreApplication::applicationPid())};
            renderItem->setData(1, ParametersRole, argsJob);
            renderItem->setData(1, TimeRole, QDateTime::currentDateTime());
            if (!exportAudio) {
                renderItem->setData(1, ExtraInfoRole, i18n("Video without audio track"));
            } else {
                renderItem->setData(1, ExtraInfoRole, QString());
            }
            m_view.running_jobs->setCurrentItem(renderItem);
            m_view.tabWidget->setCurrentIndex(1);
            checkRenderStatus();
            return;
        }
    }
    if (delayedRendering) {
        parseScriptFiles();
        return;
    }
    QList<RenderJobItem *> jobList;
    for (const QString &pl : qAsConst(playlists)) {
        renderItem = new RenderJobItem(m_view.running_jobs, QStringList() << QString() << renderedFile);
        renderItem->setData(1, TimeRole, QDateTime::currentDateTime());
        QStringList argsJob = {KdenliveSettings::rendererpath(), pl, renderedFile, QStringLiteral("-pid:%1").arg(QCoreApplication::applicationPid())};
        renderItem->setData(1, ParametersRole, argsJob);
        qDebug() << "* CREATED JOB WITH ARGS: " << argsJob;
        if (!exportAudio) {
            renderItem->setData(1, ExtraInfoRole, i18n("Video without audio track"));
        } else {
            renderItem->setData(1, ExtraInfoRole, QString());
        }
        jobList << renderItem;
    }

    m_view.running_jobs->setCurrentItem(jobList.at(0));
    m_view.tabWidget->setCurrentIndex(1);
    // check render status
    checkRenderStatus();

    // create full playlistPaths
    /*
    QString mltSuffix(QStringLiteral(".mlt"));
    QList<QString> playlistPaths;
    QList<QString> trackNames;
    for (int i = 0; i < tracksCount; i++) {
        QString plPath(playlistPath);

        // add track number to path name
        if (stemExport) {
            plPath = plPath + QLatin1Char('_') + QString(trackNames.at(i)).replace(QLatin1Char(' '), QLatin1Char('_'));
        }
        // add mlt suffix
        if (!plPath.endsWith(mltSuffix)) {
            plPath += mltSuffix;
        }
        playlistPaths << plPath;
        qCDebug(KDENLIVE_LOG) << "playlistPath: " << plPath << endl;

        // Do save scenelist
        QFile file(plPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            pCore->displayMessage(i18n("Cannot write to file %1", plPath), ErrorMessage);
            return;
        }
        file.write(docList.at(i).toString().toUtf8());
        if (file.error() != QFile::NoError) {
            pCore->displayMessage(i18n("Cannot write to file %1", plPath), ErrorMessage);
            file.close();
            return;
        }
        file.close();
    }*/
    // slotExport(delayedRendering, in, out, project->metadata(), playlistPaths, trackNames, renderName, exportAudio);
}

void RenderWidget::checkRenderStatus()
{
    // check if we have a job waiting to render
    if (m_blockProcessing) {
        return;
    }

    auto *item = static_cast<RenderJobItem *>(m_view.running_jobs->topLevelItem(0));

    // Make sure no other rendering is running
    while (item != nullptr) {
        if (item->status() == RUNNINGJOB) {
            return;
        }
        item = static_cast<RenderJobItem *>(m_view.running_jobs->itemBelow(item));
    }
    item = static_cast<RenderJobItem *>(m_view.running_jobs->topLevelItem(0));
    bool waitingJob = false;

    // Find first waiting job
    while (item != nullptr) {
        if (item->status() == WAITINGJOB) {
            item->setData(1, TimeRole, QDateTime::currentDateTime());
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
        if (item->status() == WAITINGJOB || item->status() == STARTINGJOB) {
            count++;
        }
        item = static_cast<RenderJobItem *>(m_view.running_jobs->itemBelow(item));
    }
    return count;
}

void RenderWidget::adjustViewToProfile()
{
    m_view.scanning_list->setCurrentIndex(0);
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
    m_view.formats->blockSignals(true);
    QIcon brokenIcon = QIcon::fromTheme(QStringLiteral("dialog-close"));
    QIcon warningIcon = QIcon::fromTheme(QStringLiteral("dialog-warning"));

    KColorScheme scheme(palette().currentColorGroup(), KColorScheme::Window);
    const QColor disabled = scheme.foreground(KColorScheme::InactiveText).color();
    const QColor disabledbg = scheme.background(KColorScheme::NegativeBackground).color();

    // We borrow a reference to the profile's pointer to query it more easily
    std::unique_ptr<ProfileModel> &profile = pCore->getCurrentProfile();
    double project_framerate = double(profile->frame_rate_num()) / profile->frame_rate_den();
    for (int i = 0; i < m_view.formats->topLevelItemCount(); ++i) {
        QTreeWidgetItem *group = m_view.formats->topLevelItem(i);
        for (int j = 0; j < group->childCount(); ++j) {
            QTreeWidgetItem *item = group->child(j);
            QString std = item->data(0, StandardRole).toString();
            if (std.isEmpty() ||
                (std.contains(QStringLiteral("PAL"), Qt::CaseInsensitive) && profile->frame_rate_num() == 25 && profile->frame_rate_den() == 1) ||
                (std.contains(QStringLiteral("NTSC"), Qt::CaseInsensitive) && profile->frame_rate_num() == 30000 && profile->frame_rate_den() == 1001)) {
                // Standard OK
            } else {
                item->setData(0, ErrorRole, i18n("Standard (%1) not compatible with project profile (%2)", std, project_framerate));
                item->setIcon(0, brokenIcon);
                item->setForeground(0, disabled);
                continue;
            }
            QString params = item->data(0, ParamsRole).toString();
            // Make sure the selected profile uses the same frame rate as project profile
            if (params.contains(QStringLiteral("mlt_profile="))) {
                QString profile_str = params.section(QStringLiteral("mlt_profile="), 1, 1).section(QLatin1Char(' '), 0, 0);
                std::unique_ptr<ProfileModel> &target_profile = ProfileRepository::get()->getProfile(profile_str);
                if (target_profile->frame_rate_den() > 0) {
                    double profile_rate = double(target_profile->frame_rate_num()) / target_profile->frame_rate_den();
                    if (int(1000.0 * profile_rate) != int(1000.0 * project_framerate)) {
                        item->setData(0, ErrorRole, i18n("Frame rate (%1) not compatible with project profile (%2)", profile_rate, project_framerate));
                        item->setIcon(0, brokenIcon);
                        item->setForeground(0, disabled);
                        continue;
                    }
                }
            }

            // Make sure the selected profile uses an installed avformat codec / format
            if (!supportedFormats.isEmpty()) {
                QString format;
                if (params.startsWith(QLatin1String("f="))) {
                    format = params.section(QStringLiteral("f="), 1, 1);
                } else if (params.contains(QStringLiteral(" f="))) {
                    format = params.section(QStringLiteral(" f="), 1, 1);
                }
                if (!format.isEmpty()) {
                    format = format.section(QLatin1Char(' '), 0, 0).toLower();
                    if (!supportedFormats.contains(format)) {
                        item->setData(0, ErrorRole, i18n("Unsupported video format: %1", format));
                        item->setIcon(0, brokenIcon);
                        item->setForeground(0, disabled);
                        continue;
                    }
                }
            }
            if (!acodecsList.isEmpty()) {
                QString format;
                if (params.startsWith(QLatin1String("acodec="))) {
                    format = params.section(QStringLiteral("acodec="), 1, 1);
                } else if (params.contains(QStringLiteral(" acodec="))) {
                    format = params.section(QStringLiteral(" acodec="), 1, 1);
                }
                if (!format.isEmpty()) {
                    format = format.section(QLatin1Char(' '), 0, 0).toLower();
                    if (!acodecsList.contains(format)) {
                        item->setData(0, ErrorRole, i18n("Unsupported audio codec: %1", format));
                        item->setIcon(0, brokenIcon);
                        item->setForeground(0, disabled);
                        item->setBackground(0, disabledbg);
                    }
                }
            }
            if (!vcodecsList.isEmpty()) {
                QString format;
                if (params.startsWith(QLatin1String("vcodec="))) {
                    format = params.section(QStringLiteral("vcodec="), 1, 1);
                } else if (params.contains(QStringLiteral(" vcodec="))) {
                    format = params.section(QStringLiteral(" vcodec="), 1, 1);
                }
                if (!format.isEmpty()) {
                    format = format.section(QLatin1Char(' '), 0, 0).toLower();
                    if (!vcodecsList.contains(format)) {
                        item->setData(0, ErrorRole, i18n("Unsupported video codec: %1", format));
                        item->setIcon(0, brokenIcon);
                        item->setForeground(0, disabled);
                        continue;
                    }
                }
            }
            if (params.contains(QStringLiteral(" profile=")) || params.startsWith(QLatin1String("profile="))) {
                // changed in MLT commit d8a3a5c9190646aae72048f71a39ee7446a3bd45
                // (https://github.com/mltframework/mlt/commit/d8a3a5c9190646aae72048f71a39ee7446a3bd45)
                item->setData(0, ErrorRole,
                              i18n("This render profile uses a 'profile' parameter.<br />Unless you know what you are doing you will probably "
                                   "have to change it to 'mlt_profile'."));
                item->setIcon(0, warningIcon);
                continue;
            }
        }
    }
    focusFirstVisibleItem();
    m_view.formats->blockSignals(false);
    refreshParams();
}

QUrl RenderWidget::filenameWithExtension(QUrl url, const QString &extension)
{
    if (!url.isValid()) {
        url = QUrl::fromLocalFile(pCore->currentDoc()->projectDataFolder() + QDir::separator());
    }
    QString directory = url.adjusted(QUrl::RemoveFilename).toLocalFile();
    QString filename = url.fileName();
    if (filename.isEmpty()) {
        filename = pCore->currentDoc()->url().fileName();
    }
    QString ext;

    if (extension.at(0) == '.') {
        ext = extension;
    } else {
        ext = '.' + extension;
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

void RenderWidget::refreshParams()
{
    // Format not available (e.g. codec not installed); Disable start button
    QTreeWidgetItem *item = m_view.formats->currentItem();
    if ((item == nullptr) || item->parent() == nullptr) {
        // This is a category item, not a real profile
        m_view.buttonBox->setEnabled(false);
    } else {
        m_view.buttonBox->setEnabled(true);
    }
    QString extension;
    if (item) {
        extension = item->data(0, ExtensionRole).toString();
    }
    if ((item == nullptr) || item->isHidden() || extension.isEmpty()) {
        if (!item) {
            errorMessage(ProfileError, i18n("No matching profile"));
        } else if (!item->parent()) // category
            ;
        else if (extension.isEmpty()) {
            errorMessage(ProfileError, i18n("Invalid profile"));
        }
        m_view.advanced_params->clear();
        m_view.buttonRender->setEnabled(false);
        m_view.buttonGenerateScript->setEnabled(false);
        return;
    }
    QString params = item->data(0, ParamsRole).toString();
    errorMessage(ProfileError, item->data(0, ErrorRole).toString());
    m_view.advanced_params->setPlainText(params);
    if (params.contains(QStringLiteral(" s=")) || params.startsWith(QLatin1String("s=")) || params.contains(QLatin1String("%dv_standard"))) {
        // profile has a fixed size, do not allow resize
        m_view.rescale->setEnabled(false);
        setRescaleEnabled(false);
    } else {
        m_view.rescale->setEnabled(true);
        setRescaleEnabled(m_view.rescale->isChecked());
    }
    QUrl url = filenameWithExtension(m_view.out_file->url(), extension);
    m_view.out_file->setUrl(url);
    //     if (!url.isEmpty()) {
    //         QString path = url.path();
    //         int pos = path.lastIndexOf('.') + 1;
    //  if (pos == 0) path.append('.' + extension);
    //         else path = path.left(pos) + extension;
    //         m_view.out_file->setUrl(QUrl(path));
    //     } else {
    //         m_view.out_file->setUrl(QUrl(QDir::homePath() + QStringLiteral("/untitled.") + extension));
    //     }
    m_view.out_file->setFilter("*." + extension);
    QString edit = item->data(0, EditableRole).toString();
    if (edit.isEmpty() || !edit.endsWith(QLatin1String("customprofiles.xml"))) {
        m_view.buttonDelete->setEnabled(false);
        m_view.buttonEdit->setEnabled(false);
    } else {
        m_view.buttonDelete->setEnabled(true);
        m_view.buttonEdit->setEnabled(true);
    }

    // video quality control
    m_view.video->blockSignals(true);
    bool quality = false;
    if ((params.contains(QStringLiteral("%quality")) || params.contains(QStringLiteral("%bitrate"))) &&
        item->data(0, BitratesRole).canConvert(QVariant::StringList)) {
        // bitrates or quantizers list
        QStringList qs = item->data(0, BitratesRole).toStringList();
        if (qs.count() > 1) {
            quality = true;
            int qmax = qs.constFirst().toInt();
            int qmin = qs.last().toInt();
            if (qmax < qmin) {
                // always show best quality on right
                m_view.video->setRange(qmax, qmin);
                m_view.video->setProperty("decreasing", true);
            } else {
                m_view.video->setRange(qmin, qmax);
                m_view.video->setProperty("decreasing", false);
            }
        }
    }
    m_view.video->setEnabled(quality);
    m_view.quality->setEnabled(quality);
    m_view.qualityLabel->setEnabled(quality);
    m_view.video->blockSignals(false);

    // audio quality control
    quality = false;
    m_view.audio->blockSignals(true);
    if ((params.contains(QStringLiteral("%audioquality")) || params.contains(QStringLiteral("%audiobitrate"))) &&
        item->data(0, AudioBitratesRole).canConvert(QVariant::StringList)) {
        // bitrates or quantizers list
        QStringList qs = item->data(0, AudioBitratesRole).toStringList();
        if (qs.count() > 1) {
            quality = true;
            int qmax = qs.constFirst().toInt();
            int qmin = qs.last().toInt();
            if (qmax < qmin) {
                m_view.audio->setRange(qmax, qmin);
                m_view.audio->setProperty("decreasing", true);
            } else {
                m_view.audio->setRange(qmin, qmax);
                m_view.audio->setProperty("decreasing", false);
            }
            if (params.contains(QStringLiteral("%audiobitrate"))) {
                m_view.audio->setSingleStep(32); // 32kbps step
            } else {
                m_view.audio->setSingleStep(1);
            }
        }
    }
    m_view.audio->setEnabled(quality);
    m_view.audio->blockSignals(false);
    if (m_view.quality->isEnabled()) {
        adjustAVQualities(m_view.quality->value());
    }

    if (item->data(0, SpeedsRole).canConvert(QVariant::StringList) && (item->data(0, SpeedsRole).toStringList().count() != 0)) {
        int speed = item->data(0, SpeedsRole).toStringList().count() - 1;
        m_view.speed->setEnabled(true);
        m_view.speed->setMaximum(speed);
        m_view.speed->setValue(speed * 3 / 4); // default to intermediate speed
    } else {
        m_view.speed->setEnabled(false);
    }
    if (!item->data(0, FieldRole).isNull()) {
        m_view.field_order->setCurrentIndex(item->data(0, FieldRole).toInt());
    }
    adjustSpeed(m_view.speed->value());
    bool passes = params.contains(QStringLiteral("passes"));
    m_view.checkTwoPass->setEnabled(passes);
    m_view.checkTwoPass->setChecked(passes && params.contains(QStringLiteral("passes=2")));

    m_view.encoder_threads->setEnabled(!params.contains(QStringLiteral("threads=")));

    m_view.buttonRender->setEnabled(m_view.formats->currentItem()->data(0, ErrorRole).isNull());
    m_view.buttonGenerateScript->setEnabled(m_view.formats->currentItem()->data(0, ErrorRole).isNull());
}

void RenderWidget::reloadProfiles()
{
    parseProfiles();
}

void RenderWidget::parseProfiles(const QString &selectedProfile)
{
    m_view.formats->clear();

    // Parse our xml profile
    QString exportFile = QStandardPaths::locate(QStandardPaths::AppDataLocation, QStringLiteral("export/profiles.xml"));
    parseFile(exportFile, false);

    // Parse some MLT's profiles
    parseMltPresets();
    QStringList filter = {QStringLiteral("*.xml")};
#ifdef Q_OS_WIN
    // Windows downloaded profiles are saved in AppLocalDataLocation
    QString localExportFolder = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/export/");
    QDir winDir(localExportFolder);
    QStringList winFileList = winDir.entryList(filter, QDir::Files);
    for (const QString &filename : winFileList) {
        parseFile(winDir.absoluteFilePath(filename), true);
    }
#endif
    QString exportFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/export/");
    QDir directory(exportFolder);
    QStringList fileList = directory.entryList(filter, QDir::Files);
    // We should parse customprofiles.xml in last position, so that user profiles
    // can also override profiles installed by KNewStuff
    fileList.removeAll(QStringLiteral("customprofiles.xml"));
    for (const QString &filename : qAsConst(fileList)) {
        parseFile(directory.absoluteFilePath(filename), true);
    }
    if (QFile::exists(exportFolder + QStringLiteral("customprofiles.xml"))) {
        parseFile(exportFolder + QStringLiteral("customprofiles.xml"), true);
    }

    focusFirstVisibleItem(selectedProfile);
}

void RenderWidget::parseMltPresets()
{
    QDir root(KdenliveSettings::mltpath());
    if (!root.cd(QStringLiteral("../presets/consumer/avformat"))) {
        // Cannot find MLT's presets directory
        qCWarning(KDENLIVE_LOG) << " / / / WARNING, cannot find MLT's preset folder";
        return;
    }
    if (root.cd(QStringLiteral("lossless"))) {
        QString groupName = i18n("Lossless/HQ");
        QList<QTreeWidgetItem *> foundGroup = m_view.formats->findItems(groupName, Qt::MatchExactly);
        QTreeWidgetItem *groupItem;
        if (!foundGroup.isEmpty()) {
            groupItem = foundGroup.takeFirst();
        } else {
            groupItem = new QTreeWidgetItem(QStringList(groupName));
            m_view.formats->addTopLevelItem(groupItem);
            groupItem->setExpanded(true);
        }

        const QStringList profiles = root.entryList(QDir::Files, QDir::Name);
        for (const QString &prof : profiles) {
            KConfig config(root.absoluteFilePath(prof), KConfig::SimpleConfig);
            KConfigGroup group = config.group(QByteArray());
            QString vcodec = group.readEntry("vcodec");
            QString acodec = group.readEntry("acodec");
            QString extension = group.readEntry("meta.preset.extension");
            QString note = group.readEntry("meta.preset.note");
            QString profileName = prof;
            if (!vcodec.isEmpty() || !acodec.isEmpty()) {
                profileName.append(" (");
                if (!vcodec.isEmpty()) {
                    profileName.append(vcodec);
                    if (!acodec.isEmpty()) {
                        profileName.append("+" + acodec);
                    }
                } else if (!acodec.isEmpty()) {
                    profileName.append(acodec);
                }
                profileName.append(QLatin1Char(')'));
            }
            QTreeWidgetItem *item = new QTreeWidgetItem(QStringList(profileName));
            item->setData(0, ExtensionRole, extension);
            item->setData(0, RenderRole, "avformat");
            item->setData(0, ParamsRole, QString("properties=lossless/" + prof));
            if (!note.isEmpty()) {
                item->setToolTip(0, note);
            }
            groupItem->addChild(item);
        }
    }
    if (root.cd(QStringLiteral("../stills"))) {
        QString groupName = i18nc("Category Name", "Images sequence");
        QList<QTreeWidgetItem *> foundGroup = m_view.formats->findItems(groupName, Qt::MatchExactly);
        QTreeWidgetItem *groupItem;
        if (!foundGroup.isEmpty()) {
            groupItem = foundGroup.takeFirst();
        } else {
            groupItem = new QTreeWidgetItem(QStringList(groupName));
            m_view.formats->addTopLevelItem(groupItem);
            groupItem->setExpanded(true);
        }
        QStringList profiles = root.entryList(QDir::Files, QDir::Name);
        for (const QString &prof : qAsConst(profiles)) {
            QTreeWidgetItem *item = loadFromMltPreset(groupName, root.absoluteFilePath(prof), prof);
            if (!item) {
                continue;
            }
            item->setData(0, ParamsRole, QString("properties=stills/" + prof));
            groupItem->addChild(item);
        }
        // Add GIF as image sequence
        root.cdUp();
        QTreeWidgetItem *item = loadFromMltPreset(groupName, root.absoluteFilePath(QStringLiteral("GIF")), QStringLiteral("GIF"));
        if (item) {
            item->setData(0, ParamsRole, QStringLiteral("properties=GIF"));
            groupItem->addChild(item);
        }
    }
}

QTreeWidgetItem *RenderWidget::loadFromMltPreset(const QString &groupName, const QString &path, const QString &profileName)
{
    KConfig config(path, KConfig::SimpleConfig);
    KConfigGroup group = config.group(QByteArray());
    QString extension = group.readEntry("meta.preset.extension");
    QString note = group.readEntry("meta.preset.note");
    if (extension.isEmpty()) {
        return nullptr;
    }
    QTreeWidgetItem *item = new QTreeWidgetItem(QStringList(profileName));
    item->setData(0, GroupRole, groupName);
    item->setData(0, ExtensionRole, extension);
    item->setData(0, RenderRole, "avformat");
    if (!note.isEmpty()) {
        item->setToolTip(0, note);
    }
    return item;
}

void RenderWidget::parseFile(const QString &exportFile, bool editable)
{
    QDomDocument doc;
    QFile file(exportFile);
    doc.setContent(&file, false);
    file.close();
    QDomElement documentElement;
    QDomElement profileElement;
    QString extension;
    QDomNodeList groups = doc.elementsByTagName(QStringLiteral("group"));
    QTreeWidgetItem *item = nullptr;
    bool replaceVorbisCodec = false;
    if (acodecsList.contains(QStringLiteral("libvorbis"))) {
        replaceVorbisCodec = true;
    }
    bool replaceLibfaacCodec = false;
    if (acodecsList.contains(QStringLiteral("libfaac"))) {
        replaceLibfaacCodec = true;
    }

    if (editable || groups.isEmpty()) {
        QDomElement profiles = doc.documentElement();
        if (editable && profiles.attribute(QStringLiteral("version"), nullptr).toInt() < 1) {
            // this is an old profile version, update it
            QDomDocument newdoc;
            QDomElement newprofiles = newdoc.createElement(QStringLiteral("profiles"));
            newprofiles.setAttribute(QStringLiteral("version"), 1);
            newdoc.appendChild(newprofiles);
            QDomNodeList profilelist = doc.elementsByTagName(QStringLiteral("profile"));
            for (int i = 0; i < profilelist.count(); ++i) {
                QString category = i18nc("Category Name", "Custom");
                QString ext;
                QDomNode parent = profilelist.at(i).parentNode();
                if (!parent.isNull()) {
                    QDomElement parentNode = parent.toElement();
                    if (parentNode.hasAttribute(QStringLiteral("name"))) {
                        category = parentNode.attribute(QStringLiteral("name"));
                    }
                    ext = parentNode.attribute(QStringLiteral("extension"));
                }
                if (!profilelist.at(i).toElement().hasAttribute(QStringLiteral("category"))) {
                    profilelist.at(i).toElement().setAttribute(QStringLiteral("category"), category);
                }
                if (!ext.isEmpty()) {
                    profilelist.at(i).toElement().setAttribute(QStringLiteral("extension"), ext);
                }
                QDomNode n = profilelist.at(i).cloneNode();
                newprofiles.appendChild(newdoc.importNode(n, true));
            }
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                KMessageBox::sorry(this, i18n("Unable to write to file %1", exportFile));
                return;
            }
            QTextStream out(&file);
            out << newdoc.toString();
            file.close();
            parseFile(exportFile, editable);
            return;
        }

        QDomNode node = doc.elementsByTagName(QStringLiteral("profile")).at(0);
        if (node.isNull()) {
            return;
        }
        int count = 1;
        while (!node.isNull()) {
            QDomElement profile = node.toElement();
            QString profileName = profile.attribute(QStringLiteral("name"));
            QString standard = profile.attribute(QStringLiteral("standard"));
            QTextDocument docConvert;
            docConvert.setHtml(profile.attribute(QStringLiteral("args")));
            QString params = docConvert.toPlainText().simplified();

            if (replaceVorbisCodec && params.contains(QStringLiteral("acodec=vorbis"))) {
                // replace vorbis with libvorbis
                params = params.replace(QLatin1String("=vorbis"), QLatin1String("=libvorbis"));
            }
            if (replaceLibfaacCodec && params.contains(QStringLiteral("acodec=aac"))) {
                // replace libfaac with aac
                params = params.replace(QLatin1String("aac"), QLatin1String("libfaac"));
            }

            QString prof_extension = profile.attribute(QStringLiteral("extension"));
            if (!prof_extension.isEmpty()) {
                extension = prof_extension;
            }
            QString groupName = profile.attribute(QStringLiteral("category"), i18nc("Category Name", "Custom"));
            QList<QTreeWidgetItem *> foundGroup = m_view.formats->findItems(groupName, Qt::MatchExactly);
            QTreeWidgetItem *groupItem;
            if (!foundGroup.isEmpty()) {
                groupItem = foundGroup.takeFirst();
            } else {
                groupItem = new QTreeWidgetItem(QStringList(groupName));
                if (editable) {
                    m_view.formats->insertTopLevelItem(0, groupItem);
                } else {
                    m_view.formats->addTopLevelItem(groupItem);
                    groupItem->setExpanded(true);
                }
            }

            // Check if item with same name already exists and replace it,
            // allowing to override default profiles
            QTreeWidgetItem *childitem = nullptr;
            for (int j = 0; j < groupItem->childCount(); ++j) {
                if (groupItem->child(j)->text(0) == profileName) {
                    childitem = groupItem->child(j);
                    break;
                }
            }
            if (!childitem) {
                childitem = new QTreeWidgetItem(QStringList(profileName));
            }
            childitem->setData(0, GroupRole, groupName);
            childitem->setData(0, ExtensionRole, extension);
            childitem->setData(0, RenderRole, "avformat");
            childitem->setData(0, StandardRole, standard);
            childitem->setData(0, ParamsRole, params);
            if (params.contains(QLatin1String("%quality"))) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                childitem->setData(0, BitratesRole, profile.attribute(QStringLiteral("qualities")).split(QLatin1Char(','), QString::SkipEmptyParts));
#else
                childitem->setData(0, BitratesRole, profile.attribute(QStringLiteral("qualities")).split(QLatin1Char(','), Qt::SkipEmptyParts));
#endif
            } else if (params.contains(QLatin1String("%bitrate"))) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                childitem->setData(0, BitratesRole, profile.attribute(QStringLiteral("bitrates")).split(QLatin1Char(','), QString::SkipEmptyParts));
#else
                childitem->setData(0, BitratesRole, profile.attribute(QStringLiteral("bitrates")).split(QLatin1Char(','), Qt::SkipEmptyParts));
#endif
            }
            if (params.contains(QLatin1String("%audioquality"))) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                childitem->setData(0, AudioBitratesRole, profile.attribute(QStringLiteral("audioqualities")).split(QLatin1Char(','), QString::SkipEmptyParts));
#else
                childitem->setData(0, AudioBitratesRole, profile.attribute(QStringLiteral("audioqualities")).split(QLatin1Char(','), Qt::SkipEmptyParts));
#endif
            } else if (params.contains(QLatin1String("%audiobitrate"))) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                childitem->setData(0, AudioBitratesRole, profile.attribute(QStringLiteral("audiobitrates")).split(QLatin1Char(','), QString::SkipEmptyParts));
#else
                childitem->setData(0, AudioBitratesRole, profile.attribute(QStringLiteral("audiobitrates")).split(QLatin1Char(','), Qt::SkipEmptyParts));
#endif
            }
            if (profile.hasAttribute(QStringLiteral("speeds"))) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                childitem->setData(0, SpeedsRole, profile.attribute(QStringLiteral("speeds")).split(QLatin1Char(';'), QString::SkipEmptyParts));
#else
                childitem->setData(0, SpeedsRole, profile.attribute(QStringLiteral("speeds")).split(QLatin1Char(';'), Qt::SkipEmptyParts));
#endif
            }
            if (profile.hasAttribute(QStringLiteral("url"))) {
                childitem->setData(0, ExtraRole, profile.attribute(QStringLiteral("url")));
            }
            if (profile.hasAttribute(QStringLiteral("top_field_first"))) {
                childitem->setData(0, FieldRole, profile.attribute(QStringLiteral("top_field_first")));
            }
            if (editable) {
                childitem->setData(0, EditableRole, exportFile);
                if (exportFile.endsWith(QLatin1String("customprofiles.xml"))) {
                    childitem->setIcon(0, QIcon::fromTheme(QStringLiteral("favorite")));
                } else {
                    childitem->setIcon(0, QIcon::fromTheme(QStringLiteral("internet-services")));
                }
            }
            groupItem->addChild(childitem);
            node = doc.elementsByTagName(QStringLiteral("profile")).at(count);
            count++;
        }
        return;
    }

    int i = 0;
    QString groupName;
    QString profileName;

    QString prof_extension;
    QString renderer;
    QString params;
    QString standard;

    while (!groups.item(i).isNull()) {
        documentElement = groups.item(i).toElement();
        groupName = documentElement.attribute(QStringLiteral("name"), i18nc("Attribute Name", "Custom"));
        extension = documentElement.attribute(QStringLiteral("extension"), QString());
        renderer = documentElement.attribute(QStringLiteral("renderer"), QString());
        QList<QTreeWidgetItem *> foundGroup = m_view.formats->findItems(groupName, Qt::MatchExactly);
        QTreeWidgetItem *groupItem;
        if (!foundGroup.isEmpty()) {
            groupItem = foundGroup.takeFirst();
        } else {
            groupItem = new QTreeWidgetItem(QStringList(groupName));
            m_view.formats->addTopLevelItem(groupItem);
            groupItem->setExpanded(true);
        }

        QDomNode n = groups.item(i).firstChild();
        while (!n.isNull()) {
            if (n.toElement().tagName() != QLatin1String("profile")) {
                n = n.nextSibling();
                continue;
            }
            profileElement = n.toElement();
            profileName = profileElement.attribute(QStringLiteral("name"));
            standard = profileElement.attribute(QStringLiteral("standard"));
            params = profileElement.attribute(QStringLiteral("args")).simplified();

            if (replaceVorbisCodec && params.contains(QStringLiteral("acodec=vorbis"))) {
                // replace vorbis with libvorbis
                params = params.replace(QLatin1String("=vorbis"), QLatin1String("=libvorbis"));
            }
            if (replaceLibfaacCodec && params.contains(QStringLiteral("acodec=aac"))) {
                // replace libfaac with aac
                params = params.replace(QLatin1String("aac"), QLatin1String("libfaac"));
            }

            prof_extension = profileElement.attribute(QStringLiteral("extension"));
            if (!prof_extension.isEmpty()) {
                extension = prof_extension;
            }
            item = new QTreeWidgetItem(QStringList(profileName));
            item->setData(0, GroupRole, groupName);
            item->setData(0, ExtensionRole, extension);
            item->setData(0, RenderRole, renderer);
            item->setData(0, StandardRole, standard);
            item->setData(0, ParamsRole, params);
            if (params.contains(QLatin1String("%quality"))) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                item->setData(0, BitratesRole, profileElement.attribute(QStringLiteral("qualities")).split(QLatin1Char(','), QString::SkipEmptyParts));
#else
                item->setData(0, BitratesRole, profileElement.attribute(QStringLiteral("qualities")).split(QLatin1Char(','), Qt::SkipEmptyParts));
#endif
            } else if (params.contains(QLatin1String("%bitrate"))) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                item->setData(0, BitratesRole, profileElement.attribute(QStringLiteral("bitrates")).split(QLatin1Char(','), QString::SkipEmptyParts));
#else
                item->setData(0, BitratesRole, profileElement.attribute(QStringLiteral("bitrates")).split(QLatin1Char(','), Qt::SkipEmptyParts));
#endif
            }
            if (params.contains(QLatin1String("%audioquality"))) {
                item->setData(0, AudioBitratesRole,
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                              profileElement.attribute(QStringLiteral("audioqualities")).split(QLatin1Char(','), QString::SkipEmptyParts));
#else
                              profileElement.attribute(QStringLiteral("audioqualities")).split(QLatin1Char(','), Qt::SkipEmptyParts));
#endif
            } else if (params.contains(QLatin1String("%audiobitrate"))) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                item->setData(0, AudioBitratesRole, profileElement.attribute(QStringLiteral("audiobitrates")).split(QLatin1Char(','), QString::SkipEmptyParts));
#else
                item->setData(0, AudioBitratesRole, profileElement.attribute(QStringLiteral("audiobitrates")).split(QLatin1Char(','), Qt::SkipEmptyParts));
#endif
            }
            if (profileElement.hasAttribute(QStringLiteral("speeds"))) {
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                item->setData(0, SpeedsRole, profileElement.attribute(QStringLiteral("speeds")).split(QLatin1Char(';'), QString::SkipEmptyParts));
#else
                item->setData(0, SpeedsRole, profileElement.attribute(QStringLiteral("speeds")).split(QLatin1Char(';'), Qt::SkipEmptyParts));
#endif
            }
            if (profileElement.hasAttribute(QStringLiteral("url"))) {
                item->setData(0, ExtraRole, profileElement.attribute(QStringLiteral("url")));
            }
            groupItem->addChild(item);
            n = n.nextSibling();
        }

        ++i;
    }
}

void RenderWidget::setRenderJob(const QString &dest, int progress)
{
    RenderJobItem *item;
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
        item->setData(1, TimeRole, QDateTime::currentDateTime());
        slotCheckJob();
    } else {
        QDateTime startTime = item->data(1, TimeRole).toDateTime();
        qint64 elapsedTime = startTime.secsTo(QDateTime::currentDateTime());
        qint64 remaining = elapsedTime * (100 - progress) / progress;
        int days = static_cast<int>(remaining / 86400);
        int remainingSecs = static_cast<int>(remaining % 86400);
        QTime when = QTime(0, 0, 0, 0);
        when = when.addSecs(remainingSecs);
        QString est = (days > 0) ? i18np("%1 day ", "%1 days ", days) : QString();
        est.append(when.toString(QStringLiteral("hh:mm:ss")));
        QString t = i18n("Remaining time %1", est);
        item->setData(1, Qt::UserRole, t);
    }
}

void RenderWidget::setRenderStatus(const QString &dest, int status, const QString &error)
{
    RenderJobItem *item;
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
        QDateTime startTime = item->data(1, TimeRole).toDateTime();
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
        QString itemGroup = item->data(0, Qt::UserRole).toString();
        if (itemGroup == QLatin1String("dvd")) {
            emit openDvdWizard(item->text(1));
        } else if (itemGroup == QLatin1String("websites")) {
            QString url = item->metadata();
            if (!url.isEmpty()) {
                new KRun(QUrl::fromLocalFile(url), this);
            }
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
    /*
    for (int i = 0; i < m_view.running_jobs->topLevelItemCount(); ++i) {
        current = static_cast<RenderJobItem*>(m_view.running_jobs->topLevelItem(i));
        if (current == static_cast<RenderJobItem*> (m_view.running_jobs->currentItem())) {
            current->setSizeHint(1, QSize(m_view.running_jobs->columnWidth(1), fontMetrics().height() * 3));
        } else current->setSizeHint(1, QSize(m_view.running_jobs->columnWidth(1), fontMetrics().height() * 2));
    }*/
}

void RenderWidget::slotCLeanUpJobs()
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

    QTreeWidgetItem *item;
    // List the project scripts
    QDir projectFolder(pCore->currentDoc()->projectDataFolder());
    projectFolder.mkpath(QStringLiteral("kdenlive-renderqueue"));
    projectFolder.cd(QStringLiteral("kdenlive-renderqueue"));
    QStringList scriptFiles = projectFolder.entryList(scriptsFilter, QDir::Files);
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
        if (target.isEmpty()) {
            continue;
        }
        item = new QTreeWidgetItem(m_view.scripts_list, QStringList() << QString() << scriptpath.fileName());
        auto icon = QFileIconProvider().icon(QFileInfo(f));
        item->setIcon(0, icon.isNull() ? QIcon::fromTheme(QStringLiteral("application-x-executable-script")) : icon);
        item->setSizeHint(0, QSize(m_view.scripts_list->columnWidth(0), fontMetrics().height() * 2));
        item->setData(1, Qt::UserRole, QUrl(QUrl::fromEncoded(target.toUtf8())).url(QUrl::PreferLocalFile));
        item->setData(1, Qt::UserRole + 1, scriptpath.toLocalFile());
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
                    this, i18n("There is already a job writing file:<br /><b>%1</b><br />Abort the job if you want to overwrite it...", destination),
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
        renderItem->setData(1, Qt::UserRole, i18n("Waiting..."));
        renderItem->setData(1, TimeRole, QDateTime::currentDateTime());
        QStringList argsJob = {KdenliveSettings::rendererpath(), path, destination, QStringLiteral("-pid:%1").arg(QCoreApplication::applicationPid())};
        renderItem->setData(1, ParametersRole, argsJob);
        checkRenderStatus();
        m_view.tabWidget->setCurrentIndex(1);
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

void RenderWidget::slotGenerateScript()
{
    slotPrepareExport(true);
}

void RenderWidget::slotHideLog()
{
    m_view.error_box->setVisible(false);
}

void RenderWidget::setRenderProfile(const QMap<QString, QString> &props)
{
    m_view.scanning_list->setCurrentIndex(props.value(QStringLiteral("renderscanning")).toInt());
    m_view.field_order->setCurrentIndex(props.value(QStringLiteral("renderfield")).toInt());
    int exportAudio = props.value(QStringLiteral("renderexportaudio")).toInt();
    switch (exportAudio) {
    case 1:
        m_view.export_audio->setCheckState(Qt::Unchecked);
        break;
    case 2:
        m_view.export_audio->setCheckState(Qt::Checked);
        break;
    default:
        m_view.export_audio->setCheckState(Qt::PartiallyChecked);
    }
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
    if (props.contains(QStringLiteral("rendertcoverlay"))) {
        m_view.tc_overlay->setChecked(props.value(QStringLiteral("rendertcoverlay")).toInt() != 0);
    }
    if (props.contains(QStringLiteral("rendertctype"))) {
        m_view.tc_type->setCurrentIndex(props.value(QStringLiteral("rendertctype")).toInt());
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

    if (props.value(QStringLiteral("renderzone")) == QLatin1String("1")) {
        m_view.render_zone->setChecked(true);
    } else if (props.value(QStringLiteral("renderguide")) == QLatin1String("1")) {
        m_view.render_guide->setChecked(true);
        m_view.guide_start->setCurrentIndex(props.value(QStringLiteral("renderstartguide")).toInt());
        m_view.guide_end->setCurrentIndex(props.value(QStringLiteral("renderendguide")).toInt());
    } else {
        m_view.render_full->setChecked(true);
    }
    slotUpdateGuideBox();

    QString url = props.value(QStringLiteral("renderurl"));
    if (url.isEmpty()) {
        QTreeWidgetItem *item = m_view.formats->currentItem();
        if (item && item->parent()) { // categories have no parent
            const QString extension = item->data(0, ExtensionRole).toString();
            url = filenameWithExtension(QUrl::fromLocalFile(pCore->currentDoc()->projectDataFolder() + QDir::separator()), extension).toLocalFile();
        }
    } else if (QFileInfo(url).isRelative()) {
        url.prepend(pCore->currentDoc()->documentRoot());
    }
    m_view.out_file->setUrl(QUrl::fromLocalFile(url));

    if (props.contains(QStringLiteral("renderprofile")) || props.contains(QStringLiteral("rendercategory"))) {
        focusFirstVisibleItem(props.value(QStringLiteral("renderprofile")));
    }

    if (props.contains(QStringLiteral("renderquality"))) {
        m_view.video->setValue(props.value(QStringLiteral("renderquality")).toInt());
    } else if (props.contains(QStringLiteral("renderbitrate"))) {
        m_view.video->setValue(props.value(QStringLiteral("renderbitrate")).toInt());
    } else {
        m_view.quality->setValue(m_view.quality->maximum() * 3 / 4);
    }
    if (props.contains(QStringLiteral("renderaudioquality"))) {
        m_view.audio->setValue(props.value(QStringLiteral("renderaudioquality")).toInt());
    }
    if (props.contains(QStringLiteral("renderaudiobitrate"))) {
        m_view.audio->setValue(props.value(QStringLiteral("renderaudiobitrate")).toInt());
    }

    if (props.contains(QStringLiteral("renderspeed"))) {
        m_view.speed->setValue(props.value(QStringLiteral("renderspeed")).toInt());
    }
}

bool RenderWidget::startWaitingRenderJobs()
{
    m_blockProcessing = true;
#ifdef Q_OS_WIN
    const QLatin1String ScriptFormat(".bat");
#else
    const QLatin1String ScriptFormat(".sh");
#endif
    QTemporaryFile tmp(QDir::tempPath() + QStringLiteral("/kdenlive-XXXXXX") + ScriptFormat);
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
#ifndef Q_OS_WIN
    outStream << "#! /bin/sh" << '\n' << '\n';
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
        m_infoMessage->setMessageType(KMessageWidget::Warning);
        m_infoMessage->setText(fullMessage);
        m_infoMessage->show();
    } else {
        m_infoMessage->hide();
    }
}

void RenderWidget::slotUpdateEncodeThreads(int val)
{
    KdenliveSettings::setEncodethreads(val);
}

void RenderWidget::slotUpdateRescaleWidth(int val)
{
    KdenliveSettings::setDefaultrescalewidth(val);
    if (!m_view.rescale_keep->isChecked()) {
        return;
    }
    m_view.rescale_height->blockSignals(true);
    std::unique_ptr<ProfileModel> &profile = pCore->getCurrentProfile();
    m_view.rescale_height->setValue(val * profile->height() / profile->width());
    KdenliveSettings::setDefaultrescaleheight(m_view.rescale_height->value());
    m_view.rescale_height->blockSignals(false);
}

void RenderWidget::slotUpdateRescaleHeight(int val)
{
    KdenliveSettings::setDefaultrescaleheight(val);
    if (!m_view.rescale_keep->isChecked()) {
        return;
    }
    m_view.rescale_width->blockSignals(true);
    std::unique_ptr<ProfileModel> &profile = pCore->getCurrentProfile();
    m_view.rescale_width->setValue(val * profile->width() / profile->height());
    KdenliveSettings::setDefaultrescaleheight(m_view.rescale_width->value());
    m_view.rescale_width->blockSignals(false);
}

void RenderWidget::slotSwitchAspectRatio()
{
    KdenliveSettings::setRescalekeepratio(m_view.rescale_keep->isChecked());
    if (m_view.rescale_keep->isChecked()) {
        slotUpdateRescaleWidth(m_view.rescale_width->value());
    }
}

void RenderWidget::slotUpdateAudioLabel(int ix)
{
    if (ix == Qt::PartiallyChecked) {
        m_view.export_audio->setText(i18n("Export audio (automatic)"));
    } else {
        m_view.export_audio->setText(i18n("Export audio"));
    }
    m_view.stemAudioExport->setEnabled(ix != Qt::Unchecked);
}

bool RenderWidget::automaticAudioExport() const
{
    return (m_view.export_audio->checkState() == Qt::PartiallyChecked);
}

bool RenderWidget::selectedAudioExport() const
{
    return (m_view.export_audio->checkState() != Qt::Unchecked);
}

void RenderWidget::updateProxyConfig(bool enable)
{
    m_view.proxy_render->setHidden(!enable);
}

bool RenderWidget::proxyRendering()
{
    return m_view.proxy_render->isChecked();
}

bool RenderWidget::isStemAudioExportEnabled() const
{
    return (m_view.stemAudioExport->isChecked() && m_view.stemAudioExport->isVisible() && m_view.stemAudioExport->isEnabled());
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
        case 1:
            if (m_view.start_job->isEnabled()) {
                slotStartCurrentJob();
            }
            break;
        case 2:
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
        if (m_view.tabWidget->currentIndex() == 2) {
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

void RenderWidget::adjustAVQualities(int quality)
{
    // calculate video/audio quality indexes from the general quality cursor
    // taking into account decreasing/increasing video/audio quality parameter
    double q = double(quality) / m_view.quality->maximum();

    int dq = int(q * (m_view.video->maximum() - m_view.video->minimum()));
    // prevent video spinbox to update quality cursor (loop)
    m_view.video->blockSignals(true);
    m_view.video->setValue(m_view.video->property("decreasing").toBool() ? m_view.video->maximum() - dq : m_view.video->minimum() + dq);
    m_view.video->blockSignals(false);
    dq = int(q * (m_view.audio->maximum() - m_view.audio->minimum()));
    dq -= dq % m_view.audio->singleStep(); // keep a 32 pitch  for bitrates
    m_view.audio->setValue(m_view.audio->property("decreasing").toBool() ? m_view.audio->maximum() - dq : m_view.audio->minimum() + dq);
}

void RenderWidget::adjustQuality(int videoQuality)
{
    int dq = videoQuality * m_view.quality->maximum() / (m_view.video->maximum() - m_view.video->minimum());
    m_view.quality->blockSignals(true);
    m_view.quality->setValue(m_view.video->property("decreasing").toBool() ? m_view.video->maximum() - dq : m_view.video->minimum() + dq);
    m_view.quality->blockSignals(false);
}

void RenderWidget::adjustSpeed(int speedIndex)
{
    if (m_view.formats->currentItem()) {
        QStringList speeds = m_view.formats->currentItem()->data(0, SpeedsRole).toStringList();
        if (speedIndex < speeds.count()) {
            m_view.speed->setToolTip(i18n("Codec speed parameters:\n%1", speeds.at(speedIndex)));
        }
    }
}

void RenderWidget::checkCodecs()
{
    Mlt::Profile p;
    auto *consumer = new Mlt::Consumer(p, "avformat");
    if (consumer) {
        consumer->set("vcodec", "list");
        consumer->set("acodec", "list");
        consumer->set("f", "list");
        consumer->start();
        vcodecsList.clear();
        Mlt::Properties vcodecs(mlt_properties(consumer->get_data("vcodec")));
        vcodecsList.reserve(vcodecs.count());
        for (int i = 0; i < vcodecs.count(); ++i) {
            vcodecsList << QString(vcodecs.get(i));
        }
        acodecsList.clear();
        Mlt::Properties acodecs(mlt_properties(consumer->get_data("acodec")));
        acodecsList.reserve(acodecs.count());
        for (int i = 0; i < acodecs.count(); ++i) {
            acodecsList << QString(acodecs.get(i));
        }
        supportedFormats.clear();
        Mlt::Properties formats(mlt_properties(consumer->get_data("f")));
        supportedFormats.reserve(formats.count());
        for (int i = 0; i < formats.count(); ++i) {
            supportedFormats << QString(formats.get(i));
        }
        delete consumer;
    }
}

void RenderWidget::slotProxyWarn(bool enableProxy)
{
    errorMessage(ProxyWarning, enableProxy ? i18n("Rendering using low quality proxy") : QString());
}

void RenderWidget::prepareMenu(const QPoint &pos)
{
    QTreeWidgetItem *nd = m_view.running_jobs->itemAt( pos );
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
