/*
    SPDX-FileCopyrightText: 2026 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "timelinepreviewmonitor.h"

#include "catch.hpp"
#include "core.h"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/view/timelinecontroller.h"

#include <QSemaphore>
#include <atomic>
#include <mlt++/MltConsumer.h>
#include <mlt++/MltEvent.h>
#include <mlt++/MltFrame.h>
#include <mlt++/MltPlaylist.h>
#include <mlt++/MltTractor.h>
#include <utility>

namespace TimelinePreviewTests {
namespace {
QColor frameColor(Mlt::Frame &frame)
{
    mlt_image_format format = mlt_image_rgb;
    int width = 16;
    int height = 16;
    const auto *image = frame.get_image(format, width, height);
    return image && format == mlt_image_rgb ? QColor(image[0], image[1], image[2]) : QColor();
}
} // namespace

struct PreviewMonitor::Impl
{
    explicit Impl(std::shared_ptr<TimelineItemModel> model)
        : timeline(std::move(model))
        , consumer(pCore->getProjectProfile(), "null")
    {
        consumer.set("real_time", -1);
        consumer.set("buffer", 6);
        consumer.set("prefill", 6);
        consumer.set("width", 16);
        consumer.set("height", 16);
        consumer.set("audio_off", 1);
        consumer.set("mlt_image_format", "rgb");
        frameEvent.reset(consumer.listen("consumer-frame-show", this, [](mlt_properties, void *data, mlt_event_data event) {
            auto &self = *static_cast<Impl *>(data);
            if (self.stopping) {
                return;
            }
            auto frame = Mlt::EventData(event).to_frame();
            self.deliveredColor = frameColor(frame);
            self.deliveredPosition = frame.get_position();
            self.ready.release();
            // Keep this frame stable until nextFrame() advances it or stop() releases it.
            // Only the test thread performs assertions; its bounded wait can safely unwind.
            self.advance.acquire();
        }));
    }

    ~Impl()
    {
        QObject::disconnect(refreshConnection);
        stop();
        if (overlay) {
            timeline->removeOverlayTrack();
        }
    }

    void sample()
    {
        std::unique_ptr<Mlt::Frame> frame(timeline->tractor()->get_frame());
        displayedColor = frame ? frameColor(*frame) : QColor();
    }

    void stop()
    {
        stopping = true;
        advance.release();
        consumer.stop();
        timeline->tractor()->set_speed(0);
        playing = false;
        heldFrame = false;
        ready.tryAcquire(ready.available());
        advance.tryAcquire(advance.available());
    }

    std::shared_ptr<TimelineItemModel> timeline;
    QSemaphore ready;
    QSemaphore advance;
    std::atomic<bool> stopping{true};
    QColor deliveredColor;
    int deliveredPosition = 0;
    QColor displayedColor;
    int displayedPosition = 0;
    bool playing = false;
    bool heldFrame = false;
    bool overlay = false;
    int refreshes = 0;
    Mlt::Consumer consumer;
    std::unique_ptr<Mlt::Event> frameEvent;
    QMetaObject::Connection refreshConnection;
};

PreviewMonitor::PreviewMonitor(std::shared_ptr<TimelineItemModel> timeline, TimelineController &controller)
    : m_impl(std::make_unique<Impl>(std::move(timeline)))
{
    REQUIRE(m_impl->consumer.is_valid());
    REQUIRE(m_impl->frameEvent);
    REQUIRE(m_impl->consumer.connect(*m_impl->timeline->tractor()) == 0);
    seek(0);
    m_impl->refreshConnection = QObject::connect(&controller, &TimelineController::previewRefreshRequested, &controller, [this]() {
        ++m_impl->refreshes;
        if (m_impl->playing) {
            m_impl->consumer.purge();
        } else {
            m_impl->sample();
        }
    });
}

PreviewMonitor::~PreviewMonitor() = default;

void PreviewMonitor::seek(int position)
{
    pause();
    m_impl->timeline->tractor()->seek(position);
    m_impl->displayedPosition = position;
    m_impl->sample();
    REQUIRE(m_impl->displayedColor.isValid());
}

int PreviewMonitor::position() const
{
    return m_impl->timeline->tractor()->position();
}

double PreviewMonitor::speed() const
{
    return m_impl->timeline->tractor()->get_speed();
}

QColor PreviewMonitor::color() const
{
    REQUIRE(m_impl->displayedColor.isValid());
    return m_impl->displayedColor;
}

void PreviewMonitor::play()
{
    if (m_impl->playing) {
        return;
    }
    m_impl->stopping = false;
    m_impl->playing = true;
    m_impl->timeline->tractor()->set_speed(1);
    REQUIRE(m_impl->consumer.start() == 0);
}

QColor PreviewMonitor::nextFrame()
{
    REQUIRE(m_impl->playing);
    if (m_impl->heldFrame) {
        m_impl->heldFrame = false;
        m_impl->advance.release();
    }
    if (!m_impl->ready.tryAcquire(1, 5000)) {
        FAIL("Headless preview monitor timed out waiting for a buffered consumer frame (5 seconds)");
    }
    m_impl->heldFrame = true;
    m_impl->displayedColor = m_impl->deliveredColor;
    m_impl->displayedPosition = m_impl->deliveredPosition;
    return color();
}

void PreviewMonitor::pause()
{
    if (m_impl->playing) {
        m_impl->stop();
        // Read-ahead advances the producer beyond the frame actually displayed.
        m_impl->timeline->tractor()->seek(m_impl->displayedPosition);
    } else {
        m_impl->timeline->tractor()->set_speed(0);
    }
}

int PreviewMonitor::refreshCount() const
{
    return m_impl->refreshes;
}

void PreviewMonitor::resetRefreshCount()
{
    m_impl->refreshes = 0;
}

void PreviewMonitor::setOverlay(bool enabled)
{
    if (m_impl->overlay == enabled) {
        return;
    }
    if (enabled) {
        REQUIRE(m_impl->timeline->previewManager());
        m_impl->timeline->setOverlayTrack(new Mlt::Playlist(pCore->getProjectProfile()));
    } else {
        m_impl->timeline->removeOverlayTrack();
    }
    m_impl->overlay = enabled;
}

} // namespace TimelinePreviewTests
