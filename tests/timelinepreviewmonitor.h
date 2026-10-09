/*
    SPDX-FileCopyrightText: 2026 Kdenlive contributors
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <QColor>
#include <memory>

class TimelineController;
class TimelineItemModel;

namespace TimelinePreviewTests {

// Headless monitor adapter for preview tests; this does not exercise production Monitor.
class PreviewMonitor
{
public:
    PreviewMonitor(std::shared_ptr<TimelineItemModel> timeline, TimelineController &controller);
    ~PreviewMonitor();

    void seek(int position);
    int position() const;
    double speed() const;
    QColor color() const;
    void play();
    QColor nextFrame();
    void pause();
    int refreshCount() const;
    void resetRefreshCount();
    void setOverlay(bool enabled);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace TimelinePreviewTests
