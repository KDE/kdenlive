/*
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "renderwidget.h"
#include "bin/bin.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "dialogs/renderpresetdialog.h"
#include "dialogs/wizard.h"
#include "doc/kdenlivedoc.h"
#include "kdenlivesettings.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "monitor/monitormanager.h"
#include "profiles/profilemodel.hpp"
#include "profiles/profilerepository.hpp"
#include "project/projectmanager.h"
#include "render/renderrequest.h"
#include "utils/qstringutils.h"
#include "utils/timecode.h"
#include "xml/xml.hpp"

#include "renderpresets/renderpresetmodel.hpp"
#include "renderpresets/renderpresetrepository.hpp"

#include <KColorScheme>
#include <KIO/DesktopExecParser>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenFileManagerWindowJob>
#include <KIO/OpenUrlJob>
#include <KLineEdit>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNotification>
#include <KWindowConfig>
#include <kmemoryinfo.h>

#include "kdenlive_debug.h"
#ifndef NODBUS
#include <QDBusConnectionInterface>
#endif
#include <QDesktopServices>
#include <QDir>
#include <QDomDocument>
#include <QFileIconProvider>
#include <QHeaderView>
#include <QHelpEvent>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeDatabase>
#include <QProcess>
#include <QScreen>
#include <QScrollBar>
#include <QStandardPaths>
#include <QString>
#include <QTemporaryFile>
#include <QThread>
#include <QToolTip>
#include <QTreeWidgetItem>
#include <QtGlobal>

#include <Purpose/AlternativesModel>
#include <Purpose/Menu>

#include <locale>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif

// Running job status
enum JOBSTATUS { WAITINGJOB = 0, STARTINGJOB, RUNNINGJOB, FINISHEDJOB, FAILEDJOB, ABORTEDJOB };

RenderViewDelegate::RenderViewDelegate(QWidget *parent)
    : QStyledItemDelegate(parent)
{
}

bool RenderViewDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (index.column() != 1 || !index.isValid()) {
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }
    // ... for non-tooltip events
    if (event->type() != QEvent::ToolTip) {
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }
    // Show custom tooltips when over audio/video drag areas
    const QPoint pos = event->pos() - QPoint(0, option.rect.top());
    if (m_playlistRect.contains(pos)) {
        const QString playlistFile = index.data(RenderWidget::PlaylistDisplayRole).toString();
        QToolTip::showText(event->globalPos(), i18n("Show xml playlist used for rendering:\n%1", playlistFile), view);
        return true;
    }
    if (m_logRect.contains(pos)) {
        const QString logFile = index.data(RenderWidget::LogFileRole).toString();
        QToolTip::showText(event->globalPos(), i18n("Show render log file:\n %1", logFile), view);
        return true;
    }

    // Otherwise, show default tooltip
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}

bool RenderViewDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (index.isValid()) {
        const QString playlistFile = index.data(RenderWidget::PlaylistDisplayRole).toString();
        const QString logFile = index.data(RenderWidget::LogFileRole).toString();
        if (logFile.isEmpty() && playlistFile.isEmpty()) {
            return QStyledItemDelegate::editorEvent(event, model, option, index);
        }
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (index.column() == 1) {
                const QPoint pos = me->pos() - QPoint(0, option.rect.top());
                if (m_logRect.contains(pos)) {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(logFile));
                    event->accept();
                    return true;
                }
                if (m_playlistRect.contains(pos)) {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(playlistFile));
                    event->accept();
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (index.column() == 1) {
                const QPoint pos = me->pos() - QPoint(0, option.rect.top());
                if (m_logRect.contains(pos) || m_playlistRect.contains(pos)) {
                    Q_EMIT hoverLink(true);
                } else {
                    Q_EMIT hoverLink(false);
                }
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

void RenderViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.column() == 1) {
        painter->save();
        QStyleOptionViewItem opt(option);
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

        QFont font = painter->font();
        font.setBold(true);
        painter->setFont(font);
        QRect r1 = option.rect;
        r1.adjust(0, textMargin, 0, -textMargin);
        int mid = int((r1.height() / 2));
        r1.setBottom(r1.y() + mid);
        QRect bounding;
        painter->drawText(r1, Qt::AlignLeft | Qt::AlignTop, index.data().toString(), &bounding);
        r1.moveTop(r1.bottom() - textMargin);
        font.setBold(false);
        painter->setFont(font);
        painter->drawText(r1, Qt::AlignLeft | Qt::AlignTop, index.data(Qt::UserRole).toString());
        int progress = index.data(RenderWidget::ProgressRole).toInt();
        if (progress > 0 && progress < 100) {
            // draw progress bar
            QColor color = option.palette.alternateBase().color();
            QColor fgColor = option.palette.text().color();
            color.setAlpha(150);
            fgColor.setAlpha(150);
            painter->setBrush(QBrush(color));
            painter->setPen(QPen(fgColor));
            int width = qMin(200, r1.width() - 4);
            QRect bgrect(r1.left() + 2, option.rect.bottom() - 6 - textMargin, width, 6);
            painter->drawRoundedRect(bgrect, 3, 3);
            painter->setBrush(QBrush(fgColor));
            bgrect.adjust(2, 2, 0, -1);
            painter->setPen(Qt::NoPen);
            bgrect.setWidth((width - 2) * progress / 100);
            painter->drawRect(bgrect);
        }
        r1.setBottom(opt.rect.bottom());
        r1.setTop(r1.bottom() - mid);
        painter->drawText(r1, Qt::AlignLeft | Qt::AlignBottom, index.data(RenderWidget::ExtraInfoRole).toString());
        if (!index.data(RenderWidget::LogFileRole).toString().isEmpty()) {
            QFont ft = painter->font();
            ft.setUnderline(true);
            painter->setFont(ft);
            QPalette pal = QApplication::palette();
            if ((option.state & static_cast<int>(QStyle::State_Selected)) != 0) {
                painter->setPen(option.palette.highlightedText().color());
            } else {
                painter->setPen(pal.link().color());
            }
            r1.adjust(0, 0, -textMargin, -textMargin);
            if (m_logRect.isNull()) {
                painter->drawText(r1, Qt::AlignRight | Qt::AlignBottom, i18n("Show Log"), &m_logRect);
                // On first show, invalidate xml rect as it will change
                m_playlistRect = QRect();
            } else {
                painter->drawText(r1, Qt::AlignRight | Qt::AlignBottom, i18n("Show Log"));
            }
        }
        if (!index.data(RenderWidget::PlaylistDisplayRole).toString().isEmpty()) {
            QFont ft = painter->font();
            ft.setUnderline(true);
            painter->setFont(ft);
            QPalette pal = QApplication::palette();
            if ((option.state & static_cast<int>(QStyle::State_Selected)) != 0) {
                painter->setPen(option.palette.highlightedText().color());
            } else {
                painter->setPen(pal.link().color());
            }
            r1.adjust(0, 0, -m_logRect.width() - textMargin, 0);
            if (m_playlistRect.isNull()) {
                painter->drawText(r1, Qt::AlignRight | Qt::AlignBottom, i18n("Show Xml"), &m_playlistRect);
            } else {
                painter->drawText(r1, Qt::AlignRight | Qt::AlignBottom, i18n("Show Xml"));
            }
        }
        painter->restore();
    } else {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

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
    case STARTINGJOB:
        setIcon(0, QIcon::fromTheme(QStringLiteral("media-record")));
        setData(1, Qt::UserRole, i18n("Starting…"));
        break;
    case FINISHEDJOB:
        setData(1, Qt::UserRole, i18n("Rendering finished"));
        setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-ok")));
        setData(1, RenderWidget::ProgressRole, 100);
        break;
    case FAILEDJOB:
        setData(1, Qt::UserRole, i18n("Rendering crashed"));
        setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-close")));
        setData(1, RenderWidget::ProgressRole, 100);
        break;
    case ABORTEDJOB:
        setData(1, Qt::UserRole, i18n("Rendering aborted"));
        setIcon(0, QIcon::fromTheme(QStringLiteral("dialog-cancel")));
        setData(1, RenderWidget::ProgressRole, 100);
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
    KMemoryInfo memInfo;
    if (!memInfo.isNull()) {
        // Low memory is considered when less than 10% of the physical memory is available
        int totalMemory = memInfo.totalPhysical() / 1024 / 1024;
        m_lowMemThreshold = qMax(1000, totalMemory / 10);
        // Very Low memory is considered when less than 5% of the physical memory is available
        m_veryLowMemThreshold = qMax(500, totalMemory / 20);
    } else {
        m_lowMemThreshold = 1000;
        m_veryLowMemThreshold = 1000;
    }

    // ===== "Render Project" tab =====
    m_view.buttonDelete->setIconSize(iconSize);
    m_view.buttonEdit->setIconSize(iconSize);
    m_view.buttonNew->setIconSize(iconSize);
    m_view.buttonSaveAs->setIconSize(iconSize);
    m_view.m_knsbutton->setIconSize(iconSize);

    m_view.buttonRender->setEnabled(false);
    m_view.buttonGenerateScript->setEnabled(false);

    connect(m_view.profileTree, &QTreeView::doubleClicked, this, [&](const QModelIndex &index) {
        if (!index.isValid() || index.parent() == QModelIndex()) {
            // This is a top level item - group - don't edit
            return;
        }
        slotEditPreset();
    });
    connect(m_view.buttonNew, &QAbstractButton::clicked, this, &RenderWidget::slotNewPreset);
    connect(m_view.buttonEdit, &QAbstractButton::clicked, this, &RenderWidget::slotEditPreset);
    connect(m_view.buttonDelete, &QAbstractButton::clicked, this, [this]() {
        RenderPresetRepository::get()->deletePreset(m_currentProfile);
        m_currentProfile = QString();
        parseProfiles();
        refreshView();
    });
    connect(m_view.buttonSaveAs, &QAbstractButton::clicked, this, &RenderWidget::slotSavePresetAs);

    connect(m_view.m_knsbutton, &KNSWidgets::Button::dialogFinished, this, [&](const QList<KNSCore::Entry> &changedEntries) {
        if (changedEntries.count() > 0) {
            parseProfiles();
        }
    });

    m_view.optionsGroup->setVisible(m_view.options->isChecked());
    m_view.optionsGroup->setMinimumWidth(m_view.optionsGroup->width() + m_view.optionsGroup->verticalScrollBar()->width());
    connect(m_view.options, &QAbstractButton::toggled, m_view.optionsGroup, &QWidget::setVisible);

    connect(m_view.out_file, &KUrlRequester::textChanged, this, static_cast<void (RenderWidget::*)()>(&RenderWidget::slotUpdateButtons));
    connect(m_view.out_file, &KUrlRequester::urlSelected, this, static_cast<void (RenderWidget::*)(const QUrl &)>(&RenderWidget::slotUpdateButtons));
    connect(m_view.out_file->lineEdit(), &KLineEdit::editingFinished, this, [this]() {
        const QUrl url = m_view.out_file->url();
        std::unique_ptr<RenderPresetModel> &profile = RenderPresetRepository::get()->getPreset(m_currentProfile);
        qDebug() << "HHHHHHHHHHHHHHH\nEDITING FINISHED; URL: " << url;
        m_view.out_file->setUrl(filenameWithExtension(url, profile->extension()));
    });

    connect(m_view.guide_multi_box, &QGroupBox::toggled, this, &RenderWidget::slotRenderModeChanged);
    connect(m_view.render_guide, &QAbstractButton::clicked, this, &RenderWidget::slotRenderModeChanged);
    connect(m_view.render_zone, &QAbstractButton::clicked, this, &RenderWidget::slotRenderModeChanged);
    connect(m_view.render_full, &QAbstractButton::clicked, this, &RenderWidget::slotRenderModeChanged);

    connect(m_view.guide_end, static_cast<void (KComboBox::*)(int)>(&KComboBox::activated), this, &RenderWidget::slotCheckStartGuidePosition);
    connect(m_view.guide_start, static_cast<void (KComboBox::*)(int)>(&KComboBox::activated), this, &RenderWidget::slotCheckEndGuidePosition);
    connect(m_view.guide_start, static_cast<void (KComboBox::*)(int)>(&KComboBox::activated), this, &RenderWidget::updateGuideEndOptionsForStartSelection);

    m_view.guide_zone_box->setVisible(false);

    // === "More Options" widget ===
    setRescaleEnabled(false);
    m_view.error_box->setVisible(false);

    // Interpolation
    m_view.interp_type->addItem(i18n("Nearest (fast)"), QStringLiteral("nearest"));
    m_view.interp_type->addItem(i18n("Bilinear (good)"), QStringLiteral("bilinear"));
    m_view.interp_type->addItem(i18n("Bicubic (better)"), QStringLiteral("bicubic"));
    m_view.interp_type->addItem(i18n("Lanczos (best)"), QStringLiteral("hyper"));
    int ix = m_view.interp_type->findData(KdenliveSettings::renderInterp());
    if (ix > -1) {
        m_view.interp_type->setCurrentIndex(ix);
    }
    connect(m_view.interp_type, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [&]() { KdenliveSettings::setRenderInterp(m_view.interp_type->currentData().toString()); });
    
    // Aspect Ratio
    m_view.aspect_ratio_type->addItem(i18n("Default"));
    m_view.aspect_ratio_type->addItem(i18n("Horizontal (16:9)"), QStringLiteral("horizontal"));
    m_view.aspect_ratio_type->addItem(i18n("Vertical (9:16)"), QStringLiteral("vertical"));
    m_view.aspect_ratio_type->addItem(i18n("Square (1:1)"), QStringLiteral("square"));
    m_view.aspect_ratio_type->setCurrentIndex(0);

    // Deinterlacer
    m_view.deinterlacer_type->addItem(i18n("One Field (fast)"), QStringLiteral("onefield"));
    m_view.deinterlacer_type->addItem(i18n("Linear Blend (fast)"), QStringLiteral("linearblend"));
    m_view.deinterlacer_type->addItem(i18n("YADIF - temporal only (good)"), QStringLiteral("yadif-nospatial"));
    m_view.deinterlacer_type->addItem(i18n("YADIF (better)"), QStringLiteral("yadif"));
    m_view.deinterlacer_type->addItem(i18n("BWDIF (best)"), QStringLiteral("bwdif"));
    ix = m_view.deinterlacer_type->findData(KdenliveSettings::renderDeinterlacer());
    if (ix > -1) {
        m_view.deinterlacer_type->setCurrentIndex(ix);
    }
    connect(m_view.deinterlacer_type, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            [&]() { KdenliveSettings::setRenderDeinterlacer(m_view.deinterlacer_type->currentData().toString()); });

    m_view.tc_type->addItem(i18n("None"));
    m_view.tc_type->addItem(i18n("Timecode"), QStringLiteral("#timecode#"));
    m_view.tc_type->addItem(i18n("Timecode Non Drop Frame"), QStringLiteral("#smtpe_ndf#"));
    m_view.tc_type->addItem(i18n("Frame Number"), QStringLiteral("#frame#"));
    m_view.checkTwoPass->setEnabled(false);
    m_view.checkTwoPass->setToolTip(i18nc("Explanation for the 2 pass rendering feature",
                                          "Two pass rendering allows a better control over the final rendered file size.\nNot compatible "
                                          "with variable bitrate, and only relevant for some video codecs."));
    m_view.proxy_render->setHidden(!enableProxy);
    connect(m_view.proxy_render, &QCheckBox::toggled, this,
            [&](bool enabled) { errorMessage(ProxyWarning, enabled ? i18n("Rendering using low quality proxy") : QString()); });

    connect(m_view.quality, &QAbstractSlider::valueChanged, this, &RenderWidget::refreshParams);
    connect(m_view.qualityGroup, &QGroupBox::toggled, this, &RenderWidget::refreshParams);
    connect(m_view.speed, &QAbstractSlider::valueChanged, this, &RenderWidget::adjustSpeed);

    m_view.encoder_threads->setMaximum(QThread::idealThreadCount());
    m_view.encoder_threads->setValue(KdenliveSettings::encodethreads());
    connect(m_view.encoder_threads, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &KdenliveSettings::setEncodethreads);
    connect(m_view.encoder_threads, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderWidget::refreshParams);

    connect(m_view.video_box, &QGroupBox::toggled, this, &RenderWidget::refreshParams);
    connect(m_view.audio_box, &QGroupBox::toggled, this, &RenderWidget::refreshParams);
    connect(m_view.rescale, &QAbstractButton::toggled, this, &RenderWidget::setRescaleEnabled);
    connect(m_view.rescale, &QAbstractButton::toggled, this, &RenderWidget::refreshParams);
    connect(m_view.rescale_width, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderWidget::slotUpdateRescaleWidth);
    connect(m_view.rescale_height, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderWidget::slotUpdateRescaleHeight);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(m_view.render_at_preview_res, &QCheckBox::checkStateChanged, this, &RenderWidget::refreshParams);
    connect(m_view.render_full_color, &QCheckBox::checkStateChanged, this, &RenderWidget::refreshParams);
#else
    connect(m_view.render_at_preview_res, &QCheckBox::stateChanged, this, &RenderWidget::refreshParams);
    connect(m_view.render_full_color, &QCheckBox::stateChanged, this, &RenderWidget::refreshParams);
#endif
    m_view.processing_threads->setMaximum(QThread::idealThreadCount());
    m_view.processing_threads->setValue(KdenliveSettings::processingthreads());
    connect(m_view.processing_threads, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &KdenliveSettings::setProcessingthreads);
    connect(m_view.processing_threads, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &RenderWidget::refreshParams);
    if (!KdenliveSettings::parallelrender()) {
        m_view.processing_warning->hide();
    }
    m_view.processing_box->setChecked(KdenliveSettings::parallelrender());
#if QT_POINTER_SIZE == 4
    // On 32-bit process, limit multi-threading to mitigate running out of memory.
    m_view.processing_box->setChecked(false);
    m_view.processing_box->setEnabled(false);
    m_view.processing_warning->hide();
#endif
    if (KdenliveSettings::gpu_accel()) {
        // Disable parallel rendering for movit
        m_view.processing_box->setChecked(false);
        m_view.processing_warning->hide();
        m_view.processing_box->setEnabled(false);
    }
    connect(m_view.processing_box, &QGroupBox::toggled, this, [&](bool checked) {
        KdenliveSettings::setParallelrender(checked);
        if (checked) {
            m_view.processing_warning->animatedShow();
        } else {
            m_view.processing_warning->animatedHide();
        }
        refreshParams();
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(m_view.export_meta, &QCheckBox::checkStateChanged, this, &RenderWidget::refreshParams);
    connect(m_view.checkTwoPass, &QCheckBox::checkStateChanged, this, &RenderWidget::refreshParams);
#else
    connect(m_view.export_meta, &QCheckBox::stateChanged, this, &RenderWidget::refreshParams);
    connect(m_view.checkTwoPass, &QCheckBox::stateChanged, this, &RenderWidget::refreshParams);
#endif
    connect(m_view.buttonRender, &QAbstractButton::clicked, this, [&]() { slotPrepareExport(); });
    connect(m_view.buttonGenerateScript, &QAbstractButton::clicked, this, [&]() { slotPrepareExport(true); });
    updateMetadataToolTip();
    connect(m_view.edit_metadata, &QLabel::linkActivated, []() { pCore->window()->slotEditProjectSettings(3); });

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

    m_view.outfileLabel->setBuddy(m_view.out_file);

    m_jobsDelegate = new RenderViewDelegate(this);
    m_view.running_jobs->setHeaderLabels(QStringList() << QString() << i18n("File"));
    m_view.running_jobs->setItemDelegate(m_jobsDelegate);
    m_view.running_jobs->setMouseTracking(true);
    connect(m_jobsDelegate, &RenderViewDelegate::hoverLink, this, [this](bool hover) {
        if (hover) {
            setCursor(Qt::PointingHandCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    });

    QHeaderView *header = m_view.running_jobs->header();
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    header->resizeSection(0, size + 4);
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    // ===== "Scripts" tab =====
    m_view.scripts_list->setHeaderLabels(QStringList() << QString() << i18n("Stored Playlists"));
    m_scriptsDelegate = new RenderViewDelegate(this);
    m_view.scripts_list->setItemDelegate(m_scriptsDelegate);
    header = m_view.scripts_list->header();
    header->setSectionResizeMode(0, QHeaderView::Fixed);
    header->resizeSection(0, size + 4);

    // Find path for Kdenlive renderer
    if (KdenliveSettings::kdenliverendererpath().isEmpty() || !QFileInfo::exists(KdenliveSettings::kdenliverendererpath())) {
        KdenliveSettings::setKdenliverendererpath(QString());
        Wizard::fixKdenliveRenderPath();
    } else {
        // Ensure the Kdenlive renderer is in the same folder as the running app
        qDebug() << ":::::::::: COMPARING RENDERER PATH: " << QCoreApplication::applicationDirPath()
                 << " != " << QFileInfo(KdenliveSettings::kdenliverendererpath()).absolutePath();
        if (QCoreApplication::applicationDirPath() != QFileInfo(KdenliveSettings::kdenliverendererpath()).absolutePath()) {
            KdenliveSettings::setKdenliverendererpath(QString());
            Wizard::fixKdenliveRenderPath();
        }
    }
    if (KdenliveSettings::kdenliverendererpath().isEmpty()) {
        KMessageBox::error(this, i18n("Could not find the kdenlive_render application, something is wrong with your installation. Rendering will not work"));
    }

#ifndef NODBUS
    QDBusConnectionInterface *interface = QDBusConnection::sessionBus().interface();
    if ((interface == nullptr) ||
        (!interface->isServiceRegistered(QStringLiteral("org.kde.ksmserver")) && !interface->isServiceRegistered(QStringLiteral("org.gnome.SessionManager")))) {
        m_view.shutdown->setEnabled(false);
    }
#else
    m_view.shutdown->setEnabled(false);
#endif

    m_shareMenu = new Purpose::Menu();
    m_view.shareButton->setMenu(m_shareMenu);
    m_view.shareButton->setIcon(QIcon::fromTheme(QStringLiteral("document-share")));
    connect(m_shareMenu, &Purpose::Menu::finished, this, &RenderWidget::slotShareActionFinished);

    // Search
    m_view.searchLine->setClearButtonEnabled(true);
    m_view.searchLine->setPlaceholderText(i18n("Search…"));
    connect(m_view.searchLine, &QLineEdit::textChanged, this, [this](const QString &str) {
        m_proxyModel->slotSetSearchString(str);
        if (str.isEmpty()) {
            // focus last selected item when clearing search line
            focusItem(m_currentProfile);
        } else {
            m_view.profileTree->expandAll();
        }
    });

    // Memory check timer
    connect(&m_memCheckTimer, &QTimer::timeout, this, &RenderWidget::slotCheckFreeMemory);
    // Default interval check is 10 seconds
    m_memCheckTimer.setInterval(10000);
    m_memCheckTimer.setSingleShot(false);

    loadConfig();
    refreshView();
    focusItem();
    KSharedConfig::Ptr conf = KSharedConfig::openConfig();
    winId(); // Make sure window gets created before getting the handle
    QWindow *handle = windowHandle();
    if ((handle != nullptr) && conf->hasGroup("RenderDialogSize")) {
        KWindowConfig::restoreWindowSize(handle, conf->group("RenderDialogSize"));
        resize(handle->size());
    }
    m_view.embed_subtitles->setToolTip(i18n("Only works for the matroska (mkv) format"));
    connect(this, &RenderWidget::renderStatusChanged, this, &RenderWidget::updatePowerManagement);
    m_view.keep_log_files->setChecked(KdenliveSettings::keepRenderLogFiles());
    connect(m_view.keep_log_files, &QCheckBox::toggled, this, [](bool enabled) { KdenliveSettings::setKeepRenderLogFiles(enabled); });

    m_lowSpaceTimer.setSingleShot(true);
    m_lowSpaceTimer.setInterval(3000);
    connect(&m_lowSpaceTimer, &QTimer::timeout, this, &RenderWidget::checkDriveSpace, Qt::QueuedConnection);
    checkDriveSpace();
}

void RenderWidget::slotShareActionFinished(const QJsonObject &output, int error, const QString &message)
{
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
}

QSize RenderWidget::sizeHint() const
{
    // Make sure the widget has minimum size on opening
    return {200, qMax(200, screen()->availableGeometry().height())};
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
    QWindow *handle = windowHandle();
    KConfigGroup group = config->group("RenderDialogSize");
    if (handle) {
        KWindowConfig::saveWindowSize(handle, group);
    }
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
    m_view.out_file->setStartDir(QUrl::fromLocalFile(pCore->currentDoc()->projectRenderFolder()));
    if (m_view.out_file->url().isEmpty()) {
        return;
    }
    const QString fileName = m_view.out_file->url().fileName();
    m_view.out_file->setUrl(QUrl::fromLocalFile(QDir(pCore->currentDoc()->projectRenderFolder()).absoluteFilePath(fileName)));
    parseScriptFiles();
}

void RenderWidget::slotRenderModeChanged()
{
    m_view.guide_zone_box->setVisible(m_view.render_guide->isChecked());
    m_view.buttonGenerateScript->setVisible(!m_view.guide_multi_box->isChecked());
    showRenderDuration();
}

void RenderWidget::slotUpdateRescaleWidth(int val)
{
    KdenliveSettings::setDefaultrescalewidth(val);
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
    m_view.rescale_width->blockSignals(true);
    std::unique_ptr<ProfileModel> &profile = pCore->getCurrentProfile();
    m_view.rescale_width->setValue(val * profile->width() / profile->height());
    KdenliveSettings::setDefaultrescaleheight(m_view.rescale_width->value());
    m_view.rescale_width->blockSignals(false);
    refreshParams();
}

void RenderWidget::slotCheckStartGuidePosition()
{
    if (m_view.guide_start->currentIndex() > m_view.guide_end->currentIndex()) {
        m_view.guide_start->setCurrentIndex(m_view.guide_end->currentIndex());
    }
    showRenderDuration();
}

void RenderWidget::slotCheckEndGuidePosition()
{
    if (m_view.guide_end->currentIndex() < m_view.guide_start->currentIndex()) {
        m_view.guide_end->setCurrentIndex(m_view.guide_start->currentIndex());
    }
    showRenderDuration();
}

void RenderWidget::updateGuideEndOptionsForStartSelection()
{
    int currentIndex = m_view.guide_start->currentIndex();

    if (currentIndex <= 0 || m_currentMarkers.isEmpty()) {
        m_view.guide_end->setEnabled(true);
        return;
    }

    int markerIndex = currentIndex - 1;
    if (markerIndex < m_currentMarkers.size()) {
        const CommentedTime &marker = m_currentMarkers.at(markerIndex);
        if (marker.hasRange()) {
            m_view.guide_end->setEnabled(false);
            double endTime = marker.endTime().seconds();
            int endIndex = m_view.guide_end->findData(endTime);
            if (endIndex >= 0) {
                m_view.guide_end->setCurrentIndex(endIndex);
            }
        } else {
            m_view.guide_end->setEnabled(true);
        }
    }

    showRenderDuration();
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
    QVariant startData = m_view.guide_start->currentData();
    QVariant endData = m_view.guide_end->currentData();
    m_view.guide_start->clear();
    m_view.guide_end->clear();
    if (auto ptr = m_guidesModel.lock()) {
        int sequenceOffset = pCore->currentTimelineOffset();
        m_view.guideCategoryChooser->setMarkerModel(ptr.get());
        QList<CommentedTime> markers = ptr->getAllMarkers();
        m_currentMarkers = markers;
        double fps = pCore->getCurrentFps();
        m_view.render_guide->setDisabled(markers.isEmpty());
        m_view.guide_multi_box->setDisabled(markers.isEmpty());
        if (markers.isEmpty()) {
            m_view.guide_multi_box->setChecked(false);
        }
        if (!markers.isEmpty()) {
            QIcon zoneIn = QIcon::fromTheme(QStringLiteral("zone-in"));
            QIcon zoneOut = QIcon::fromTheme(QStringLiteral("zone-out"));
            QIcon zoneRange = QIcon::fromTheme(QStringLiteral("timeline-use-zone-on"));

            m_view.guide_start->addItem(zoneIn, i18n("Beginning"), 0);
            for (const auto &marker : std::as_const(markers)) {
                GenTime pos = marker.time();
                const QString guidePos = Timecode::getStringTimecode(pos.frames(fps) + sequenceOffset, fps);
                QString displayText = marker.comment() + QLatin1Char('/') + guidePos;

                if (marker.hasRange()) {
                    GenTime duration = marker.duration();
                    const QString durationString = Timecode::formatMarkerDuration(duration.frames(fps), fps);
                    displayText.append(QStringLiteral(" (%1)").arg(durationString));

                    m_view.guide_start->addItem(zoneRange, displayText, pos.seconds());
                    m_view.guide_end->addItem(zoneRange, displayText, pos.seconds());
                } else {
                    m_view.guide_start->addItem(zoneIn, displayText, pos.seconds());
                    m_view.guide_end->addItem(zoneOut, displayText, pos.seconds());
                }
            }
            m_view.guide_end->addItem(zoneOut, i18n("End"), -1);
            if (!startData.isNull()) {
                int ix = qMax(0, m_view.guide_start->findData(startData));
                m_view.guide_start->setCurrentIndex(ix);
            }
            if (!endData.isNull()) {
                int ix = qMax(m_view.guide_start->currentIndex(), m_view.guide_end->findData(endData));
                m_view.guide_end->setCurrentIndex(ix);
            }
        } else {
            if (m_view.render_guide->isChecked()) {
                m_view.render_full->setChecked(true);
            }
            m_view.guide_multi_box->setChecked(false);
        }
    } else {
        m_view.render_guide->setEnabled(false);
        m_view.guide_multi_box->setEnabled(false);
        if (m_view.render_guide->isChecked()) {
            m_view.render_full->setChecked(true);
        }
        m_view.guide_multi_box->setChecked(false);
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
        QStorageInfo info(QFileInfo(url.toLocalFile()).absolutePath());
        if (m_lastCheckedDevice != info.device()) {
            // User selected a new device, check now if it has enough space
            checkDriveSpace();
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

void RenderWidget::focusItem(const QString &profile)
{
    QItemSelectionModel *selection = m_view.profileTree->selectionModel();
    QModelIndex index = m_treeModel->findPreset(profile);
    if (!index.isValid() && RenderPresetRepository::get()->presetExists(KdenliveSettings::renderProfile())) {
        index = m_treeModel->findPreset(KdenliveSettings::renderProfile());
    }
    if (index.isValid()) {
        auto mapped = m_proxyModel->mapFromSource(index);
        selection->select(mapped, QItemSelectionModel::ClearAndSelect);
        // expand corresponding category
        auto parent = m_treeModel->parent(index);
        m_view.profileTree->expand(m_proxyModel->mapFromSource(parent));
        m_view.profileTree->scrollTo(mapped, QAbstractItemView::PositionAtCenter);
    }
}

void RenderWidget::slotPrepareExport(bool delayedRendering)
{
    if (pCore->projectDuration() < 2) {
        // Empty project, don't attempt to render
        m_view.infoMessage->setMessageType(KMessageWidget::Warning);
        m_view.infoMessage->setText(i18n("Add a clip to timeline before rendering"));
        m_view.infoMessage->animatedShow();
        return;
    }
    if (!QFile::exists(KdenliveSettings::meltpath())) {
        m_view.infoMessage->setMessageType(KMessageWidget::Warning);
        m_view.infoMessage->setText(i18n("Cannot find the melt program required for rendering (part of Mlt)"));
        m_view.infoMessage->animatedShow();
        return;
    }
    if (pCore->bin()->usesVariableFpsClip()) {
        // Empty project, don't attempt to render
        m_view.infoMessage->setMessageType(KMessageWidget::Warning);
        m_view.infoMessage->setText(
            i18nc("@label:textbox",
                  "Rendering a project with variable framerate clips can lead to audio/video desync.\nWe recommend to transcode to an edit friendly format."));
        // Remove previous actions if any
        QList<QAction *> acts = m_view.infoMessage->actions();
        while (!acts.isEmpty()) {
            QAction *a = acts.takeFirst();
            m_view.infoMessage->removeAction(a);
            delete a;
        }

        QAction *b = new QAction(i18nc("@action:button", "Render Anyway"), this);
        connect(b, &QAction::triggered, this, [this, delayedRendering]() {
            m_view.infoMessage->animatedHide();
            slotPrepareExport2(delayedRendering);
        });
        m_view.infoMessage->addAction(b);
        QAction *a = new QAction(i18nc("@action:button", "Transcode"), this);
        connect(a, &QAction::triggered, this, [this]() {
            QList<QAction *> acts = m_view.infoMessage->actions();
            while (!acts.isEmpty()) {
                QAction *a = acts.takeFirst();
                m_view.infoMessage->removeAction(a);
                delete a;
            }
            m_view.infoMessage->hide();
            updateRenderInfoMessage();
            pCore->bin()->transcodeUsedClips();
        });
        m_view.infoMessage->addAction(a);
        m_view.infoMessage->animatedShow();
        return;
    }
    slotPrepareExport2(delayedRendering);
}

void RenderWidget::slotPrepareExport2(bool delayedRendering)
{
    if (QFile::exists(m_view.out_file->url().toLocalFile())) {
        if (KMessageBox::warningTwoActions(this, i18n("Output file already exists. Do you want to overwrite it?"), {}, KStandardGuiItem::overwrite(),
                                           KStandardGuiItem::cancel()) != KMessageBox::PrimaryAction) {
            return;
        }
    }
    // mantisbt 1051
    QDir dir(m_view.out_file->url().adjusted(QUrl::RemoveFilename).toLocalFile());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        KMessageBox::error(this, i18n("The directory %1, could not be created.\nPlease make sure you have the required permissions.",
                                      m_view.out_file->url().adjusted(QUrl::RemoveFilename).toLocalFile()));
        return;
    }

    saveRenderProfile();

    RenderRequest request;

    request.setOutputFile(m_view.out_file->url().toLocalFile());

    request.setPresetParams(m_params);
    request.setDelayedRendering(delayedRendering);
    request.setProxyRendering(m_view.proxy_render->isChecked());
    request.setEmbedSubtitles(m_view.embed_subtitles->isEnabled() && m_view.embed_subtitles->isChecked());
    request.setTwoPass(m_view.checkTwoPass->isChecked());
    request.setAudioFilePerTrack(m_view.stemAudioExport->isChecked() && m_view.stemAudioExport->isEnabled());

    bool guideMultiExport = m_view.guide_multi_box->isChecked();
    int guideCategory = m_view.guideCategoryChooser->currentCategory();
    request.setGuideParams(m_guidesModel, guideMultiExport, guideCategory);

    request.setOverlayData(m_view.tc_type->currentData().toString());
    request.setAspectRatio(m_view.aspect_ratio_type->currentData().toString());

    if (m_view.render_zone->isChecked()) {
        Monitor *pMon = pCore->getMonitor(Kdenlive::ProjectMonitor);
        request.setBounds(pMon->getZoneStart(), pMon->getZoneEnd() - 1);
    } else if (m_view.render_guide->isChecked()) {
        double fps = pCore->getCurrentProfile()->fps();
        int startIndex = m_view.guide_start->currentIndex();

        if (startIndex > 0 && !m_currentMarkers.isEmpty()) {
            int markerIndex = startIndex - 1;
            if (markerIndex < m_currentMarkers.size()) {
                const CommentedTime &marker = m_currentMarkers.at(markerIndex);
                if (marker.hasRange()) {
                    double guideStart = marker.time().seconds();
                    double guideEnd = marker.endTime().seconds();

                    int in = int(GenTime(guideStart).frames(fps));
                    int out = int(GenTime(guideEnd).frames(fps)) - 1;
                    request.setBounds(in, out);
                } else {
                    double guideStart = m_view.guide_start->itemData(startIndex).toDouble();
                    double guideEnd = m_view.guide_end->itemData(m_view.guide_end->currentIndex()).toDouble();

                    int in = int(GenTime(qMin(guideStart, guideEnd)).frames(fps));
                    int out = int(GenTime(qMax(guideStart, guideEnd)).frames(fps)) - 1;
                    request.setBounds(in, out);
                }
            } else {
                double guideStart = m_view.guide_start->itemData(startIndex).toDouble();
                double guideEnd = m_view.guide_end->itemData(m_view.guide_end->currentIndex()).toDouble();

                int in = int(GenTime(qMin(guideStart, guideEnd)).frames(fps));
                int out = int(GenTime(qMax(guideStart, guideEnd)).frames(fps)) - 1;
                request.setBounds(in, out);
            }
        } else {
            double guideStart = m_view.guide_start->itemData(startIndex).toDouble();
            double guideEnd = m_view.guide_end->itemData(m_view.guide_end->currentIndex()).toDouble();

            int in = int(GenTime(qMin(guideStart, guideEnd)).frames(fps));
            int out = int(GenTime(qMax(guideStart, guideEnd)).frames(fps)) - 1;
            request.setBounds(in, out);
        }
    }

    std::vector<RenderRequest::RenderJob> jobs = request.process();

    if (!request.errorMessages().isEmpty()) {
        KMessageBox::errorList(this, i18n("The following errors occurred while trying to render"), request.errorMessages());
    }

    // Create jobs
    if (delayedRendering) {
        if (jobs.size() > 0) {
            // Focus scripts page
            RenderRequest::RenderJob j = jobs.front();
            qDebug() << "//// GOT PLAYLIST PATH: " << j.playlistPath;
            parseScriptFiles(QFileInfo(j.playlistPath).fileName());
            m_view.tabWidget->setCurrentIndex(Tabs::ScriptsTab);
        }
        return;
    }

    QList<RenderJobItem *> jobList;
    for (auto &job : jobs) {
        RenderJobItem *renderItem = createRenderJob(job);
        if (renderItem != nullptr) {
            jobList << renderItem;
        }
    }
    if (jobList.count() > 0) {
        m_view.running_jobs->setCurrentItem(jobList.at(0));
    }
    m_view.tabWidget->setCurrentIndex(Tabs::JobsTab);
    // check render status
    checkRenderStatus(-1);
}

RenderJobItem *RenderWidget::createRenderJob(const RenderRequest::RenderJob &job)
{
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(job.outputPath, Qt::MatchExactly, 1);
    RenderJobItem *renderItem = nullptr;
    if (!existing.isEmpty()) {
        renderItem = static_cast<RenderJobItem *>(existing.at(0));
        if (renderItem->data(1, TwoPassRole).isNull()) {
            if (renderItem->status() == RUNNINGJOB || renderItem->status() == WAITINGJOB || renderItem->status() == STARTINGJOB) {
                // There is an existing job that is still pending
                KMessageBox::information(
                    this, i18n("There is already a job writing file:<br /><b>%1</b><br />Abort the job if you want to overwrite it…", job.outputPath),
                    i18n("Already running"));
                // focus the running job
                m_view.running_jobs->setCurrentItem(renderItem);
                return nullptr;
            }
            // There is an existing job that already finished
            delete renderItem;
            renderItem = nullptr;
        }
    }
    const QStringList itemData = {QString(), job.outputFile};
    renderItem = new RenderJobItem(m_view.running_jobs, itemData);

    QDateTime t = QDateTime::currentDateTime();
    renderItem->setData(1, StartTimeRole, t);
    renderItem->setData(1, LastTimeRole, t);
    renderItem->setData(1, LastFrameRole, 0);
    renderItem->setData(1, PlaylistFileRole, job.playlistPath);
    if (job.outputFile != job.outputPath) {
        renderItem->setData(1, TwoPassRole, 1);
    }
    QStringList argsJob = RenderRequest::argsByJob(job);

    renderItem->setData(1, ParametersRole, argsJob);
    qDebug() << "* CREATED JOB WITH ARGS: " << argsJob;
    renderItem->setData(1, OpenBrowserRole, m_view.open_browser->isChecked());
    renderItem->setData(1, PlayAfterRole, m_view.play_after->isChecked());
    if (!m_view.audio_box->isChecked()) {
        renderItem->setData(1, ExtraInfoRole, i18n("Video without audio track"));
    } else if (!m_view.video_box->isChecked()) {
        renderItem->setData(1, ExtraInfoRole, i18n("Audio without video track"));
    } else {
        renderItem->setData(1, ExtraInfoRole, QString());
    }
    return renderItem;
}

void RenderWidget::checkRenderStatus(int lastStatus)
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
            const QStringList jobData = item->data(1, ParametersRole).toStringList();
            if (jobData.size() > 2 && jobData.at(2).endsWith(QStringLiteral("-pass2.mlt"))) {
                // Find and remove 1st pass job
                QString firstPassName = jobData.at(2);
                firstPassName.replace(QStringLiteral("-pass2.mlt"), QStringLiteral("-pass1.mlt"));
                QTreeWidgetItem *above = m_view.running_jobs->itemAbove(item);
                while (above) {
                    QStringList aboveData = above->data(1, ParametersRole).toStringList();
                    if (aboveData.size() > 2 && aboveData.at(2) == firstPassName) {
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
    if (!waitingJob) {
        if (m_renderStatus == Rendering) {
            m_renderStatus = NotRendering;
            Q_EMIT renderStatusChanged();
        }
        if (m_view.shutdown->isChecked() && lastStatus != -3) {
            Q_EMIT shutdown();
        }
    }
}

void RenderWidget::startRendering(RenderJobItem *item)
{
    auto rendererArgs = item->data(1, ParametersRole).toStringList();
    qDebug() << "starting kdenlive_render process using: " << KdenliveSettings::kdenliverendererpath();
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!KdenliveSettings::hwDecoding().isEmpty()) {
        env.insert(QLatin1String("MLT_AVFORMAT_HWACCEL"), KdenliveSettings::hwDecoding());
    }
    proc.setProgram(KdenliveSettings::kdenliverendererpath());
    proc.setProcessEnvironment(env);
    if (KdenliveSettings::keepRenderLogFiles()) {
        rendererArgs << QStringLiteral("--debug");
    }
    proc.setArguments(rendererArgs);
    if (!proc.startDetached()) {
        item->setStatus(FAILEDJOB);
    } else {

        KNotification::event(QStringLiteral("RenderStarted"), i18n("Rendering %1 started", item->text(1)), QPixmap());
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
    m_view.rescale_height->setValue(KdenliveSettings::defaultrescaleheight());
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
        url = QUrl::fromLocalFile(pCore->currentDoc()->projectRenderFolder() + QDir::separator());
    }
    QDir directory(url.adjusted(QUrl::RemoveFilename).toLocalFile());
    if (!url.isValid() || directory.isRelative()) {
        directory = QDir(pCore->currentDoc()->projectRenderFolder());
    }

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

    return QUrl::fromLocalFile(directory.absoluteFilePath(filename));
}

void RenderWidget::slotChangeSelection(const QModelIndex &current, const QModelIndex &previous)
{
    auto mapped = m_proxyModel->mapToSource(current);
    if (m_treeModel->parent(mapped) == QModelIndex()) {
        // in that case, we have selected a category, which we don't want
        QItemSelectionModel *selection = m_view.profileTree->selectionModel();
        selection->select(previous, QItemSelectionModel::ClearAndSelect);
        // expand corresponding category
        auto parent = m_treeModel->parent(m_proxyModel->mapToSource(previous));
        m_view.profileTree->expand(parent);
        m_view.profileTree->scrollTo(previous, QAbstractItemView::PositionAtCenter);
        return;
    }
    m_currentProfile = m_treeModel->getPreset(mapped);
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
        m_params.clear();
        m_view.advanced_params->clear();
        m_view.buttonRender->setEnabled(false);
        m_view.buttonGenerateScript->setEnabled(false);
        m_view.optionsGroup->setEnabled(false);
        return;
    }
    std::unique_ptr<RenderPresetModel> &profile = RenderPresetRepository::get()->getPreset(m_currentProfile);

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
    m_view.out_file->setNameFilter("*." + profile->extension());
    m_view.buttonDelete->setEnabled(profile->editable());
    m_view.buttonEdit->setEnabled(profile->editable());

    if (!profile->speeds().isEmpty()) {
        m_view.speed->setEnabled(true);
        m_view.speed->setMaximum(profile->speeds().count() - 1);
        m_view.speed->setValue(profile->defaultSpeedIndex());
    } else {
        m_view.speed->setEnabled(false);
    }
    adjustSpeed(m_view.speed->value());
    bool passes = profile->hasParam(QStringLiteral("passes"));
    bool hasCrf = profile->hasParam(QStringLiteral("crf"));
    QString vcodec = profile->getParam(QStringLiteral("vcodec"));
    if (vcodec.isEmpty()) {
        vcodec = profile->getParam(QStringLiteral("c:v"));
    }
    m_view.checkTwoPass->setEnabled(!hasCrf && (passes || vcodec.contains(QLatin1String("x26")) || vcodec.contains(QLatin1String("vpx"))));
    m_view.checkTwoPass->setChecked(passes && profile->getParam(QStringLiteral("passes")) == QStringLiteral("2"));

    m_view.encoder_threads->setEnabled(!profile->hasParam(QStringLiteral("threads")));
    m_view.embed_subtitles->setEnabled(profile->extension() == QLatin1String("mkv") || profile->extension() == QLatin1String("matroska"));

    m_view.video_box->setChecked(profile->getParam(QStringLiteral("vn")) != QStringLiteral("1"));
    bool audioAllowed = profile->getParam(QStringLiteral("an")) != QStringLiteral("1");
    if (audioAllowed) {
        const QString mltProperties = profile->getParam(QStringLiteral("properties"));
        if (!mltProperties.isEmpty()) {
            Mlt::Properties props;
            props.set("mlt_type", "consumer");
            props.set("mlt_service", "avformat");
            props.preset(mltProperties.toUtf8().constData());
            if (props.get_int("an") == 1) {
                audioAllowed = false;
            }
        }
    }
    m_view.audio_box->setChecked(audioAllowed);

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
    std::unique_ptr<RenderPresetModel> &preset = RenderPresetRepository::get()->getPreset(m_currentProfile);

    if (preset->hasFixedSize()) {
        // profile has a fixed size, do not allow resize
        m_view.rescale->setEnabled(false);
        setRescaleEnabled(false);
    } else {
        m_view.rescale->setEnabled(m_view.video_box->isChecked());
        setRescaleEnabled(m_view.video_box->isChecked() && m_view.rescale->isChecked());
    }

    m_params = preset->params();
    // Audio Channels: don't override, only set if it is not yet set
    if (!preset->hasParam(QStringLiteral("channels"))) {
        m_params.insert(QStringLiteral("channels"), QString::number(pCore->audioChannels()));
    }

    // Check for movit
    if (KdenliveSettings::gpu_accel()) {
        m_params.insert(QStringLiteral("glsl."), QString::number(1));
    }

    // In case of libx265 add x265-param
    m_params.refreshX265Params();

    // Rescale
    bool rescale = m_view.rescale->isEnabled() && m_view.rescale->isChecked();
    if (rescale) {
        QSize frameSize = pCore->getCurrentFrameSize();
        double scale;
        if (frameSize.width() > frameSize.height()) {
            scale = (double)m_view.rescale_width->value() / frameSize.width();
        } else {
            scale = (double)m_view.rescale_height->value() / frameSize.height();
        }
        m_params.insert(QStringLiteral("scale"), QString::number(scale));
        m_params.remove(QStringLiteral("width"));
        m_params.remove(QStringLiteral("height"));
        m_params.remove(QStringLiteral("s"));
    } else {
        // Preview rendering
        bool previewRes = m_view.render_at_preview_res->isChecked() && pCore->getMonitorProfile().height() != pCore->getCurrentFrameSize().height();
        if (previewRes) {
            m_params.insert(QStringLiteral("scale"), QString::number(double(pCore->getMonitorProfile().height()) / pCore->getCurrentFrameSize().height()));
        }
    }

    // Full color range
    if (m_view.render_full_color->isChecked()) {
        m_params.insert(QStringLiteral("color_range"), QStringLiteral("pc"));
    }

    // disable audio if requested
    if (!m_params.contains(QStringLiteral("audio_off"))) {
        m_view.audio_box->setEnabled(true);
        if (!m_view.audio_box->isChecked()) {
            m_params.insert(QStringLiteral("an"), QString::number(1));
            m_params.insert(QStringLiteral("audio_off"), QString::number(1));
        } else {
            m_params.remove(QStringLiteral("an"));
            m_params.remove(QStringLiteral("audio_off"));
        }
    } else {
        m_view.audio_box->setEnabled(false);
    }

    if (!m_params.contains(QStringLiteral("video_off"))) {
        m_view.video_box->setEnabled(true);
        if (!m_view.video_box->isChecked()) {
            m_params.insert(QStringLiteral("vn"), QString::number(1));
            m_params.insert(QStringLiteral("video_off"), QString::number(1));
        } else {
            m_params.remove(QStringLiteral("vn"));
            m_params.remove(QStringLiteral("video_off"));
        }
    } else {
        m_view.video_box->setEnabled(false);
    }

    if (!(m_view.video_box->isChecked() || m_view.audio_box->isChecked())) {
        errorMessage(OptionsError, i18n("Rendering without audio and video does not work. Please enable at least one of both."));
        m_view.buttonRender->setEnabled(false);
    } else {
        errorMessage(OptionsError, QString());
        m_view.buttonRender->setEnabled(preset->error().isEmpty());
    }

    // Parallel Processing
    int threadCount = KdenliveSettings::processingthreads();
    if (!m_view.processing_box->isChecked() || !m_view.processing_box->isEnabled()) {
        threadCount = 1;
    }
    m_params.insert(QStringLiteral("real_time"), QString::number(-threadCount));

    // Adjust encoding speed
    if (m_view.speed->isEnabled()) {
        QStringList speeds = preset->speeds();
        if (m_view.speed->value() < speeds.count()) {
            const QString &speedValue = speeds.at(m_view.speed->value());
            m_params.insertFromString(speedValue, false);
        }
    }

    // Set the thread counts
    if (!m_params.contains(QStringLiteral("threads"))) {
        int ffmpegThreads = KdenliveSettings::encodethreads();
        if (m_params.toString().contains(QLatin1String("libx265"))) {
            // libx265 encoding is limited to 16 threads max
            ffmpegThreads = qMin(16, ffmpegThreads);
        }
        m_params.insert(QStringLiteral("threads"), QString::number(ffmpegThreads));
    }

    // Project metadata
    if (m_view.export_meta->isChecked()) {
        m_params.insert(pCore->currentDoc()->metadata());
    }

    // Timeline sequence metadata
    int timecodeOffset = pCore->currentTimelineOffset();
    if (timecodeOffset > 0) {
        m_params.insert(QStringLiteral("meta.attr.TIMECODE.markup"), pCore->timecode().getDisplayTimecodeFromFrames(timecodeOffset, false));
    }

    QString paramString = m_params.toString();
    if (paramString.contains(QStringLiteral("%quality")) || paramString.contains(QStringLiteral("%audioquality"))) {
        m_view.qualityGroup->setEnabled(true);
    } else {
        m_view.qualityGroup->setEnabled(false);
    }

    // historically qualities are sorted from best to worse for some reason
    int vmin = preset->videoQualities().last().toInt();
    int vmax = preset->videoQualities().first().toInt();
    int vrange = abs(vmax - vmin);
    int amin = preset->audioQualities().last().toInt();
    int amax = preset->audioQualities().first().toInt();
    int arange = abs(amax - amin);

    double factor = double(m_view.quality->value()) / double(m_view.quality->maximum());
    m_view.quality->setMaximum(qMin(100, qMax(vrange, arange)));
    m_view.quality->setValue(qRound(m_view.quality->maximum() * factor));
    double percent = double(m_view.quality->value()) / double(m_view.quality->maximum());
    m_view.qualityPercent->setText(QStringLiteral("%1%").arg(qRound(percent * 100)));

    int val = preset->defaultVQuality().toInt();

    if (m_view.qualityGroup->isChecked()) {
        if (vmin < vmax) {
            val = vmin + int(vrange * percent);
        } else {
            val = vmin - int(vrange * percent);
        }
    }
    m_params.replacePlaceholder(QStringLiteral("%quality"), QString::number(val));
    //  TODO check if this is finally correct
    m_params.replacePlaceholder(QStringLiteral("%bitrate+'k'"), QStringLiteral("%1k").arg(preset->defaultVBitrate()));
    m_params.replacePlaceholder(QStringLiteral("%bitrate"), QStringLiteral("%1").arg(preset->defaultVBitrate()));

    val = preset->defaultABitrate().toInt() * 1000;
    if (m_view.qualityGroup->isChecked()) {
        val *= percent;
    }
    // cvbr = Constrained Variable Bit Rate
    m_params.replacePlaceholder(QStringLiteral("%cvbr"), QString::number(val));

    val = preset->defaultAQuality().toInt();
    if (m_view.qualityGroup->isChecked()) {
        if (amin < amax) {
            val = amin + int(arange * percent);
        } else {
            val = amin - int(arange * percent);
        }
    }
    m_params.replacePlaceholder(QStringLiteral("%audioquality"), QString::number(val));
    // TODO check if this is finally correct
    m_params.replacePlaceholder(QStringLiteral("%audiobitrate+'k'"), QStringLiteral("%1k").arg(preset->defaultABitrate()));
    m_params.replacePlaceholder(QStringLiteral("%audiobitrate"), QStringLiteral("%1").arg(preset->defaultABitrate()));

    std::unique_ptr<ProfileModel> &projectProfile = pCore->getCurrentProfile();
    QString dvstd;
    if (fmod(double(projectProfile->frame_rate_num()) / projectProfile->frame_rate_den(), 30.01) > 27) {
        dvstd = QStringLiteral("ntsc");
    } else {
        dvstd = QStringLiteral("pal");
    }
    if (double(projectProfile->display_aspect_num()) / projectProfile->display_aspect_den() > 1.5) {
        dvstd += QLatin1String("_wide");
    }
    m_params.replacePlaceholder(QLatin1String("%dv_standard"), dvstd);

    m_params.replacePlaceholder(QLatin1String("%dar"), QStringLiteral("@%1/%2").arg(QString::number(projectProfile->display_aspect_num()),
                                                                                    QString::number(projectProfile->display_aspect_den())));
    if (m_view.checkTwoPass->isEnabled()) {
        if (m_view.checkTwoPass->isChecked()) {
            m_params.insert(QStringLiteral("pass"), QStringLiteral("2"));
        } else {
            m_params.remove(QStringLiteral("pass"));
        }
    }
    m_view.advanced_params->setPlainText(m_params.toString());
}

void RenderWidget::parseProfiles(const QString &selectedProfile)
{
    m_treeModel.reset();
    m_treeModel = RenderPresetTreeModel::construct(this);
    m_proxyModel = std::make_unique<TreeProxyModel>(this);
    m_view.profileTree->setModel(m_proxyModel.get());
    // Connect models
    m_proxyModel->setSourceModel(m_treeModel.get());
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
    qDebug() << "RECEIVED PROGRESS INFO: " << dest << ", progress:" << progress << ", FRM: " << frame;
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(dest, Qt::MatchExactly, 1);
    if (!existing.isEmpty()) {
        item = static_cast<RenderJobItem *>(existing.at(0));
    } else {
        if (dest == QLatin1String("/dev/null") || dest == QLatin1String("NUL")) {
            // 2 pass rendering, look for first matching job
            for (int i = 0; i < m_view.running_jobs->topLevelItemCount(); i++) {
                item = static_cast<RenderJobItem *>(m_view.running_jobs->topLevelItem(i));
                if (item && item->data(1, TwoPassRole).toInt() == 1) {
                    break;
                }
                item = nullptr;
            }
        }
        if (!item) {
            const QStringList itemData = {QString(), dest};
            item = new RenderJobItem(m_view.running_jobs, itemData);
        }
    }
    item->setData(1, ProgressRole, progress);
    if (progress == 0) {
        item->setStatus(STARTINGJOB);
        item->setIcon(0, QIcon::fromTheme(QStringLiteral("media-record")));
        slotCheckJob();
    } else {
        item->setStatus(RUNNINGJOB);
        QDateTime startTime = item->data(1, StartTimeRole).toDateTime();
        if (startTime.isNull()) {
            // Recovering a job
            QDateTime t = QDateTime::currentDateTime();
            item->setData(1, StartTimeRole, t);
            item->setData(1, LastTimeRole, 0);
            item->setData(1, LastFrameRole, frame);
            return;
        }
        if (m_renderStatus == NotRendering) {
            m_renderStatus = Rendering;
            Q_EMIT renderStatusChanged();
        }
        qint64 elapsedTime = startTime.secsTo(QDateTime::currentDateTime());
        qint64 lastTimeRole = item->data(1, LastTimeRole).toInt();
        int dt = elapsedTime - lastTimeRole;
        if (dt < 1) {
            return;
        }
        qint64 remaining;
        if (lastTimeRole == 0) {
            remaining = elapsedTime * (100 - progress);
        } else {
            remaining = elapsedTime * (100 - progress) / progress;
        }
        int days = int(remaining / 86400);
        int remainingSecs = int(remaining % 86400);
        QTime when = QTime(0, 0, 0, 0).addSecs(remainingSecs);
        QString est = i18n("Remaining time ");
        if (days > 0) {
            est.append(i18np("%1 day ", "%1 days ", days));
        }
        est.append(when.toString(QStringLiteral("hh:mm:ss")));
        int lastFrame = item->data(1, LastFrameRole).toInt();
        if (frame < lastFrame) {
            // Something is wrong, ignore progress
            // return;
        }
        int speed = (frame - lastFrame) / dt;
        if (speed < 0) {
            // return;
        }
        est.append(i18n(" (frame %1 @ %2 fps)", frame, speed));
        item->setData(1, Qt::UserRole, est);
        item->setData(1, LastTimeRole, elapsedTime);
        item->setData(1, LastFrameRole, frame);
    }
    if (!m_memCheckTimer.isActive()) {
        m_memCheckTimer.start();
    }
}

void RenderWidget::slotCheckFreeMemory()
{
    KMemoryInfo memInfo;
    // if system doesn't have much available memory, warn user
    if (!memInfo.isNull()) {
        int availableMemory = memInfo.availablePhysical() / 1024 / 1024;
        if (availableMemory < m_lowMemThreshold) {
            int totalMemory = memInfo.totalPhysical() / 1024 / 1024;
            qDebug() << "Low memory:" << availableMemory << "MB free, " << totalMemory << "MB total";
            m_view.jobInfo->show();
            bool veryLow = availableMemory < m_veryLowMemThreshold;
            m_view.jobInfo->setMessageType(veryLow ? KMessageWidget::Error : KMessageWidget::Warning);
            const QString errorMessage = i18n("Less than %1MB of available memory remaining.", availableMemory);
            m_view.jobInfo->setText(errorMessage);
            m_view.infoMessage->setMessageType(veryLow ? KMessageWidget::Error : KMessageWidget::Warning);
            m_view.infoMessage->setText(errorMessage);
            if ((m_lowMemStatus == LowMemory && !veryLow) || (m_lowMemStatus == VeryLowMemory && veryLow)) {
                // No status change
                return;
            }
            m_lowMemStatus = veryLow ? VeryLowMemory : LowMemory;
            KNotification *notify = new KNotification(QStringLiteral("ErrorMessage"));
            notify->setText(errorMessage);
            notify->sendEvent();
            // Increase the memory check frequency
            m_memCheckTimer.setInterval(5000);
        } else if (m_lowMemStatus != NoWarning) {
            m_lowMemStatus = NoWarning;
            m_view.jobInfo->hide();
            updateRenderInfoMessage();
            // Get back to the default timeout
            m_memCheckTimer.setInterval(10000);
            if (runningJobsCount() == 0) {
                m_memCheckTimer.stop();
            }
        }
    }
}

void RenderWidget::setRenderStatus(const QString &dest, int status, const QString &error)
{
    RenderJobItem *item = nullptr;
    QList<QTreeWidgetItem *> existing = m_view.running_jobs->findItems(dest, Qt::MatchExactly, 1);
    bool firstPassRendering = false;
    if (!existing.isEmpty()) {
        item = static_cast<RenderJobItem *>(existing.at(0));
    } else {
        if (dest == QLatin1String("/dev/null") || dest == QLatin1String("NUL")) {
            // 2 pass rendering, look for first matching job
            for (int i = 0; i < m_view.running_jobs->topLevelItemCount(); i++) {
                item = static_cast<RenderJobItem *>(m_view.running_jobs->topLevelItem(i));
                if (item && item->data(1, TwoPassRole).toInt() == 1) {
                    firstPassRendering = true;
                    break;
                }
                item = nullptr;
            }
        }
        if (!item) {
            if (dest.isEmpty() && status == -2) {
                // start failure returns an empty url
                item = startingJob();
            }
            if (item == nullptr) {
                const QStringList itemData = {QString(), dest};
                item = new RenderJobItem(m_view.running_jobs, itemData);
            }
        }
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

        m_shareMenu->model()->setInputData(QJsonObject{{QStringLiteral("mimeType"), QMimeDatabase().mimeTypeForFile(item->text(1)).name()},
                                                       {QStringLiteral("urls"), QJsonArray({item->text(1)})}});
        m_shareMenu->model()->setPluginType(QStringLiteral("Export"));
        m_shareMenu->reload();

        KNotification *notify = new KNotification(QStringLiteral("RenderFinished"));
        QString notif;
        if (!firstPassRendering) {
            notif = i18n("Rendering of %1 finished in %2", item->text(1), est);
            notify->setUrls({QUrl::fromLocalFile(dest)});
        } else {
            notif = i18n("First pass rendering of %1 finished", item->text(1));
        }
        notify->setText(notif);
        notify->sendEvent();
        const QUrl url = QUrl::fromLocalFile(item->text(1));
        bool exists = QFile(url.toLocalFile()).exists();
        if (exists && !firstPassRendering) {
            if (item->data(1, OpenBrowserRole).toBool()) {
                pCore->highlightFileInExplorer({url});
            }
            if (item->data(1, PlayAfterRole).toBool()) {
                auto *job = new KIO::OpenUrlJob(url);
                job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
                job->start();
            }
        }
    } else if (status == -2) {
        // Rendering crashed
        item->setStatus(FAILEDJOB);
        if (dest.isEmpty()) {
            m_view.error_log->append(i18n("<strong>Rendering crashed</strong><br />"));
        } else {
            m_view.error_log->append(i18n("<strong>Rendering of %1 crashed</strong><br />", dest));
        }
        m_view.error_log->append(error);
        m_view.error_log->append(QStringLiteral("<hr />"));
        m_view.error_box->setVisible(true);
    } else if (status == -3) {
        // User aborted job
        item->setStatus(ABORTEDJOB);
    } else {
        delete item;
        item = nullptr;
    }
    if (item) {
        if (QFile::exists(dest + ".log")) {
            const QString logFile = dest + QStringLiteral(".log");
            item->setData(1, RenderWidget::LogFileRole, logFile);
        }
        if (QFile::exists(item->data(1, RenderWidget::PlaylistFileRole).toString())) {
            item->setData(1, RenderWidget::PlaylistDisplayRole, item->data(1, RenderWidget::PlaylistFileRole));
        }
    }
    m_view.clean_up->setEnabled(true);
    slotCheckJob();
    checkRenderStatus(status);
}

RenderJobItem *RenderWidget::startingJob()
{
    auto *item = static_cast<RenderJobItem *>(m_view.running_jobs->topLevelItem(0));
    while (item != nullptr) {
        if (item->status() == STARTINGJOB) {
            return item;
        }
        item = static_cast<RenderJobItem *>(m_view.running_jobs->itemBelow(item));
    }
    return nullptr;
}

void RenderWidget::slotAbortCurrentJob()
{
    auto *current = static_cast<RenderJobItem *>(m_view.running_jobs->currentItem());
    if (current) {
        if (current->status() == RUNNINGJOB) {
            Q_EMIT abortProcess(current->text(1));
        } else {
            delete current;
            slotCheckJob();
            checkRenderStatus(-3);
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
        if (current->status() == FINISHEDJOB) {
            m_shareMenu->model()->setInputData(QJsonObject{{QStringLiteral("mimeType"), QMimeDatabase().mimeTypeForFile(current->text(1)).name()},
                                                           {QStringLiteral("urls"), QJsonArray({current->text(1)})}});
            m_shareMenu->model()->setPluginType(QStringLiteral("Export"));
            m_shareMenu->reload();
            m_view.shareButton->setEnabled(true);
        } else {
            m_view.shareButton->setEnabled(false);
        }
    }
    if (runningJobsCount() == 0 && m_lowMemStatus == NoWarning) {
        m_memCheckTimer.stop();
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

void RenderWidget::parseScriptFiles(const QString lastScript)
{
    QStringList scriptsFilter;
    scriptsFilter << QStringLiteral("*.mlt");
    m_view.scripts_list->clear();

    // List the project scripts
    QDir projectFolder(pCore->currentDoc()->projectRenderFolder());
    if (!projectFolder.exists(QStringLiteral("kdenlive-renderqueue"))) {
        return;
    }
    projectFolder.cd(QStringLiteral("kdenlive-renderqueue"));
    QStringList scriptFiles = projectFolder.entryList(scriptsFilter, QDir::Files);
    if (scriptFiles.isEmpty()) {
        // No scripts, delete directory
        if (projectFolder.dirName() == QStringLiteral("kdenlive-renderqueue") &&
            projectFolder.entryList(scriptsFilter, QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
            projectFolder.removeRecursively();
            return;
        }
    }
    QTreeWidgetItem *lastCreatedScript = nullptr;
    for (int i = 0; i < scriptFiles.size(); ++i) {
        QUrl scriptpath = QUrl::fromLocalFile(projectFolder.absoluteFilePath(scriptFiles.at(i)));
        QDomDocument doc;
        if (!Xml::docContentFromFile(doc, scriptpath.toLocalFile(), false)) {
            continue;
        }
        QDomElement consumer = doc.documentElement().firstChildElement(QStringLiteral("consumer"));
        if (consumer.isNull()) {
            continue;
        }
        QString target = consumer.attribute(QStringLiteral("target"));
        if (target.isEmpty()) {
            continue;
        }
        QTreeWidgetItem *item = new QTreeWidgetItem(m_view.scripts_list, QStringList() << QString() << scriptpath.fileName());
        QFile f(scriptpath.toLocalFile());
        auto icon = QFileIconProvider().icon(QFileInfo(f));
        item->setIcon(0, icon.isNull() ? QIcon::fromTheme(QStringLiteral("application-x-executable-script")) : icon);
        item->setSizeHint(0, QSize(m_view.scripts_list->columnWidth(0), fontMetrics().height() * 2));
        item->setData(1, Qt::UserRole, QUrl(QUrl::fromEncoded(target.toUtf8())).url(QUrl::PreferLocalFile));
        item->setData(1, ParametersRole, scriptpath.toLocalFile());
        if (scriptFiles.at(i) == lastScript) {
            lastCreatedScript = item;
        }
    }
    if (lastCreatedScript == nullptr) {
        lastCreatedScript = m_view.scripts_list->topLevelItem(0);
    }
    if (lastCreatedScript) {
        m_view.scripts_list->setCurrentItem(lastCreatedScript);
        lastCreatedScript->setSelected(true);
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
            if (KMessageBox::warningTwoActions(this, i18n("Output file already exists. Do you want to overwrite it?"), {}, KStandardGuiItem::overwrite(),
                                               KStandardGuiItem::cancel()) != KMessageBox::PrimaryAction) {
                return;
            }
        }
        const QString path = item->data(1, ParametersRole).toString();
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
            const QStringList itemData = {QString(), destination};
            renderItem = new RenderJobItem(m_view.running_jobs, itemData);
        }
        renderItem->setData(1, ProgressRole, 0);
        renderItem->setStatus(WAITINGJOB);
        renderItem->setIcon(0, QIcon::fromTheme(QStringLiteral("media-playback-pause")));
        renderItem->setData(1, Qt::UserRole, i18n("Waiting…"));
        QDateTime t = QDateTime::currentDateTime();
        renderItem->setData(1, StartTimeRole, t);
        renderItem->setData(1, LastTimeRole, t);
        QStringList argsJob = {QStringLiteral("delivery"), KdenliveSettings::meltpath(), path, QStringLiteral("--pid"),
                               QString::number(QCoreApplication::applicationPid())};
        renderItem->setData(1, ParametersRole, argsJob);
        renderItem->setData(1, PlaylistFileRole, path);
        checkRenderStatus(-1);
        m_view.tabWidget->setCurrentIndex(Tabs::JobsTab);
    }
}

void RenderWidget::slotDeleteScript()
{
    QTreeWidgetItem *item = m_view.scripts_list->currentItem();
    if (item) {
        QString path = item->data(1, ParametersRole).toString();
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
    if (props.contains(QStringLiteral("rendercustomquality")) && props.value(QStringLiteral("rendercustomquality")).toInt() >= 0) {
        m_view.qualityGroup->setChecked(true);
        m_view.quality->setValue(props.value(QStringLiteral("rendercustomquality")).toInt());
    } else {
        m_view.qualityGroup->setChecked(false);
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

    if (props.contains(QStringLiteral("renderfullcolorrange"))) {
        m_view.render_full_color->setChecked(props.value(QStringLiteral("renderfullcolorrange")).toInt() != 0);
    } else {
        m_view.render_full_color->setChecked(false);
    }

    int mode = props.value(QStringLiteral("rendermode")).toInt();
    if (mode == 1) {
        m_view.render_zone->setChecked(true);
    } else if (mode == 2) {
        m_view.render_guide->setChecked(true);
        m_view.guide_start->setCurrentIndex(props.value(QStringLiteral("renderstartguide")).toInt());
        m_view.guide_end->setCurrentIndex(props.value(QStringLiteral("renderendguide")).toInt());
    } else {
        m_view.render_full->setChecked(true);
    }
    slotRenderModeChanged();

    QString url = props.value(QStringLiteral("renderurl"));
    if (url.isEmpty()) {
        if (RenderPresetRepository::get()->presetExists(m_currentProfile)) {
            std::unique_ptr<RenderPresetModel> &profile = RenderPresetRepository::get()->getPreset(m_currentProfile);
            url =
                filenameWithExtension(QUrl::fromLocalFile(pCore->currentDoc()->projectRenderFolder() + QDir::separator()), profile->extension()).toLocalFile();
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

    // If stemaudio is not defined, will return 0
    m_view.stemAudioExport->setChecked(props.value(QStringLiteral("renderstemaudio")).toInt());
    // New document opened, check space on drive
    m_lowSpaceTimer.start();
}

void RenderWidget::saveRenderProfile()
{
    // Save rendering profile to document
    QMap<QString, QString> renderProps;
    std::unique_ptr<RenderPresetModel> &preset = RenderPresetRepository::get()->getPreset(m_currentProfile);
    renderProps.insert(QStringLiteral("rendercategory"), preset->groupId());
    renderProps.insert(QStringLiteral("renderprofile"), preset->name());
    renderProps.insert(QStringLiteral("renderurl"), m_view.out_file->url().toLocalFile());
    int mode = 0; // 0 = full project
    if (m_view.render_zone->isChecked()) {
        mode = 1;
    } else if (m_view.render_guide->isChecked()) {
        mode = 2;
    }
    renderProps.insert(QStringLiteral("rendermode"), QString::number(mode));
    renderProps.insert(QStringLiteral("renderstemaudio"), m_view.stemAudioExport->isChecked() ? QString::number(1) : QString::number(0));
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
    renderProps.insert(QStringLiteral("renderplay"), QString::number(static_cast<int>(m_view.play_after->isChecked())));
    renderProps.insert(QStringLiteral("rendertwopass"), QString::number(static_cast<int>(m_view.checkTwoPass->isChecked())));
    renderProps.insert(QStringLiteral("rendercustomquality"), QString::number(m_view.qualityGroup->isChecked() ? m_view.quality->value() : -1));
    renderProps.insert(QStringLiteral("renderspeed"), QString::number(m_view.speed->value()));
    renderProps.insert(QStringLiteral("renderpreview"), QString::number(static_cast<int>(m_view.render_at_preview_res->isChecked())));
    renderProps.insert(QStringLiteral("renderfullcolorrange"), QString::number(static_cast<int>(m_view.render_full_color->isChecked())));

    Q_EMIT selectedRenderProfile(renderProps);
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
#ifndef Q_OS_WIN
    outStream << "#!/bin/sh\n\n";
#endif
    auto *item = static_cast<RenderJobItem *>(m_view.running_jobs->topLevelItem(0));
    while (item != nullptr) {
        if (item->status() == WAITINGJOB) {
            // Add render process for item
            const QString params = item->data(1, ParametersRole).toStringList().join(QLatin1Char(' '));
            outStream << '\"' << KdenliveSettings::kdenliverendererpath() << "\" " << params << '\n';
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
    QString fileName = item->text(1);
    if (!QFile::exists(fileName) && fileName.contains(QLatin1Char('&'))) {
        fileName.replace(QLatin1Char('&'), QStringLiteral("&#38;"));
    }
    auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(fileName));
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
    job->start();
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
        showRenderDuration();
        // m_view.infoMessage->hide();
    }
}

void RenderWidget::projectDurationChanged(int duration)
{
    showRenderDuration(duration + 1);
}

void RenderWidget::zoneDurationChanged()
{
    showRenderDuration();
}

void RenderWidget::updateRenderOffset()
{
    updateRenderInfoMessage();
    // Update guides if duration or timecode offset changed
    reloadGuides();
    // Update params to keep timecode offset in sync
    refreshParams();
}

void RenderWidget::checkDriveSpace()
{
    QStorageInfo info(QFileInfo(m_view.out_file->url().toLocalFile()).absolutePath());
    m_lastCheckedDevice = info.device();
    DriveSpaceStatus previousState = m_freeSpaceStatus;
#ifdef Q_OS_MAC
    // Device always returns readonly on Mac
    if (!info.isReady() || !info.isValid()) {
#else
    if (!info.isReady() || !info.isValid() || info.isReadOnly()) {
#endif
        m_freeSpaceStatus = SpaceNotWritable;
        if (!info.isReady()) {
            // Drive may be mounting, check again in a few seconds
            m_lowSpaceTimer.start();
        }
        if (previousState != m_freeSpaceStatus) {
            updateRenderInfoMessage();
        }
        return;
    }
    m_lastFreeSpace = static_cast<KIO::filesize_t>(info.bytesAvailable());

    KIO::filesize_t minimumSize = qMax(static_cast<KIO::filesize_t>(10000000), static_cast<KIO::filesize_t>(m_renderDuration * 60000));
    if (m_lastFreeSpace < 5 * minimumSize) {
        m_freeSpaceStatus = SpaceLow;
    } else if (m_lastFreeSpace < minimumSize) {
        m_freeSpaceStatus = SpaceNone;
    } else {
        m_freeSpaceStatus = SpaceOk;
    }
    if (previousState != m_freeSpaceStatus) {
        updateRenderInfoMessage();
    }
}

void RenderWidget::updateRenderInfoMessage()
{
    if (m_freeSpaceStatus != SpaceOk) {
        m_view.infoMessage->setMessageType(m_freeSpaceStatus == SpaceNone || m_freeSpaceStatus == SpaceNotWritable ? KMessageWidget::Error
                                                                                                                   : KMessageWidget::Warning);
    } else {
        m_view.infoMessage->setMessageType(m_renderDuration <= 0 || m_missingClips > 0 ? KMessageWidget::Warning : KMessageWidget::Information);
    }
    if (m_renderDuration <= 0) {
        m_view.infoMessage->setText(i18n("Add clips in timeline before rendering"));
        m_view.infoMessage->show();
        return;
    }
    if (m_freeSpaceStatus != SpaceOk) {
        switch (m_freeSpaceStatus) {
        case SpaceNotWritable:
            m_view.infoMessage->setText(i18n("Output location is not writable, please select another one"));
            break;
        case SpaceNone:
            m_view.infoMessage->setText(i18n("Your disk is almost full, rendering might fail"));
            break;
        case SpaceLow:
            m_view.infoMessage->setText(i18n("Your disk space is limited (%1)", KIO::convertSize(m_lastFreeSpace)));
            break;
        default:
            break;
        }
        m_view.infoMessage->show();
        return;
    }

    QString stringDuration = pCore->timecode().getDisplayTimecodeFromFrames(qMax(0, m_renderDuration), false);
    QString infoMessage = i18n("Rendered File Length: %1", stringDuration);
    int sequenceOffset = pCore->currentTimelineOffset();
    if (sequenceOffset > 0) {
        infoMessage.append(QStringLiteral("\n%1 %2").arg(i18n("Timecode Offset:"), pCore->timecode().getDisplayTimecodeFromFrames(sequenceOffset, false)));
    }
    if (m_missingClips > 0) {
        infoMessage.append(QStringLiteral(". "));
        if (m_missingUsedClips == m_missingClips) {
            infoMessage.append(i18np("<b>One missing clip used in the project.</b>", "<b>%1 missing clips used in the project.</b>", m_missingClips));
        } else if (m_missingUsedClips == 0) {
            infoMessage.append(i18np("One missing clip (unused).", "%1 missing clips (unused).", m_missingClips));
        } else {
            infoMessage.append(i18n("%1 missing clips (<b>%2 used in the project</b>).", m_missingClips, m_missingUsedClips));
        }
    }
    m_view.infoMessage->setText(infoMessage);
    m_view.infoMessage->show();
}

void RenderWidget::showRenderDuration(int projectLength)
{
    // rendering full project
    int maxFrame = projectLength > -1 ? projectLength : pCore->projectDuration();
    qDebug() << ":::: GOT PROJECT DURATION: " << pCore->projectDuration();
    if (m_view.render_zone->isChecked()) {
        Monitor *pMon = pCore->getMonitor(Kdenlive::ProjectMonitor);
        int out = qMin(maxFrame, pMon->getZoneEnd());
        m_renderDuration = out - pMon->getZoneStart();
    } else if (m_view.render_guide->isChecked()) {
        double fps = pCore->getCurrentProfile()->fps();
        int startIndex = m_view.guide_start->currentIndex();

        if (startIndex > 0 && !m_currentMarkers.isEmpty()) {
            int markerIndex = startIndex - 1;
            if (markerIndex < m_currentMarkers.size()) {
                const CommentedTime &marker = m_currentMarkers.at(markerIndex);
                if (marker.hasRange()) {
                    double guideStart = marker.time().seconds();
                    double guideEnd = marker.endTime().seconds();
                    int out = qMin(int(GenTime(guideEnd).frames(fps)), maxFrame);
                    m_renderDuration = out - int(GenTime(guideStart).frames(fps));
                } else {
                    double guideStart = m_view.guide_start->itemData(startIndex).toDouble();
                    double guideEnd = m_view.guide_end->itemData(m_view.guide_end->currentIndex()).toDouble();
                    int out;
                    if (guideEnd == -1) {
                        out = pCore->projectDuration() - 1;
                    } else {
                        out = qMin(int(GenTime(guideEnd).frames(fps)), maxFrame);
                    }
                    m_renderDuration = out - int(GenTime(guideStart).frames(fps));
                }
            } else {
                double guideStart = m_view.guide_start->itemData(startIndex).toDouble();
                double guideEnd = m_view.guide_end->itemData(m_view.guide_end->currentIndex()).toDouble();
                int out;
                if (guideEnd == -1) {
                    out = pCore->projectDuration() - 1;
                } else {
                    out = qMin(int(GenTime(guideEnd).frames(fps)), maxFrame);
                }
                m_renderDuration = out - int(GenTime(guideStart).frames(fps));
            }
        } else {
            double guideStart = m_view.guide_start->itemData(startIndex).toDouble();
            double guideEnd = m_view.guide_end->itemData(m_view.guide_end->currentIndex()).toDouble();
            int out;
            if (guideEnd == -1) {
                out = pCore->projectDuration() - 1;
            } else {
                out = qMin(int(GenTime(guideEnd).frames(fps)), maxFrame);
            }
            m_renderDuration = out - int(GenTime(guideStart).frames(fps));
        }
    } else {
        m_renderDuration = maxFrame;
    }

    if (m_freeSpaceStatus != SpaceNotWritable) {
        KIO::filesize_t minimumSize = qMax(static_cast<KIO::filesize_t>(0), static_cast<KIO::filesize_t>(m_renderDuration * 60000));
        if (5 * minimumSize > m_lastFreeSpace || m_freeSpaceStatus != SpaceOk) {
            // Space is limited, inform user
            m_lowSpaceTimer.start();
        }
    }
    updateRenderInfoMessage();
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
    QTreeWidgetItem *node = m_view.running_jobs->itemAt(pos);
    RenderJobItem *renderItem = nullptr;
    if (node) {
        renderItem = static_cast<RenderJobItem *>(node);
    }
    if (!renderItem) {
        return;
    }
    if (renderItem->status() != FINISHEDJOB) {
        return;
    }
    QMenu menu(this);
    QAction *newAct = new QAction(i18n("Add to Current Project"), this);
    connect(newAct, &QAction::triggered, [&, renderItem]() {
        QString fileName = renderItem->text(1);
        if (!QFile::exists(fileName) && fileName.contains(QLatin1Char('&'))) {
            fileName.replace(QLatin1Char('&'), QStringLiteral("&#38;"));
        }
        pCore->activeBin()->slotAddClipToProject(QUrl::fromLocalFile(fileName));
    });
    menu.addAction(newAct);
    QAction *openContainingFolder = new QAction(QIcon::fromTheme(QStringLiteral("edit-find")), i18n("Open Containing Folder"), this);
    connect(openContainingFolder, &QAction::triggered, [&, renderItem]() {
        QString fileName = renderItem->text(1);
        if (!QFile::exists(fileName) && fileName.contains(QLatin1Char('&'))) {
            fileName.replace(QLatin1Char('&'), QStringLiteral("&#38;"));
        }
        pCore->highlightFileInExplorer({QUrl::fromLocalFile(fileName)});
    });
    menu.addAction(openContainingFolder);
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
    QFileInfo updatedPath(path);
    const QString fileName = QDir(pCore->currentDoc()->projectRenderFolder(updatedPath.absolutePath())).absoluteFilePath(updatedPath.fileName());
    QString url = filenameWithExtension(QUrl::fromLocalFile(fileName), extension).toLocalFile();
    if (QFileInfo(url).isRelative()) {
        url.prepend(pCore->currentDoc()->documentRoot());
    }
    m_view.out_file->setUrl(QUrl::fromLocalFile(url));
    QMap<QString, QString> renderProps;
    renderProps.insert(QStringLiteral("renderurl"), url);
    Q_EMIT selectedRenderProfile(renderProps);
}

void RenderWidget::updateMetadataToolTip()
{
    QString tipText;
    QMapIterator<QString, QString> i(pCore->currentDoc()->metadata());
    while (i.hasNext()) {
        i.next();
        QString metaName = i.key().section(QLatin1Char('.'), 2, 2);
        metaName[0] = metaName[0].toUpper();
        tipText.append(QStringLiteral("%1: <b>%2</b><br/>").arg(metaName, i.value()));
    }
    m_view.edit_metadata->setToolTip(tipText);
}

void RenderWidget::updateMissingClipsCount(int total, int used)
{
    m_missingClips = total;
    m_missingUsedClips = used;
    updateRenderInfoMessage();
}

void RenderWidget::updatePowerManagement()
{
    switch (m_renderStatus) {
    case Rendering:
        pCore->window()->mPowerInterface.setPreventSleep(true);
        break;
    default:
        pCore->window()->mPowerInterface.setPreventSleep(false);
        break;
    }
}

bool RenderWidget::isRendering() const
{
    return m_renderStatus == Rendering;
}
