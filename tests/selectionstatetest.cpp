/*
    SPDX-FileCopyrightText: 2026 KDE Contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "test_utils.hpp"

#include "dialogs/subtitleedit.h"
#include "doc/kdenlivedoc.h"
#include "timeline2/model/timelinefunctions.hpp"
#include "timeline2/view/timelinecontroller.h"
#include "transitions/transitionsrepository.hpp"

namespace {
struct SelectionProject
{
    std::unique_ptr<KdenliveDoc> document;
    std::shared_ptr<TimelineItemModel> timeline;
    TimelineController controller{nullptr};
    TimelineController::SelectionState emitted;
    bool received{false};

    ~SelectionProject()
    {
        if (timeline) {
            controller.prepareClose();
            timeline.reset();
        }
        if (document) {
            pCore->projectManager()->closeCurrentDocument(false, false);
        }
    }

    void create()
    {
        pCore->projectItemModel()->clean();
        document = std::make_unique<KdenliveDoc>(std::make_shared<DocUndoStack>(nullptr));
        pCore->projectManager()->testSetDocument(document.get());
        REQUIRE(KdenliveTests::updateTimeline(false, QString(), QString(), QDateTime::currentDateTime(), false));
        timeline = document->getTimeline(document->uuid());
        pCore->projectManager()->testSetActiveTimeline(timeline);
        controller.setModel(timeline, false);
        QObject::disconnect(&controller, &TimelineController::selectionChanged, &controller, &TimelineController::updateTrimmingMode);
        QObject::connect(&controller, &TimelineController::selectionStateChanged, &controller, [this](const TimelineController::SelectionState &state) {
            emitted = state;
            received = true;
        });
    }

    int insert(const QString &binId, int position)
    {
        int id = -1;
        REQUIRE(timeline->requestClipInsertion(binId, timeline->getTrackIndexFromPosition(2), position, id, true, true, false));
        return id;
    }

    const TimelineController::SelectionState &select(const std::unordered_set<int> &ids)
    {
        received = false;
        REQUIRE(timeline->requestSetSelection(ids));
        REQUIRE(received);
        return emitted;
    }

    const TimelineController::SelectionState &clear()
    {
        received = false;
        REQUIRE(timeline->requestClearSelection());
        REQUIRE(received);
        return emitted;
    }
};
} // namespace

TEST_CASE_METHOD(SelectionProject, "Subtitle editor deletes only its displayed subtitle and undo restores grouping", "[TimelineSelection][SubtitleDeletion]")
{
    create();
    const QString binId = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", pCore->projectItemModel());
    const int clip = insert(binId, 0);
    auto subtitles = timeline->createSubtitleModel();
    subtitles->removeAllSubtitles();
    const int subtitle = KdenliveTests::getNextId();
    const int otherSubtitle = KdenliveTests::getNextId();
    const double fps = pCore->getCurrentFps();
    REQUIRE(subtitles->addSubtitle(subtitle, {0, GenTime(100, fps)},
                                   SubtitleEvent(true, GenTime(120, fps), "Default", "", 0, 0, 0, "", QStringLiteral("Edited")), false, false));
    REQUIRE(subtitles->addSubtitle(otherSubtitle, {0, GenTime(140, fps)},
                                   SubtitleEvent(true, GenTime(160, fps), "Default", "", 0, 0, 0, "", QStringLiteral("Other")), false, false));

    SubtitleEdit editor;
    editor.setModel(subtitles);
    editor.setActiveSubtitle(subtitle);
    int requestedId = -1;
    QObject::connect(&editor, &SubtitleEdit::deleteSubtitleRequested, &editor, [&](int id) { requestedId = id; });

    std::unordered_set<int> originalGroup;
    SECTION("Mixed temporary selection")
    {
        select({subtitle, clip, otherSubtitle});
    }
    SECTION("An unrelated clip is selected")
    {
        select({clip});
    }
    SECTION("Saved group")
    {
        REQUIRE(timeline->requestClipsGroup({subtitle, clip}, true, GroupType::Normal) > 0);
        originalGroup = timeline->getGroupElements(subtitle);
        select({subtitle});
    }
    SECTION("Nested saved group")
    {
        REQUIRE(timeline->requestClipsGroup({subtitle, clip}, true, GroupType::Normal) > 0);
        REQUIRE(timeline->requestClipsGroup({clip, otherSubtitle}, true, GroupType::Normal) > 0);
        originalGroup = timeline->getGroupElements(subtitle);
        select({subtitle});
    }

    const auto undoStack = pCore->undoStack();
    const int undoIndex = undoStack->index();
    editor.buttonDelete->click();
    CHECK(requestedId == subtitle);
    CHECK_FALSE(timeline->isSubTitle(subtitle));
    CHECK(timeline->isSubTitle(otherSubtitle));
    CHECK(timeline->isClip(clip));
    CHECK_FALSE(editor.buttonDelete->isEnabled());
    CHECK(undoStack->index() == undoIndex + 1);
    // The timeline cross-check only recognizes clips and compositions as group leaves.
    CHECK(KdenliveTests::groupsModel(timeline)->checkConsistency(true, false));

    undoStack->undo();
    CHECK(timeline->isSubTitle(subtitle));
    CHECK(subtitles->getText(subtitle) == QStringLiteral("Edited"));
    CHECK(timeline->isSubTitle(otherSubtitle));
    CHECK(timeline->isClip(clip));
    if (!originalGroup.empty()) {
        CHECK(timeline->getGroupElements(subtitle) == originalGroup);
    }
    CHECK(KdenliveTests::groupsModel(timeline)->checkConsistency(true, false));
    undoStack->redo();
    CHECK_FALSE(timeline->isSubTitle(subtitle));
    CHECK(timeline->isSubTitle(otherSubtitle));
    CHECK(timeline->isClip(clip));
    CHECK(KdenliveTests::groupsModel(timeline)->checkConsistency(true, false));
}

TEST_CASE_METHOD(SelectionProject, "Targeted subtitle deletion rejects invalid or locked targets", "[TimelineSelection][SubtitleDeletion]")
{
    create();
    const QString binId = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", pCore->projectItemModel());
    const int clip = insert(binId, 0);
    auto subtitles = timeline->createSubtitleModel();
    subtitles->removeAllSubtitles();
    const int subtitle = KdenliveTests::getNextId();
    const double fps = pCore->getCurrentFps();
    REQUIRE(subtitles->addSubtitle(subtitle, {0, GenTime(100, fps)},
                                   SubtitleEvent(true, GenTime(120, fps), "Default", "", 0, 0, 0, "", QStringLiteral("Edited")), false, false));
    select({clip, subtitle});
    const auto selection = timeline->getCurrentSelection();
    const int undoIndex = pCore->undoStack()->index();

    CHECK_FALSE(timeline->requestSingleSubtitleDeletion(clip));
    CHECK_FALSE(timeline->requestSingleSubtitleDeletion(-1));
    SubtitleEdit editor;
    editor.setModel(subtitles);
    editor.setActiveSubtitle(subtitle);
    subtitles->switchLocked();
    editor.buttonDelete->click();
    CHECK_FALSE(timeline->requestSingleSubtitleDeletion(subtitle));
    subtitles->switchLocked();

    editor.setModel(nullptr);
    CHECK_FALSE(editor.buttonDelete->isEnabled());
    Q_EMIT editor.deleteSubtitleRequested(subtitle); // The old timeline must be disconnected.
    CHECK(timeline->isSubTitle(subtitle));
    CHECK(timeline->isClip(clip));
    CHECK(timeline->getCurrentSelection() == selection);
    CHECK(pCore->undoStack()->index() == undoIndex);
}

TEST_CASE_METHOD(SelectionProject, "Timeline selection payload counts all selected item kinds", "[TimelineSelection]")
{
    create();
    const QString color = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", pCore->projectItemModel());
    const QString title = KdenliveTests::createTextProducer(pCore->getProjectProfile(), pCore->projectItemModel(),
                                                            QStringLiteral("<kdenlivetitle width=\"1920\" height=\"1080\"/>"), QStringLiteral("Title"), 20);
    const int color1 = insert(color, 0);
    const int color2 = insert(color, 40);
    const int text = insert(title, 80);

    select({color1});
    CHECK(clear().clipCounts.isEmpty());
    CHECK(emitted.compositionCount == 0);
    CHECK(emitted.subtitleCount == 0);
    CHECK_FALSE(emitted.allEnabled);
    CHECK_FALSE(emitted.allDisabled);
    CHECK_FALSE(emitted.hasGroupedItems);
    CHECK_FALSE(emitted.hasSelectedMix);
    CHECK_FALSE(emitted.isAvSplitPair);
    CHECK(emitted.audioAndVideoClipCount == 0);
    CHECK_FALSE(emitted.doesAnyClipHaveSpeedAdjustment);
    CHECK_FALSE(emitted.doesAnyClipHaveTimeRemap);

    QString compositionService;
    for (const auto &transition : TransitionsRepository::get()->getNames()) {
        if (TransitionsRepository::get()->isComposition(transition.first)) {
            compositionService = transition.first;
            break;
        }
    }
    REQUIRE_FALSE(compositionService.isEmpty());
    const int composition = CompositionModel::construct(timeline, compositionService, QString());
    REQUIRE(timeline->requestCompositionMove(composition, timeline->getTrackIndexFromPosition(2), 110));

    auto subtitles = timeline->createSubtitleModel();
    const int subtitle = KdenliveTests::getNextId();
    const double fps = pCore->getCurrentFps();
    REQUIRE(subtitles->addSubtitle(subtitle, {0, GenTime(150, fps)},
                                   SubtitleEvent(true, GenTime(170, fps), "Default", "", 0, 0, 0, "", QStringLiteral("Caption")), false, false));

    const auto &mixed = select({color1, color2, text, composition, subtitle});
    CHECK(mixed.clipCounts.value(ClipType::Color) == 2);
    CHECK(mixed.clipCounts.value(ClipType::Text) == 1);
    CHECK(mixed.clipCounts.size() == 2);
    CHECK(mixed.compositionCount == 1);
    CHECK(mixed.subtitleCount == 1);
    CHECK(mixed.allEnabled);
    CHECK_FALSE(mixed.allDisabled);
    CHECK_FALSE(mixed.hasGroupedItems);
    CHECK_FALSE(mixed.isAvSplitPair);
    CHECK(mixed.audioAndVideoClipCount == 0);

    const auto &nonClips = select({composition, subtitle});
    CHECK(nonClips.clipCounts.isEmpty());
    CHECK(nonClips.compositionCount == 1);
    CHECK(nonClips.subtitleCount == 1);
    CHECK_FALSE(nonClips.allEnabled);
    CHECK_FALSE(nonClips.allDisabled);
}

TEST_CASE_METHOD(SelectionProject, "Timeline selection summary tracks mixes without selected clips", "[TimelineSelection]")
{
    create();
    const QString binId = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", pCore->projectItemModel(), 100, false);
    const int first = insert(binId, 0);
    REQUIRE(timeline->requestItemResize(first, 20, true, true));
    const int second = insert(binId, 20);
    REQUIRE(timeline->requestItemResize(second, 20, true, true));
    REQUIRE(timeline->mixClip(second));
    REQUIRE(timeline->requestClearSelection());

    received = false;
    timeline->requestMixSelection(second);
    REQUIRE(received);
    CHECK(emitted.hasSelectedMix);
    CHECK(emitted.clipCounts.isEmpty());
    CHECK(emitted.compositionCount == 0);
    CHECK(emitted.subtitleCount == 0);
    CHECK(controller.selectionState().hasSelectedMix);

    // Refreshing an empty item selection must preserve the selected mix.
    received = false;
    controller.handleSelectionChange();
    REQUIRE(received);
    CHECK(emitted.hasSelectedMix);

    // Clearing only a mix does not emit the ordinary item-selection signal.
    CHECK_FALSE(clear().hasSelectedMix);
    CHECK_FALSE(controller.selectionState().hasSelectedMix);

    timeline->requestMixSelection(second);
    // The model can also retain a mix while adding an item selection.
    CHECK(select({first}).hasSelectedMix);
    CHECK(emitted.clipCounts.value(ClipType::Color) == 1);

    timeline->requestMixSelection(second);
    received = false;
    controller.deleteSelectedClips();
    REQUIRE(received);
    CHECK_FALSE(emitted.hasSelectedMix);
    CHECK(timeline->isClip(first));
    CHECK(timeline->isClip(second));
    CHECK(timeline->getMixDuration(second) == 0);
}

TEST_CASE_METHOD(SelectionProject, "Timeline selection payload distinguishes temporary selection from saved groups", "[TimelineSelection]")
{
    create();
    const QString binId = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", pCore->projectItemModel());
    const int first = insert(binId, 0);
    const int second = insert(binId, 40);
    CHECK_FALSE(select({first, second}).hasGroupedItems);
    clear();
    REQUIRE(timeline->requestClipsGroup({first, second}, true, GroupType::Normal) > 0);
    CHECK(select({first}).hasGroupedItems);
    CHECK(emitted.clipCounts.value(ClipType::Color) == 2);
    clear();
    REQUIRE(timeline->requestClipUngroup(first));
    CHECK_FALSE(select({first, second}).hasGroupedItems);
}

TEST_CASE_METHOD(SelectionProject, "Timeline selection payload tracks disabled clips across selection changes", "[TimelineSelection]")
{
    create();
    const QString binId = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", pCore->projectItemModel());
    const int first = insert(binId, 0);
    const int second = insert(binId, 40);
    CHECK(select({first, second}).allEnabled);
    CHECK_FALSE(emitted.allDisabled);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    received = false;
    REQUIRE(TimelineFunctions::changeClipState(timeline, second, PlaylistState::Disabled, undo, redo));
    REQUIRE(received);
    CHECK_FALSE(emitted.allEnabled);
    CHECK_FALSE(emitted.allDisabled);
    CHECK(select({second}).allDisabled);
    CHECK_FALSE(emitted.allEnabled);
    CHECK(select({first}).allEnabled);
    received = false;
    REQUIRE(TimelineFunctions::changeClipState(timeline, first, PlaylistState::Disabled, undo, redo));
    REQUIRE(received);
    CHECK(emitted.allDisabled);
    CHECK_FALSE(emitted.allEnabled);
    CHECK(select({first, second}).allDisabled);
    CHECK_FALSE(clear().allDisabled);
}

TEST_CASE_METHOD(SelectionProject, "Timeline selection recognizes linked AV partners rather than any two clips", "[TimelineSelection]")
{
    create();
    const QString binId = KdenliveTests::createAVProducer(pCore->getProjectProfile(), pCore->projectItemModel());
    const int first = insert(binId, 0);
    const int second = insert(binId, 80);
    const int partner = timeline->getClipSplitPartner(first);
    REQUIRE(partner != -1);

    CHECK(select({first}).isAvSplitPair);
    CHECK(emitted.audioAndVideoClipCount == 2);
    CHECK(emitted.audioOnlyClipCount == 1);
    CHECK(emitted.videoOnlyClipCount == 1);
    CHECK(select({partner}).isAvSplitPair);
    CHECK_FALSE(select({first, second}).isAvSplitPair);
    CHECK(emitted.audioAndVideoClipCount == 4);
    CHECK_FALSE(clear().isAvSplitPair);

    // Once unlinked, two instances of the same source are not an AV pair.
    REQUIRE(timeline->requestClipUngroup(first));
    CHECK_FALSE(select({first, partner}).isAvSplitPair);
    CHECK_FALSE(select({first}).isAvSplitPair);
    CHECK(emitted.audioAndVideoClipCount == 1);

    const int videoOnly = insert(QStringLiteral("V") + binId, 160);
    clear();
    REQUIRE(timeline->requestClipsGroup({first, videoOnly}, true, GroupType::Normal) > 0);
    CHECK_FALSE(select({first}).isAvSplitPair);
    CHECK(emitted.audioAndVideoClipCount == 2);
    CHECK(emitted.videoOnlyClipCount == 2);
    CHECK(emitted.audioOnlyClipCount == 0);

    // An all-video selection can still contain sources without audio to restore.
    const QString color = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", pCore->projectItemModel());
    const int colorClip = insert(color, 240);
    select({first, colorClip});
    CHECK(emitted.videoOnlyClipCount == 3);
    CHECK(emitted.audioAndVideoClipCount == 2);
}

TEST_CASE_METHOD(SelectionProject, "Timeline selection payload aggregates time warp and remap on nonrepresentative clips", "[TimelineSelection]")
{
    create();
    const QString binId = KdenliveTests::createAVProducer(pCore->getProjectProfile(), pCore->projectItemModel());
    const int normal = insert(binId, 0);
    const int warped = insert(binId, 80);
    const int remapped = insert(binId, 160);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    REQUIRE(timeline->requestClipTimeWarp(warped, 0.5, false, true, undo, redo));
    REQUIRE(timeline->requestClipTimeRemap(remapped, true));
    CHECK_FALSE(select({normal}).doesAnyClipHaveSpeedAdjustment);
    CHECK_FALSE(emitted.doesAnyClipHaveTimeRemap);
    CHECK(emitted.isAvSplitPair);
    CHECK(select({warped}).isAvSplitPair);
    CHECK(emitted.doesAnyClipHaveSpeedAdjustment);
    CHECK(select({remapped}).isAvSplitPair);
    CHECK(emitted.doesAnyClipHaveTimeRemap);
    CHECK(select({normal, warped}).doesAnyClipHaveSpeedAdjustment);
    CHECK_FALSE(emitted.doesAnyClipHaveTimeRemap);
    CHECK(select({normal, remapped}).doesAnyClipHaveTimeRemap);
    CHECK_FALSE(emitted.doesAnyClipHaveSpeedAdjustment);
    CHECK(select({normal, warped, remapped}).doesAnyClipHaveSpeedAdjustment);
    CHECK(emitted.doesAnyClipHaveTimeRemap);
    CHECK_FALSE(clear().doesAnyClipHaveSpeedAdjustment);
    CHECK_FALSE(emitted.doesAnyClipHaveTimeRemap);
}
