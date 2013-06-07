/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "trackheaderwidget.h"
#include "project/timelinetrack.h"


TrackHeaderWidget::TrackHeaderWidget(TimelineTrack* track, QWidget* parent) :
    QWidget(parent),
    m_track(track)
{
    // do properly
    setFixedHeight(50);

    setupUi(this);

    editName->setText(track->name());
    connect(editName, SIGNAL(editingFinished()), this, SLOT(rename()));
    connect(m_track, SIGNAL(nameChanged(QString)), this, SLOT(onNameChanged(QString)));

    setupButton(buttonVideo, "Hide track", track->isHidden(), &TrackHeaderWidget::onVisibilityChanged, SLOT(hide(bool)));
    connect(m_track, SIGNAL(visibilityChanged(bool)), this, SLOT(onVisibilityChanged(bool)));

    setupButton(buttonAudio, "Mute track", track->isMute(), &TrackHeaderWidget::onAudibilityChanged, SLOT(mute(bool)));
    connect(m_track, SIGNAL(audibilityChanged(bool)), this, SLOT(onAudibilityChanged(bool)));

    setupButton(buttonLock, "Lock track", track->isLocked(), &TrackHeaderWidget::onLockStateChanged, SLOT(lock(bool)));
    connect(m_track, SIGNAL(lockStateChanged(bool)), this, SLOT(onLockStateChanged(bool)));

    if (track->type() == TimelineTrack::VideoTrack) {
        setBackgroundRole(QPalette::AlternateBase);
        setAutoFillBackground(true);
    } else {
        buttonVideo->setHidden(true);
    }

    // Don't show track buttons if size is too small
    if (height() < 40) {
        buttonVideo->setHidden(true);
        buttonAudio->setHidden(true);
        buttonLock->setHidden(true);
    }
}

void TrackHeaderWidget::rename()
{
    m_track->setName(editName->text());
}

void TrackHeaderWidget::onNameChanged(const QString &name)
{
    editName->setText(name);
}

void TrackHeaderWidget::onVisibilityChanged(bool isHidden)
{
    setButtonState(buttonVideo, isHidden, "kdenlive-show-video", "kdenlive-hide-video");
}

void TrackHeaderWidget::onAudibilityChanged(bool isMute)
{
    setButtonState(buttonAudio, isMute, "kdenlive-show-audio", "kdenlive-hide-audio");
}

void TrackHeaderWidget::onLockStateChanged(bool isLocked)
{
    setButtonState(buttonLock, isLocked, "kdenlive-unlock", "kdenlive-lock");
}

void TrackHeaderWidget::setupButton(QToolButton* button, const char* toolTip, bool isChecked, void (TrackHeaderWidget::*stateSetter)(bool), const char *setter)
{
    (this->*stateSetter)(isChecked);
    button->setToolTip(i18n(toolTip));
    connect(button, SIGNAL(toggled(bool)), m_track, setter);
}

void TrackHeaderWidget::setButtonState(QToolButton* button, bool isChecked, const QString& iconName, const QString& checkedIconName)
{
    button->blockSignals(true);
    button->setChecked(isChecked);
    if (isChecked) {
        button->setIcon(KIcon(checkedIconName));
    } else {
        button->setIcon(KIcon(iconName));
    }
    button->blockSignals(false);
}

#include "trackheaderwidget.moc"
