/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TRACKHEADERWIDGET_H
#define TRACKHEADERWIDGET_H

#include <QWidget>
#include "ui_trackheader_ui.h"

class TimelineTrack;


/**
 * @class TrackHeaderWidget
 * @brief Widget to be displayed in front of the track in the timeline view. Provides basic track configuration.
 */


class TrackHeaderWidget : public QWidget, public Ui::TrackHeader_UI
{
    Q_OBJECT

public:
    /** @brief Constructor. */
    explicit TrackHeaderWidget(TimelineTrack *track, QWidget* parent = 0);

private slots:
    void rename();
    void onNameChanged(const QString &name);
    void onVisibilityChanged(bool isHidden);
    void onAudibilityChanged(bool isMute);
    void onLockStateChanged(bool isLocked);

private:
    void setupButton(QToolButton *button, const char *toolTip, bool isChecked, void (TrackHeaderWidget::*stateSetter)(bool), const char *setter);
    void setButtonState(QToolButton *button, bool isChecked, const QString &iconName, const QString &checkedIconName);

    TimelineTrack *m_track;
};

#endif
