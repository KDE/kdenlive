#include "test_utils.hpp"
#define private public
#define protected public

#include "bin/binplaylist.hpp"
#include "doc/kdenlivedoc.h"
#include "timeline2/model/builders/meltBuilder.hpp"
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
        // When(Method(docMock, getDocumentProperty)).AlwaysDo([](const QString &name, const QString &defaultValue) {
        //     Q_UNUSED(name) Q_UNUSED(defaultValue)
        //     qDebug() << "Intercepted call";
        //     return QStringLiteral("dummyId");
        // });
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
        QDomDocument doc = mockedDoc.createEmptyDocument(2, 2);
        QScopedPointer<Mlt::Producer> xmlProd(new Mlt::Producer(profile_file, "xml-string", doc.toString().toUtf8()));

        Mlt::Service s(*xmlProd);
        Mlt::Tractor tractor(s);
        binModel->loadBinPlaylist(&tractor, timeline->tractor(), binIdCorresp, expandedFolders, nullptr);

        RESET(timMock)

        QString binId = createProducerWithSound(profile_file, binModel);
        QString binId2 = createProducer(profile_file, "red", binModel, 20, false);

        TrackModel::construct(timeline, -1, -1, QString(), true);
        TrackModel::construct(timeline, -1, -1, QString(), true);
        int tid1 = TrackModel::construct(timeline);
        TrackModel::construct(timeline);

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
        QScopedPointer<Mlt::Producer> xmlProd(new Mlt::Producer(profile_file, "xml-string", playlist));
        QDomDocument doc;
        doc.setContent(playlist);
        QDomNodeList list = doc.elementsByTagName(QStringLiteral("playlist"));
        QDomElement pl = list.at(0).toElement();
        QString hash = Xml::getXmlProperty(pl, QStringLiteral("kdenlive:docproperties.timelineHash"));

        Mlt::Service s(*xmlProd);
        Mlt::Tractor tractor(s);
        bool projectErrors;
        constructTimelineFromMelt(timeline, tractor, nullptr, QString(), QString(), QString(), 0, &projectErrors);
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
    SECTION("Open a file with AV clips")
    {
        // Create new document
        Mock<KdenliveDoc> docMock;
        KdenliveDoc &mockedDoc = docMock.get();

        // We mock the project class so that the undoStack function returns our undoStack, and our mocked document
        Mock<ProjectManager> pmMock;
        When(Method(pmMock, undoStack)).AlwaysReturn(undoStack);
        When(Method(pmMock, cacheDir)).AlwaysReturn(QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)));
        When(Method(pmMock, current)).AlwaysReturn(&mockedDoc);
        When(Method(pmMock, slotAddTextNote)).AlwaysDo([](const QString &text) {
            Q_UNUSED(text)
            qDebug() << "Intercepted Add Notes call";
            return;
        });

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
        // This file contains an AV clip on tracks A1/V1 at 0, duration 500 frames
        QString path = sourcesPath + "/dataset/av.kdenlive";
        QFile file(path);
        REQUIRE(file.open(QIODevice::ReadOnly | QIODevice::Text) == true);
        QByteArray playlist = file.readAll();
        file.close();
        QScopedPointer<Mlt::Producer> xmlProd(new Mlt::Producer(profile_file, "xml-string", playlist));
        QDomDocument doc;
        doc.setContent(playlist);
        Mlt::Service s(*xmlProd);
        Mlt::Tractor tractor(s);
        bool projectErrors;
        constructTimelineFromMelt(timeline, tractor, nullptr, QString(), QString(), QString(), 0, &projectErrors);
        REQUIRE(timeline->checkConsistency());
        int tid1 = timeline->getTrackIndexFromPosition(0);
        int tid2 = timeline->getTrackIndexFromPosition(1);
        int tid3 = timeline->getTrackIndexFromPosition(2);
        int tid4 = timeline->getTrackIndexFromPosition(3);
        // Check we have audio and video tracks
        REQUIRE(timeline->isAudioTrack(tid1));
        REQUIRE(timeline->isAudioTrack(tid2));
        REQUIRE(timeline->isAudioTrack(tid3) == false);
        REQUIRE(timeline->isAudioTrack(tid4) == false);
        int cid1 = timeline->getClipByStartPosition(tid1, 0);
        int cid2 = timeline->getClipByStartPosition(tid2, 0);
        int cid3 = timeline->getClipByStartPosition(tid3, 0);
        int cid4 = timeline->getClipByStartPosition(tid4, 0);
        // Check we have our clips
        REQUIRE(cid1 == -1);
        REQUIRE(cid2 > -1);
        REQUIRE(cid3 > -1);
        REQUIRE(cid4 == -1);
        REQUIRE(timeline->getClipPlaytime(cid2) == 500);
        REQUIRE(timeline->getClipPlaytime(cid3) == 500);
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
}

TEST_CASE("Non-BMP Unicode", "[NONBMP]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();

    // A Kdenlive bug (bugzilla bug 435768) caused characters outside the Basic
    // Multilingual Plane to be lost when the file is loaded. This string
    // contains the onigiri emoji which should survive a save+open round trip.
    // If Kdenlive drops the emoji, then the result is just "testtest" which can
    // be checked for.

    const QString emojiTestString = QString::fromUtf8("test\xF0\x9F\x8D\x99test");

    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    std::shared_ptr<MarkerListModel> guideModel = std::make_shared<MarkerListModel>(undoStack);

    QTemporaryFile saveFile(QDir::temp().filePath("kdenlive_test_XXXXXX.kdenlive"));
    qDebug() << "Choosing temp file with template" << (QDir::temp().filePath("kdenlive_test_XXXXXX.kdenlive"));
    REQUIRE(saveFile.open());
    saveFile.close();
    qDebug() << "New temporary file:" << saveFile.fileName();

    SECTION("Save title with special chars")
    {

        // Create document
        Mock<KdenliveDoc> docMock;
        // When(Method(docMock, getDocumentProperty)).AlwaysDo([](const QString &name, const QString &defaultValue) {
        //     Q_UNUSED(name) Q_UNUSED(defaultValue)
        //     qDebug() << "Intercepted call";
        //     return QStringLiteral("dummyId");
        // });
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
        QDomDocument doc = mockedDoc.createEmptyDocument(2, 2);
        QScopedPointer<Mlt::Producer> xmlProd(new Mlt::Producer(profile_file, "xml-string", doc.toString().toUtf8()));

        Mlt::Service s(*xmlProd);
        Mlt::Tractor tractor(s);
        binModel->loadBinPlaylist(&tractor, timeline->tractor(), binIdCorresp, expandedFolders, nullptr);

        RESET(timMock)

        // create a simple title with the non-BMP test string
        auto titleXml = ("<kdenlivetitle duration=\"150\" LC_NUMERIC=\"C\" width=\"1920\" height=\"1080\" out=\"149\">\n <item type=\"QGraphicsTextItem\" z-index=\"0\">\n  <position x=\"777\" y=\"482\">\n   <transform>1,0,0,0,1,0,0,0,1</transform>\n  </position>\n  <content shadow=\"0;#64000000;3;3;3\" font-underline=\"0\" box-height=\"138\" font-outline-color=\"0,0,0,255\" font=\"DejaVu Sans\" letter-spacing=\"0\" font-pixel-size=\"120\" font-italic=\"0\" typewriter=\"0;2;1;0;0\" alignment=\"0\" font-weight=\"63\" font-outline=\"3\" box-width=\"573.25\" font-color=\"252,233,79,255\">"+
            emojiTestString+
            "</content>\n </item>\n <startviewport rect=\"0,0,1920,1080\"/>\n <endviewport rect=\"0,0,1920,1080\"/>\n <background color=\"0,0,0,0\"/>\n</kdenlivetitle>\n");

        QString binId2 = createTextProducer(profile_file, binModel, titleXml, emojiTestString, 150);

        TrackModel::construct(timeline, -1, -1, QString(), true);
        TrackModel::construct(timeline, -1, -1, QString(), true);
        int tid1 = TrackModel::construct(timeline);
        /*int tid1b =*/ TrackModel::construct(timeline);

        // Setup timeline audio drop info
        QMap<int, QString> audioInfo;
        audioInfo.insert(1, QStringLiteral("stream1"));
        timeline->m_binAudioTargets = audioInfo;
        timeline->m_videoTarget = tid1;

        mocked.testSaveFileAs(saveFile.fileName());
        // Undo resize
        undoStack->undo();
        // Undo first insert
        undoStack->undo();
        // Undo second insert
        undoStack->undo();

        // open the file and check that it contains emojiTestString
        QFile file(saveFile.fileName());
        REQUIRE(file.open(QIODevice::ReadOnly));
        QByteArray contents = file.readAll();
        if (contents.contains(emojiTestString.toUtf8())) {
            qDebug() << "File contains test string";
        } else {
            qDebug() << "File does not contain test string:" << contents;
        }
        REQUIRE(contents.contains(emojiTestString.toUtf8()));

        // try opening the file as a Kdenlivedoc and check that the title hasn't
        // lost the emoji

        // convert qtemporaryfile saveFile to QUrl
        QUrl openURL = QUrl::fromLocalFile(saveFile.fileName());
        QUndoGroup *undoGroup = new QUndoGroup();
        undoGroup->addStack(undoStack.get());
        auto openResults = KdenliveDoc::Open(openURL, QDir::temp().path(),
            undoGroup, false, nullptr);
        REQUIRE(openResults.isSuccessful() == true);

        KdenliveDoc *openedDoc = openResults.getDocument();
        QDomDocument *newDoc = &openedDoc->m_document;
        auto producers = newDoc->elementsByTagName(QStringLiteral("producer"));
        QDomElement textTitle;
        for (int i = 0; i < producers.size(); i++) {
            auto kdenliveid = getProperty(producers.at(i).toElement(), QStringLiteral("kdenlive:id"));
            if (kdenliveid != nullptr && kdenliveid->text() == binId2) {
                textTitle = producers.at(i).toElement();
                break;
            }
        }
        auto clipname = getProperty(textTitle, QStringLiteral("kdenlive:clipname"));
        REQUIRE(clipname != nullptr);
        CHECK(clipname->text() == emojiTestString);
        auto xmldata = getProperty(textTitle, QStringLiteral("xmldata"));
        REQUIRE(xmldata != nullptr);
        CHECK(clipname->text().contains(emojiTestString));
    }
    binModel->clean();
    pCore->m_projectManager = nullptr;
}
