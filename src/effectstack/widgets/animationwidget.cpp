/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "utils/KoIconUtils.h"

#include <QDebug>
#include <QVBoxLayout>
#include <QToolBar>
#include <QDomElement>
#include <QMenu>

#include <KDualAction>
#include <KSelectAction>
#include <klocalizedstring.h>

#include "mlt++/MltAnimation.h"

#include "animationwidget.h"
#include "doubleparameterwidget.h"
#include "monitor/monitor.h"
#include "timecodedisplay.h"
#include "effectstack/parametercontainer.h"
#include "../animkeyframeruler.h"

AnimationWidget::AnimationWidget(EffectMetaInfo *info, int clipPos, int min, int max, QDomElement xml, int activeKeyframe, QWidget *parent) :
    QWidget(parent)
    , m_monitor(info->monitor)
    , m_timePos(new TimecodeDisplay(info->monitor->timecode()))
    , m_clipPos(clipPos)
    , m_inPoint(min)
    , m_outPoint(max)
    , m_factor(1)
{
    // Anim properties might at some point require some more infos like profile
    m_animProperties.set("anim", xml.hasAttribute(QStringLiteral("value")) ? xml.attribute(QStringLiteral("value")).toUtf8().constData() : xml.attribute(QStringLiteral("default")).toUtf8().constData());
    // Required to initialize anim property
    m_animProperties.anim_get_int("anim", 0);
    m_animController = m_animProperties.get_anim("anim");
    QVBoxLayout* vbox2 = new QVBoxLayout(this);

    // Keyframe ruler
    m_ruler = new AnimKeyframeRuler(min, max, this);
    connect(m_ruler, &AnimKeyframeRuler::addKeyframe, this, &AnimationWidget::slotAddKeyframe);
    connect(m_ruler, &AnimKeyframeRuler::removeKeyframe, this, &AnimationWidget::slotDeleteKeyframe);
    vbox2->addWidget(m_ruler);
    vbox2->setContentsMargins(0, 0, 0, 0);
    QToolBar *tb = new QToolBar;
    vbox2->addWidget(tb);
    setLayout(vbox2);
    connect(m_ruler, &AnimKeyframeRuler::requestSeek, this, &AnimationWidget::seekToPos);
    connect(m_ruler, &AnimKeyframeRuler::moveKeyframe, this, &AnimationWidget::moveKeyframe);
    connect(m_timePos, SIGNAL(timeCodeEditingFinished()), this, SLOT(slotPositionChanged()));

    // seek to previous
    tb->addAction(KoIconUtils::themedIcon(QStringLiteral("media-skip-backward")), i18n("Previous keyframe"), this, SLOT(slotPrevious()));

    // Add/remove keyframe
    m_addKeyframe = new KDualAction(i18n("Add keyframe"), i18n("Remove keyframe"), this);
    m_addKeyframe->setInactiveIcon(KoIconUtils::themedIcon(QStringLiteral("list-add")));
    m_addKeyframe->setActiveIcon(KoIconUtils::themedIcon(QStringLiteral("list-remove")));
    connect(m_addKeyframe, SIGNAL(activeChangedByUser(bool)), this, SLOT(slotAddDeleteKeyframe(bool)));
    tb->addAction(m_addKeyframe);

    // seek to previous
    tb->addAction(KoIconUtils::themedIcon(QStringLiteral("media-skip-forward")), i18n("Next keyframe"), this, SLOT(slotNext()));

    // Keyframe type widget
    m_selectType = new KSelectAction(i18n("Keyframe interpolation"), this);
    QAction *discrete = new QAction(i18n("Discrete"), this);
    discrete->setData((int) mlt_keyframe_discrete);
    discrete->setCheckable(true);
    m_selectType->addAction(discrete);
    QAction *linear = new QAction(i18n("Linear"), this);
    linear->setData((int) mlt_keyframe_linear);
    linear->setCheckable(true);
    m_selectType->addAction(linear);
    QAction *curve = new QAction(i18n("Curve"), this);
    curve->setData((int) mlt_keyframe_smooth);
    curve->setCheckable(true);
    m_selectType->addAction(curve);
    m_selectType->setCurrentAction(linear);
    connect(m_selectType, SIGNAL(triggered(QAction*)), this, SLOT(slotEditKeyframeType(QAction*)));

    m_relativeToEnd = new QAction(i18n("Relative to end"), this);
    m_relativeToEnd->setCheckable(true);

    QMenu *container = new QMenu;
    container->addAction(m_selectType);
    container->addAction(m_relativeToEnd);

    QToolButton *menuButton = new QToolButton;
    menuButton->setIcon(KoIconUtils::themedIcon(QStringLiteral("kdenlive-menu")));
    menuButton->setToolTip(i18n("Options"));
    menuButton->setMenu(container);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    tb->addWidget(menuButton);

    // Spacer
    QWidget* empty = new QWidget();
    empty->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    tb->addWidget(empty);

    // Timecode
    tb->addWidget(m_timePos);
    m_timePos->setRange(0, m_outPoint - m_inPoint);

    // Display keyframe parameter
    addParameter(xml, activeKeyframe);

    // Load keyframes
    rebuildKeyframes();
}



void AnimationWidget::updateTimecodeFormat()
{
    m_timePos->slotUpdateTimeCodeFormat();
}

void AnimationWidget::slotPrevious()
{
    int previous = m_animController->previous_key(m_timePos->getValue() - 1);
    slotPositionChanged(previous, true);
}

void AnimationWidget::slotNext()
{
    int next = m_animController->next_key(m_timePos->getValue() + 1);
    if (next == 1 && m_timePos->getValue() != 0) {
        // No keyframe after current pos, return end position
        next = m_timePos->maximum();
    }
    slotPositionChanged(next, true);
}

void AnimationWidget::slotAddKeyframe(int pos)
{
    double val = m_animProperties.anim_get_double("anim", pos, m_timePos->maximum());
    // Get current keyframe type
    m_animProperties.anim_set("anim", val, pos, m_timePos->maximum(), (mlt_keyframe_type) m_selectType->currentAction()->data().toInt());
    rebuildKeyframes();
    m_selectType->setEnabled(true);
    m_relativeToEnd->setEnabled(true);
    m_addKeyframe->setActive(true);
    DoubleParameterWidget *slider = m_doubleWidgets.at(0);
    slider->setEnabled(true);
    emit parameterChanged();
}

void AnimationWidget::slotDeleteKeyframe(int pos)
{
    m_animController->remove(pos);
    rebuildKeyframes();
    m_selectType->setEnabled(false);
    m_relativeToEnd->setEnabled(false);
    m_addKeyframe->setActive(false);
    DoubleParameterWidget *slider = m_doubleWidgets.at(0);
    slider->setEnabled(false);
    emit parameterChanged();
}

void AnimationWidget::slotAddDeleteKeyframe(bool add)
{
    int pos = m_timePos->getValue();
    if (!add) {
        // Delete keyframe at current pos
        if (m_animController->is_key(pos)) {
            slotDeleteKeyframe(pos);
        }
    } else {
        // Create keyframe
        if (!m_animController->is_key(pos)) {
            slotAddKeyframe(pos);
        }
    }
}

void AnimationWidget::slotSyncPosition(int relTimelinePos)
{
    // do only sync if this effect is keyframable
    if (m_timePos->maximum() > 0) {
        relTimelinePos = qBound(0, relTimelinePos, m_timePos->maximum());
        slotPositionChanged(relTimelinePos, false);
    }
}

void AnimationWidget::moveKeyframe(int oldPos, int newPos)
{
    bool isKey;
    mlt_keyframe_type type;
    if (!m_animController->get_item(oldPos, isKey, type)) {
        double val = m_animProperties.anim_get_double("anim", oldPos, m_timePos->maximum());
        m_animController->remove(oldPos);
        m_animProperties.anim_set("anim", val, newPos, m_timePos->maximum(), type);
        rebuildKeyframes();
        updateToolbar();
        emit parameterChanged();
    }
}

void AnimationWidget::rebuildKeyframes()
{
    // Fetch keyframes
    QVector <int> keyframes;
    QVector <int> types;
    int frame;
    mlt_keyframe_type type;
    for (int i = 0; i < m_animController->key_count(); i++) {
        if (!m_animController->key_get(i, frame, type)) {
            keyframes << frame;
            types << (int) type;
        }
    }
    m_ruler->updateKeyframes(keyframes, types);
}

void AnimationWidget::updateToolbar()
{
    int pos = m_timePos->getValue();
    double val = m_animProperties.anim_get_double("anim", pos, m_timePos->maximum());
    //TODO: manage several params
    DoubleParameterWidget *slider = m_doubleWidgets.at(0);
    slider->setValue(val * m_factor);
    if (m_animController->is_key(pos)) {
        QList<QAction *> types = m_selectType->actions();
        for (int i = 0; i < types.count(); i++) {
            if (types.at(i)->data().toInt() == (int) m_animController->keyframe_type(pos)) {
                m_selectType->setCurrentAction(types.at(i));
                break;
            }
        }
        m_selectType->setEnabled(true);
        m_addKeyframe->setActive(true);
        slider->setEnabled(true);
    } else {
        m_selectType->setEnabled(false);
        m_addKeyframe->setActive(false);
        slider->setEnabled(false);
    }
}

void AnimationWidget::slotPositionChanged(int pos, bool seek)
{
    if (pos == -1)
        pos = m_timePos->getValue();
    else
        m_timePos->setValue(pos);

    m_ruler->setValue(pos);
    //TODO: manage several params
    DoubleParameterWidget *slider = m_doubleWidgets.at(0);
    if (!m_animController->is_key(pos)) {
        // no keyframe
        if (m_animController->key_count() == 1) {
            // Special case: only one keyframe, allow adjusting whatever the position is
            m_monitor->setEffectKeyframe(true);
        } else {
            m_monitor->setEffectKeyframe(false);
        }
        m_selectType->setEnabled(false);
        m_addKeyframe->setActive(false);
        slider->setEnabled(false);
    } else {
        // keyframe
        m_monitor->setEffectKeyframe(true);
        m_addKeyframe->setActive(true);
        m_selectType->setEnabled(true);
        QList<QAction *> types = m_selectType->actions();
        for (int i = 0; i < types.count(); i++) {
            if (types.at(i)->data().toInt() == (int) m_animController->keyframe_type(pos)) {
                m_selectType->setCurrentAction(types.at(i));
                break;
            }
        }
        slider->setEnabled(true);
    }
    double val = m_animProperties.anim_get_double("anim", pos, m_timePos->maximum());
    slider->setValue(val * m_factor);
    if (seek)
        emit seekToPos(pos);
}

void AnimationWidget::slotEditKeyframeType(QAction *action)
{
    int pos = m_timePos->getValue();
    if (m_animController->is_key(pos)) {
        // This is a keyframe
        double val = m_animProperties.anim_get_double("anim", pos, m_timePos->maximum());
        m_animProperties.anim_set("anim", val, pos, m_timePos->maximum(), (mlt_keyframe_type) action->data().toInt());
        rebuildKeyframes();
        emit parameterChanged();
    }
}

void AnimationWidget::addParameter(const QDomElement &e, int activeKeyframe)
{
    int index = 0;
    if (!m_params.isEmpty()) {
        index = m_params.count();
    }
    m_params.append(e.cloneNode().toElement());
    QDomElement na = e.firstChildElement(QStringLiteral("name"));
    QString paramName = i18n(na.text().toUtf8().data());
    QDomElement commentElem = e.firstChildElement(QStringLiteral("comment"));
    QString comment;
    QLocale locale;
    if (!commentElem.isNull())
        comment = i18n(commentElem.text().toUtf8().data());
    m_factor = e.hasAttribute(QStringLiteral("factor")) ? locale.toDouble(e.attribute(QStringLiteral("factor"))) : 1;
    DoubleParameterWidget *doubleparam = new DoubleParameterWidget(paramName, 0,
                                                                   e.attribute(QStringLiteral("min")).toDouble(), e.attribute(QStringLiteral("max")).toDouble(),
                                                                   e.attribute(QStringLiteral("default")).toDouble(), comment, index, e.attribute(QStringLiteral("suffix")), e.attribute(QStringLiteral("decimals")).toInt(), this);
    connect(doubleparam, SIGNAL(valueChanged(double)), this, SLOT(slotAdjustKeyframeValue(double)));
    //connect(this, SIGNAL(showComments(bool)), doubleparam, SLOT(slotShowComment(bool)));
    //connect(doubleparam, SIGNAL(setInTimeline(int)), this, SLOT(slotUpdateVisibleParameter(int)));
    layout()->addWidget(doubleparam);
    if (e.attribute(QStringLiteral("intimeline")) == QLatin1String("1")) {
        doubleparam->setInTimelineProperty(true);
    }
    m_doubleWidgets << doubleparam;
    int framePos = qBound<int>(0, m_monitor->render->seekFramePosition() - m_clipPos, m_timePos->maximum());
    slotPositionChanged(framePos);
}

void AnimationWidget::slotAdjustKeyframeValue(double value)
{
    int pos = m_timePos->getValue();
    if (m_animController->is_key(pos)) {
        // This is a keyframe
        m_animProperties.anim_set("anim", value / m_factor, pos, m_timePos->maximum(), (mlt_keyframe_type) m_selectType->currentAction()->data().toInt());
    }
    emit parameterChanged();
}

QString AnimationWidget::getAnimation()
{
    m_animController->interpolate();
    QString result = m_animController->serialize_cut(0, m_timePos->maximum());
    return result;
}
