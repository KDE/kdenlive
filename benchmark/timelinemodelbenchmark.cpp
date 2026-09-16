/*
    SPDX-FileCopyrightText: 2026 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "mltconnection.h"
#include "project/projectmanager.h"
#include "timeline2/model/timelinefunctions.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/trackmodel.hpp"

#include <mlt++/MltFactory.h>
#include <mlt++/MltRepository.h>

#include <QApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QVector>

#include <clocale>
#include <iomanip>
#include <iostream>
#include <memory>
#include <unordered_set>
#include <vector>

// Allow accessing some internal methods
class KdenliveTests
{
public:
    static std::shared_ptr<TrackModel> getTrackById(TimelineItemModel &timeline, int trackId) { return timeline.getTrackById(trackId); }

    static bool isAvailableWithExceptions(TrackModel &track, int position, int duration, const QVector<int> &exceptions)
    {
        return track.isAvailableWithExceptions(position, duration, exceptions);
    }
};

namespace {

void require(bool condition, const char *message)
{
    if (!condition) {
        qFatal("%s", message);
    }
}

// Print csv format to Stdout, human readable to stderr
template <typename Operation> void measure(const char *name, int size, int sample, int operations, Operation operation)
{
    QElapsedTimer timer;
    timer.start();
    operation();
    const qint64 elapsed = timer.nsecsElapsed();
    std::cerr << name << ": " << elapsed << " ns" << std::endl;
    std::cout << name << ',' << size << ',' << sample << ',' << operations << ',' << elapsed << ',' << static_cast<double>(elapsed) / operations << std::endl;
}

// A real bin producer, with no media files or asynchronous loading needed.
QString createProducer(const char *color, int length)
{
    auto bin = pCore->projectItemModel();
    auto producer = std::make_shared<Mlt::Producer>(pCore->getProjectProfile(), "color", color);
    require(producer->is_valid(), "Cannot create color producer");
    producer->set("length", length);
    producer->set_in_and_out(0, length - 1);
    QString binId;
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    require(bin->requestAddBinClip(binId, producer, bin->getRootFolder()->clipId(), undo, redo), "Cannot add bin clip");
    bin->getClipByBinID(binId)->forceLimitedDuration();
    return binId;
}

void populationAndQueries(int sample)
{
    for (int count : {32, 128, 512}) {
        std::cerr << "\nPopulation / queries: " << count << " clips, two video tracks\n";
        auto undoStack = std::make_shared<DocUndoStack>(nullptr);
        KdenliveDoc document(undoStack, {2, 0});
        pCore->projectManager()->testSetDocument(&document);
        auto timeline = TimelineItemModel::construct(document.uuid(), undoStack);
        pCore->projectManager()->testSetActiveTimeline(timeline);
        int v1, v2;
        require(timeline->requestTrackInsertion(-1, v1), "Cannot insert V1");
        require(timeline->requestTrackInsertion(-1, v2), "Cannot insert V2");
        const QString binId = createProducer("red", 20);
        std::vector<int> clips(count);
        undoStack->clear();

        measure("insert clips", count, sample, count, [&]() {
            for (int i = 0; i < count; ++i) {
                require(timeline->requestClipInsertion(binId, i % 2 == 0 ? v1 : v2, 100 + (i / 2) * 30, clips[i], false, false, false),
                        "Clip insertion failed");
            }
        });
        require(timeline->getClipsCount() == count && timeline->checkConsistency(), "Invalid populated timeline");

        const int queries = 8192;
        qint64 expectedIds = 0;
        qint64 expectedMetadata = 0;
        for (int i = 0; i < count; ++i) {
            expectedIds += clips[i];
            expectedMetadata += (i % 2 == 0 ? v1 : v2) + 100 + (i / 2) * 30 + 20;
        }
        expectedIds *= queries / count;
        expectedMetadata *= queries / count;
        qint64 sum = 0;
        measure("clip by position", count, sample, queries, [&]() {
            for (int i = 0; i < queries; ++i) {
                const int index = (i * 37) % count;
                sum += timeline->getClipByPosition(index % 2 == 0 ? v1 : v2, 105 + (index / 2) * 30);
            }
        });
        require(sum == expectedIds, "Position lookup returned wrong clips");
        std::cerr << "  checksum: " << sum << '\n';

        sum = 0;
        measure("clip by start", count, sample, queries, [&]() {
            for (int i = 0; i < queries; ++i) {
                const int index = (i * 53) % count;
                sum += timeline->getClipByStartPosition(index % 2 == 0 ? v1 : v2, 100 + (index / 2) * 30);
            }
        });
        require(sum == expectedIds, "Start lookup returned wrong clips");
        std::cerr << "  checksum: " << sum << '\n';

        sum = 0;
        measure("blank neighbors", count, sample, 2 * queries, [&]() {
            for (int i = 0; i < queries; ++i) {
                const int index = (i * 71) % count;
                const int track = index % 2 == 0 ? v1 : v2;
                const int position = 120 + (index / 2) * 30;
                sum += timeline->getPreviousBlank(track, position);
                sum += timeline->getNextBlank(track, position);
            }
        });
        std::cerr << "  checksum: " << sum << '\n';

        sum = 0;
        measure("track availability", count, sample, 2 * queries, [&]() {
            for (int i = 0; i < queries; ++i) {
                const int index = (i * 89) % count;
                const int track = index % 2 == 0 ? v1 : v2;
                const int position = 100 + (index / 2) * 30;
                sum += timeline->trackIsAvailable(track, position + 20, 5, -1);
                sum += 2 * timeline->trackIsAvailable(track, position, 5, -1);
            }
        });
        require(sum == queries, "Unexpected track availability");
        std::cerr << "  checksum: " << sum << '\n';

        const std::shared_ptr<TrackModel> tracks[] = {KdenliveTests::getTrackById(*timeline, v1), KdenliveTests::getTrackById(*timeline, v2)};
        const QVector<int> noExceptions;
        std::vector<QVector<int>> singleExceptions(count);
        std::vector<QVector<int>> groupExceptions(count);
        for (int index = 0; index < count; ++index) {
            singleExceptions[index] = {clips[index]};
            // make up a group of clips that will be ignored
            const int firstRow = (index / 32) * 16 + (index / 2) % 2;
            groupExceptions[index].reserve(16);
            for (int member = 0; member < 8; ++member) {
                const int firstClip = 2 * (firstRow + 2 * member);
                groupExceptions[index].push_back(clips[firstClip]);
                groupExceptions[index].push_back(clips[firstClip + 1]);
            }
        }

        sum = 0;
        measure("exception availability empty occupied", count, sample, queries, [&]() {
            for (int i = 0; i < queries; ++i) {
                const int index = (i * 89) % count;
                sum += KdenliveTests::isAvailableWithExceptions(*tracks[index % 2], 100 + (index / 2) * 30, 20, noExceptions);
            }
        });
        require(sum == 0, "Empty exceptions accepted an occupied range");
        std::cerr << "  checksum: " << sum << '\n';

        sum = 0;
        measure("exception availability single unrelated", count, sample, queries, [&]() {
            for (int i = 0; i < queries; ++i) {
                const int index = (i * 89) % count;
                sum += KdenliveTests::isAvailableWithExceptions(*tracks[index % 2], 100 + (index / 2) * 30, 20, singleExceptions[index ^ 1]);
            }
        });
        require(sum == 0, "An exception on another track hid the occupied clip");
        std::cerr << "  checksum: " << sum << '\n';

        sum = 0;
        measure("exception availability single matching", count, sample, queries, [&]() {
            for (int i = 0; i < queries; ++i) {
                const int index = (i * 89) % count;
                sum += KdenliveTests::isAvailableWithExceptions(*tracks[index % 2], 100 + (index / 2) * 30, 20, singleExceptions[index]);
            }
        });
        require(sum == queries, "Matching single exceptions did not free the occupied range");
        std::cerr << "  checksum: " << sum << '\n';

        sum = 0;
        measure("exception availability single neighbor", count, sample, queries, [&]() {
            for (int i = 0; i < queries; ++i) {
                const int index = (i * 89) % count;
                // The last clip uses its preceding neighbor instead of empty track tail.
                const int position = 100 + (index / 2) * 30 - (index >= count - 2 ? 30 : 0);
                sum += KdenliveTests::isAvailableWithExceptions(*tracks[index % 2], position, 50, singleExceptions[index]);
            }
        });
        require(sum == 0, "A single exception hid the neighboring collision");
        std::cerr << "  checksum: " << sum << '\n';

        sum = 0;
        measure("exception availability group unrelated", count, sample, queries, [&]() {
            for (int i = 0; i < queries; ++i) {
                const int index = (i * 89) % count;
                sum += KdenliveTests::isAvailableWithExceptions(*tracks[index % 2], 100 + (index / 2) * 30, 20, groupExceptions[index ^ 2]);
            }
        });
        require(sum == 0, "Unrelated group exceptions hid the occupied clip");
        std::cerr << "  checksum: " << sum << '\n';

        sum = 0;
        measure("exception availability group matching", count, sample, queries, [&]() {
            for (int i = 0; i < queries; ++i) {
                const int index = (i * 89) % count;
                sum += KdenliveTests::isAvailableWithExceptions(*tracks[index % 2], 100 + (index / 2) * 30, 20, groupExceptions[index]);
            }
        });
        require(sum == queries, "Matching group exceptions did not free the occupied range");
        std::cerr << "  checksum: " << sum << '\n';

        sum = 0;
        measure("exception availability group neighbor", count, sample, queries, [&]() {
            for (int i = 0; i < queries; ++i) {
                const int index = (i * 89) % count;
                const int position = 100 + (index / 2) * 30 - (index >= count - 2 ? 30 : 0);
                sum += KdenliveTests::isAvailableWithExceptions(*tracks[index % 2], position, 50, groupExceptions[index]);
            }
        });
        require(sum == 0, "Group exceptions hid the neighboring collision");
        std::cerr << "  checksum: " << sum << '\n';

        sum = 0;
        measure("clip metadata", count, sample, 3 * queries, [&]() {
            for (int i = 0; i < queries; ++i) {
                const int id = clips[(i * 97) % count];
                sum += timeline->getClipTrackId(id);
                sum += timeline->getClipPosition(id);
                sum += timeline->getClipPlaytime(id);
            }
        });
        require(sum == expectedMetadata, "Wrong clip metadata");
        std::cerr << "  checksum: " << sum << '\n';
        require(pCore->projectManager()->closeCurrentDocument(false, false), "Cannot close document");
    }
}

void clipAndTrackEdits(int sample)
{
    std::cerr << "\nClip and track edits: 32 background clips, three video tracks\n";
    auto undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveDoc document(undoStack, {3, 0});
    pCore->projectManager()->testSetDocument(&document);
    auto timeline = TimelineItemModel::construct(document.uuid(), undoStack);
    pCore->projectManager()->testSetActiveTimeline(timeline);
    int v1, v2, v3;
    require(timeline->requestTrackInsertion(-1, v1), "Cannot insert V1");
    require(timeline->requestTrackInsertion(-1, v2), "Cannot insert V2");
    require(timeline->requestTrackInsertion(-1, v3), "Cannot insert V3");
    const QString shortBin = createProducer("red", 20);
    const QString longBin = createProducer("blue", 120);
    for (int track : {v1, v2}) {
        for (int i = 0; i < 16; ++i) {
            int id;
            require(timeline->requestClipInsertion(shortBin, track, 100 + i * 30, id, false, false, false), "Cannot populate track");
        }
    }
    int clip;
    require(timeline->requestClipInsertion(longBin, v3, 100, clip, false, false, false), "Cannot insert edit clip");
    require(timeline->requestItemResize(clip, 20, true, false) == 20, "Cannot trim edit clip");
    undoStack->clear();
    require(timeline->checkConsistency(), "Invalid edit setup");

    measure("move on same track", 1, sample, 1, [&]() { require(timeline->requestClipMove(clip, v3, 200), "Move failed"); });
    require(timeline->getClipPosition(clip) == 200, "Wrong moved position");
    measure("undo move", 1, sample, 1, [&]() { undoStack->undo(); });
    require(timeline->getClipPosition(clip) == 100, "Undo did not restore position");
    measure("redo move", 1, sample, 1, [&]() { undoStack->redo(); });
    require(timeline->getClipPosition(clip) == 200, "Redo did not restore position");
    undoStack->undo();

    measure("move across tracks", 1, sample, 1, [&]() { require(timeline->requestClipMove(clip, v2, 1000), "Cross-track move failed"); });
    require(timeline->getClipTrackId(clip) == v2 && timeline->getClipPosition(clip) == 1000, "Wrong cross-track destination");
    undoStack->undo();
    require(timeline->getClipTrackId(clip) == v3 && timeline->getClipPosition(clip) == 100, "Cross-track undo failed");

    measure("reject occupied destination", 1, sample, 1, [&]() { require(!timeline->requestClipMove(clip, v1, 100), "Collision accepted"); });
    require(timeline->getClipTrackId(clip) == v3 && timeline->getClipPosition(clip) == 100, "Rejected move changed clip");

    measure("trim right", 1, sample, 1, [&]() { require(timeline->requestItemResize(clip, 15, true) == 15, "Right trim failed"); });
    require(timeline->getClipPlaytime(clip) == 15 && timeline->getClipPosition(clip) == 100, "Wrong right trim");
    undoStack->undo();
    measure("trim left", 1, sample, 1, [&]() { require(timeline->requestItemResize(clip, 15, false) == 15, "Left trim failed"); });
    require(timeline->getClipPlaytime(clip) == 15 && timeline->getClipPosition(clip) == 105, "Wrong left trim");
    undoStack->undo();
    require(timeline->getClipPlaytime(clip) == 20 && timeline->getClipPosition(clip) == 100, "Trim undo failed");

    const int originalIn = timeline->getClipIn(clip);
    // Negative offset advances into the source; positive offset at in=0 is clamped.
    measure("slip", 1, sample, 1, [&]() { require(timeline->requestClipSlip(clip, -3) == -3, "Slip failed"); });
    require(timeline->getClipIn(clip) == originalIn + 3 && timeline->getClipPlaytime(clip) == 20 && timeline->getClipPosition(clip) == 100,
            "Slip did not change only the source range");
    undoStack->undo();
    require(timeline->getClipIn(clip) == originalIn, "Slip undo failed");

    int inserted;
    measure("insert clip", 1, sample, 1,
            [&]() { require(timeline->requestClipInsertion(shortBin, v3, 500, inserted, true, false, false), "Insertion failed"); });
    require(timeline->getClipsCount() == 34, "Wrong insertion count");
    measure("undo insertion", 1, sample, 1, [&]() { undoStack->undo(); });
    require(timeline->getClipsCount() == 33, "Insertion undo failed");
    measure("delete clip", 1, sample, 1, [&]() { require(timeline->requestItemDeletion(clip), "Deletion failed"); });
    require(timeline->getClipsCount() == 32, "Wrong deletion count");
    measure("undo deletion", 1, sample, 1, [&]() { undoStack->undo(); });
    require(timeline->getClipsCount() == 33 && timeline->getClipPosition(clip) == 100, "Deletion undo failed");

    int extraTrack;
    measure("insert track", 1, sample, 1, [&]() { require(timeline->requestTrackInsertion(-1, extraTrack), "Track insertion failed"); });
    require(timeline->getTracksCount() == 4, "Wrong track count");
    measure("undo track insertion", 1, sample, 1, [&]() { undoStack->undo(); });
    require(timeline->getTracksCount() == 3, "Track insertion undo failed");
    measure("move populated track", 1, sample, 1, [&]() { require(timeline->requestTrackMove(timeline, v2, true), "Track move failed"); });
    require(timeline->getTrackPosition(v2) == 2, "Wrong track position");
    measure("undo track move", 1, sample, 1, [&]() { undoStack->undo(); });
    require(timeline->getTrackPosition(v2) == 1, "Track move undo failed");
    measure("delete populated track", 1, sample, 1, [&]() { require(timeline->requestTrackDeletion(v2), "Track deletion failed"); });
    require(timeline->getTracksCount() == 2 && timeline->getClipsCount() == 17, "Wrong track deletion result");
    measure("undo track deletion", 1, sample, 1, [&]() { undoStack->undo(); });
    require(timeline->getTracksCount() == 3 && timeline->getClipsCount() == 33, "Track deletion undo failed");
    require(timeline->checkConsistency(), "Inconsistent timeline after edits");
    require(pCore->projectManager()->closeCurrentDocument(false, false), "Cannot close document");
}

void groupMoves(int sample)
{
    for (int count : {2, 8, 32, 64, 128, 512, 1000}) {
        std::cerr << "\nGroup moves: " << count << " clips\n";
        auto undoStack = std::make_shared<DocUndoStack>(nullptr);
        KdenliveDoc document(undoStack, {2, 0});
        pCore->projectManager()->testSetDocument(&document);
        auto timeline = TimelineItemModel::construct(document.uuid(), undoStack);
        pCore->projectManager()->testSetActiveTimeline(timeline);
        int v1, v2;
        require(timeline->requestTrackInsertion(-1, v1), "Cannot insert V1");
        require(timeline->requestTrackInsertion(-1, v2), "Cannot insert V2");
        const QString binId = createProducer("red", 20);
        std::vector<int> clips(count);
        for (int i = 0; i < count; ++i) {
            require(timeline->requestClipInsertion(binId, v1, 100 + i * 30, clips[i], false, false, false), "Cannot populate group");
        }
        const std::unordered_set<int> members(clips.begin(), clips.end());
        measure("group clips", count, sample, 1, [&]() { require(timeline->requestClipsGroup(members, false) >= 0, "Cannot group clips"); });
        undoStack->clear();
        require(timeline->checkConsistency(), "Invalid group setup");

        // Checking every member catches partial moves and partial undo/redo.
        const auto checkGroup = [&](int track, int start) {
            for (int i = 0; i < count; ++i) {
                require(timeline->getClipTrackId(clips[i]) == track && timeline->getClipPosition(clips[i]) == start + i * 30, "Wrong group position");
            }
            require(timeline->checkConsistency(), "Inconsistent group");
        };
        measure("move group on same track", count, sample, 1, [&]() { require(timeline->requestClipMove(clips[0], v1, 107), "Group move failed"); });
        checkGroup(v1, 107);
        measure("undo group move", count, sample, 1, [&]() { undoStack->undo(); });
        checkGroup(v1, 100);
        measure("redo group move", count, sample, 1, [&]() { undoStack->redo(); });
        checkGroup(v1, 107);
        undoStack->undo();
        checkGroup(v1, 100);

        if (count <= 32) {
            measure("move group across tracks", count, sample, 1,
                    [&]() { require(timeline->requestClipMove(clips[0], v2, 100), "Cross-track group move failed"); });
            checkGroup(v2, 100);
            measure("undo cross-track group move", count, sample, 1, [&]() { undoStack->undo(); });
            checkGroup(v1, 100);
            measure("redo cross-track group move", count, sample, 1, [&]() { undoStack->redo(); });
            checkGroup(v2, 100);
            undoStack->undo();
        }
        if (count <= 64) {
            int blocker;
            // Block the last member, so rejection must handle the whole group.
            require(timeline->requestClipInsertion(binId, v2, 100 + (count - 1) * 30, blocker, false, false, false), "Cannot insert blocker");
            measure("reject blocked group destination", count, sample, 1,
                    [&]() { require(!timeline->requestClipMove(clips[0], v2, 100), "Blocked group move succeeded"); });
            checkGroup(v1, 100);
            require(timeline->getClipTrackId(blocker) == v2 && timeline->getClipPosition(blocker) == 100 + (count - 1) * 30, "Blocker moved");
        }
        if (count == 32) {
            require(timeline->requestClipUngroup(clips[0], false), "Cannot ungroup clips");
            const int first = timeline->requestClipsGroup(std::unordered_set<int>(clips.begin(), clips.begin() + 16), false);
            const int second = timeline->requestClipsGroup(std::unordered_set<int>(clips.begin() + 16, clips.end()), false);
            require(first >= 0 && second >= 0 && timeline->requestClipsGroup({first, second}, false) >= 0, "Cannot create nested group");
            undoStack->clear();
            require(timeline->getGroupElements(clips[0]).size() == 32, "Wrong nested group size");
            measure("move nested group", count, sample, 1, [&]() { require(timeline->requestClipMove(clips[0], v1, 107), "Nested move failed"); });
            checkGroup(v1, 107);
            measure("undo nested group move", count, sample, 1, [&]() { undoStack->undo(); });
            checkGroup(v1, 100);
            measure("redo nested group move", count, sample, 1, [&]() { undoStack->redo(); });
            checkGroup(v1, 107);
        }
        require(pCore->projectManager()->closeCurrentDocument(false, false), "Cannot close document");
    }
}

void mixesAndSpacers(int sample)
{
    std::cerr << "\nMixes and spacers\n";
    auto undoStack = std::make_shared<DocUndoStack>(nullptr);
    KdenliveDoc document(undoStack, {2, 0});
    pCore->projectManager()->testSetDocument(&document);
    auto timeline = TimelineItemModel::construct(document.uuid(), undoStack);
    pCore->projectManager()->testSetActiveTimeline(timeline);
    int v1, v2;
    require(timeline->requestTrackInsertion(-1, v1), "Cannot insert V1");
    require(timeline->requestTrackInsertion(-1, v2), "Cannot insert V2");
    const QString binId = createProducer("blue", 120);
    int left, right;
    // Use 50-frame source zones with handles on both sides for a centered mix.
    const QString zone = binId + QStringLiteral("/20/69");
    require(timeline->requestClipInsertion(zone, v1, 100, left, false, false, false), "Cannot insert left clip");
    require(timeline->requestClipInsertion(zone, v1, 150, right, false, false, false), "Cannot insert right clip");
    undoStack->clear();
    require(timeline->checkConsistency(), "Invalid mix setup");
    measure("create mix", 2, sample, 1, [&]() { require(timeline->mixClip(right, QStringLiteral("luma"), -1), "Mix creation failed"); });
    require(timeline->getMixInOut(right).first >= 0 && timeline->checkConsistency(), "Missing mix");
    measure("undo mix", 2, sample, 1, [&]() { undoStack->undo(); });
    require(timeline->getMixInOut(right).first == -1 && timeline->getClipPosition(right) == 150 && timeline->checkConsistency(), "Mix undo failed");
    measure("redo mix", 2, sample, 1, [&]() { undoStack->redo(); });
    require(timeline->getMixInOut(right).first >= 0 && timeline->checkConsistency(), "Mix redo failed");
    undoStack->undo();

    const QString shortBin = createProducer("red", 20);
    int a, b, c;
    require(timeline->requestClipInsertion(shortBin, v2, 10, a, false, false, false), "Cannot insert spacer clip A");
    require(timeline->requestClipInsertion(shortBin, v2, 80, b, false, false, false), "Cannot insert spacer clip B");
    require(timeline->requestClipInsertion(shortBin, v2, 150, c, false, false, false), "Cannot insert spacer clip C");
    undoStack->clear();
    measure("remove blanks after frame 20", 3, sample, 1, [&]() { require(TimelineFunctions::requestDeleteAllBlanksFrom(timeline, v2, 20), "Spacer failed"); });
    require(timeline->getClipPosition(a) == 10 && timeline->getClipPosition(b) == 30 && timeline->getClipPosition(c) == 50, "Wrong spacer positions");
    require(timeline->getClipPosition(left) == 100 && timeline->getClipPosition(right) == 150 && timeline->checkConsistency(), "Spacer changed another track");
    measure("undo spacer", 3, sample, 1, [&]() { undoStack->undo(); });
    require(timeline->getClipPosition(b) == 80 && timeline->getClipPosition(c) == 150 && timeline->checkConsistency(), "Spacer undo failed");
    measure("redo spacer", 3, sample, 1, [&]() { undoStack->redo(); });
    require(timeline->getClipPosition(b) == 30 && timeline->getClipPosition(c) == 50 && timeline->checkConsistency(), "Spacer redo failed");
    require(pCore->projectManager()->closeCurrentDocument(false, false), "Cannot close document");
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("MLT_REPOSITORY_DENY", "libmltqt:libmltglaxnimate");
    qputenv("MLT_TESTS", "1");
    QHashSeed::setDeterministicGlobalSeed();
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kdenlive"));
    std::setlocale(LC_NUMERIC, "C");
    // Debug logging would dominate small operations; retain warnings and errors.
    QLoggingCategory::setFilterRules(QStringLiteral("*.debug=false"));
    std::unique_ptr<Mlt::Repository> repository(Mlt::Factory::init());
    require(repository != nullptr, "Cannot initialize MLT");
    require(Core::build(LinuxPackageType::Unknown, true), "Cannot initialize Kdenlive");
    MltConnection::construct(QString());
    pCore->setCurrentProfile(QStringLiteral("atsc_1080p_25"));
    pCore->projectItemModel()->buildPlaylist(QUuid());

    std::cout << std::setprecision(12) << "scenario,size,sample,operations,elapsed_ns,per_operation_ns\n";

    // Three complete, independent samples. No scenario registry or command-line
    // options: edit these calls and the workloads above when investigating a path.
    for (int sample = 1; sample <= 3; ++sample) {
        std::cerr << "\n=== Sample " << sample << " ===\n";
        populationAndQueries(sample);
        clipAndTrackEdits(sample);
        mixesAndSpacers(sample);
        groupMoves(sample);
    }

    pCore->cleanup();
    pCore->mediaUnavailable.reset();
    pCore->projectItemModel()->clean();
    pCore->cleanup();
    std::cerr << "\nAll timeline scenarios completed.\n";
    return 0;
}
