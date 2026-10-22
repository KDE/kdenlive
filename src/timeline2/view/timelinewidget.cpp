/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include <KActionCollection>
#include <KLocalizedQmlContext>

#include "../model/builders/meltBuilder.hpp"

// Required to pass the c++ classes to qml
#include "bin/bin.h"
#include "bin/model/markerlistmodel.hpp"
#include "bin/model/markersortmodel.h"
#include "bin/model/subtitlemodel.hpp"
#include "capture/mediacapture.h"

#include "core.h"
#include "doc/kdenlivedoc.h"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"
#include "monitor/monitorproxy.h"
#include "timelinewidget.h"

#include <QAction>
#include <QActionGroup>
#include <QFontDatabase>
#include <QMenu>
#include <QPointer>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QUuid>

const int TimelineWidget::comboScale[] = {1, 2, 4, 8, 15, 30, 50, 75, 100, 150, 200, 300, 500, 800, 1000, 1500, 2000, 3000, 6000, 15000, 30000};

TimelineWidget::TimelineWidget(const QUuid uuid, QWidget *parent)
    : QQuickWidget(Core::sharedQmlEngine(), parent)
    , timelineController(this)
    , m_uuid(uuid)
{
    setClearColor(palette().window().color());
    m_sortModel = std::make_unique<QSortFilterProxyModel>(this);
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setVisible(false);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setFocusPolicy(Qt::StrongFocus);
    m_favEffects = new QMenu(i18n("Insert an effect..."), this);
    m_favCompositions = new QMenu(i18n("Insert a composition..."), this);
    installEventFilter(this);
    connect(&timelineController, &TimelineController::zoneMoved, this, &TimelineWidget::zoneMoved);
    connect(&timelineController, &TimelineController::ungrabHack, this, &TimelineWidget::slotUngrabHack);
    connect(&timelineController, &TimelineController::regainFocus, this, &TimelineWidget::regainFocus, Qt::DirectConnection);
    connect(&timelineController, &TimelineController::stopAudioRecord, this, &TimelineWidget::stopAudioRecord, Qt::DirectConnection);
    m_targetsMenu = new QMenu(this);
}

TimelineWidget::~TimelineWidget()
{
    QObject::disconnect(m_addMenuConnection);
    rootObject()->blockSignals(true);
    timelineController.prepareClose();
    setSource(QUrl());
}

void TimelineWidget::updateEffectFavorites()
{
    const QMap<QString, QString> effects = sortedItems(KdenliveSettings::favorite_effects(), false);
    QMapIterator<QString, QString> i(effects);
    m_favEffects->clear();
    while (i.hasNext()) {
        i.next();
        QAction *ac = m_favEffects->addAction(i.key());
        ac->setData(i.value());
    }
}

void TimelineWidget::updateTransitionFavorites()
{
    const QMap<QString, QString> effects = sortedItems(KdenliveSettings::favorite_transitions(), true);
    QMapIterator<QString, QString> i(effects);
    m_favCompositions->clear();
    while (i.hasNext()) {
        i.next();
        QAction *ac = m_favCompositions->addAction(i.key());
        ac->setData(i.value());
    }
}

const QMap<QString, QString> TimelineWidget::sortedItems(const QStringList &items, bool isTransition)
{
    QMap<QString, QString> sortedItems;
    for (const QString &effect : items) {
        sortedItems.insert(timelineController.getAssetName(effect, isTransition), effect);
    }
    return sortedItems;
}

void TimelineWidget::populateActions(KActionCollection *actions)
{
    Q_ASSERT(actions);
    Q_ASSERT(!m_timelineClipMenu);
    if (m_timelineClipMenu) {
        return;
    }
    timelineController.populateActions(actions);

    m_timelineClipMenu = new QMenu(this);
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("edit_copy")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("duplicate_timeline_clip")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("paste_effects")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("delete_effects")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("group_clip")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("ungroup_clip")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("edit_item_duration")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("clip_split")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("clip_enable_all")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("clip_disable_all")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("delete_timeline_selection")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("extract_clip")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("replace_timeline_clip")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("save_to_bin")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("send_sequence")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("copy_to_sequence")));

    auto *markers = new QMenu(i18n("Markers"), this);
    markers->addAction(actions->action(QStringLiteral("add_marker_guide_quickly")));
    markers->addSeparator();
    markers->addAction(actions->action(QStringLiteral("add_clip_marker")));
    auto *markerCategories = new QMenu(i18n("Add Markers by Category"), this);
    markerCategories->addAction(actions->action(QStringLiteral("add_marker_guide_1")));
    markerCategories->addAction(actions->action(QStringLiteral("add_marker_guide_2")));
    markerCategories->addAction(actions->action(QStringLiteral("add_marker_guide_3")));
    markerCategories->addAction(actions->action(QStringLiteral("add_marker_guide_4")));
    markerCategories->addAction(actions->action(QStringLiteral("add_marker_guide_5")));
    markerCategories->addAction(actions->action(QStringLiteral("add_marker_guide_6")));
    markerCategories->addAction(actions->action(QStringLiteral("add_marker_guide_7")));
    markerCategories->addAction(actions->action(QStringLiteral("add_marker_guide_8")));
    markerCategories->addAction(actions->action(QStringLiteral("add_marker_guide_9")));
    markerCategories->addAction(actions->action(QStringLiteral("add_marker_guide_10")));
    markers->addMenu(markerCategories);
    markers->addAction(actions->action(QStringLiteral("edit_clip_marker")));
    markers->addAction(actions->action(QStringLiteral("delete_clip_marker")));
    markers->addAction(actions->action(QStringLiteral("delete_all_clip_markers")));
    markers->addSeparator();
    markers->addAction(actions->action(QStringLiteral("add_sequence_marker")));
    markers->addAction(actions->action(QStringLiteral("edit_sequence_marker")));
    markers->addAction(actions->action(QStringLiteral("delete_sequence_marker")));
    markers->addAction(actions->action(QStringLiteral("delete_all_sequence_markers")));
    markers->addAction(actions->action(QStringLiteral("lock_guides")));
    markers->addSeparator();
    markers->addAction(actions->action(QStringLiteral("search_guide")));
    markers->addAction(actions->action(QStringLiteral("export_guides")));
    markers->addSeparator();
    m_timelineClipMenu->addMenu(markers);

    auto *alignment = new QMenu(i18n("Align to Reference"), this);
    alignment->addAction(actions->action(QStringLiteral("set_audio_align_ref")));
    alignment->addAction(actions->action(QStringLiteral("align_audio")));
    alignment->addAction(actions->action(QStringLiteral("set_timecode_ref")));
    alignment->addAction(actions->action(QStringLiteral("align_timecode")));
    m_timelineClipMenu->addMenu(alignment);
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("edit_item_speed")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("edit_item_remap")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("clip_in_project_tree")));
    m_timelineClipMenu->addAction(actions->action(QStringLiteral("cut_timeline_clip")));

    m_timelineCompositionMenu = new QMenu(this);
    m_timelineCompositionMenu->addAction(actions->action(QStringLiteral("edit_item_duration")));
    m_timelineCompositionMenu->addAction(actions->action(QStringLiteral("edit_copy")));
    m_timelineCompositionMenu->addAction(actions->action(QStringLiteral("delete_timeline_selection")));
    m_timelineMixMenu = new QMenu(this);
    m_timelineMixMenu->addAction(actions->action(QStringLiteral("delete_timeline_selection")));
    m_timelineSubtitleClipMenu = new QMenu(this);
    m_timelineSubtitleClipMenu->addAction(actions->action(QStringLiteral("edit_copy")));
    m_timelineSubtitleClipMenu->addAction(actions->action(QStringLiteral("delete_timeline_selection")));

    m_guideMenu = new QMenu(i18n("Go to Marker…"), this);
    m_timelineMenu = new QMenu(this);
    m_timelineMenu->addAction(actions->action(QStringLiteral("edit_paste")));
    m_timelineMenu->addAction(actions->action(QStringLiteral("insert_space")));
    m_timelineMenu->addAction(actions->action(QStringLiteral("delete_space")));
    m_timelineMenu->addAction(actions->action(QStringLiteral("delete_space_all_tracks")));
    m_timelineMenu->addAction(actions->action(QStringLiteral("add_sequence_marker")));
    m_timelineMenu->addAction(actions->action(QStringLiteral("edit_sequence_marker")));
    auto *timelineGaps = new QMenu(i18n("Identify Gaps"), this);
    timelineGaps->addAction(actions->action(QStringLiteral("identify_gaps_all_tracks")));
    timelineGaps->addAction(actions->action(QStringLiteral("add_markers_at_gaps_on_track")));
    m_timelineMenu->addMenu(timelineGaps);
    m_timelineMenu->addMenu(m_guideMenu);

    m_timelineRulerMenu = new QMenu(this);
    m_timelineRulerMenu->addAction(actions->action(QStringLiteral("add_sequence_marker")));
    m_timelineRulerMenu->addAction(actions->action(QStringLiteral("edit_sequence_marker")));
    m_timelineRulerMenu->addAction(actions->action(QStringLiteral("lock_guides")));
    m_timelineRulerMenu->addAction(actions->action(QStringLiteral("export_guides")));
    m_timelineRulerMenu->addMenu(m_guideMenu);
    m_timelineRulerMenu->addAction(actions->action(QStringLiteral("mark_in")));
    m_timelineRulerMenu->addAction(actions->action(QStringLiteral("mark_out")));
    m_timelineRulerMenu->addAction(actions->action(QStringLiteral("select_timeline_zone")));
    m_timelineRulerMenu->addAction(actions->action(QStringLiteral("create_marker_from_zone")));
    m_timelineRulerMenu->addAction(actions->action(QStringLiteral("create_marker_from_zone_quick")));
    m_timelineRulerMenu->addAction(actions->action(QStringLiteral("add_project_note")));
    m_timelineRulerMenu->addAction(actions->action(QStringLiteral("add_subtitle")));

    m_headerMenu = new QMenu(this);
    m_headerMenu->addAction(actions->action(QStringLiteral("insert_track")));
    m_headerMenu->addAction(actions->action(QStringLiteral("delete_track")));
    m_headerMenu->addAction(actions->action(QStringLiteral("move_track_up")));
    m_headerMenu->addAction(actions->action(QStringLiteral("move_track_down")));
    m_headerMenu->addAction(actions->action(QStringLiteral("fit_all_tracks")));
    m_headerMenu->addAction(actions->action(QStringLiteral("show_track_record")));
    m_headerMenu->addAction(actions->action(QStringLiteral("select_track")));
    m_headerMenu->addAction(actions->action(QStringLiteral("separate_audio_channels")));
    m_headerMenu->addAction(actions->action(QStringLiteral("normalize_audio_thumbnails")));
    auto *headerGaps = new QMenu(i18n("Identify Gaps"), this);
    headerGaps->addAction(actions->action(QStringLiteral("identify_gaps_all_tracks")));
    headerGaps->addAction(actions->action(QStringLiteral("add_markers_at_gaps_on_track")));
    m_headerMenu->addMenu(headerGaps);
    connect(m_headerMenu, &QMenu::aboutToShow, this,
            [this, up = actions->action(QStringLiteral("move_track_up")), down = actions->action(QStringLiteral("move_track_down"))]() {
                up->setEnabled(timelineController.canMoveTrackUp());
                down->setEnabled(timelineController.canMoveTrackDown());
            });
    m_thumbsMenu = new QMenu(i18n("Thumbnails"), this);
    auto *thumbGroup = new QActionGroup(m_thumbsMenu);
    const auto addThumbnailFormat = [this, thumbGroup](const QString &label, const QString &format) {
        auto *action = m_thumbsMenu->addAction(label);
        action->setData(format);
        action->setCheckable(true);
        thumbGroup->addAction(action);
    };
    addThumbnailFormat(i18n("In Frame"), QStringLiteral("2"));
    addThumbnailFormat(i18n("In/Out Frames"), QStringLiteral("0"));
    addThumbnailFormat(i18n("All Frames"), QStringLiteral("1"));
    addThumbnailFormat(i18n("No Thumbnails"), QStringLiteral("3"));
    m_headerMenu->addMenu(m_thumbsMenu);
    m_editGuideAcion = actions->action(QStringLiteral("edit_sequence_marker"));
    m_addClipMenu = new QMenu(i18n("Add Clip"), this);
    m_addClipMenu->addAction(actions->action(QStringLiteral("add_clip")));
    m_addClipMenu->addAction(actions->action(QStringLiteral("add_color_clip")));
    m_addClipMenu->addAction(actions->action(QStringLiteral("add_slide_clip")));
    m_addClipMenu->addAction(actions->action(QStringLiteral("add_text_clip")));
    m_addClipMenu->addAction(actions->action(QStringLiteral("add_text_template_clip")));
    m_addClipMenu->addAction(actions->action(QStringLiteral("add_animation_clip")));
    m_addClipMenu->addAction(actions->action(QStringLiteral("add_playlist_clip")));
    m_addClipMenu->addAction(actions->action(QStringLiteral("download_resource")));
    updateEffectFavorites();
    updateTransitionFavorites();
    connect(m_favEffects, &QMenu::triggered, this, [&](QAction *ac) { timelineController.addEffectToClip(ac->data().toString()); });
    connect(m_favCompositions, &QMenu::triggered, this, [&](QAction *ac) { timelineController.addCompositionToClip(ac->data().toString()); });
    connect(m_guideMenu, &QMenu::triggered, this, [&](QAction *ac) { timelineController.setPosition(ac->data().toInt()); });
    connect(m_thumbsMenu, &QMenu::triggered, this,
            [&](QAction *ac) { timelineController.setActiveTrackProperty(QStringLiteral("kdenlive:thumbs_format"), ac->data().toString()); });
    // Fix qml focus issue
    connect(m_headerMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineClipMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineClipMenu, &QMenu::triggered, this, &TimelineWidget::slotResetContextPos);
    connect(m_timelineCompositionMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineRulerMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);
    connect(m_timelineMenu, &QMenu::triggered, this, &TimelineWidget::slotResetContextPos);
    connect(m_timelineMenu, &QMenu::aboutToShow, this, &TimelineWidget::updateAddClipMenuStatus);
    connect(m_timelineMenu, &QMenu::triggered, this, &TimelineWidget::updateAddClipMenuStatus);
    connect(m_timelineMenu, &QMenu::aboutToHide, this, [this]() { QObject::disconnect(m_addMenuConnection); });
    connect(m_timelineSubtitleClipMenu, &QMenu::aboutToHide, this, &TimelineWidget::slotUngrabHack, Qt::DirectConnection);

    m_timelineClipMenu->addMenu(m_favEffects);
    m_timelineClipMenu->addMenu(m_favCompositions);
    m_timelineMenu->addMenu(m_favCompositions);
    m_timelineMenu->addMenu(m_addClipMenu);
}

const QUuid &TimelineWidget::getUuid() const
{
    return m_uuid;
}

void TimelineWidget::setModel(const std::shared_ptr<TimelineItemModel> &model, MonitorProxy *proxy, bool previewEnabled)
{
    loading = true;
    Q_ASSERT(model != nullptr);
    connect(&timelineController, &TimelineController::timelineMouseOffsetChanged, this, &TimelineWidget::emitMousePos, Qt::QueuedConnection);
    m_sortModel->setSourceModel(model.get());
    m_sortModel->setSortRole(TimelineItemModel::SortRole);
    m_sortModel->sort(0, Qt::DescendingOrder);
    timelineController.setModel(model, previewEnabled);
    setInitialProperties({{"controller", QVariant::fromValue(model.get())},
                          {"timeline", QVariant::fromValue(&timelineController)},
                          {"multitrack", QVariant::fromValue(m_sortModel.get())},
                          {"guidesModel", QVariant::fromValue(model->getFilteredGuideModel().get())},
                          {"proxy", QVariant::fromValue(proxy)},
                          {"subtitleModel", QVariant::fromValue(model->getSubtitleModel().get())}});
    loadFromModule(QStringLiteral("org.kde.kdenlive"), QStringLiteral("Timeline"));

    connect(rootObject(), SIGNAL(zoomIn(bool)), this, SIGNAL(zoomIn(bool)));
    connect(rootObject(), SIGNAL(zoomOut(bool)), this, SIGNAL(zoomOut(bool)));
    connect(rootObject(), SIGNAL(processingDrag(bool)), this, SIGNAL(processingDrag(bool)));
    connect(&timelineController, &TimelineController::seeked, proxy, &MonitorProxy::setPosition);
    connect(rootObject(), SIGNAL(showClipMenu(int)), this, SLOT(showClipMenu(int)));
    connect(rootObject(), SIGNAL(showMixMenu(int)), this, SLOT(showMixMenu(int)));
    connect(rootObject(), SIGNAL(showCompositionMenu()), this, SLOT(showCompositionMenu()));
    connect(rootObject(), SIGNAL(showTimelineMenu()), this, SLOT(showTimelineMenu()));
    connect(rootObject(), SIGNAL(showRulerMenu()), this, SLOT(showRulerMenu()));
    connect(rootObject(), SIGNAL(showHeaderMenu()), this, SLOT(showHeaderMenu()));
    connect(rootObject(), SIGNAL(showTargetMenu(int)), this, SLOT(showTargetMenu(int)));
    connect(rootObject(), SIGNAL(showSubtitleClipMenu()), this, SLOT(showSubtitleClipMenu()));
    connect(rootObject(), SIGNAL(markerActivated(int)), this, SIGNAL(markerActivated(int)));
    connect(rootObject(), SIGNAL(updateTimelineMousePos(int, int)), this, SIGNAL(updateTimelineMousePos(int, int)));
    timelineController.setRoot(rootObject());
    setVisible(true);
    loading = false;
    timelineController.checkDuration();
}

void TimelineWidget::emitMousePos(int offset)
{
    Q_EMIT updateTimelineMousePos(int((offset + mapFromGlobal(QCursor::pos()).x()) / timelineController.scaleFactor()), timelineController.duration());
}

void TimelineWidget::mousePressEvent(QMouseEvent *event)
{
    Q_EMIT focusProjectMonitor();
    m_clickPos = event->globalPosition().toPoint();
    QQuickWidget::mousePressEvent(event);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (isEnabled()) {
        emitMousePos(timelineController.timelineMouseOffset());
    }
    QQuickWidget::mouseMoveEvent(event);
}

void TimelineWidget::hideEvent(QHideEvent *event)
{
    // Menus borrow active-timeline commands: none may survive a tab switch.
    for (QMenu *menu : findChildren<QMenu *>()) {
        menu->close();
    }
    QObject::disconnect(m_addMenuConnection);
    QQuickWidget::hideEvent(event);
}

void TimelineWidget::showClipMenu(int cid)
{
    if (!isVisible() || !isEnabled()) {
        return;
    }
    // Hide not applicable effects
    QList<QAction *> effects = m_favEffects->actions();
    int tid = model()->getClipTrackId(cid);
    bool isAudioTrack = false;
    if (tid > -1) {
        isAudioTrack = model()->isAudioTrack(tid);
    }
    m_favCompositions->setEnabled(!isAudioTrack);
    for (auto ac : std::as_const(effects)) {
        const QString &id = ac->data().toString();
        if (EffectsRepository::get()->isAudioEffect(id) != isAudioTrack) {
            ac->setVisible(false);
        } else {
            ac->setVisible(true);
        }
    }
    m_timelineClipMenu->popup(m_clickPos);
}

void TimelineWidget::showMixMenu(int /*cid*/)
{
    if (!isVisible() || !isEnabled()) {
        return;
    }
    // Show mix menu
    m_timelineMixMenu->popup(m_clickPos);
}

void TimelineWidget::showCompositionMenu()
{
    if (!isVisible() || !isEnabled()) {
        return;
    }
    m_timelineCompositionMenu->popup(m_clickPos);
}

void TimelineWidget::showHeaderMenu()
{
    if (!isVisible() || !isEnabled()) {
        return;
    }
    bool isAudio = timelineController.isActiveTrackAudio();
    QList<QAction *> menuActions = m_headerMenu->actions();
    QList<QAction *> audioActions;
    QStringList allowedActions = {QLatin1String("show_track_record"), QLatin1String("separate_channels"), QLatin1String("normalize_channels")};
    for (QAction *ac : std::as_const(menuActions)) {
        if (allowedActions.contains(ac->data().toString())) {
            if (ac->data().toString() == QLatin1String("separate_channels")) {
                ac->setChecked(KdenliveSettings::displayallchannels());
            }
            audioActions << ac;
        }
    }
    if (!isAudio) {
        // Video track
        int currentThumbs = timelineController.getActiveTrackProperty(QStringLiteral("kdenlive:thumbs_format")).toInt();
        QList<QAction *> actions = m_thumbsMenu->actions();
        for (QAction *ac : std::as_const(actions)) {
            if (ac->data().toInt() == currentThumbs) {
                ac->setChecked(true);
                break;
            }
        }
        m_thumbsMenu->menuAction()->setVisible(true);
        for (auto ac : std::as_const(audioActions)) {
            ac->setVisible(false);
        }
    } else {
        // Audio track
        m_thumbsMenu->menuAction()->setVisible(false);
        for (auto ac : std::as_const(audioActions)) {
            ac->setVisible(true);
            if (ac->data().toString() == QLatin1String("show_track_record")) {
                ac->setChecked(timelineController.getActiveTrackProperty(QStringLiteral("kdenlive:audio_rec")).toInt() == 1);
            }
        }
    }
    m_headerMenu->popup(m_clickPos);
}

void TimelineWidget::showTargetMenu(int tid)
{
    if (!isVisible() || !isEnabled()) {
        return;
    }
    int currentTargetStream;
    if (tid == -1) {
        // Called through shortcut
        tid = timelineController.activeTrack();
        if (tid == -1) {
            return;
        }
        if (timelineController.clipTargets() < 2 || !model()->isAudioTrack(tid)) {
            pCore->displayMessage(i18n("No available stream"), MessageType::ErrorMessage);
            return;
        }
        QVariant returnedValue;
        QMetaObject::invokeMethod(rootObject(), "getActiveTrackStreamPos", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
        m_clickPos = mapToGlobal(QPoint(5, y())) + QPoint(0, returnedValue.toInt());
    }
    QMap<int, QString> possibleTargets = timelineController.getCurrentTargets(tid, currentTargetStream);
    m_targetsMenu->clear();
    if (m_targetsGroup) {
        delete m_targetsGroup;
    }
    m_targetsGroup = new QActionGroup(this);
    QMapIterator<int, QString> i(possibleTargets);
    while (i.hasNext()) {
        i.next();
        QAction *ac = m_targetsMenu->addAction(i.value());
        ac->setData(i.key());
        m_targetsGroup->addAction(ac);
        ac->setCheckable(true);
        if (i.key() == currentTargetStream) {
            ac->setChecked(true);
        }
    }
    connect(m_targetsGroup, &QActionGroup::triggered, this, [this, tid](QAction *action) {
        int targetStream = action->data().toInt();
        timelineController.assignAudioTarget(tid, targetStream);
    });
    if (m_targetsMenu->isEmpty() || possibleTargets.isEmpty()) {
        m_headerMenu->popup(m_clickPos);
    } else {
        m_targetsMenu->popup(m_clickPos);
    }
}

void TimelineWidget::showRulerMenu()
{
    if (!isVisible() || !isEnabled()) {
        return;
    }
    m_guideMenu->clear();
    const QList<CommentedTime> guides = pCore->currentDoc()->getGuideModel(m_uuid)->getAllMarkers();
    m_editGuideAcion->setEnabled(false);
    double fps = pCore->getCurrentFps();
    int currentPos = rootObject()->property("consumerPosition").toInt();
    for (const auto &guide : guides) {
        auto *ac = new QAction(guide.comment(), m_guideMenu);
        int frame = guide.time().frames(fps);
        ac->setData(frame);
        if (frame == currentPos) {
            m_editGuideAcion->setEnabled(true);
        }
        m_guideMenu->addAction(ac);
    }
    m_timelineRulerMenu->popup(m_clickPos);
}

void TimelineWidget::showTimelineMenu()
{
    if (!isVisible() || !isEnabled()) {
        return;
    }
    m_guideMenu->clear();
    const QList<CommentedTime> guides = pCore->currentDoc()->getGuideModel(m_uuid)->getAllMarkers();
    m_editGuideAcion->setEnabled(false);
    double fps = pCore->getCurrentFps();
    int currentPos = rootObject()->property("consumerPosition").toInt();
    for (const auto &guide : guides) {
        auto *ac = new QAction(guide.comment(), m_guideMenu);
        int frame = guide.time().frames(fps);
        ac->setData(frame);
        if (frame == currentPos) {
            m_editGuideAcion->setEnabled(true);
        }
        m_guideMenu->addAction(ac);
    }
    QObject::disconnect(m_addMenuConnection);
    m_addMenuConnection = connect(m_addClipMenu, &QMenu::aboutToShow, this, [this]() {
        QPoint posInWidget = mapFromGlobal(m_clickPos);
        int addClipFrame = timelineController.getMousePos(posInWidget);
        int addClipTrack = timelineController.getMouseTrack(posInWidget);
        // Calculate maximum available space on this track
        int maxSpace = timelineController.getFreeSpace(addClipTrack, addClipFrame);
        pCore->bin()->setSuggestedDuration(maxSpace);
        pCore->bin()->setReadyCallBack([timeline = QPointer<TimelineWidget>(this), addClipTrack, addClipFrame](const QString &clipId) {
            if (timeline) {
                timeline->controller()->insertClips(addClipTrack, addClipFrame, QStringList(clipId), true, true);
            }
        });
        QObject::disconnect(m_addMenuConnection);
    });
    m_timelineMenu->popup(m_clickPos);
}

void TimelineWidget::showSubtitleClipMenu()
{
    if (!isVisible() || !isEnabled()) {
        return;
    }
    m_timelineSubtitleClipMenu->popup(m_clickPos);
}

void TimelineWidget::updateAddClipMenuStatus()
{
    int tid = timelineController.getMouseTrack();
    if (tid == -2 || tid == -1 || !model()->isTrack(tid) || model()->isAudioTrack(tid)) {
        m_addClipMenu->setEnabled(false);
    } else {
        m_addClipMenu->setEnabled(true);
    }
}

void TimelineWidget::slotChangeZoom(int value, bool zoomOnMouse)
{
    double pixelScale = QFontMetrics(font()).maxWidth() * 2;
    timelineController.setScaleFactorOnMouse(pixelScale / comboScale[value], zoomOnMouse);
}

void TimelineWidget::slotCenterView()
{
    QMetaObject::invokeMethod(rootObject(), "centerViewOnCursor");
}

void TimelineWidget::slotFitZoom()
{
    QVariant returnedValue;
    double prevScale = timelineController.scaleFactor();
    QMetaObject::invokeMethod(rootObject(), "fitZoom", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    double scale = returnedValue.toDouble();
    QMetaObject::invokeMethod(rootObject(), "scrollPos", Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue));
    int scrollPos = returnedValue.toInt();
    if (qFuzzyCompare(prevScale, scale) && scrollPos == 0) {
        scale = m_prevScale;
        scrollPos = m_scrollPos;
    } else {
        m_prevScale = prevScale;
        m_scrollPos = scrollPos;
        scrollPos = 0;
    }
    timelineController.setScaleFactorOnMouse(scale, false);
    // Update zoom slider
    Q_EMIT timelineController.updateZoom(scale);
    QMetaObject::invokeMethod(rootObject(), "goToStart", Q_ARG(QVariant, scrollPos));
}

Mlt::Tractor *TimelineWidget::tractor()
{
    return timelineController.tractor();
}

TimelineController *TimelineWidget::controller()
{
    return &timelineController;
}

std::shared_ptr<TimelineItemModel> TimelineWidget::model()
{
    return timelineController.getModel();
}

void TimelineWidget::zoneUpdated(const QPoint &zone)
{
    timelineController.setZone(zone, false);
}

void TimelineWidget::zoneUpdatedWithUndo(const QPoint &oldZone, const QPoint &newZone)
{
    timelineController.updateZone(oldZone, newZone);
}

QPair<int, int> TimelineWidget::getAvTracksCount() const
{
    return timelineController.getAvTracksCount();
}

void TimelineWidget::slotUngrabHack()
{
    // Workaround bug: https://bugreports.qt.io/browse/QTBUG-59044
    // https://phabricator.kde.org/D5515
    QTimer::singleShot(250, this, [this]() {
        // Reset menu position, necessary if user closes the menu without selecting any action
        rootObject()->setProperty("clickFrame", -1);
    });
    if (quickWindow()) {
        if (quickWindow()->mouseGrabberItem()) {
            quickWindow()->mouseGrabberItem()->ungrabMouse();
            QPoint mousePos = mapFromGlobal(QCursor::pos());
            QMetaObject::invokeMethod(rootObject(), "regainFocus", Qt::DirectConnection, Q_ARG(QVariant, mousePos));
        } else {
            QMetaObject::invokeMethod(rootObject(), "endDrag", Qt::DirectConnection);
        }
    }
}

void TimelineWidget::slotResetContextPos(QAction *)
{
    rootObject()->setProperty("clickFrame", -1);
    m_clickPos = QPoint();
}

int TimelineWidget::zoomForScale(double value) const
{
    int scale = int(100 / value);
    int ix = 13;
    while (comboScale[ix] > scale && ix > 0) {
        ix--;
    }
    return ix;
}

void TimelineWidget::focusTimeline()
{
    setFocus();
    if (rootObject()) {
        rootObject()->setFocus(true);
    }
}

void TimelineWidget::endDrag()
{
    if (rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "endBinDrag");
    }
}

void TimelineWidget::startAudioRecord(int tid)
{
    if (rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "startAudioRecord", Qt::DirectConnection, Q_ARG(QVariant, tid));
    }
}

void TimelineWidget::stopAudioRecord()
{
    if (rootObject()) {
        QMetaObject::invokeMethod(rootObject(), "stopAudioRecord", Qt::DirectConnection);
    }
}

void TimelineWidget::regainFocus()
{
    if (underMouse() && rootObject()) {
        QPoint mousePos = mapFromGlobal(QCursor::pos());
        QMetaObject::invokeMethod(rootObject(), "regainFocus", Qt::DirectConnection, Q_ARG(QVariant, mousePos));
    }
}

bool TimelineWidget::hasSubtitles() const
{
    return timelineController.getModel()->hasSubtitleModel();
}

void TimelineWidget::connectSubtitleModel(bool firstConnect)
{
    qDebug() << "root context get sub model new function";
    if (!model()->hasSubtitleModel()) {
        return;
    }

    if (firstConnect) {
        rootObject()->setProperty("subtitleModel", QVariant::fromValue(model()->getSubtitleModel().get()));
        QQmlEngine::setObjectOwnership(model()->getSubtitleModel().get(), QQmlEngine::CppOwnership);
    }
}
