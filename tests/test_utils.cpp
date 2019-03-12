#include "test_utils.hpp"
#include "logger.hpp"

QString createProducer(Mlt::Profile &prof, std::string color, std::shared_ptr<ProjectItemModel> binModel, int length, bool limited)
{
    Logger::log_create_producer("test_producer", {color, binModel, length, limited});
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

QString createProducerWithSound(Mlt::Profile &prof, std::shared_ptr<ProjectItemModel> binModel)
{
    Logger::log_create_producer("test_producer_sound", {binModel});
    // std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(prof,
    // QFileInfo("../tests/small.mkv").absoluteFilePath().toStdString().c_str());

    // In case the test system does not have avformat support, we can switch to the integrated blipflash producer
    std::shared_ptr<Mlt::Producer> producer = std::make_shared<Mlt::Producer>(prof, "blipflash");
    producer->set_in_and_out(0, 9);
    producer->set("kdenlive:duration", 10);

    REQUIRE(producer->is_valid());

    QString binId = QString::number(binModel->getFreeClipId());
    auto binClip = ProjectClip::construct(binId, QIcon(), binModel, producer);
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    REQUIRE(binModel->addItem(binClip, binModel->getRootFolder()->clipId(), undo, redo));

    return binId;
}
