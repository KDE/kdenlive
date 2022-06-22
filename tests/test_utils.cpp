#include "test_utils.hpp"

QString createProducer(Mlt::Profile &prof, std::string color, std::shared_ptr<ProjectItemModel> binModel, int length, bool limited)
{
    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(prof, "color", color.c_str());
    producer->set("length", length);
    producer->set("out", length - 1);

    REQUIRE(producer->is_valid());

    QString binId = QString::number(binModel->getFreeClipId());
    auto binClip = ProjectClip::construct(binId, QIcon(), binModel, producer);
    if (limited) {
        binClip->forceLimitedDuration();
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    REQUIRE(binModel->addItem(binClip, binModel->getRootFolder()->clipId(), undo, redo));

    return binId;
}

QString createProducerWithSound(Mlt::Profile &prof, std::shared_ptr<ProjectItemModel> binModel, int length)
{
    // std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(prof, QFileInfo(QCoreApplication::applicationDirPath()  +
    // "/../../tests/small.mkv").absoluteFilePath().toStdString().c_str()); In case the test system does not have avformat support, we can switch to the
    // integrated blipflash producer
    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(prof, "blipflash");
    REQUIRE(producer->is_valid());

    producer->set("length", length);
    producer->set_in_and_out(0, length - 1);
    producer->set("kdenlive:duration", length);

    QString binId = QString::number(binModel->getFreeClipId());
    auto binClip = ProjectClip::construct(binId, QIcon(), binModel, producer);
    binClip->forceLimitedDuration();
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    REQUIRE(binModel->addItem(binClip, binModel->getRootFolder()->clipId(), undo, redo));

    return binId;
}

QString createAVProducer(Mlt::Profile &prof, std::shared_ptr<ProjectItemModel> binModel)
{
    std::shared_ptr<Mlt::Producer> producer =
        std::make_shared<Mlt::Producer>(prof, QFileInfo(sourcesPath + "/small.mkv").absoluteFilePath().toStdString().c_str());

    // In case the test system does not have avformat support, we can switch to the integrated blipflash producer
    int length = -1;
    if (!producer || !producer->is_valid()) {
        length = 40;
        producer.reset(new Mlt::Producer(prof, "blipflash"));
        producer->set("length", length);
        producer->set_in_and_out(0, length - 1);
        producer->set("kdenlive:duration", length);
    }
    REQUIRE(producer->is_valid());

    QString binId = QString::number(binModel->getFreeClipId());
    auto binClip = ProjectClip::construct(binId, QIcon(), binModel, producer);
    if (length > -1) {
        binClip->forceLimitedDuration();
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    REQUIRE(binModel->addItem(binClip, binModel->getRootFolder()->clipId(), undo, redo));
    return binId;
}

QString createTextProducer(Mlt::Profile &prof, std::shared_ptr<ProjectItemModel> binModel, const QString &xmldata, const QString &clipname, int length)
{
    std::shared_ptr<Mlt::Producer> producer =
        std::make_shared<Mlt::Producer>(prof, "kdenlivetitle");

    REQUIRE(producer->is_valid());

    producer->set("length", length);
    producer->set_in_and_out(0, length - 1);
    producer->set("kdenlive:duration", length);
    producer->set_string("kdenlive:clipname", clipname.toUtf8());
    producer->set_string("xmldata", xmldata.toUtf8());

    QString binId = QString::number(binModel->getFreeClipId());
    auto binClip = ProjectClip::construct(binId, QIcon(), binModel, producer);
    binClip->forceLimitedDuration();
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    REQUIRE(binModel->addItem(binClip, binModel->getRootFolder()->clipId(), undo, redo));

    REQUIRE(binClip->clipType() == ClipType::Text);
    return binId;
}

std::unique_ptr<QDomElement> getProperty(const QDomElement &doc, const QString &name) {
    QDomNodeList list = doc.elementsByTagName("property");
    for (int i = 0; i < list.count(); i++) {
        QDomElement e = list.at(i).toElement();
        if (e.attribute("name") == name) {
            return std::make_unique<QDomElement>(e);
        }
    }
    return nullptr;
}
