/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#include "simplekeyframes/simplekeyframewidget.h"
#include "simpletimelinewidget.h"
#include "timecodedisplay.h"

#include <QToolButton>
#include <QGridLayout>

#include <KIcon>
#include <KLocale>

SimpleKeyframeWidget::SimpleKeyframeWidget(Timecode t, int in, int out, QWidget *parent) :
        QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    QGridLayout *l = new QGridLayout(this);

    m_timeline = new SimpleTimelineWidget(this);
    m_timeline->setRange(in ,out);

    m_buttonAddDelete = new QToolButton(this);
    m_buttonAddDelete->setAutoRaise(true);
    m_buttonAddDelete->setIcon(KIcon("document-new"));
    m_buttonAddDelete->setToolTip(i18n("Add keyframe"));

    m_buttonPrevious = new QToolButton(this);
    m_buttonPrevious->setAutoRaise(true);
    m_buttonPrevious->setIcon(KIcon("media-skip-backward"));
    m_buttonPrevious->setToolTip(i18n("Go to previous keyframe"));

    m_buttonNext = new QToolButton(this);
    m_buttonNext->setAutoRaise(true);
    m_buttonNext->setIcon(KIcon("media-skip-forward"));
    m_buttonNext->setToolTip(i18n("Go to next keyframe"));

    m_time = new TimecodeDisplay(t, this);
    m_time->setRange(in, out);

    l->addWidget(m_timeline, 0, 0, 1, -1);
    l->addWidget(m_buttonPrevious, 1, 0);
    l->addWidget(m_buttonAddDelete, 1, 1);
    l->addWidget(m_buttonNext, 1, 2);
    l->addWidget(m_time, 1, 3, Qt::AlignRight);

    connect(m_time, SIGNAL(editingFinished()), this, SLOT(slotSetPosition()));
    connect(m_timeline, SIGNAL(positionChanged(int)), this, SLOT(slotSetPosition(int)));
    connect(m_timeline, SIGNAL(keyframeAdded(int)), this, SIGNAL(keyframeAdded(int)));
    connect(m_timeline, SIGNAL(keyframeRemoved(int)), this, SIGNAL(keyframeRemoved(int)));

    connect(m_buttonAddDelete, SIGNAL(pressed()), m_timeline, SLOT(slotAddRemove()));
    connect(m_buttonPrevious, SIGNAL(pressed()), m_timeline, SLOT(slotGoToPrev()));
    connect(m_buttonNext, SIGNAL(pressed()), m_timeline, SLOT(slotGoToNext()));
}

SimpleKeyframeWidget::~SimpleKeyframeWidget()
{
    delete m_timeline;
    delete m_buttonAddDelete;
    delete m_buttonPrevious;
    delete m_buttonNext;
    //delete m_buttonSync;
    delete m_time;
}

void SimpleKeyframeWidget::slotSetPosition(int pos, bool update)
{
    if (pos < 0) {
        pos = m_time->getValue();
        m_timeline->slotSetPosition(pos);
    } else {
        m_time->setValue(pos);
        m_timeline->slotSetPosition(pos);
    }

    if (update)
        emit positionChanged(pos);
}

int SimpleKeyframeWidget::getPosition()
{
    return m_time->getValue();
}

void SimpleKeyframeWidget::setKeyframes(const QList< int >& keyframes)
{
    m_timeline->setKeyframes(keyframes);
}

void SimpleKeyframeWidget::addKeyframe(int pos)
{
    blockSignals(true);
    m_timeline->slotAddKeyframe(pos);
    blockSignals(false);
}


#include "simplekeyframewidget.moc"
