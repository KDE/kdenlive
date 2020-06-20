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

#include "kdenlive_debug.h"
#include <KConfig>
#include <KConfigGroup>
#include <KDualAction>
#include <KSelectAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QLineEdit>
#include <QMenu>
#include <QStandardPaths>
#include <QToolBar>
#include <QVBoxLayout>
#include <float.h>
#include <klocalizedstring.h>

#include "mlt++/MltAnimation.h"
#include "mlt++/MltProfile.h"

#include "animationwidget.h"
#include "assets/model/assetparametermodel.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "mltcontroller/effectscontroller.h"
#include "monitor/monitor.h"
#include "timecodedisplay.h"
#include "widgets/doublewidget.h"
#include "widgets/dragvalue.h"

AnimationWidget::AnimationWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QPair<int, int> range, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_active(false)
    //, m_clipPos(clipPos)
    , m_editedKeyframe(-1)
    , m_attachedToEnd(-2)
    //, m_xml(xml)
    //, m_effectId(effectId)
    , m_spinX(nullptr)
    , m_spinY(nullptr)
    , m_spinWidth(nullptr)
    , m_spinHeight(nullptr)
    , m_spinSize(nullptr)
    , m_spinOpacity(nullptr)
{
    setAcceptDrops(true);
    auto *vbox2 = new QVBoxLayout(this);

    // Keyframe ruler
    if (range.second >= 0) {
        m_inPoint = range.first;
        m_outPoint = range.second;
        m_offset = m_model->data(m_index, AssetParameterModel::InRole).toInt();
    } else {
        m_offset = 0;
        m_inPoint = m_model->data(m_index, AssetParameterModel::InRole).toInt();
        m_outPoint = m_model->data(m_index, AssetParameterModel::OutRole).toInt() + 1;
    }
    m_monitorSize = pCore->getCurrentFrameSize();
    m_monitor = pCore->getMonitor(m_model->monitorId);
    m_timePos = new TimecodeDisplay(pCore->timecode(), this);
    m_ruler = new AnimKeyframeRuler(0, m_outPoint - m_inPoint, this);
    connect(m_ruler, &AnimKeyframeRuler::addKeyframe, this, &AnimationWidget::slotAddKeyframe);
    connect(m_ruler, &AnimKeyframeRuler::removeKeyframe, this, &AnimationWidget::slotDeleteKeyframe);
    vbox2->addWidget(m_ruler);
    vbox2->setContentsMargins(0, 0, 0, 0);
    auto *tb = new QToolBar(this);
    vbox2->addWidget(tb);
    setLayout(vbox2);
    connect(m_ruler, &AnimKeyframeRuler::requestSeek, this, &AnimationWidget::requestSeek);
    connect(m_ruler, &AnimKeyframeRuler::moveKeyframe, this, &AnimationWidget::moveKeyframe);
    connect(m_timePos, SIGNAL(timeCodeEditingFinished()), this, SLOT(slotPositionChanged()));
    connect(m_monitor, &Monitor::seekPosition, this, &AnimationWidget::monitorSeek, Qt::UniqueConnection);

    if (m_frameSize.isNull() || m_frameSize.width() == 0 || m_frameSize.height() == 0) {
        m_frameSize = m_monitorSize;
    }

    // seek to previous
    m_previous = tb->addAction(QIcon::fromTheme(QStringLiteral("media-skip-backward")), i18n("Previous keyframe"), this, SLOT(slotPrevious()));

    // Add/remove keyframe
    m_addKeyframe = new KDualAction(i18n("Add keyframe"), i18n("Remove keyframe"), this);
    m_addKeyframe->setInactiveIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    m_addKeyframe->setActiveIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
    connect(m_addKeyframe, SIGNAL(activeChangedByUser(bool)), this, SLOT(slotAddDeleteKeyframe(bool)));
    tb->addAction(m_addKeyframe);

    // seek to next
    m_next = tb->addAction(QIcon::fromTheme(QStringLiteral("media-skip-forward")), i18n("Next keyframe"), this, SLOT(slotNext()));

    // Preset combo
    m_presetCombo = new QComboBox(this);
    m_presetCombo->setToolTip(i18n("Presets"));
    connect(m_presetCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(applyPreset(int)));
    tb->addWidget(m_presetCombo);

    // Keyframe type widget
    m_selectType = new KSelectAction(QIcon::fromTheme(QStringLiteral("keyframes")), i18n("Keyframe interpolation"), this);
    QAction *discrete = new QAction(QIcon::fromTheme(QStringLiteral("discrete")), i18n("Discrete"), this);
    discrete->setData((int)mlt_keyframe_discrete);
    discrete->setCheckable(true);
    m_selectType->addAction(discrete);
    QAction *linear = new QAction(QIcon::fromTheme(QStringLiteral("linear")), i18n("Linear"), this);
    linear->setData((int)mlt_keyframe_linear);
    linear->setCheckable(true);
    m_selectType->addAction(linear);
    QAction *curve = new QAction(QIcon::fromTheme(QStringLiteral("smooth")), i18n("Smooth"), this);
    curve->setData((int)mlt_keyframe_smooth);
    curve->setCheckable(true);
    m_selectType->addAction(curve);
    m_selectType->setCurrentAction(linear);
    connect(m_selectType, SIGNAL(triggered(QAction *)), this, SLOT(slotEditKeyframeType(QAction *)));

    KSelectAction *defaultInterp = new KSelectAction(QIcon::fromTheme(QStringLiteral("keyframes")), i18n("Default interpolation"), this);
    discrete = new QAction(QIcon::fromTheme(QStringLiteral("discrete")), i18n("Discrete"), this);
    discrete->setData((int)mlt_keyframe_discrete);
    discrete->setCheckable(true);
    defaultInterp->addAction(discrete);
    linear = new QAction(QIcon::fromTheme(QStringLiteral("linear")), i18n("Linear"), this);
    linear->setData((int)mlt_keyframe_linear);
    linear->setCheckable(true);
    defaultInterp->addAction(linear);
    curve = new QAction(QIcon::fromTheme(QStringLiteral("smooth")), i18n("Smooth"), this);
    curve->setData((int)mlt_keyframe_smooth);
    curve->setCheckable(true);
    defaultInterp->addAction(curve);
    switch (KdenliveSettings::defaultkeyframeinterp()) {
    case mlt_keyframe_discrete:
        defaultInterp->setCurrentAction(discrete);
        break;
    case mlt_keyframe_smooth:
        defaultInterp->setCurrentAction(curve);
        break;
    default:
        defaultInterp->setCurrentAction(linear);
        break;
    }
    connect(defaultInterp, SIGNAL(triggered(QAction *)), this, SLOT(slotSetDefaultInterp(QAction *)));
    m_selectType->setToolBarMode(KSelectAction::ComboBoxMode);

    m_endAttach = new QAction(i18n("Attach keyframe to end"), this);
    m_endAttach->setCheckable(true);
    connect(m_endAttach, &QAction::toggled, this, &AnimationWidget::slotReverseKeyframeType);

    // copy/paste keyframes from clipboard
    QAction *copy = new QAction(i18n("Copy keyframes to clipboard"), this);
    connect(copy, &QAction::triggered, this, &AnimationWidget::slotCopyKeyframes);
    QAction *paste = new QAction(i18n("Import keyframes from clipboard"), this);
    connect(paste, &QAction::triggered, this, &AnimationWidget::slotImportKeyframes);

    QAction *removeNext = new QAction(i18n("Remove all keyframes after cursor"), this);
    connect(removeNext, &QAction::triggered, this, &AnimationWidget::slotRemoveNext);

    // save preset
    QAction *savePreset = new QAction(QIcon::fromTheme(QStringLiteral("document-save")), i18n("Save preset"), this);
    connect(savePreset, &QAction::triggered, this, &AnimationWidget::savePreset);

    // delete preset
    QAction *delPreset = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("Delete preset"), this);
    connect(delPreset, &QAction::triggered, this, &AnimationWidget::deletePreset);

    auto *container = new QMenu;
    tb->addAction(m_selectType);
    container->addAction(m_endAttach);
    container->addSeparator();
    container->addAction(copy);
    container->addAction(paste);
    container->addAction(removeNext);
    container->addSeparator();
    container->addAction(savePreset);
    container->addAction(delPreset);
    container->addAction(defaultInterp);

    auto *menuButton = new QToolButton;
    menuButton->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-menu")));
    menuButton->setToolTip(i18n("Options"));
    menuButton->setMenu(container);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    tb->addWidget(menuButton);

    // Spacer
    QWidget *empty = new QWidget();
    empty->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(empty);

    // Timecode
    tb->addWidget(m_timePos);
    m_timePos->setFrame(false);
    m_timePos->setRange(0, m_outPoint - m_inPoint - 1);

    // Prepare property
    mlt_profile profile = m_monitor->profile()->get_profile();
    m_animProperties.set("_profile", profile, 0, nullptr, nullptr);

    // Display keyframe parameter
    addParameter(m_index);
    finishSetup();
    // Update displayed values
    monitorSeek(m_monitor->position());
}

AnimationWidget::~AnimationWidget() {}

void AnimationWidget::finishSetup()
{
    // Load effect presets
    loadPresets();
}

// static
QString AnimationWidget::getDefaultKeyframes(int start, const QString &defaultValue, bool linearOnly)
{
    QString keyframes = QString::number(start);
    if (linearOnly) {
        keyframes.append(QLatin1Char('='));
    } else {
        switch (KdenliveSettings::defaultkeyframeinterp()) {
        case mlt_keyframe_discrete:
            keyframes.append(QStringLiteral("|="));
            break;
        case mlt_keyframe_smooth:
            keyframes.append(QStringLiteral("~="));
            break;
        default:
            keyframes.append(QLatin1Char('='));
            break;
        }
    }
    keyframes.append(defaultValue);
    return keyframes;
}

void AnimationWidget::updateTimecodeFormat()
{
    m_timePos->slotUpdateTimeCodeFormat();
}

void AnimationWidget::slotPrevious()
{
    int previous = qMax(-m_offset, m_animController.previous_key(m_timePos->getValue() - m_offset - 1)) + m_offset;
    if (previous == m_timePos->getValue() && previous > 0) {
        // Make sure we can seek to effect start even if there is no keyframe
        previous = 0;
    }
    m_ruler->setActiveKeyframe(previous);
    slotPositionChanged(previous, true);
}

void AnimationWidget::slotNext()
{
    int next = m_animController.next_key(m_timePos->getValue() - m_offset + 1) + m_offset;
    if (!m_animController.is_key(next - m_offset)) {
        // No keyframe after current pos, return end position
        next = m_timePos->maximum();
    } else {
        m_ruler->setActiveKeyframe(next - m_offset);
    }
    slotPositionChanged(next, true);
}

void AnimationWidget::slotAddKeyframe(int pos)
{
    slotAddDeleteKeyframe(true, pos);
}

void AnimationWidget::doAddKeyframe(int pos, QString paramName, bool directUpdate)
{
    if (paramName.isEmpty()) {
        paramName = m_inTimeline;
    }
    if (pos == -1) {
        pos = m_timePos->getValue();
    }
    pos -= m_offset;
    // Try to get previous key's type
    mlt_keyframe_type type;
    if (m_selectType->isVisible()) {
        type = (mlt_keyframe_type)KdenliveSettings::defaultkeyframeinterp();
        if (m_animController.key_count() > 1) {
            int previous = m_animController.previous_key(pos);
            if (m_animController.is_key(previous)) {
                type = m_animController.keyframe_type(previous);
            } else {
                int next = m_animController.next_key(pos);
                if (m_animController.is_key(next)) {
                    type = m_animController.keyframe_type(next);
                }
            }
        }
    } else {
        type = mlt_keyframe_linear;
    }

    if (paramName == m_rectParameter) {
        mlt_rect rect = m_animProperties.anim_get_rect(paramName.toUtf8().constData(), pos, m_outPoint);
        m_animProperties.anim_set(paramName.toUtf8().constData(), rect, pos, m_outPoint, type);
    } else {
        double val = m_animProperties.anim_get_double(paramName.toUtf8().constData(), pos, m_outPoint);
        m_animProperties.anim_set(paramName.toUtf8().constData(), val, pos, m_outPoint, type);
    }
    slotPositionChanged(-1, false);
    if (directUpdate) {
        m_ruler->setActiveKeyframe(pos);
        rebuildKeyframes();
        emit valueChanged(m_index, QString(m_animController.serialize_cut()), true);
    }
}

void AnimationWidget::slotDeleteKeyframe(int pos)
{
    slotAddDeleteKeyframe(false, pos);
}

void AnimationWidget::slotAddDeleteKeyframe(bool add, int pos)
{
    if (pos == -1) {
        pos = m_timePos->getValue();
    }
    QStringList paramNames = m_doubleWidgets.keys();
    if (!m_rectParameter.isEmpty()) {
        paramNames << m_rectParameter;
    }
    if (!add) {
        // Delete keyframe in all animations at current pos
        for (int i = 0; i < paramNames.count(); i++) {
            m_animController = m_animProperties.get_animation(paramNames.at(i).toUtf8().constData());
            if (m_animController.is_key(pos - m_offset)) {
                m_animController.remove(pos - m_offset);
            }
        }
        m_selectType->setEnabled(false);
        m_addKeyframe->setActive(false);
        slotPositionChanged(-1, false);
    } else {
        // Add keyframe in all animations
        for (int i = 0; i < paramNames.count(); i++) {
            m_animController = m_animProperties.get_animation(paramNames.at(i).toUtf8().constData());
            if (!m_animController.is_key(pos - m_offset)) {
                doAddKeyframe(pos, paramNames.at(i), false);
            }
        }
        m_ruler->setActiveKeyframe(pos);
    }
    // Rebuild
    rebuildKeyframes();

    // Send updates
    for (int i = 0; i < m_parameters.count(); i++) {
        m_animController = m_animProperties.get_animation(m_parameters.at(i).second.toUtf8().constData());
        emit valueChanged(m_parameters.at(i).first, QString(m_animController.serialize_cut()), true);
    }
    // Restore default controller
    m_animController = m_animProperties.get_animation(m_inTimeline.toUtf8().constData());
}

void AnimationWidget::slotRemoveNext()
{
    int pos = m_timePos->getValue();
    // Delete keyframe in all animations at current pos
    QStringList paramNames = m_doubleWidgets.keys();
    if (!m_rectParameter.isEmpty()) {
        paramNames << m_rectParameter;
    }
    int kfrPos;
    for (int i = 0; i < paramNames.count(); i++) {
        m_animController = m_animProperties.get_animation(paramNames.at(i).toUtf8().constData());
        int j = 0;
        while (j < m_animController.key_count()) {
            kfrPos = m_animController.key_get_frame(j);
            if (kfrPos > (pos - m_offset)) {
                m_animController.remove(kfrPos);
            } else {
                j++;
            }
        }
    }
    m_selectType->setEnabled(false);
    m_addKeyframe->setActive(false);
    slotPositionChanged(-1, false);

    // Send updates
    for (int i = 0; i < m_parameters.count(); i++) {
        m_animController = m_animProperties.get_animation(m_parameters.at(i).second.toUtf8().constData());
        emit valueChanged(m_parameters.at(i).first, QString(m_animController.serialize_cut()), true);
    }
    // Restore default controller
    m_animController = m_animProperties.get_animation(m_inTimeline.toUtf8().constData());
    // Rebuild
    rebuildKeyframes();
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
    if (m_animController.get_item(oldPos - m_offset, isKey, type) != 0) {
        qCDebug(KDENLIVE_LOG) << "////////ERROR NO KFR";
        return;
    }
    if (!m_rectParameter.isEmpty()) {
        m_animController = m_animProperties.get_animation(m_rectParameter.toUtf8().constData());
        mlt_rect rect = m_animProperties.anim_get_rect(m_rectParameter.toUtf8().constData(), oldPos - m_offset, m_outPoint);
        m_animController.remove(oldPos - m_offset);
        m_animProperties.anim_set(m_rectParameter.toUtf8().constData(), rect, newPos - m_offset, m_outPoint, type);
    }
    QStringList paramNames = m_doubleWidgets.keys();
    for (int i = 0; i < paramNames.count(); i++) {
        const QString &param = paramNames.at(i);
        m_animController = m_animProperties.get_animation(param.toUtf8().constData());
        double val = m_animProperties.anim_get_double(param.toUtf8().constData(), oldPos - m_offset, m_outPoint);
        m_animController.remove(oldPos - m_offset);
        m_animProperties.anim_set(param.toUtf8().constData(), val, newPos - m_offset, m_outPoint, type);
    }
    m_ruler->setActiveKeyframe(newPos);
    if (m_attachedToEnd == oldPos) {
        m_attachedToEnd = newPos;
    }
    rebuildKeyframes();
    slotPositionChanged(m_ruler->position(), false);

    // Send updates
    for (int i = 0; i < m_parameters.count(); i++) {
        m_animController = m_animProperties.get_animation(m_parameters.at(i).second.toUtf8().constData());
        emit valueChanged(m_parameters.at(i).first, QString(m_animController.serialize_cut()), true);
    }
    // Restore default controller
    m_animController = m_animProperties.get_animation(m_inTimeline.toUtf8().constData());
}

void AnimationWidget::rebuildKeyframes()
{
    // Fetch keyframes
    QVector<int> keyframes;
    QVector<int> types;
    int frame;
    mlt_keyframe_type type;
    int count = m_animController.key_count();
    for (int i = 0; i < count; i++) {
        if (m_animController.key_get(i, frame, type) == 0) {
            frame += m_offset;
            if (frame >= 0) {
                keyframes << frame;
                types << (count > 1 ? (int)type : mlt_keyframe_linear);
            }
        }
    }
    m_ruler->updateKeyframes(keyframes, types, m_attachedToEnd);
}

void AnimationWidget::updateToolbar()
{
    int pos = m_timePos->getValue();
    QMapIterator<QString, DoubleWidget *> i(m_doubleWidgets);
    while (i.hasNext()) {
        i.next();
        double val = m_animProperties.anim_get_double(i.key().toUtf8().constData(), pos, m_outPoint);
        i.value()->setValue(val * i.value()->factor);
    }
    if (m_animController.is_key(pos) && m_selectType->isVisible()) {
        QList<QAction *> types = m_selectType->actions();
        for (int j = 0; j < types.count(); j++) {
            if (types.at(j)->data().toInt() == (int)m_animController.keyframe_type(pos)) {
                m_selectType->setCurrentAction(types.at(j));
                break;
            }
        }
        m_selectType->setEnabled(m_animController.key_count() > 1);
        m_addKeyframe->setActive(true);
        m_addKeyframe->setEnabled(m_animController.key_count() > 1);
        if (m_doubleWidgets.value(m_inTimeline) != nullptr) {
            m_doubleWidgets.value(m_inTimeline)->enableEdit(true);
        }
    } else {
        m_selectType->setEnabled(false);
        m_addKeyframe->setActive(false);
        m_addKeyframe->setEnabled(true);
        if (m_doubleWidgets.value(m_inTimeline) != nullptr) {
            m_doubleWidgets.value(m_inTimeline)->enableEdit(false);
        }
    }
}

void AnimationWidget::slotPositionChanged(int pos, bool seek)
{
    if (pos == -1) {
        pos = m_timePos->getValue();
    } else {
        m_timePos->setValue(pos);
    }
    m_ruler->setValue(pos);
    if (m_spinX) {
        updateRect(pos);
    }
    updateSlider(pos);
    m_previous->setEnabled(pos > 0);
    m_next->setEnabled(pos < (m_outPoint - 1));

    // scene ratio lock
    if ((m_spinWidth != nullptr) && m_spinWidth->isEnabled()) {
        double ratio =
            m_originalSize->isChecked() ? (double)m_frameSize.width() / m_frameSize.height() : (double)m_monitorSize.width() / m_monitorSize.height();
        bool lockRatio = m_spinHeight->value() == (int)(m_spinWidth->value() / ratio + 0.5);
        m_lockRatio->blockSignals(true);
        m_lockRatio->setChecked(lockRatio);
        m_lockRatio->blockSignals(false);
        m_monitor->setEffectSceneProperty(QStringLiteral("lockratio"), m_lockRatio->isChecked() ? ratio : -1);
    }

    if (seek) {
        m_monitor->requestSeek(pos + m_inPoint);
    }
}

void AnimationWidget::requestSeek(int pos)
{
    m_monitor->requestSeek(pos + m_inPoint);
}

void AnimationWidget::updateSlider(int pos)
{
    m_endAttach->blockSignals(true);
    QMapIterator<QString, DoubleWidget *> i(m_doubleWidgets);
    while (i.hasNext()) {
        i.next();
        m_animController = m_animProperties.get_animation(i.key().toUtf8().constData());
        double val = m_animProperties.anim_get_double(i.key().toUtf8().constData(), pos, m_outPoint);
        if (!m_animController.is_key(pos)) {
            // no keyframe
            m_addKeyframe->setEnabled(true);
            if (m_animController.key_count() <= 1) {
                // Special case: only one keyframe, allow adjusting whatever the position is
                i.value()->enableEdit(true);
                if (!i.value()->hasEditFocus()) {
                    i.value()->setValue(val * i.value()->factor);
                }
                if (i.key() == m_inTimeline) {
                    if (m_active) {
                        m_monitor->setEffectKeyframe(true);
                    }
                    m_endAttach->setEnabled(true);
                    m_endAttach->setChecked(m_attachedToEnd > -2 && m_animController.key_get_frame(0) >= m_attachedToEnd);
                }
            } else {
                i.value()->enableEdit(false);
                i.value()->setValue(val * i.value()->factor);
                if (i.key() == m_inTimeline) {
                    if (m_active) {
                        m_monitor->setEffectKeyframe(false);
                    }
                    m_endAttach->setEnabled(false);
                }
            }
            if (i.key() == m_inTimeline) {
                m_selectType->setEnabled(false);
                m_addKeyframe->setActive(false);
            }
        } else {
            // keyframe
            i.value()->setValue(val * i.value()->factor);
            if (i.key() == m_inTimeline) {
                if (m_active) {
                    m_monitor->setEffectKeyframe(true);
                }
                m_addKeyframe->setActive(true);
                m_addKeyframe->setEnabled(m_animController.key_count() > 1);
                m_selectType->setEnabled(m_animController.key_count() > 1);
                m_endAttach->setEnabled(true);
                m_endAttach->setChecked(m_attachedToEnd > -2 && pos >= m_attachedToEnd);
                if (m_selectType->isVisible()) {
                    mlt_keyframe_type currentType = m_animController.keyframe_type(pos);
                    QList<QAction *> types = m_selectType->actions();
                    for (int j = 0; j < types.count(); j++) {
                        if ((mlt_keyframe_type)types.at(j)->data().toInt() == currentType) {
                            m_selectType->setCurrentAction(types.at(j));
                            break;
                        }
                    }
                }
            }
            i.value()->enableEdit(true);
        }
    }
    m_endAttach->blockSignals(false);
    // Restore default controller
    m_animController = m_animProperties.get_animation(m_inTimeline.toUtf8().constData());
}

void AnimationWidget::updateRect(int pos)
{
    m_endAttach->blockSignals(true);
    m_animController = m_animProperties.get_animation(m_rectParameter.toUtf8().constData());
    mlt_rect rect = m_animProperties.anim_get_rect(m_rectParameter.toUtf8().constData(), pos, m_outPoint);
    m_spinX->blockSignals(true);
    m_spinY->blockSignals(true);
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinSize->blockSignals(true);
    m_spinX->setValue(rect.x);
    m_spinY->setValue(rect.y);
    m_spinWidth->setValue(rect.w);
    m_spinHeight->setValue(rect.h);
    double size;
    if (rect.w / pCore->getCurrentDar() > rect.h) {
        if (m_originalSize->isChecked()) {
            size = rect.w * 100.0 / m_frameSize.width();
        } else {
            size = rect.w * 100.0 / m_monitorSize.width();
        }
    } else {
        if (m_originalSize->isChecked()) {
            size = rect.h * 100.0 / m_frameSize.height();
        } else {
            size = rect.h * 100.0 / m_monitorSize.height();
        }
    }

    m_spinSize->setValue(size);
    if (m_spinOpacity) {
        m_spinOpacity->blockSignals(true);
        m_spinOpacity->setValue(100.0 * rect.o);
        m_spinOpacity->blockSignals(false);
    }
    bool enableEdit = false;
    if (!m_animController.is_key(pos)) {
        // no keyframe
        m_addKeyframe->setEnabled(true);
        if (m_animController.key_count() <= 1) {
            // Special case: only one keyframe, allow adjusting whatever the position is
            enableEdit = true;
            if (m_active) {
                m_monitor->setEffectKeyframe(true);
            }
            m_endAttach->setEnabled(true);
            m_endAttach->setChecked(m_attachedToEnd > -2 && m_animController.key_get_frame(0) >= m_attachedToEnd);
        } else {
            enableEdit = false;
            if (m_active) {
                m_monitor->setEffectKeyframe(false);
            }
            m_endAttach->setEnabled(false);
        }
        m_selectType->setEnabled(false);
        m_addKeyframe->setActive(false);
    } else {
        // keyframe
        enableEdit = true;
        if (m_active) {
            m_monitor->setEffectKeyframe(true);
        }
        m_addKeyframe->setActive(true);
        m_addKeyframe->setEnabled(m_animController.key_count() > 1);
        m_selectType->setEnabled(m_animController.key_count() > 1);
        m_endAttach->setEnabled(true);
        m_endAttach->setChecked(m_attachedToEnd > -2 && pos >= m_attachedToEnd);
        if (m_selectType->isVisible()) {
            mlt_keyframe_type currentType = m_animController.keyframe_type(pos);
            QList<QAction *> types = m_selectType->actions();
            for (int i = 0; i < types.count(); i++) {
                if ((mlt_keyframe_type)types.at(i)->data().toInt() == currentType) {
                    m_selectType->setCurrentAction(types.at(i));
                    break;
                }
            }
        }
    }
    m_spinSize->setEnabled(enableEdit);
    m_spinX->setEnabled(enableEdit);
    m_spinY->setEnabled(enableEdit);
    m_spinWidth->setEnabled(enableEdit);
    m_spinHeight->setEnabled(enableEdit);
    m_spinSize->setEnabled(enableEdit);
    if (m_spinOpacity) {
        m_spinOpacity->setEnabled(enableEdit);
    }
    m_spinX->blockSignals(false);
    m_spinY->blockSignals(false);
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    m_spinSize->blockSignals(false);
    m_endAttach->blockSignals(false);
    setupMonitor(QRect(rect.x, rect.y, rect.w, rect.h));
    // Restore default controller
    m_animController = m_animProperties.get_animation(m_inTimeline.toUtf8().constData());
}

void AnimationWidget::slotEditKeyframeType(QAction *action)
{
    int pos = m_timePos->getValue() - m_offset;
    if (m_animController.is_key(pos)) {
        for (int i = 0; i < m_parameters.count(); i++) {
            m_animController = m_animProperties.get_animation(m_parameters.at(i).second.toUtf8().constData());
            if (m_parameters.at(i).second == m_rectParameter) {
                mlt_rect rect = m_animProperties.anim_get_rect(m_parameters.at(i).second.toUtf8().constData(), pos, m_outPoint);
                m_animProperties.anim_set(m_parameters.at(i).second.toUtf8().constData(), rect, pos, m_outPoint, (mlt_keyframe_type)action->data().toInt());
            } else {
                double val = m_animProperties.anim_get_double(m_parameters.at(i).second.toUtf8().constData(), pos, m_outPoint);
                m_animProperties.anim_set(m_parameters.at(i).second.toUtf8().constData(), val, pos, m_outPoint, (mlt_keyframe_type)action->data().toInt());
            }
            emit valueChanged(m_parameters.at(i).first, QString(m_animController.serialize_cut()), true);
        }
        rebuildKeyframes();
        setupMonitor();
        // Restore default controller
        m_animController = m_animProperties.get_animation(m_inTimeline.toUtf8().constData());
    }
}

void AnimationWidget::slotSetDefaultInterp(QAction *action)
{
    KdenliveSettings::setDefaultkeyframeinterp(action->data().toInt());
}

void AnimationWidget::addParameter(QModelIndex ix)
{
    // Anim properties might at some point require some more infos like profile
    ParamType type = (ParamType)m_model->data(ix, AssetParameterModel::TypeRole).toInt();
    QString keyframes = m_model->data(ix, AssetParameterModel::ValueRole).toString();
    QString paramTag = m_model->data(ix, AssetParameterModel::NameRole).toString();
    m_animProperties.set(paramTag.toUtf8().constData(), keyframes.toUtf8().constData());
    m_attachedToEnd = KeyframeView::checkNegatives(keyframes, m_outPoint);
    if (type == ParamType::Animated || type == ParamType::RestrictedAnim) {
        // one dimension parameter
        // Required to initialize anim property
        m_inTimeline = paramTag;
        m_animProperties.anim_get_int(paramTag.toUtf8().constData(), 0, m_outPoint);
        buildSliderWidget(paramTag, ix);
    } else {
        // one dimension parameter
        // TODO: multiple rect parameters in effect ?
        m_rectParameter = paramTag;
        m_inTimeline = paramTag;
        // Required to initialize anim property
        m_animProperties.anim_get_rect(paramTag.toUtf8().constData(), 0, m_outPoint);
        buildRectWidget(paramTag, ix);
    }
    if (type == ParamType::RestrictedAnim) {
        // This param only support linear keyframes
        m_selectType->setVisible(false);
        m_selectType->setCurrentItem(0);
    }
    m_parameters << QPair<QModelIndex, QString>(ix, paramTag);
    // Load keyframes
    rebuildKeyframes();
}

void AnimationWidget::buildSliderWidget(const QString &paramTag, QModelIndex ix)
{
    QString paramName = i18n(m_model->data(ix, Qt::DisplayRole).toString().toUtf8().data());
    QString comment = m_model->data(ix, AssetParameterModel::CommentRole).toString();
    if (!comment.isEmpty()) {
        comment = i18n(comment.toUtf8().data());
    }

    int index = m_params.count() - 1;

    DoubleWidget *doubleparam = new DoubleWidget(
        paramName, 0, m_model->data(ix, AssetParameterModel::MinRole).toDouble(), m_model->data(ix, AssetParameterModel::MaxRole).toDouble(),
        m_model->data(ix, AssetParameterModel::FactorRole).toDouble() m_model->data(ix, AssetParameterModel::DefaultRole).toDouble(), comment, index,
        m_model->data(ix, AssetParameterModel::SuffixRole).toString(), m_model->data(ix, AssetParameterModel::DecimalsRole).toInt(), m_model->data(ix, AssetParameterModel::OddRole).toBool(), this);
    doubleparam->setObjectName(paramTag);
    doubleparam->setProperty("index", ix);
    connect(doubleparam, &DoubleWidget::valueChanged, this, &AnimationWidget::slotAdjustKeyframeValue);
    layout()->addWidget(doubleparam);

    // TODO: in timeline
    /*if ((!e.hasAttribute(QStringLiteral("intimeline")) || e.attribute(QStringLiteral("intimeline")) == QLatin1String("1")) &&
        !e.hasAttribute(QStringLiteral("notintimeline"))) {*/
    {
        m_inTimeline = paramTag;
        m_animController = m_animProperties.get_animation(m_inTimeline.toUtf8().constData());
    }
    m_doubleWidgets.insert(paramTag, doubleparam);
}

void AnimationWidget::buildRectWidget(const QString &paramTag, QModelIndex ix)
{
    QString paramName = i18n(paramTag.toUtf8().data());
    QString comment = m_model->data(ix, AssetParameterModel::CommentRole).toString();
    if (!comment.isEmpty()) {
        comment = i18n(comment.toUtf8().data());
    }

    auto *horLayout = new QHBoxLayout;
    m_spinX = new DragValue(i18nc("x axis position", "X"), 0, 0, -99000, 99000, -1, QString(), false, false, this);
    connect(m_spinX, &DragValue::valueChanged, this, &AnimationWidget::slotAdjustRectKeyframeValue);
    horLayout->addWidget(m_spinX);
    m_spinX->setProperty("index", ix);

    m_spinY = new DragValue(i18nc("y axis position", "Y"), 0, 0, -99000, 99000, -1, QString(), false, false, this);
    connect(m_spinY, &DragValue::valueChanged, this, &AnimationWidget::slotAdjustRectKeyframeValue);
    horLayout->addWidget(m_spinY);

    m_spinWidth = new DragValue(i18nc("Frame width", "W"), m_monitorSize.width(), 0, 1, 99000, -1, QString(), false, false, this);
    connect(m_spinWidth, &DragValue::valueChanged, this, &AnimationWidget::slotAdjustRectWidth);
    horLayout->addWidget(m_spinWidth);

    // Lock ratio stuff
    m_lockRatio = new QAction(QIcon::fromTheme(QStringLiteral("link")), i18n("Lock aspect ratio"), this);
    m_lockRatio->setCheckable(true);
    connect(m_lockRatio, &QAction::triggered, this, &AnimationWidget::slotLockRatio);
    auto *ratioButton = new QToolButton;
    ratioButton->setDefaultAction(m_lockRatio);
    horLayout->addWidget(ratioButton);

    m_spinHeight = new DragValue(i18nc("Frame height", "H"), m_monitorSize.height(), 0, 1, 99000, -1, QString(), false, false, this);
    connect(m_spinHeight, &DragValue::valueChanged, this, &AnimationWidget::slotAdjustRectHeight);
    horLayout->addWidget(m_spinHeight);
    horLayout->addStretch(10);

    auto *horLayout2 = new QHBoxLayout;
    m_spinSize = new DragValue(i18n("Size"), 100, 2, 1, 99000, -1, i18n("%"), false, false, this);
    m_spinSize->setStep(10);
    connect(m_spinSize, &DragValue::valueChanged, this, &AnimationWidget::slotResize);
    horLayout2->addWidget(m_spinSize);
    if (m_model->data(ix, AssetParameterModel::OpacityRole).toBool()) {
        m_spinOpacity = new DragValue(i18n("Opacity"), 100, 0, 0, 100, -1, i18n("%"), true, false, this);
        connect(m_spinOpacity, &DragValue::valueChanged, this, &AnimationWidget::slotAdjustRectKeyframeValue);
        horLayout2->addWidget(m_spinOpacity);
    }

    // Build buttons
    m_originalSize = new QAction(QIcon::fromTheme(QStringLiteral("zoom-original")), i18n("Adjust to original size"), this);
    connect(m_originalSize, &QAction::triggered, this, &AnimationWidget::slotAdjustToSource);
    m_originalSize->setCheckable(true);
    QAction *adjustSize = new QAction(QIcon::fromTheme(QStringLiteral("zoom-fit-best")), i18n("Adjust and center in frame"), this);
    connect(adjustSize, &QAction::triggered, this, &AnimationWidget::slotAdjustToFrameSize);
    QAction *fitToWidth = new QAction(QIcon::fromTheme(QStringLiteral("zoom-fit-width")), i18n("Fit to width"), this);
    connect(fitToWidth, &QAction::triggered, this, &AnimationWidget::slotFitToWidth);
    QAction *fitToHeight = new QAction(QIcon::fromTheme(QStringLiteral("zoom-fit-height")), i18n("Fit to height"), this);
    connect(fitToHeight, &QAction::triggered, this, &AnimationWidget::slotFitToHeight);

    QAction *alignleft = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-align-left")), i18n("Align left"), this);
    connect(alignleft, &QAction::triggered, this, &AnimationWidget::slotMoveLeft);
    QAction *alignhcenter = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-align-hor")), i18n("Center horizontally"), this);
    connect(alignhcenter, &QAction::triggered, this, &AnimationWidget::slotCenterH);
    QAction *alignright = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-align-right")), i18n("Align right"), this);
    connect(alignright, &QAction::triggered, this, &AnimationWidget::slotMoveRight);
    QAction *aligntop = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-align-top")), i18n("Align top"), this);
    connect(aligntop, &QAction::triggered, this, &AnimationWidget::slotMoveTop);
    QAction *alignvcenter = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-align-vert")), i18n("Center vertically"), this);
    connect(alignvcenter, &QAction::triggered, this, &AnimationWidget::slotCenterV);
    QAction *alignbottom = new QAction(QIcon::fromTheme(QStringLiteral("kdenlive-align-bottom")), i18n("Align bottom"), this);
    connect(alignbottom, &QAction::triggered, this, &AnimationWidget::slotMoveBottom);

    auto *alignLayout = new QHBoxLayout;
    alignLayout->setSpacing(0);
    auto *alignButton = new QToolButton;
    alignButton->setDefaultAction(alignleft);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(alignhcenter);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(alignright);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(aligntop);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(alignvcenter);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(alignbottom);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(m_originalSize);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(adjustSize);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(fitToWidth);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);

    alignButton = new QToolButton;
    alignButton->setDefaultAction(fitToHeight);
    alignButton->setAutoRaise(true);
    alignLayout->addWidget(alignButton);
    alignLayout->addStretch(10);

    static_cast<QVBoxLayout *>(layout())->addLayout(horLayout);
    static_cast<QVBoxLayout *>(layout())->addLayout(alignLayout);
    static_cast<QVBoxLayout *>(layout())->addLayout(horLayout2);
    m_animController = m_animProperties.get_animation(paramTag.toUtf8().constData());
}

void AnimationWidget::slotUpdateVisibleParameter(bool display)
{
    if (!display) {
        return;
    }
    DoubleWidget *slider = qobject_cast<DoubleWidget *>(QObject::sender());
    if (slider) {
        if (slider->objectName() == m_inTimeline) {
            return;
        }
        m_inTimeline = slider->objectName();
        m_animController = m_animProperties.get_animation(m_inTimeline.toUtf8().constData());
        rebuildKeyframes();
        emit valueChanged(m_index, QString(m_animController.serialize_cut()), true);
    }
}

void AnimationWidget::slotAdjustKeyframeValue(double value)
{
    DoubleWidget *slider = qobject_cast<DoubleWidget *>(QObject::sender());
    if (!slider) {
        return;
    }
    m_inTimeline = slider->objectName();
    QModelIndex ix = slider->property("index").toModelIndex();
    m_animController = m_animProperties.get_animation(m_inTimeline.toUtf8().constData());

    int pos = m_ruler->position() - m_offset;
    mlt_keyframe_type type = m_selectType->isVisible() ? (m_selectType->isEnabled() ? (mlt_keyframe_type)m_selectType->currentAction()->data().toInt()
                                                                                    : (mlt_keyframe_type)KdenliveSettings::defaultkeyframeinterp())
                                                       : mlt_keyframe_linear;
    if (m_animController.is_key(pos)) {
        // This is a keyframe
        type = m_animController.keyframe_type(pos);
        m_animProperties.anim_set(m_inTimeline.toUtf8().constData(), value / slider->factor, pos, m_outPoint, type);
        emit valueChanged(ix, QString(m_animController.serialize_cut()), true);
    } else if (m_animController.key_count() <= 1) {
        pos = m_animController.key_get_frame(0);
        if (pos >= 0) {
            if (m_animController.is_key(pos)) {
                type = m_animController.keyframe_type(pos);
            }
            m_animProperties.anim_set(m_inTimeline.toUtf8().constData(), value / slider->factor, pos, m_outPoint, type);
            emit valueChanged(ix, QString(m_animController.serialize_cut()), true);
        }
    }
}

void AnimationWidget::slotAdjustRectKeyframeValue()
{
    m_animController = m_animProperties.get_animation(m_rectParameter.toUtf8().constData());
    m_inTimeline = m_rectParameter;
    int pos = m_ruler->position() - m_offset;
    mlt_rect rect;
    rect.x = m_spinX->value();
    rect.y = m_spinY->value();
    rect.w = m_spinWidth->value();
    rect.h = m_spinHeight->value();
    rect.o = m_spinOpacity ? m_spinOpacity->value() / 100.0 : DBL_MIN;

    double size;
    if (m_spinWidth->value() / pCore->getCurrentDar() > m_spinHeight->value()) {
        if (m_originalSize->isChecked()) {
            size = m_spinWidth->value() * 100.0 / m_frameSize.width();
        } else {
            size = m_spinWidth->value() * 100.0 / m_monitorSize.width();
        }
    } else {
        if (m_originalSize->isChecked()) {
            size = m_spinHeight->value() * 100.0 / m_frameSize.height();
        } else {
            size = m_spinHeight->value() * 100.0 / m_monitorSize.height();
        }
    }
    m_spinSize->blockSignals(true);
    m_spinSize->setValue(size);
    m_spinSize->blockSignals(false);
    if (m_animController.is_key(pos)) {
        // This is a keyframe
        m_animProperties.anim_set(m_rectParameter.toUtf8().constData(), rect, pos, m_outPoint,
                                  (mlt_keyframe_type)m_selectType->currentAction()->data().toInt());
        emit valueChanged(m_index, QString(m_animController.serialize_cut()), true);
        setupMonitor(QRect(rect.x, rect.y, rect.w, rect.h));
    } else if (m_animController.key_count() <= 1) {
        pos = m_animController.key_get_frame(0);
        if (pos >= 0) {
            m_animProperties.anim_set(m_rectParameter.toUtf8().constData(), rect, pos, m_outPoint,
                                      (mlt_keyframe_type)m_selectType->currentAction()->data().toInt());
            emit valueChanged(m_index, QString(m_animController.serialize_cut()), true);
            setupMonitor(QRect(rect.x, rect.y, rect.w, rect.h));
        }
    }
}

void AnimationWidget::slotResize(double value)
{
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    int w = m_originalSize->isChecked() ? m_frameSize.width() : m_monitorSize.width();
    int h = m_originalSize->isChecked() ? m_frameSize.height() : m_monitorSize.height();
    m_spinWidth->setValue(w * value / 100.0);
    m_spinHeight->setValue(h * value / 100.0);
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    slotAdjustRectKeyframeValue();
    setupMonitor();
}

bool AnimationWidget::isActive(const QString &name) const
{
    return name == m_inTimeline;
}

const QMap<QString, QString> AnimationWidget::getAnimation()
{
    QMap<QString, QString> animationResults;

    if (m_spinX) {
        m_animController = m_animProperties.get_animation(m_rectParameter.toUtf8().constData());
        // TODO: keyframes attached to end
        animationResults.insert(m_rectParameter, m_animController.serialize_cut());
    }

    QMapIterator<QString, DoubleWidget *> i(m_doubleWidgets);
    while (i.hasNext()) {
        i.next();
        m_animController = m_animProperties.get_animation(i.key().toUtf8().constData());
        // no negative keyframe trick, return simple serialize
        if (m_attachedToEnd == -2) {
            animationResults.insert(i.key(), m_animController.serialize_cut());
        } else {
            // Do processing ourselves to include negative values for keyframes relative to end
            int pos;
            mlt_keyframe_type type;
            QString key;
            QStringList result;
            int duration = m_outPoint;
            for (int j = 0; j < m_animController.key_count(); ++j) {
                m_animController.key_get(j, pos, type);
                double val = m_animProperties.anim_get_double(i.key().toUtf8().constData(), pos, duration);
                if (pos >= m_attachedToEnd) {
                    pos = qMin(pos - duration, -1);
                }
                key = QString::number(pos);
                switch (type) {
                case mlt_keyframe_discrete:
                    key.append(QStringLiteral("|="));
                    break;
                case mlt_keyframe_smooth:
                    key.append(QStringLiteral("~="));
                    break;
                default:
                    key.append(QStringLiteral("="));
                    break;
                }
                key.append(QString::number(val, 'f'));
                result << key;
            }
            animationResults.insert(i.key(), result.join(QLatin1Char(';')));
        }
    }
    // restore original controller
    m_animController = m_animProperties.get_animation(m_inTimeline.toUtf8().constData());
    return animationResults;
}

void AnimationWidget::slotReverseKeyframeType(bool reverse)
{
    int pos = m_timePos->getValue();
    if (m_animController.is_key(pos)) {
        if (reverse) {
            m_attachedToEnd = pos;
        } else {
            m_attachedToEnd = -2;
        }
        rebuildKeyframes();
        emit valueChanged(m_index, QString(m_animController.serialize_cut()), true);
    }
}

void AnimationWidget::loadPresets(QString currentText)
{
    m_presetCombo->blockSignals(true);
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
    if (currentText.isEmpty()) {
        currentText = m_presetCombo->currentText();
    }
    while (m_presetCombo->count() > 0) {
        m_presetCombo->removeItem(0);
    }
    m_presetCombo->removeItem(0);
    QMap<QString, QVariant> defaultEntry;
    QStringList paramNames = m_doubleWidgets.keys();
    for (int i = 0; i < paramNames.count(); i++) {
        defaultEntry.insert(paramNames.at(i), getDefaultKeyframes(m_offset, m_model->data(m_index, AssetParameterModel::DefaultRole).toString()));
    }
    m_presetCombo->addItem(i18n("Default"), defaultEntry);
    loadPreset(dir.absoluteFilePath(m_model->data(m_index, AssetParameterModel::TypeRole).toString()));
    loadPreset(dir.absoluteFilePath(m_effectId));
    if (!currentText.isEmpty()) {
        int ix = m_presetCombo->findText(currentText);
        if (ix >= 0) {
            m_presetCombo->setCurrentIndex(ix);
        }
    }
    m_presetCombo->blockSignals(false);
}

void AnimationWidget::loadPreset(const QString &path)
{
    KConfig confFile(path, KConfig::SimpleConfig);
    const QStringList groups = confFile.groupList();
    for (const QString &grp : groups) {
        QMap<QString, QString> entries = KConfigGroup(&confFile, grp).entryMap();
        QMap<QString, QVariant> variantEntries;
        QMapIterator<QString, QString> i(entries);
        while (i.hasNext()) {
            i.next();
            variantEntries.insert(i.key(), i.value());
        }
        m_presetCombo->addItem(grp, variantEntries);
    }
}

void AnimationWidget::applyPreset(int ix)
{
    QMap<QString, QVariant> entries = m_presetCombo->itemData(ix).toMap();
    QStringList presetNames = entries.keys();
    QStringList paramNames = m_doubleWidgets.keys();
    for (int i = 0; i < paramNames.count() && i < presetNames.count(); i++) {
        QString keyframes = entries.value(paramNames.at(i)).toString();
        if (!keyframes.isEmpty()) {
            m_animProperties.set(paramNames.at(i).toUtf8().constData(), keyframes.toUtf8().constData());
            // Required to initialize anim property
            m_animProperties.anim_get_int(m_inTimeline.toUtf8().constData(), 0, m_outPoint);
        }
    }
    if (!m_rectParameter.isEmpty()) {
        QString keyframes = entries.value(m_rectParameter).toString();
        if (!keyframes.isEmpty()) {
            m_animProperties.set(m_rectParameter.toUtf8().constData(), keyframes.toUtf8().constData());
            m_animProperties.anim_get_rect(m_rectParameter.toUtf8().constData(), 0, m_outPoint);
        }
    }
    m_animController = m_animProperties.get_animation(m_inTimeline.toUtf8().constData());
    rebuildKeyframes();
    emit valueChanged(m_index, QString(m_animController.serialize_cut()), true);
}

void AnimationWidget::savePreset()
{
    QDialog d(this);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
    auto *l = new QVBoxLayout;
    d.setLayout(l);
    QLineEdit effectName(&d);
    effectName.setPlaceholderText(i18n("Enter preset name"));
    QCheckBox cb(i18n("Save as global preset (available to all effects)"), &d);
    l->addWidget(&effectName);
    l->addWidget(&cb);
    l->addWidget(buttonBox);
    d.connect(buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    d.connect(buttonBox, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    if (d.exec() != QDialog::Accepted) {
        return;
    }
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    QString fileName = cb.isChecked() ? dir.absoluteFilePath(m_xml.attribute(QStringLiteral("type"))) : m_effectId;

    KConfig confFile(dir.absoluteFilePath(fileName), KConfig::SimpleConfig);
    KConfigGroup grp(&confFile, effectName.text());
    // Parse keyframes
    QMap<QString, QString> currentKeyframes = getAnimation();
    QMapIterator<QString, QString> i(currentKeyframes);
    while (i.hasNext()) {
        i.next();
        grp.writeEntry(i.key(), i.value());
    }
    confFile.sync();
    loadPresets(effectName.text());
}

void AnimationWidget::deletePreset()
{
    QString effectName = m_presetCombo->currentText();
    // try deleting as effect preset first
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/effects/presets/"));
    KConfig confFile(dir.absoluteFilePath(m_effectId), KConfig::SimpleConfig);
    KConfigGroup grp(&confFile, effectName);
    if (grp.exists()) {
    } else {
        // try global preset
        grp = KConfigGroup(&confFile, m_xml.attribute(QStringLiteral("type")));
    }
    grp.deleteGroup();
    confFile.sync();
    loadPresets();
}

void AnimationWidget::setActiveKeyframe(int frame)
{
    m_ruler->setActiveKeyframe(frame);
}

void AnimationWidget::slotUpdateGeometryRect(const QRect r)
{
    int pos = m_timePos->getValue();
    if (!m_animController.is_key(pos)) {
        // no keyframe
        if (m_animController.key_count() <= 1) {
            // Special case: only one keyframe, allow adjusting whatever the position is
            pos = m_animController.key_get_frame(0);
            if (pos < 0) {
                // error
                qCDebug(KDENLIVE_LOG) << "* * *NOT on a keyframe, something is wrong";
                return;
            }
        }
    }
    m_spinX->blockSignals(true);
    m_spinY->blockSignals(true);
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinX->setValue(r.x());
    m_spinY->setValue(r.y());
    m_spinWidth->setValue(r.width());
    m_spinHeight->setValue(r.height());
    m_spinX->blockSignals(false);
    m_spinY->blockSignals(false);
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    slotAdjustRectKeyframeValue();
    setupMonitor();
}

void AnimationWidget::slotUpdateCenters(const QVariantList &centers)
{
    if (centers.count() != m_animController.key_count()) {
        qCDebug(KDENLIVE_LOG) << "* * * *CENTER POINTS MISMATCH, aborting edit";
    }
    int pos;
    mlt_keyframe_type type;
    for (int i = 0; i < m_animController.key_count(); ++i) {
        m_animController.key_get(i, pos, type);
        mlt_rect rect = m_animProperties.anim_get_rect(m_rectParameter.toUtf8().constData(), pos, m_outPoint);
        // Center rect to new pos
        QPointF offset = centers.at(i).toPointF() - QPointF(rect.x + rect.w / 2, rect.y + rect.h / 2);
        rect.x += offset.x();
        rect.y += offset.y();
        m_animProperties.anim_set(m_rectParameter.toUtf8().constData(), rect, pos, m_outPoint, type);
    }
    slotAdjustRectKeyframeValue();
}

void AnimationWidget::setupMonitor(QRect r)
{
    QVariantList points;
    QVariantList types;
    int pos;
    mlt_keyframe_type type;
    for (int j = 0; j < m_animController.key_count(); ++j) {
        m_animController.key_get(j, pos, type);
        if (m_animController.key_get_type(j) == mlt_keyframe_smooth) {
            types << 1;
        } else {
            types << 0;
        }
        mlt_rect rect = m_animProperties.anim_get_rect(m_rectParameter.toUtf8().constData(), pos, m_outPoint);
        QRectF frameRect(rect.x, rect.y, rect.w, rect.h);
        points.append(QVariant(frameRect.center()));
    }
    if (m_active) {
        m_monitor->setUpEffectGeometry(r, points, types);
    }
}

void AnimationWidget::slotSeekToKeyframe(int ix)
{
    int pos = m_animController.key_get_frame(ix);
    slotPositionChanged(pos, true);
}

void AnimationWidget::connectMonitor(bool activate)
{
    if (m_active == activate) {
        return;
    }
    m_active = activate;
    if (!m_spinX) {
        // No animated rect displayed in monitor, return
        return;
    }
    if (activate) {
        connect(m_monitor, &Monitor::effectChanged, this, &AnimationWidget::slotUpdateGeometryRect, Qt::UniqueConnection);
        connect(m_monitor, &Monitor::effectPointsChanged, this, &AnimationWidget::slotUpdateCenters, Qt::UniqueConnection);
        connect(m_monitor, &Monitor::seekToKeyframe, this, &AnimationWidget::slotSeekToKeyframe, Qt::UniqueConnection);
        connect(m_monitor, &Monitor::seekToNextKeyframe, this, &AnimationWidget::slotNext, Qt::UniqueConnection);
        connect(m_monitor, &Monitor::seekToPreviousKeyframe, this, &AnimationWidget::slotPrevious, Qt::UniqueConnection);
        connect(m_monitor, SIGNAL(addKeyframe()), this, SLOT(slotAddKeyframe()), Qt::UniqueConnection);
        connect(m_monitor, SIGNAL(deleteKeyframe()), this, SLOT(slotDeleteKeyframe()), Qt::UniqueConnection);
        int framePos = qBound<int>(0, m_monitor->position() - m_inPoint, m_timePos->maximum());
        slotPositionChanged(framePos, false);
        double ratio = (double)m_spinWidth->value() / m_spinHeight->value();
        if (m_frameSize.width() != m_monitorSize.width() || m_frameSize.height() != m_monitorSize.height()) {
            // Source frame size different than project frame size, enable original size option accordingly
            bool isOriginalSize =
                qAbs((double)m_frameSize.width() / m_frameSize.height() - ratio) < qAbs((double)m_monitorSize.width() / m_monitorSize.height() - ratio);
            if (isOriginalSize) {
                m_originalSize->blockSignals(true);
                m_originalSize->setChecked(true);
                m_originalSize->blockSignals(false);
            }
        }
    } else {
        disconnect(m_monitor, &Monitor::effectChanged, this, &AnimationWidget::slotUpdateGeometryRect);
        disconnect(m_monitor, &Monitor::effectPointsChanged, this, &AnimationWidget::slotUpdateCenters);
        disconnect(m_monitor, SIGNAL(addKeyframe()), this, SLOT(slotAddKeyframe()));
        disconnect(m_monitor, SIGNAL(deleteKeyframe()), this, SLOT(slotDeleteKeyframe()));
        disconnect(m_monitor, &Monitor::seekToNextKeyframe, this, &AnimationWidget::slotNext);
        disconnect(m_monitor, &Monitor::seekToPreviousKeyframe, this, &AnimationWidget::slotPrevious);
        disconnect(m_monitor, &Monitor::seekToKeyframe, this, &AnimationWidget::slotSeekToKeyframe);
    }
}

void AnimationWidget::offsetAnimation(int offset)
{
    int pos = 0;
    mlt_keyframe_type type;
    QString offsetAnimation = QStringLiteral("kdenliveOffset");
    if (m_spinX) {
        m_animController = m_animProperties.get_animation(m_rectParameter.toUtf8().constData());
        for (int j = 0; j < m_animController.key_count(); ++j) {
            m_animController.key_get(j, pos, type);
            mlt_rect rect = m_animProperties.anim_get_rect(m_rectParameter.toUtf8().constData(), pos, m_outPoint);
            m_animProperties.anim_set(offsetAnimation.toUtf8().constData(), rect, pos + offset, m_outPoint, type);
        }
        Mlt::Animation offsetAnim = m_animProperties.get_animation(offsetAnimation.toUtf8().constData());
        m_animProperties.set(m_rectParameter.toUtf8().constData(), offsetAnim.serialize_cut());
        // Required to initialize anim property
        m_animProperties.anim_get_rect(m_rectParameter.toUtf8().constData(), 0, m_outPoint);
        m_animProperties.set(offsetAnimation.toUtf8().constData(), "");
    }

    QMapIterator<QString, DoubleWidget *> i(m_doubleWidgets);
    while (i.hasNext()) {
        i.next();
        m_animController = m_animProperties.get_animation(i.key().toUtf8().constData());
        for (int j = 0; j < m_animController.key_count(); ++j) {
            m_animController.key_get(j, pos, type);
            double val = m_animProperties.anim_get_double(i.key().toUtf8().constData(), pos, m_outPoint);
            m_animProperties.anim_set(offsetAnimation.toUtf8().constData(), val, pos + offset, m_outPoint, type);
        }
        Mlt::Animation offsetAnim = m_animProperties.get_animation(offsetAnimation.toUtf8().constData());
        m_animProperties.set(i.key().toUtf8().constData(), offsetAnim.serialize_cut());
        // Required to initialize anim property
        m_animProperties.anim_get_int(i.key().toUtf8().constData(), 0, m_outPoint);
        m_animProperties.set(offsetAnimation.toUtf8().constData(), "");
    }
    m_offset -= offset;
}

void AnimationWidget::reload(const QString &tag, const QString &data)
{
    m_animProperties.set(tag.toUtf8().constData(), data.toUtf8().constData());
    if (tag == m_rectParameter) {
        m_animProperties.anim_get_rect(tag.toUtf8().constData(), 0, m_outPoint);
    } else {
        m_animProperties.anim_get_int(tag.toUtf8().constData(), 0, m_outPoint);
        m_attachedToEnd = KeyframeView::checkNegatives(data, m_outPoint);
        m_inTimeline = tag;
    }
    // Also add keyframes positions in other parameters
    QStringList paramNames = m_doubleWidgets.keys();
    m_animController = m_animProperties.get_animation(tag.toUtf8().constData());
    for (int i = 0; i < paramNames.count(); i++) {
        const QString &currentParam = paramNames.at(i);
        if (currentParam == tag) {
            continue;
        }
        // simple anim parameter, get default value
        double def = m_model->data(m_index, AssetParameterModel::DefaultRole).toDouble();
        // Clear current keyframes
        m_animProperties.set(currentParam.toUtf8().constData(), "");
        // Add default keyframes
        int pos;
        mlt_keyframe_type type;
        for (int j = 0; j < m_animController.key_count(); ++j) {
            m_animController.key_get(j, pos, type);
            m_animProperties.anim_set(currentParam.toUtf8().constData(), def, pos, m_outPoint, type);
        }
        m_animProperties.anim_get_int(currentParam.toUtf8().constData(), 0, m_outPoint);
    }
    if (!m_rectParameter.isEmpty() && tag != m_rectParameter) {
        // reset geometry keyframes
        // simple anim parameter, get default value
        QString def = m_model->data(m_index, AssetParameterModel::DefaultRole).toString();
        // Clear current keyframes
        m_animProperties.set(m_rectParameter.toUtf8().constData(), def.toUtf8().constData());
        // Add default keyframes
        int pos;
        mlt_keyframe_type type;
        m_animProperties.anim_get_rect(m_rectParameter.toUtf8().constData(), 0, m_outPoint);
        mlt_rect rect = m_animProperties.anim_get_rect(m_rectParameter.toUtf8().constData(), 0, m_outPoint);
        for (int j = 0; j < m_animController.key_count(); ++j) {
            m_animController.key_get(j, pos, type);
            m_animProperties.anim_set(m_rectParameter.toUtf8().constData(), rect, pos, m_outPoint, type);
        }
        m_animProperties.anim_get_rect(m_rectParameter.toUtf8().constData(), 0, m_outPoint);
    }
    rebuildKeyframes();
    emit valueChanged(m_index, QString(m_animController.serialize_cut()), true);
}

QString AnimationWidget::defaultValue(const QString &paramName)
{
    QStringList paramNames = m_doubleWidgets.keys();
    for (int i = 0; i < paramNames.count(); i++) {
        if (m_params.at(i).attribute(QStringLiteral("name")) == paramName) {
            QString def = m_params.at(i).attribute(QStringLiteral("default"));
            if (def.contains(QLatin1Char('%'))) {
                def = EffectsController::getStringRectEval(def);
            }
            return def;
        }
    }
    return QString();
}

void AnimationWidget::slotAdjustToSource()
{
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinWidth->setValue((int)(m_frameSize.width() / pCore->getCurrentSar() + 0.5), false);
    m_spinHeight->setValue(m_frameSize.height(), false);
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    slotAdjustRectKeyframeValue();
    if (m_lockRatio->isChecked()) {
        m_monitor->setEffectSceneProperty(QStringLiteral("lockratio"), m_originalSize->isChecked() ? (double)m_frameSize.width() / m_frameSize.height()
                                                                                                   : (double)m_monitorSize.width() / m_monitorSize.height());
    }
}

void AnimationWidget::slotAdjustToFrameSize()
{
    double monitorDar = pCore->getCurrentDar();
    double sourceDar = m_frameSize.width() / m_frameSize.height();
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    if (sourceDar > monitorDar) {
        // Fit to width
        double factor = (double)m_monitorSize.width() / m_frameSize.width() * pCore->getCurrentSar();
        m_spinHeight->setValue((int)(m_frameSize.height() * factor + 0.5));
        m_spinWidth->setValue(m_monitorSize.width());
        // Center
        m_spinY->blockSignals(true);
        m_spinY->setValue((m_monitorSize.height() - m_spinHeight->value()) / 2);
        m_spinY->blockSignals(false);
    } else {
        // Fit to height
        double factor = (double)m_monitorSize.height() / m_frameSize.height();
        m_spinHeight->setValue(m_monitorSize.height());
        m_spinWidth->setValue((int)(m_frameSize.width() / pCore->getCurrentSar() * factor + 0.5));
        // Center
        m_spinX->blockSignals(true);
        m_spinX->setValue((m_monitorSize.width() - m_spinWidth->value()) / 2);
        m_spinX->blockSignals(false);
    }
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    slotAdjustRectKeyframeValue();
}

void AnimationWidget::slotFitToWidth()
{
    double factor = (double)m_monitorSize.width() / m_frameSize.width() * pCore->getCurrentSar();
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinHeight->setValue((int)(m_frameSize.height() * factor + 0.5));
    m_spinWidth->setValue(m_monitorSize.width());
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    slotAdjustRectKeyframeValue();
}

void AnimationWidget::slotFitToHeight()
{
    double factor = (double)m_monitorSize.height() / m_frameSize.height();
    m_spinWidth->blockSignals(true);
    m_spinHeight->blockSignals(true);
    m_spinHeight->setValue(m_monitorSize.height());
    m_spinWidth->setValue((int)(m_frameSize.width() / pCore->getCurrentSar() * factor + 0.5));
    m_spinWidth->blockSignals(false);
    m_spinHeight->blockSignals(false);
    slotAdjustRectKeyframeValue();
}

void AnimationWidget::slotMoveLeft()
{
    m_spinX->setValue(0);
}

void AnimationWidget::slotCenterH()
{
    m_spinX->setValue((m_monitorSize.width() - m_spinWidth->value()) / 2);
}

void AnimationWidget::slotMoveRight()
{
    m_spinX->setValue(m_monitorSize.width() - m_spinWidth->value());
}

void AnimationWidget::slotMoveTop()
{
    m_spinY->setValue(0);
}

void AnimationWidget::slotCenterV()
{
    m_spinY->setValue((m_monitorSize.height() - m_spinHeight->value()) / 2);
}

void AnimationWidget::slotMoveBottom()
{
    m_spinY->setValue(m_monitorSize.height() - m_spinHeight->value());
}

void AnimationWidget::slotCopyKeyframes()
{
    const QMap<QString, QString> anims = getAnimation();
    if (anims.isEmpty()) {
        return;
    }
    QString result;
    QMapIterator<QString, QString> i(anims);
    while (i.hasNext()) {
        i.next();
        result.append(i.key() + QLatin1Char('=') + i.value() + QLatin1Char('\n'));
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(result);
}

void AnimationWidget::slotImportKeyframes()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString values = clipboard->text();
    emit setKeyframes(values);
}

void AnimationWidget::slotLockRatio()
{
    QAction *lockRatio = qobject_cast<QAction *>(QObject::sender());
    if (lockRatio->isChecked()) {
        m_monitor->setEffectSceneProperty(QStringLiteral("lockratio"), m_originalSize->isChecked() ? (double)m_frameSize.width() / m_frameSize.height()
                                                                                                   : (double)m_monitorSize.width() / m_monitorSize.height());
    } else {
        m_monitor->setEffectSceneProperty(QStringLiteral("lockratio"), -1);
    }
}

void AnimationWidget::slotAdjustRectWidth()
{
    if (m_lockRatio->isChecked()) {
        m_spinHeight->blockSignals(true);
        if (m_originalSize->isChecked()) {
            m_spinHeight->setValue((int)(m_spinWidth->value() * m_frameSize.height() / m_frameSize.width() + 0.5));
        } else {
            m_spinHeight->setValue((int)(m_spinWidth->value() * m_monitorSize.height() / m_monitorSize.width() + 0.5));
        }
        m_spinHeight->blockSignals(false);
    }
    slotAdjustRectKeyframeValue();
}

void AnimationWidget::slotAdjustRectHeight()
{
    if (m_lockRatio->isChecked()) {
        m_spinWidth->blockSignals(true);
        if (m_originalSize->isChecked()) {
            m_spinWidth->setValue((int)(m_spinHeight->value() * m_frameSize.width() / m_frameSize.height() + 0.5));
        } else {
            m_spinWidth->setValue((int)(m_spinHeight->value() * m_monitorSize.width() / m_monitorSize.height() + 0.5));
        }
        m_spinWidth->blockSignals(false);
    }
    slotAdjustRectKeyframeValue();
}

void AnimationWidget::monitorSeek(int pos)
{
    slotPositionChanged(pos - m_inPoint, false);
    if (m_spinX) {
        // Update monitor scene for geometry params
        if (pos > m_inPoint && pos < m_outPoint) {
            if (!m_active) {
                connectMonitor(true);
                m_monitor->slotShowEffectScene(MonitorSceneGeometry);
            }
        } else {
            if (m_active) {
                connectMonitor(false);
                m_monitor->slotShowEffectScene(MonitorSceneDefault);
            }
        }
    }
}

void AnimationWidget::slotShowComment(bool show) {}

void AnimationWidget::slotRefresh()
{
    for (int i = 0; i < m_parameters.count(); i++) {
        QModelIndex ix = m_parameters.at(i).first;
        ParamType type = (ParamType)m_model->data(ix, AssetParameterModel::TypeRole).toInt();
        QString keyframes = m_model->data(ix, AssetParameterModel::ValueRole).toString();
        QString paramTag = m_model->data(ix, AssetParameterModel::NameRole).toString();
        m_animProperties.set(paramTag.toUtf8().constData(), keyframes.toUtf8().constData());
        m_attachedToEnd = KeyframeView::checkNegatives(keyframes, m_outPoint);
        if (type == ParamType::Animated || type == ParamType::RestrictedAnim) {
            // one dimension parameter
            // Required to initialize anim property
            m_animProperties.anim_get_int(paramTag.toUtf8().constData(), 0, m_outPoint);
        } else {
            // rect parameter
            // TODO: multiple rect parameters in effect ?
            m_rectParameter = paramTag;
            m_inTimeline = paramTag;
            // Required to initialize anim property
            m_animProperties.anim_get_rect(paramTag.toUtf8().constData(), 0, m_outPoint);
        }
    }
    rebuildKeyframes();
    monitorSeek(m_monitor->position());
}

void AnimationWidget::slotSetRange(QPair<int, int> range)
{
    m_inPoint = range.first;
    m_outPoint = range.second;
    m_offset = m_model->data(m_index, AssetParameterModel::InRole).toInt();
    m_ruler->setRange(0, m_outPoint - m_inPoint);
    m_timePos->setRange(0, m_outPoint - m_inPoint - 1);
}
