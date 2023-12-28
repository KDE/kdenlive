/*
    SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>
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

TEST_CASE("Test saving sequences elements", "[TSG]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    SECTION("Group save")
    {
        // Create document
        KdenliveDoc document(undoStack);
        pCore->projectManager()->m_project = &document;
        QDateTime documentDate = QDateTime::currentDateTime();
        pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
        auto timeline = document.getTimeline(document.uuid());
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
        REQUIRE(timeline->requestClipInsertion(binId, tid1, 80 + l, cid2, true, true, false));
        // Resize first clip (length=100)
        REQUIRE(timeline->requestItemResize(cid1, 100, false) == 100);

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == 100);
            REQUIRE(timeline->getClipPlaytime(cid1) == 100);
            REQUIRE(timeline->getClipPlaytime(cid2) == 10);
        };
        state();
        REQUIRE(timeline->checkConsistency());
        pCore->projectManager()->testSaveFileAs(dir.absoluteFilePath(QStringLiteral("test.kdenlive")));
        // Undo resize
        undoStack->undo();
        // Undo first insert
        undoStack->undo();
        // Undo second insert
        undoStack->undo();
        pCore->projectManager()->closeCurrentDocument(false, false);
        // Reopen and check for groups
        QString saveFile = dir.absoluteFilePath(QStringLiteral("test.kdenlive"));
        QUrl openURL = QUrl::fromLocalFile(saveFile);

        QUndoGroup *undoGroup = new QUndoGroup(&document);
        undoGroup->addStack(undoStack.get());
        DocOpenResult openResults = KdenliveDoc::Open(openURL, QDir::temp().path(), undoGroup, false, nullptr);
        REQUIRE(openResults.isSuccessful() == true);

        std::unique_ptr<KdenliveDoc> openedDoc = openResults.getDocument();
        pCore->projectManager()->m_project = openedDoc.get();
        const QUuid uuid = openedDoc->uuid();
        documentDate = QFileInfo(openURL.toLocalFile()).lastModified();
        pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
        QMap<QUuid, QString> allSequences = binModel->getAllSequenceClips();
        const QString firstSeqId = allSequences.value(uuid);
        pCore->projectManager()->openTimeline(firstSeqId, uuid);
        timeline = pCore->projectManager()->m_project->getTimeline(uuid);
        pCore->projectManager()->testSetActiveDocument(openedDoc.get(), timeline);
        REQUIRE(timeline->getTracksCount() == 4);
        REQUIRE(timeline->m_allGroups.size() == 1);
        timeline.reset();
        pCore->projectManager()->closeCurrentDocument(false, false);
    }
    SECTION("Save / Load multiple sequences")
    {
        // Create document
        binModel->clean();
        KdenliveDoc document(undoStack);
        pCore->projectManager()->m_project = &document;
        QDateTime documentDate = QDateTime::currentDateTime();
        pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
        auto timeline = document.getTimeline(document.uuid());
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
        REQUIRE(timeline->requestClipInsertion(binId, tid1, 80 + l, cid2, true, true, false));
        // Resize first clip (length=100)
        REQUIRE(timeline->requestItemResize(cid1, 100, false) == 100);

        auto state = [&]() {
            REQUIRE(timeline->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getClipTrackId(cid1) == tid1);
            REQUIRE(timeline->getClipTrackId(cid2) == tid1);
            REQUIRE(timeline->getClipPosition(cid1) == 0);
            REQUIRE(timeline->getClipPosition(cid2) == 100);
            REQUIRE(timeline->getClipPlaytime(cid1) == 100);
            REQUIRE(timeline->getClipPlaytime(cid2) == 10);
        };
        state();
        REQUIRE(timeline->checkConsistency());
        // Add secondary timeline
        const QString seq2 = ClipCreator::createPlaylistClip(QStringLiteral("Sequence2"), {2, 2}, QString("-1"), binModel);
        QMap<QUuid, QString> uids = pCore->projectItemModel()->getAllSequenceClips();
        QUuid secondaryUuid;
        QMapIterator<QUuid, QString> i(uids);
        while (i.hasNext()) {
            i.next();
            if (i.value() == seq2) {
                secondaryUuid = i.key();
                break;
            }
        }
        auto timeline2 = pCore->currentDoc()->getTimeline(secondaryUuid);
        pCore->projectManager()->m_activeTimelineModel = timeline2;
        tid1 = timeline2->getTrackIndexFromPosition(2);
        // Setup timeline audio drop info
        QMap<int, QString> audioInfo2;
        audioInfo2.insert(1, QStringLiteral("stream1"));
        timeline2->m_binAudioTargets = audioInfo;
        timeline2->m_videoTarget = tid1;

        int cid3 = -1;
        REQUIRE(timeline2->requestClipInsertion(binId, tid1, 80 + l, cid3, true, true, false));
        int cid4 = -1;
        REQUIRE(timeline2->requestClipInsertion(binId, tid1, 160 + l, cid4, true, true, false));

        pCore->projectManager()->m_activeTimelineModel = timeline;

        pCore->projectManager()->testSaveFileAs(dir.absoluteFilePath(QStringLiteral("test.kdenlive")));
        timeline.reset();
        timeline2.reset();
        pCore->projectManager()->closeCurrentDocument(false, false);

        // Reopen and check for groups
        QString saveFile = dir.absoluteFilePath(QStringLiteral("test.kdenlive"));
        QUrl openURL = QUrl::fromLocalFile(saveFile);

        QUndoGroup *undoGroup = new QUndoGroup(&document);
        undoGroup->addStack(undoStack.get());
        DocOpenResult openResults = KdenliveDoc::Open(openURL, QDir::temp().path(), undoGroup, false, nullptr);
        REQUIRE(openResults.isSuccessful() == true);

        std::unique_ptr<KdenliveDoc> openedDoc = openResults.getDocument();
        pCore->projectManager()->m_project = openedDoc.get();
        QUuid uuid = openedDoc->uuid();
        documentDate = QFileInfo(openURL.toLocalFile()).lastModified();
        pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
        QMap<QUuid, QString> allSequences = binModel->getAllSequenceClips();
        QString firstSeqId = allSequences.value(uuid);
        QString secondSeqId = allSequences.value(secondaryUuid);
        pCore->projectManager()->openTimeline(firstSeqId, uuid);
        timeline = pCore->currentDoc()->getTimeline(uuid);
        pCore->projectManager()->testSetActiveDocument(openedDoc.get(), timeline);
        REQUIRE(timeline->getTracksCount() == 4);
        REQUIRE(timeline->m_allGroups.size() == 1);
        // pCore->projectManager()->openTimeline(secondSeqId, secondaryUuid);
        timeline2 = pCore->currentDoc()->getTimeline(secondaryUuid);
        Q_ASSERT(timeline2 != nullptr);
        REQUIRE(timeline2->getTracksCount() == 4);
        REQUIRE(timeline2->m_allGroups.size() == 2);
        std::shared_ptr<ProjectClip> sequenceClip = binModel->getClipByBinID(secondSeqId);
        REQUIRE(sequenceClip->clipName() == QLatin1String("Sequence2"));
        pCore->projectManager()->m_activeTimelineModel = timeline;
        // Save again without any changes
        pCore->projectManager()->testSaveFileAs(dir.absoluteFilePath(QStringLiteral("test.kdenlive")));
        sequenceClip.reset();
        timeline2.reset();
        timeline.reset();
        pCore->projectManager()->closeCurrentDocument(false, false);

        // Reopen test project and check closed sequence properties
        DocOpenResult openResults2 = KdenliveDoc::Open(openURL, QDir::temp().path(), undoGroup, false, nullptr);
        REQUIRE(openResults2.isSuccessful() == true);

        std::unique_ptr<KdenliveDoc> openedDoc2 = openResults2.getDocument();
        pCore->projectManager()->m_project = openedDoc2.get();
        uuid = openedDoc2->uuid();
        documentDate = QFileInfo(openURL.toLocalFile()).lastModified();
        pCore->projectManager()->updateTimeline(false, QString(), QString(), documentDate, 0);
        allSequences = binModel->getAllSequenceClips();
        firstSeqId = allSequences.value(uuid);
        secondSeqId = allSequences.value(secondaryUuid);
        pCore->projectManager()->openTimeline(firstSeqId, uuid);
        timeline = pCore->currentDoc()->getTimeline(uuid);
        pCore->projectManager()->testSetActiveDocument(openedDoc2.get(), timeline);
        REQUIRE(timeline->getTracksCount() == 4);
        REQUIRE(timeline->m_allGroups.size() == 1);
        timeline2 = pCore->currentDoc()->getTimeline(secondaryUuid);
        qDebug() << "::: LOOKIG FOR UUID: " << secondaryUuid << ", ALL SEQS: " << allSequences.keys();
        Q_ASSERT(timeline2 != nullptr);
        REQUIRE(timeline2->getTracksCount() == 4);
        REQUIRE(timeline2->m_allGroups.size() == 2);
        sequenceClip = binModel->getClipByBinID(secondSeqId);
        REQUIRE(sequenceClip->clipName() == QLatin1String("Sequence2"));
        sequenceClip.reset();
        timeline2.reset();
        timeline.reset();
        pCore->projectManager()->closeCurrentDocument(false, false);
    }
}
