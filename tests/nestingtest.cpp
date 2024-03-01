/*
    SPDX-FileCopyrightText: 2018-2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2022 Eric Jiang
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "test_utils.hpp"
// test specific headers
#include "bin/binplaylist.hpp"
#include "doc/kdenlivedoc.h"
#include "timeline2/model/builders/meltBuilder.hpp"
#include "xml/xml.hpp"

#include <QTemporaryFile>
#include <QUndoGroup>

using namespace fakeit;

TEST_CASE("Open and Close Sequence", "[OCS]")
{
    auto binModel = pCore->projectItemModel();
    Q_ASSERT(binModel->clipsCount() == 0);
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    SECTION("Create sequence, add grouped clips, close sequence and reopen")
    {
        // Create document
        KdenliveDoc document(undoStack);
        Mock<KdenliveDoc> docMock(document);
        KdenliveDoc &mockedDoc = docMock.get();

        pCore->projectManager()->m_project = &mockedDoc;
        QDateTime documentDate = QDateTime::currentDateTime();
        pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
        auto timeline = mockedDoc.getTimeline(mockedDoc.uuid());
        pCore->projectManager()->m_activeTimelineModel = timeline;
        pCore->projectManager()->testSetActiveDocument(&mockedDoc, timeline);
        KdenliveDoc::next_id = 0;
        QDir dir = QDir::temp();
        QString binId = createProducerWithSound(pCore->getProjectProfile(), binModel);

        // Create a new sequence clip
        std::pair<int, int> tracks = {2, 2};
        const QString seqId = ClipCreator::createPlaylistClip(QStringLiteral("Seq 2"), tracks, QStringLiteral("-1"), binModel);
        REQUIRE(seqId != QLatin1String("-1"));

        timeline.reset();
        // Now use the new timeline sequence
        QUuid uuid;
        QMap<QUuid, QString> allSequences = binModel->getAllSequenceClips();
        QMapIterator<QUuid, QString> i(allSequences);
        while (i.hasNext()) {
            // Find clips with the tag
            i.next();
            if (i.value() == seqId) {
                uuid = i.key();
            }
        }
        timeline = mockedDoc.getTimeline(uuid);
        pCore->projectManager()->m_activeTimelineModel = timeline;

        // Insert an AV clip
        int tid1 = timeline->getTrackIndexFromPosition(2);

        // Setup timeline audio drop info
        QMap<int, QString> audioInfo;
        audioInfo.insert(1, QStringLiteral("stream1"));
        timeline->m_binAudioTargets = audioInfo;
        timeline->m_videoTarget = tid1;
        // Insert
        int cid1 = -1;
        REQUIRE(timeline->requestClipInsertion(binId, tid1, 80, cid1, true, true, false));

        // Ensure the clip is grouped (part af an AV group)
        REQUIRE(timeline->m_groups->isInGroup(cid1));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);

        // Add a few guides
        timeline->getGuideModel()->addMarker(GenTime(40, pCore->getCurrentFps()), i18n("guide 1"));
        timeline->getGuideModel()->addMarker(GenTime(60, pCore->getCurrentFps()), i18n("guide 2"));

        REQUIRE(timeline->checkConsistency(timeline->getGuideModel()->getSnapPoints()));

        // Now close timeline, the reopen and check the clips, groups and guides are still here
        mockedDoc.setModified(true);
        pCore->projectManager()->closeTimeline(uuid);
        timeline.reset();

        // Reopen
        pCore->projectManager()->openTimeline(seqId, uuid);
        timeline = mockedDoc.getTimeline(uuid);
        pCore->projectManager()->m_activeTimelineModel = timeline;
        tid1 = timeline->getTrackIndexFromPosition(2);
        REQUIRE(timeline->getTrackClipsCount(tid1) == 1);
        cid1 = timeline->getClipByStartPosition(tid1, 80);
        REQUIRE(cid1 > -1);
        REQUIRE(timeline->m_groups->isInGroup(cid1));
        REQUIRE(timeline->getGuideModel()->hasMarker(40));
        REQUIRE(timeline->checkConsistency(timeline->getGuideModel()->getSnapPoints()));
        pCore->projectManager()->closeCurrentDocument(false, false);
    }
}

TEST_CASE("Save File With 2 Sequences", "[SF2]")
{
    auto binModel = pCore->projectItemModel();
    // Q_ASSERT(binModel->clipsCount() == 0);
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    SECTION("Simple insert and save")
    {
        // Create document
        KdenliveDoc document(undoStack);
        pCore->projectManager()->m_project = &document;
        QDateTime documentDate = QDateTime::currentDateTime();
        pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
        auto timeline = document.getTimeline(document.uuid());
        pCore->projectManager()->m_activeTimelineModel = timeline;
        pCore->projectManager()->testSetActiveDocument(&document, timeline);

        KdenliveDoc::next_id = 0;
        QDir dir = QDir::temp();

        QString binId = createProducerWithSound(pCore->getProjectProfile(), binModel);
        QString binId2 = createProducer(pCore->getProjectProfile(), "red", binModel, 20, false);

        int tid1 = timeline->getTrackIndexFromPosition(2);

        // Setup timeline audio drop info
        QMap<int, QString> audioInfo;
        audioInfo.insert(1, QStringLiteral("stream1"));
        timeline->m_binAudioTargets = audioInfo;
        timeline->m_videoTarget = tid1;
        // Insert 2 clips (length=20, pos = 80 / 100)
        int cid1 = -1;
        REQUIRE(timeline->requestClipInsertion(binId2, tid1, 80, cid1, true, true, false));
        int l = timeline->getClipPlaytime(cid1);
        int cid2 = -1;
        REQUIRE(timeline->requestClipInsertion(binId2, tid1, 80 + l, cid2, true, true, false));
        // Resize first clip (length=100)
        REQUIRE(timeline->requestItemResize(cid1, 100, false) == 100);

        // Group the 2 clips
        REQUIRE(timeline->requestClipsGroup({cid1, cid2}, true));

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == 100);
            REQUIRE(timeline->getClipPlaytime(cid1) == 100);
            REQUIRE(timeline->getClipPlaytime(cid2) == 20);
        };
        state();

        // Add a few guides
        timeline->getGuideModel()->addMarker(GenTime(40, pCore->getCurrentFps()), i18n("guide 1"));
        timeline->getGuideModel()->addMarker(GenTime(60, pCore->getCurrentFps()), i18n("guide 2"));
        timeline->getGuideModel()->addMarker(GenTime(10, pCore->getCurrentFps()), i18n("guide 3"));

        REQUIRE(timeline->checkConsistency(timeline->getGuideModel()->getSnapPoints()));

        // Add another sequence clip
        std::pair<int, int> tracks = {1, 2};
        const QString seqId = ClipCreator::createPlaylistClip(QStringLiteral("Seq 2"), tracks, QStringLiteral("-1"), binModel);
        REQUIRE(seqId != QLatin1String("-1"));

        QUuid uuid;
        QMap<QUuid, QString> allSequences = binModel->getAllSequenceClips();
        QString firstSeqId = allSequences.value(document.uuid());
        QMapIterator<QUuid, QString> i(allSequences);
        while (i.hasNext()) {
            // Find clips with the tag
            i.next();
            if (i.value() == seqId) {
                uuid = i.key();
            }
        }
        REQUIRE(!uuid.isNull());
        // Make last sequence active
        timeline.reset();
        timeline = document.getTimeline(uuid);
        timeline->getGuideModel()->addMarker(GenTime(10, pCore->getCurrentFps()), i18n("guide 4"));

        // Save and close
        pCore->projectManager()->testSaveFileAs(dir.absoluteFilePath(QStringLiteral("test-nest.kdenlive")));
        Q_ASSERT(QFile::exists(dir.absoluteFilePath(QStringLiteral("test-nest.kdenlive"))));
        pCore->projectManager()->closeCurrentDocument(false, false);
    }

    SECTION("Open and check in/out points")
    {
        // Open document
        KdenliveDoc::next_id = 0;
        QString saveFile = QDir::temp().absoluteFilePath(QStringLiteral("test-nest.kdenlive"));
        QUrl openURL = QUrl::fromLocalFile(saveFile);

        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        QUndoGroup *undoGroup = new QUndoGroup();
        undoGroup->addStack(undoStack.get());
        DocOpenResult openResults = KdenliveDoc::Open(openURL, QDir::temp().path(), undoGroup, false, nullptr);
        REQUIRE(openResults.isSuccessful() == true);
        std::unique_ptr<KdenliveDoc> openedDoc = openResults.getDocument();

        pCore->projectManager()->m_project = openedDoc.get();
        const QUuid uuid = openedDoc->uuid();
        QDateTime documentDate = QFileInfo(openURL.toLocalFile()).lastModified();
        pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);

        QMap<QUuid, QString> allSequences = binModel->getAllSequenceClips();
        const QString firstSeqId = allSequences.value(uuid);
        pCore->projectManager()->openTimeline(firstSeqId, uuid);
        std::shared_ptr<TimelineItemModel> timeline = openedDoc->getTimeline(uuid);
        // Now reopen all timeline sequences
        QList<QUuid> allUuids = allSequences.keys();
        allUuids.removeAll(uuid);
        pCore->projectManager()->testSetActiveDocument(openedDoc.get(), timeline);

        REQUIRE(timeline->getTracksCount() == 4);
        REQUIRE(timeline->checkConsistency(timeline->getGuideModel()->getSnapPoints()));
        int tid1 = timeline->getTrackIndexFromPosition(2);
        int cid1 = timeline->getClipByStartPosition(tid1, 0);
        int cid2 = timeline->getClipByStartPosition(tid1, 100);
        REQUIRE(cid1 > -1);
        REQUIRE(cid2 > -1);
        // Check che clips are still grouped
        REQUIRE(timeline->m_groups->isInGroup(cid1));
        REQUIRE(timeline->m_groups->isInGroup(cid2));

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency(timeline->getGuideModel()->getSnapPoints()));
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == 100);
            REQUIRE(timeline->getClipPlaytime(cid1) == 100);
            REQUIRE(timeline->getClipPlaytime(cid2) == 20);
        };
        state();
        QByteArray updatedHex = timeline->timelineHash().toHex();
        const QString hash = openedDoc->getSequenceProperty(uuid, QStringLiteral("timelineHash"));
        REQUIRE(updatedHex == hash.toLatin1());
        const QString binId1 = timeline->getClipBinId(cid1);

        const QUuid secondSequence = allUuids.first();
        timeline = openedDoc->getTimeline(secondSequence);
        pCore->projectManager()->setActiveTimeline(secondSequence);
        REQUIRE(timeline->getTracksCount() == 3);

        // Now, add one clip, close sequence and reopen to ensure it is correctly stored as removed
        tid1 = timeline->getTrackIndexFromPosition(2);
        int cid5 = -1;
        int cid6 = -1;
        REQUIRE(timeline->requestClipInsertion(binId1, tid1, 330, cid5, true, true, false));
        REQUIRE(timeline->requestClipInsertion(binId1, tid1, 120, cid6, true, true, false));
        REQUIRE(timeline->getClipPosition(cid5) == 330);
        REQUIRE(timeline->getClipPosition(cid6) == 120);
        // Group the 2 clips
        REQUIRE(timeline->requestClipsGroup({cid5, cid6}, true));

        // Close sequence
        pCore->projectManager()->closeTimeline(secondSequence, false);
        timeline.reset();

        // Reopen sequence and check clip is still here
        pCore->projectManager()->openTimeline(allSequences.value(secondSequence), secondSequence);
        timeline = openedDoc->getTimeline(secondSequence);
        tid1 = timeline->getTrackIndexFromPosition(2);
        cid5 = timeline->getClipByStartPosition(tid1, 330);
        REQUIRE(cid5 > -1);
        cid6 = timeline->getClipByStartPosition(tid1, 120);
        REQUIRE(cid6 > -1);
        REQUIRE(timeline->m_groups->isInGroup(cid5));
        REQUIRE(timeline->m_groups->isInGroup(cid6));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
        timeline.reset();

        // Save again
        QDir dir = QDir::temp();
        pCore->projectManager()->testSaveFileAs(dir.absoluteFilePath(QStringLiteral("test-nest.kdenlive")));
        pCore->projectManager()->closeCurrentDocument(false, false);
    }
    SECTION("Reopen and check in/out points")
    {
        // Create new document
        // We mock the project class so that the undoStack function returns our undoStack, and our mocked document
        KdenliveDoc::next_id = 0;
        Q_ASSERT(QFile::exists(QDir::temp().absoluteFilePath(QStringLiteral("test-nest.kdenlive"))));
        QString saveFile = QDir::temp().absoluteFilePath(QStringLiteral("test-nest.kdenlive"));
        QUrl openURL = QUrl::fromLocalFile(saveFile);

        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        QUndoGroup *undoGroup = new QUndoGroup();
        undoGroup->addStack(undoStack.get());
        DocOpenResult openResults = KdenliveDoc::Open(openURL, QDir::temp().path(), undoGroup, false, nullptr);
        REQUIRE(openResults.isSuccessful() == true);
        std::unique_ptr<KdenliveDoc> openedDoc = openResults.getDocument();

        pCore->projectManager()->m_project = openedDoc.get();
        const QUuid uuid = openedDoc->uuid();
        QDateTime documentDate = QFileInfo(openURL.toLocalFile()).lastModified();
        pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);

        QMap<QUuid, QString> allSequences = binModel->getAllSequenceClips();
        const QString firstSeqId = allSequences.value(uuid);
        pCore->projectManager()->openTimeline(firstSeqId, uuid);
        std::shared_ptr<TimelineItemModel> timeline = openedDoc->getTimeline(uuid);
        // Now reopen all timeline sequences
        QList<QUuid> allUuids = allSequences.keys();
        allUuids.removeAll(uuid);
        pCore->projectManager()->testSetActiveDocument(openedDoc.get(), timeline);

        REQUIRE(timeline->getTracksCount() == 4);
        REQUIRE(timeline->checkConsistency(timeline->getGuideModel()->getSnapPoints()));
        int tid1 = timeline->getTrackIndexFromPosition(2);
        int cid1 = timeline->getClipByStartPosition(tid1, 0);
        int cid2 = timeline->getClipByStartPosition(tid1, 100);
        REQUIRE(cid1 > -1);
        REQUIRE(cid2 > -1);
        // Check the clips are still grouped
        REQUIRE(timeline->m_groups->isInGroup(cid1));
        REQUIRE(timeline->m_groups->isInGroup(cid2));

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency(timeline->getGuideModel()->getSnapPoints()));
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == 100);
            REQUIRE(timeline->getClipPlaytime(cid1) == 100);
            REQUIRE(timeline->getClipPlaytime(cid2) == 20);
        };
        state();
        QByteArray updatedHex = timeline->timelineHash().toHex();
        const QString hash = openedDoc->getSequenceProperty(uuid, QStringLiteral("timelineHash"));
        REQUIRE(updatedHex == hash.toLatin1());
        const QString binId1 = timeline->getClipBinId(cid1);

        const QUuid secondSequence = allUuids.first();
        timeline = openedDoc->getTimeline(secondSequence);
        pCore->projectManager()->setActiveTimeline(secondSequence);
        REQUIRE(timeline->getTracksCount() == 3);

        tid1 = timeline->getTrackIndexFromPosition(2);
        int cid5 = timeline->getClipByStartPosition(tid1, 330);
        REQUIRE(cid5 > -1);
        int cid6 = timeline->getClipByStartPosition(tid1, 120);
        REQUIRE(cid6 > -1);
        REQUIRE(timeline->m_groups->isInGroup(cid5));
        REQUIRE(timeline->m_groups->isInGroup(cid6));
        REQUIRE(timeline->getTrackClipsCount(tid1) == 2);

        QDir dir = QDir::temp();
        QFile::remove(dir.absoluteFilePath(QStringLiteral("test-nest.kdenlive")));
        // binModel->clean();
        // pCore->m_projectManager = nullptr;
        timeline.reset();
        pCore->projectManager()->closeCurrentDocument(false, false);
    }
}

TEST_CASE("Save File, Reopen and check for corruption", "[SF3]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    SECTION("Open and simply save file")
    {
        QString path = sourcesPath + "/dataset/test-nesting-effects.kdenlive";
        Q_ASSERT(QFile::exists(sourcesPath));
        QUrl openURL = QUrl::fromLocalFile(path);

        QUndoGroup *undoGroup = new QUndoGroup();
        undoGroup->addStack(undoStack.get());
        DocOpenResult openResults = KdenliveDoc::Open(openURL, QDir::temp().path(), undoGroup, false, nullptr);
        REQUIRE(openResults.isSuccessful() == true);
        std::unique_ptr<KdenliveDoc> openedDoc = openResults.getDocument();

        pCore->projectManager()->m_project = openedDoc.get();
        const QUuid uuid = openedDoc->uuid();
        QDateTime documentDate = QFileInfo(openURL.toLocalFile()).lastModified();
        pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
        QMap<QUuid, QString> allSequences = binModel->getAllSequenceClips();
        const QString firstSeqId = allSequences.take(uuid);
        pCore->projectManager()->openTimeline(firstSeqId, uuid);
        std::shared_ptr<TimelineItemModel> timeline = openedDoc->getTimeline(uuid);
        pCore->projectManager()->testSetActiveDocument(openedDoc.get(), timeline);
        // Now reopen all timeline sequences
        REQUIRE(openedDoc->checkConsistency());
        // Save file
        QDir dir = QDir::temp();
        pCore->projectManager()->testSaveFileAs(dir.absoluteFilePath(QStringLiteral("test-nest.kdenlive")));
        timeline.reset();
        pCore->projectManager()->closeCurrentDocument(false, false);
    }
    SECTION("Reopen and check in/out points")
    {
        // Create new document
        // We mock the project class so that the undoStack function returns our undoStack, and our mocked document
        KdenliveDoc::next_id = 0;
        QString saveFile = QDir::temp().absoluteFilePath(QStringLiteral("test-nest.kdenlive"));
        QUrl openURL = QUrl::fromLocalFile(saveFile);

        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        QUndoGroup *undoGroup = new QUndoGroup();
        undoGroup->addStack(undoStack.get());
        DocOpenResult openResults = KdenliveDoc::Open(openURL, QDir::temp().path(), undoGroup, false, nullptr);
        REQUIRE(openResults.isSuccessful() == true);
        std::unique_ptr<KdenliveDoc> openedDoc = openResults.getDocument();

        pCore->projectManager()->m_project = openedDoc.get();
        const QUuid uuid = openedDoc->uuid();
        QDateTime documentDate = QFileInfo(openURL.toLocalFile()).lastModified();
        pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);

        QMap<QUuid, QString> allSequences = binModel->getAllSequenceClips();
        const QString firstSeqId = allSequences.take(uuid);
        pCore->projectManager()->openTimeline(firstSeqId, uuid);
        std::shared_ptr<TimelineItemModel> timeline = openedDoc->getTimeline(uuid);
        pCore->projectManager()->testSetActiveDocument(openedDoc.get(), timeline);
        //  Now reopen all timeline sequences
        QList<QUuid> allUuids = binModel->getAllSequenceClips().keys();
        // Collect saved hashes
        QMap<QUuid, QString> timelineHashes;
        for (auto &u : allUuids) {
            timelineHashes.insert(u, openedDoc->getSequenceProperty(u, QStringLiteral("timelineHash")));
        }
        REQUIRE(openedDoc->checkConsistency());
        openedDoc->documentProperties(true);
        for (auto &u : allUuids) {
            QByteArray updatedHex = openedDoc->getTimeline(u)->timelineHash().toHex();
            REQUIRE(updatedHex == timelineHashes.value(u));
        }

        QDir dir = QDir::temp();
        QFile::remove(dir.absoluteFilePath(QStringLiteral("test-nest.kdenlive")));
        //  binModel->clean();
        //  pCore->m_projectManager = nullptr;
        pCore->projectManager()->closeCurrentDocument(false, false);
    }
}

TEST_CASE("Save File And Check Sequence Effects", "[SF2]")
{
    auto binModel = pCore->projectItemModel();
    Q_ASSERT(binModel->clipsCount() == 0);
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    SECTION("Simple insert, add master effect and save")
    {
        // Create document
        KdenliveDoc document(undoStack);
        Mock<KdenliveDoc> docMock(document);
        KdenliveDoc &mockedDoc = docMock.get();

        pCore->projectManager()->m_project = &mockedDoc;
        QDateTime documentDate = QDateTime::currentDateTime();
        pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
        auto timeline = mockedDoc.getTimeline(mockedDoc.uuid());
        pCore->projectManager()->m_activeTimelineModel = timeline;

        pCore->projectManager()->testSetActiveDocument(&mockedDoc, timeline);
        KdenliveDoc::next_id = 0;
        QDir dir = QDir::temp();
        REQUIRE(timeline->checkConsistency(timeline->getGuideModel()->getSnapPoints()));

        // Add another sequence clip
        std::pair<int, int> tracks = {1, 2};
        const QString seqId = ClipCreator::createPlaylistClip(QStringLiteral("Seq 2"), tracks, QStringLiteral("-1"), binModel);
        REQUIRE(seqId != QLatin1String("-1"));

        QUuid uuid;
        QMap<QUuid, QString> allSequences = binModel->getAllSequenceClips();
        QString firstSeqId = allSequences.value(mockedDoc.uuid());
        QMapIterator<QUuid, QString> i(allSequences);
        while (i.hasNext()) {
            // Find clips with the tag
            i.next();
            if (i.value() == seqId) {
                uuid = i.key();
            }
        }
        REQUIRE(!uuid.isNull());
        // Make last sequence active
        timeline.reset();
        timeline = mockedDoc.getTimeline(uuid);
        // Add an effect to Bin sequence clip
        std::shared_ptr<EffectStackModel> binStack = binModel->getClipEffectStack(seqId.toInt());
        REQUIRE(binStack->appendEffect(QStringLiteral("sepia")));
        REQUIRE(binStack->checkConsistency());

        // Add an effect to master
        std::shared_ptr<EffectStackModel> masterStack = timeline->getMasterEffectStackModel();
        REQUIRE(masterStack->rowCount() == 1);
        REQUIRE(masterStack->appendEffect(QStringLiteral("sepia")));
        REQUIRE(masterStack->checkConsistency());
        REQUIRE(masterStack->rowCount() == 2);

        // Save and close
        REQUIRE(pCore->projectManager()->testSaveFileAs(dir.absoluteFilePath(QStringLiteral("test-nest2.kdenlive"))));
        pCore->projectManager()->closeCurrentDocument(false, false);
    }

    SECTION("Open and check that effects are restored")
    {
        // Create new document
        // We mock the project class so that the undoStack function returns our undoStack, and our mocked document
        KdenliveDoc::next_id = 0;
        QString saveFile = QDir::temp().absoluteFilePath(QStringLiteral("test-nest2.kdenlive"));
        QUrl openURL = QUrl::fromLocalFile(saveFile);

        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        QUndoGroup *undoGroup = new QUndoGroup();
        undoGroup->addStack(undoStack.get());
        DocOpenResult openResults = KdenliveDoc::Open(openURL, QDir::temp().path(), undoGroup, false, nullptr);
        REQUIRE(openResults.isSuccessful() == true);
        std::unique_ptr<KdenliveDoc> openedDoc = openResults.getDocument();

        pCore->projectManager()->m_project = openedDoc.get();
        const QUuid uuid = openedDoc->uuid();
        QDateTime documentDate = QFileInfo(openURL.toLocalFile()).lastModified();
        pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);

        QMap<QUuid, QString> allSequences = binModel->getAllSequenceClips();
        const QString firstSeqId = allSequences.value(uuid);
        pCore->projectManager()->openTimeline(firstSeqId, uuid);
        std::shared_ptr<TimelineItemModel> timeline = openedDoc->getTimeline(uuid);
        // Now reopen all timeline sequences
        QList<QUuid> allUuids = allSequences.keys();
        allUuids.removeAll(uuid);
        QString secondaryId;
        for (auto &u : allUuids) {
            secondaryId = allSequences.value(u);
            pCore->projectManager()->openTimeline(secondaryId, u);
        }

        pCore->projectManager()->testSetActiveDocument(openedDoc.get(), timeline);

        REQUIRE(timeline->getTracksCount() == 4);
        REQUIRE(timeline->checkConsistency(timeline->getGuideModel()->getSnapPoints()));

        const QUuid secondSequence = allUuids.first();
        timeline = openedDoc->getTimeline(secondSequence);
        pCore->projectManager()->setActiveTimeline(secondSequence);
        REQUIRE(timeline->getTracksCount() == 3);
        std::shared_ptr<EffectStackModel> stack = timeline->getMasterEffectStackModel();
        REQUIRE(stack->checkConsistency());
        REQUIRE(stack->rowCount() == 2);
        std::shared_ptr<EffectStackModel> binStack = binModel->getClipEffectStack(secondaryId.toInt());
        REQUIRE(binStack->checkConsistency());
        REQUIRE(binStack->rowCount() == 2);

        pCore->projectManager()->closeCurrentDocument(false, false);
        QFile::remove(QDir::temp().absoluteFilePath(QStringLiteral("test-nest2.kdenlive")));
    }
}

TEST_CASE("Check nested sequences on opening", "[NEST]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    SECTION("Open and simply save file")
    {
        QString path = sourcesPath + "/dataset/test-nesting.kdenlive";
        Q_ASSERT(QFile::exists(sourcesPath));
        QUrl openURL = QUrl::fromLocalFile(path);

        QUndoGroup *undoGroup = new QUndoGroup();
        undoGroup->addStack(undoStack.get());
        DocOpenResult openResults = KdenliveDoc::Open(openURL, QDir::temp().path(), undoGroup, false, nullptr);
        REQUIRE(openResults.isSuccessful() == true);
        std::unique_ptr<KdenliveDoc> openedDoc = openResults.getDocument();

        pCore->projectManager()->m_project = openedDoc.get();
        const QUuid uuid = openedDoc->uuid();
        QDateTime documentDate = QFileInfo(openURL.toLocalFile()).lastModified();
        pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
        QMap<QUuid, QString> allSequences = binModel->getAllSequenceClips();
        const QString firstSeqId = allSequences.take(uuid);
        pCore->projectManager()->openTimeline(firstSeqId, uuid);
        std::shared_ptr<TimelineItemModel> timeline1 = openedDoc->getTimeline(uuid);
        pCore->projectManager()->testSetActiveDocument(openedDoc.get(), timeline1);
        REQUIRE(openedDoc->checkConsistency());
        // Sequence 2 {9228b6f1-e900-4ae9-8591-3cd674dbae98}
        std::shared_ptr<TimelineItemModel> timeline2 = openedDoc->getTimeline(QUuid(QStringLiteral("{9228b6f1-e900-4ae9-8591-3cd674dbae98}")));
        // Sequence 3 {147c3db9-2050-4453-91bd-4e4ad0033fb5}
        std::shared_ptr<TimelineItemModel> timeline3 = openedDoc->getTimeline(QUuid(QStringLiteral("{147c3db9-2050-4453-91bd-4e4ad0033fb5}")));
        // Check that each timeline has the correct clipcount
        REQUIRE(timeline1->getClipsCount() == 1);
        REQUIRE(timeline2->getClipsCount() == 2);
        REQUIRE(timeline3->getClipsCount() == 2);
        timeline1.reset();
        timeline2.reset();
        timeline3.reset();
        pCore->projectManager()->closeCurrentDocument(false, false);
    }
}
