#include "test_utils.hpp"
#define private public
#define protected public

#include "doc/kdenlivedoc.h"
#include "timeline2/model/builders/meltBuilder.hpp"
#include "bin/binplaylist.hpp"
#include "xml/xml.hpp"

using namespace fakeit;
Mlt::Profile profile_file;


TEST_CASE("Save File", "[SF]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);

    // Here we do some trickery to enable testing.

    // we mock a doc to stub the getDocumentProperty

    SECTION("Simple insert and save")
    {
        // Create document
        Mock<KdenliveDoc> docMock;
        /*When(Method(docMock, getDocumentProperty)).AlwaysDo([](const QString &name, const QString &defaultValue) {
            Q_UNUSED(name) Q_UNUSED(defaultValue)
            qDebug() << "Intercepted call";
            return QStringLiteral("dummyId");
        });*/
        KdenliveDoc &mockedDoc = docMock.get();

        // We mock the project class so that the undoStack function returns our undoStack, and our mocked document
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);

        ProjectManager &mocked = pmMock.get();
        pCore->m_projectManager = &mocked;
        pCore->m_projectManager->m_project = &mockedDoc;
        pCore->m_projectManager->m_project->m_guideModel = guideModel;

        // We also mock timeline object to spy few functions and mock others
        TimelineItemModel tim(&profile_file, undoStack);
        Mock<TimelineItemModel> timMock(tim);
        auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline, guideModel);
        mocked.testSetActiveDocument(&mockedDoc, timeline);
        QDir dir = QDir::temp();
        std::unordered_map<QString, QString> binIdCorresp;
        QStringList expandedFolders;
        QDomDocument doc = mockedDoc.createEmptyDocument(2,2);
        QScopedPointer<Mlt::Producer> xmlProd(new Mlt::Producer(profile_file, "xml-string",
                                                            doc.toString().toUtf8()));

        Mlt::Service s(*xmlProd);
        Mlt::Tractor tractor(s);
        binModel->loadBinPlaylist(&tractor, timeline->tractor(), binIdCorresp, expandedFolders, nullptr);

        RESET(timMock)

        QString binId = createProducerWithSound(profile_file, binModel);
        QString binId2 = createProducer(profile_file, "red", binModel, 20, false);

        /*int tid2b =*/ TrackModel::construct(timeline, -1, -1, QString(), true);
        /*int tid2 =*/ TrackModel::construct(timeline, -1, -1, QString(), true);
        int tid1 = TrackModel::construct(timeline);
        /*int tid1b =*/ TrackModel::construct(timeline);
    
        // Setup timeline audio drop info
        QMap <int, QString>audioInfo;
        audioInfo.insert(1,QStringLiteral("stream1"));
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
        REQUIRE(timeline->checkConsistency());
        mocked.testSaveFileAs(dir.absoluteFilePath(QStringLiteral("test.kdenlive")));
        // Undo resize
        undoStack->undo();
        // Undo first insert
        undoStack->undo();
        // Undo second insert 
        undoStack->undo();
    }
    binModel->clean();
    SECTION("Reopen and check in/out points")
    {
        // Create new document
        Mock<KdenliveDoc> docMock;
        KdenliveDoc &mockedDoc = docMock.get();

        // We mock the project class so that the undoStack function returns our undoStack, and our mocked document
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);

        ProjectManager &mocked = pmMock.get();
        pCore->m_projectManager = &mocked;
        pCore->m_projectManager->m_project = &mockedDoc;
        pCore->m_projectManager->m_project->m_guideModel = guideModel;

        // We also mock timeline object to spy few functions and mock others
        TimelineItemModel tim(&profile_file, undoStack);
        Mock<TimelineItemModel> timMock(tim);
        auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
        TimelineItemModel::finishConstruct(timeline, guideModel);
        mocked.testSetActiveDocument(&mockedDoc, timeline);
        QDir dir = QDir::temp();
        RESET(timMock)

        QString path = dir.absoluteFilePath(QStringLiteral("test.kdenlive"));
        QFile file(path);
        REQUIRE(file.open(QIODevice::ReadOnly | QIODevice::Text) == true);
        QByteArray playlist = file.readAll();
        file.close();
        QScopedPointer<Mlt::Producer> xmlProd(new Mlt::Producer(profile_file, "xml-string",
                                                            playlist));
        QDomDocument doc;
        doc.setContent(playlist);
        QDomNodeList list = doc.elementsByTagName(QStringLiteral("playlist"));
        QDomElement pl = list.at(0).toElement();
        QString hash = Xml::getXmlProperty(pl, QStringLiteral("kdenlive:docproperties.timelineHash"));

        Mlt::Service s(*xmlProd);
        Mlt::Tractor tractor(s);
        bool projectErrors;
        constructTimelineFromMelt(timeline, tractor, nullptr, QString(), QString(), QString(), QDateTime(), 0, &projectErrors);
        REQUIRE(timeline->checkConsistency());
        int tid1 = timeline->getTrackIndexFromPosition(2);
        int cid1 = timeline->getClipByStartPosition(tid1, 0);
        int cid2 = timeline->getClipByStartPosition(tid1, 100);
        REQUIRE(cid1 > -1);
        REQUIRE(cid2 > -1);
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
        QByteArray updatedHex = timeline->timelineHash().toHex();
        REQUIRE(updatedHex == hash);
        QFile::remove(dir.absoluteFilePath(QStringLiteral("test.kdenlive")));
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
}
