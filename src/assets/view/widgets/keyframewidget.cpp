/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   Copyright (C) 2017 by Nicolas Carion                                  *
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

#include "keyframewidget.hpp"
#include "assets/keyframes/model/corners/cornershelper.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/keyframes/model/rotoscoping/rotohelper.hpp"
#include "assets/keyframes/model/keyframemonitorhelper.hpp"
#include "assets/keyframes/view/keyframeview.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "assets/view/widgets/keyframeimport.h"
#include "core.h"
#include "kdenlivesettings.h"
#include "monitor/monitor.h"
#include "timecode.h"
#include "timecodedisplay.h"

#include "widgets/doublewidget.h"
#include "widgets/geometrywidget.h"

#include <KSelectAction>
#include <QApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QMenu>
#include <QPointer>
#include <QToolButton>
#include <QVBoxLayout>
#include <klocalizedstring.h>
#include <utility>

KeyframeWidget::KeyframeWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QSize frameSize, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_monitorHelper(nullptr)
    , m_neededScene(MonitorSceneType::MonitorSceneDefault)
    , m_sourceFrameSize(frameSize.isValid() && !frameSize.isNull() ? frameSize : pCore->getCurrentFrameSize())
    , m_baseHeight(0)
    , m_addedHeight(0)
    , m_effectIsSelected(false)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_lay = new QVBoxLayout(this);
    m_lay->setSpacing(0);

    bool ok = false;
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    Q_ASSERT(ok);
    m_model->prepareKeyframes();
    m_keyframes = m_model->getKeyframeModel();
    m_keyframeview = new KeyframeView(m_keyframes, duration, this);

    m_buttonAddDelete = new QToolButton(this);
    m_buttonAddDelete->setAutoRaise(true);
    m_buttonAddDelete->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    m_buttonAddDelete->setToolTip(i18n("Add keyframe"));

    m_buttonPrevious = new QToolButton(this);
    m_buttonPrevious->setAutoRaise(true);
    m_buttonPrevious->setIcon(QIcon::fromTheme(QStringLiteral("media-skip-backward")));
    m_buttonPrevious->setToolTip(i18n("Go to previous keyframe"));

    m_buttonNext = new QToolButton(this);
    m_buttonNext->setAutoRaise(true);
    m_buttonNext->setIcon(QIcon::fromTheme(QStringLiteral("media-skip-forward")));
    m_buttonNext->setToolTip(i18n("Go to next keyframe"));
    // Keyframe type widget
    m_selectType = new KSelectAction(QIcon::fromTheme(QStringLiteral("keyframes")), i18n("Keyframe interpolation"), this);
    QAction *linear = new QAction(QIcon::fromTheme(QStringLiteral("linear")), i18n("Linear"), this);
    linear->setData((int)mlt_keyframe_linear);
    linear->setCheckable(true);
    m_selectType->addAction(linear);
    QAction *discrete = new QAction(QIcon::fromTheme(QStringLiteral("discrete")), i18n("Discrete"), this);
    discrete->setData((int)mlt_keyframe_discrete);
    discrete->setCheckable(true);
    m_selectType->addAction(discrete);
    QAction *curve = new QAction(QIcon::fromTheme(QStringLiteral("smooth")), i18n("Smooth"), this);
    curve->setData((int)mlt_keyframe_smooth);
    curve->setCheckable(true);
    m_selectType->addAction(curve);
    m_selectType->setCurrentAction(linear);
    connect(m_selectType, static_cast<void (KSelectAction::*)(QAction *)>(&KSelectAction::triggered), this, &KeyframeWidget::slotEditKeyframeType);
    m_selectType->setToolBarMode(KSelectAction::ComboBoxMode);
    m_toolbar = new QToolBar(this);

    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    connect(monitor, &Monitor::seekPosition, this, &KeyframeWidget::monitorSeek, Qt::UniqueConnection);
    connect(monitor, &Monitor::seekToKeyframe, this, &KeyframeWidget::slotSeekToKeyframe, Qt::UniqueConnection);
    m_time = new TimecodeDisplay(pCore->timecode(), this);
    m_time->setRange(0, duration - 1);

    m_toolbar->addWidget(m_buttonPrevious);
    m_toolbar->addWidget(m_buttonAddDelete);
    m_toolbar->addWidget(m_buttonNext);
    m_toolbar->addAction(m_selectType);

    // copy/paste keyframes from clipboard
    QAction *copy = new QAction(i18n("Copy keyframes to clipboard"), this);
    connect(copy, &QAction::triggered, this, &KeyframeWidget::slotCopyKeyframes);
    QAction *paste = new QAction(i18n("Import keyframes from clipboard"), this);
    connect(paste, &QAction::triggered, this, &KeyframeWidget::slotImportKeyframes);
    // Remove keyframes
    QAction *removeNext = new QAction(i18n("Remove all keyframes after cursor"), this);
    connect(removeNext, &QAction::triggered, this, &KeyframeWidget::slotRemoveNextKeyframes);

    // Default kf interpolation
    KSelectAction *kfType = new KSelectAction(i18n("Default keyframe type"), this);
    QAction *discrete2 = new QAction(QIcon::fromTheme(QStringLiteral("discrete")), i18n("Discrete"), this);
    discrete2->setData((int)mlt_keyframe_discrete);
    discrete2->setCheckable(true);
    kfType->addAction(discrete2);
    QAction *linear2 = new QAction(QIcon::fromTheme(QStringLiteral("linear")), i18n("Linear"), this);
    linear2->setData((int)mlt_keyframe_linear);
    linear2->setCheckable(true);
    kfType->addAction(linear2);
    QAction *curve2 = new QAction(QIcon::fromTheme(QStringLiteral("smooth")), i18n("Smooth"), this);
    curve2->setData((int)mlt_keyframe_smooth);
    curve2->setCheckable(true);
    kfType->addAction(curve2);
    switch (KdenliveSettings::defaultkeyframeinterp()) {
    case mlt_keyframe_discrete:
        kfType->setCurrentAction(discrete2);
        break;
    case mlt_keyframe_smooth:
        kfType->setCurrentAction(curve2);
        break;
    default:
        kfType->setCurrentAction(linear2);
        break;
    }
    connect(kfType, static_cast<void (KSelectAction::*)(QAction *)>(&KSelectAction::triggered),
            this, [&](QAction *ac) { KdenliveSettings::setDefaultkeyframeinterp(ac->data().toInt()); });
    auto *container = new QMenu(this);
    container->addAction(copy);
    container->addAction(paste);
    container->addSeparator();
    container->addAction(kfType);
    container->addAction(removeNext);

    // Menu toolbutton
    auto *menuButton = new QToolButton(this);
    menuButton->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-menu")));
    menuButton->setToolTip(i18n("Options"));
    menuButton->setMenu(container);
    menuButton->setPopupMode(QToolButton::InstantPopup);
    m_toolbar->addWidget(menuButton);
    m_toolbar->addWidget(m_time);

    m_lay->addWidget(m_keyframeview);
    m_lay->addWidget(m_toolbar);
    monitorSeek(monitor->position());

    connect(m_time, &TimecodeDisplay::timeCodeEditingFinished, this, [&]() { slotSetPosition(-1, true); });
    connect(m_keyframeview, &KeyframeView::seekToPos, this, [&](int p) { slotSetPosition(p, true); });
    connect(m_keyframeview, &KeyframeView::atKeyframe, this, &KeyframeWidget::slotAtKeyframe);
    connect(m_keyframeview, &KeyframeView::modified, this, &KeyframeWidget::slotRefreshParams);
    connect(m_keyframeview, &KeyframeView::activateEffect, this, &KeyframeWidget::activateEffect);

    connect(m_buttonAddDelete, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotAddRemove);
    connect(m_buttonPrevious, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotGoToPrev);
    connect(m_buttonNext, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotGoToNext);
    //m_baseHeight = m_keyframeview->height() + m_selectType->defaultWidget()->sizeHint().height();
    QMargins mrg = m_lay->contentsMargins();
    m_baseHeight = m_keyframeview->height() + m_toolbar->sizeHint().height() + mrg.top() + mrg.bottom();
    setFixedHeight(m_baseHeight);
    addParameter(index);

    connect(monitor, &Monitor::seekToNextKeyframe, m_keyframeview, &KeyframeView::slotGoToNext, Qt::UniqueConnection);
    connect(monitor, &Monitor::seekToPreviousKeyframe, m_keyframeview, &KeyframeView::slotGoToPrev, Qt::UniqueConnection);
    connect(monitor, &Monitor::addRemoveKeyframe, m_keyframeview, &KeyframeView::slotAddRemove, Qt::UniqueConnection);
}

KeyframeWidget::~KeyframeWidget()
{
    delete m_keyframeview;
    delete m_buttonAddDelete;
    delete m_buttonPrevious;
    delete m_buttonNext;
    delete m_time;
}

void KeyframeWidget::monitorSeek(int pos)
{
    int in = pCore->getItemPosition(m_model->getOwnerId());
    int out = in + pCore->getItemDuration(m_model->getOwnerId());
    bool isInRange = pos >= in && pos < out;
    m_buttonAddDelete->setEnabled(isInRange && pos > in);
    connectMonitor(isInRange);
    int framePos = qBound(in, pos, out) - in;
    if (isInRange && framePos != m_time->getValue()) {
        slotSetPosition(framePos, false);
    }
}

void KeyframeWidget::slotEditKeyframeType(QAction *action)
{
    int type = action->data().toInt();
    m_keyframeview->slotEditType(type, m_index);
    emit activateEffect();
}

void KeyframeWidget::slotRefreshParams()
{
    int pos = getPosition();
    KeyframeType keyType = m_keyframes->keyframeType(GenTime(pos, pCore->getCurrentFps()));
    int i = 0;
    while (auto ac = m_selectType->action(i)) {
        if (ac->data().toInt() == (int)keyType) {
            m_selectType->setCurrentItem(i);
            break;
        }
        i++;
    }
    for (const auto &w : m_parameters) {
        auto type = m_model->data(w.first, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::KeyframeParam) {
            ((DoubleWidget *)w.second)->setValue(m_keyframes->getInterpolatedValue(pos, w.first).toDouble());
        } else if (type == ParamType::AnimatedRect) {
            const QString val = m_keyframes->getInterpolatedValue(pos, w.first).toString();
            const QStringList vals = val.split(QLatin1Char(' '));
            QRect rect;
            double opacity = -1;
            if (vals.count() >= 4) {
                rect = QRect(vals.at(0).toInt(), vals.at(1).toInt(), vals.at(2).toInt(), vals.at(3).toInt());
                if (vals.count() > 4) {
                    opacity = vals.at(4).toDouble();
                }
            }
            ((GeometryWidget *)w.second)->setValue(rect, opacity);
        }
    }
    if (m_monitorHelper && m_effectIsSelected) {
        m_monitorHelper->refreshParams(pos);
        return;
    }
}
void KeyframeWidget::slotSetPosition(int pos, bool update)
{
    if (pos < 0) {
        pos = m_time->getValue();
        m_keyframeview->slotSetPosition(pos, true);
    } else {
        m_time->setValue(pos);
        m_keyframeview->slotSetPosition(pos, true);
    }
    m_buttonAddDelete->setEnabled(pos > 0);
    slotRefreshParams();

    if (update) {
        emit seekToPos(pos);
    }
}

int KeyframeWidget::getPosition() const
{
    return m_time->getValue() + pCore->getItemIn(m_model->getOwnerId());
}

void KeyframeWidget::addKeyframe(int pos)
{
    blockSignals(true);
    m_keyframeview->slotAddKeyframe(pos);
    blockSignals(false);
    setEnabled(true);
}

void KeyframeWidget::updateTimecodeFormat()
{
    m_time->slotUpdateTimeCodeFormat();
}

void KeyframeWidget::slotAtKeyframe(bool atKeyframe, bool singleKeyframe)
{
    if (atKeyframe) {
        m_buttonAddDelete->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
        m_buttonAddDelete->setToolTip(i18n("Delete keyframe"));
    } else {
        m_buttonAddDelete->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
        m_buttonAddDelete->setToolTip(i18n("Add keyframe"));
    }
    pCore->getMonitor(m_model->monitorId)->setEffectKeyframe(atKeyframe || singleKeyframe);
    m_selectType->setEnabled(atKeyframe || singleKeyframe);
    for (const auto &w : m_parameters) {
        w.second->setEnabled(atKeyframe || singleKeyframe);
    }
}

void KeyframeWidget::slotRefresh()
{
    // update duration
    bool ok = false;
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    Q_ASSERT(ok);
    // m_model->dataChanged(QModelIndex(), QModelIndex());
    //->getKeyframeModel()->getKeyModel(m_index)->dataChanged(QModelIndex(), QModelIndex());
    m_keyframeview->setDuration(duration);
    m_time->setRange(0, duration - 1);
    slotRefreshParams();
}

void KeyframeWidget::resetKeyframes()
{
    // update duration
    bool ok = false;
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    Q_ASSERT(ok);
    // reset keyframes
    m_keyframes->refresh();
    // m_model->dataChanged(QModelIndex(), QModelIndex());
    m_keyframeview->setDuration(duration);
    m_time->setRange(0, duration - 1);
    slotRefreshParams();
}

void KeyframeWidget::addParameter(const QPersistentModelIndex &index)
{
    // Retrieve parameters from the model
    QString name = m_model->data(index, Qt::DisplayRole).toString();
    QString comment = m_model->data(index, AssetParameterModel::CommentRole).toString();
    QString suffix = m_model->data(index, AssetParameterModel::SuffixRole).toString();

    auto type = m_model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
    // Construct object
    QWidget *paramWidget = nullptr;
    if (type == ParamType::AnimatedRect) {
        m_neededScene = MonitorSceneType::MonitorSceneGeometry;
        int inPos = m_model->data(index, AssetParameterModel::ParentInRole).toInt();
        QPair<int, int> range(inPos, inPos + m_model->data(index, AssetParameterModel::ParentDurationRole).toInt());
        const QString value = m_keyframes->getInterpolatedValue(getPosition(), index).toString();
        m_monitorHelper = new KeyframeMonitorHelper(pCore->getMonitor(m_model->monitorId), m_model, index, this);
        QRect rect;
        double opacity = 0;
        QStringList vals = value.split(QLatin1Char(' '));
        if (vals.count() > 3) {
            rect = QRect(vals.at(0).toInt(), vals.at(1).toInt(), vals.at(2).toInt(), vals.at(3).toInt());
            if (vals.count() > 4) {
                opacity = vals.at(4).toDouble();
            }
        }
        // qtblend uses an opacity value in the (0-1) range, while older geometry effects use (0-100)
        bool integerOpacity = m_model->getAssetId() != QLatin1String("qtblend");
        GeometryWidget *geomWidget = new GeometryWidget(pCore->getMonitor(m_model->monitorId), range, rect, opacity, m_sourceFrameSize, false,
                                                        m_model->data(m_index, AssetParameterModel::OpacityRole).toBool(), integerOpacity, this);
        connect(geomWidget, &GeometryWidget::valueChanged,
                this, [this, index](const QString v) {
                    emit activateEffect();
                    m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(v), index); });
        paramWidget = geomWidget;
    } else if (type == ParamType::Roto_spline) {
        m_monitorHelper = new RotoHelper(pCore->getMonitor(m_model->monitorId), m_model, index, this);
        connect(m_monitorHelper, &KeyframeMonitorHelper::updateKeyframeData, this, &KeyframeWidget::slotUpdateKeyframesFromMonitor, Qt::UniqueConnection);
        m_neededScene = MonitorSceneType::MonitorSceneRoto;
    } else {
        if (m_model->getAssetId() == QLatin1String("frei0r.c0rners")) {
            if (m_neededScene == MonitorSceneDefault && !m_monitorHelper) {
                m_neededScene = MonitorSceneType::MonitorSceneCorners;
                m_monitorHelper = new CornersHelper(pCore->getMonitor(m_model->monitorId), m_model, index, this);
                connect(m_monitorHelper, &KeyframeMonitorHelper::updateKeyframeData, this, &KeyframeWidget::slotUpdateKeyframesFromMonitor,
                        Qt::UniqueConnection);
                connect(this, &KeyframeWidget::addIndex, m_monitorHelper, &CornersHelper::addIndex);
            } else {
                if (type == ParamType::KeyframeParam) {
                    int paramName = m_model->data(index, AssetParameterModel::NameRole).toInt();
                    if (paramName < 8) {
                        emit addIndex(index);
                    }
                }
            }
        }
        double value = m_keyframes->getInterpolatedValue(getPosition(), index).toDouble();
        double min = m_model->data(index, AssetParameterModel::MinRole).toDouble();
        double max = m_model->data(index, AssetParameterModel::MaxRole).toDouble();
        double defaultValue = m_model->data(index, AssetParameterModel::DefaultRole).toDouble();
        int decimals = m_model->data(index, AssetParameterModel::DecimalsRole).toInt();
        double factor = m_model->data(index, AssetParameterModel::FactorRole).toDouble();
        factor = qFuzzyIsNull(factor) ? 1 : factor;
        auto doubleWidget = new DoubleWidget(name, value, min, max, factor, defaultValue, comment, -1, suffix, decimals, m_model->data(index, AssetParameterModel::OddRole).toBool(), this);
        connect(doubleWidget, &DoubleWidget::valueChanged,
                this, [this, index](double v) {
            emit activateEffect();
            m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(v), index);
        });
        paramWidget = doubleWidget;
    }
    if (paramWidget) {
        m_parameters[index] = paramWidget;
        m_lay->addWidget(paramWidget);
        m_addedHeight += paramWidget->minimumHeight();
        setFixedHeight(m_baseHeight + m_addedHeight);
    }
}

void KeyframeWidget::slotInitMonitor(bool active)
{
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    m_effectIsSelected = active;
    if (m_keyframeview) {
        m_keyframeview->initKeyframePos();
        connect(monitor, &Monitor::updateScene, m_keyframeview, &KeyframeView::slotModelChanged, Qt::UniqueConnection);
    }
    connectMonitor(active);
}

void KeyframeWidget::connectMonitor(bool active)
{
    if (m_monitorHelper) {
        if (m_monitorHelper->connectMonitor(active) && m_effectIsSelected) {
            slotRefreshParams();
        }
    }
    for (const auto &w : m_parameters) {
        auto type = m_model->data(w.first, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::AnimatedRect) {
            ((GeometryWidget *)w.second)->connectMonitor(active);
            break;
        }
    }
}

void KeyframeWidget::slotUpdateKeyframesFromMonitor(const QPersistentModelIndex &index, const QVariant &res)
{
    emit activateEffect();
    if (m_keyframes->isEmpty()) {
        GenTime pos(pCore->getItemIn(m_model->getOwnerId()) + m_time->getValue(), pCore->getCurrentFps());
        if (m_time->getValue() > 0) {
            GenTime pos0(pCore->getItemIn(m_model->getOwnerId()), pCore->getCurrentFps());
            m_keyframes->addKeyframe(pos0, KeyframeType::Linear);
            m_keyframes->updateKeyframe(pos0, res, index);
        }
        m_keyframes->addKeyframe(pos, KeyframeType::Linear);
        m_keyframes->updateKeyframe(pos, res, index);
    } else if (m_keyframes->hasKeyframe(getPosition()) || m_keyframes->singleKeyframe()) {
        GenTime pos(getPosition(), pCore->getCurrentFps());
        if (m_keyframes->singleKeyframe() && KdenliveSettings::autoKeyframe() && m_neededScene == MonitorSceneType::MonitorSceneRoto) {
            m_keyframes->addKeyframe(pos, KeyframeType::Linear);
        }
        m_keyframes->updateKeyframe(pos, res, index);
    }
}

MonitorSceneType KeyframeWidget::requiredScene() const
{
    qDebug() << "// // // RESULTING REQUIRED SCENE: " << m_neededScene;
    return m_neededScene;
}

bool KeyframeWidget::keyframesVisible() const
{
    return m_keyframeview->isVisible();
}

void KeyframeWidget::showKeyframes(bool enable)
{
    if (enable && m_toolbar->isVisible()) {
        return;
    }
    m_toolbar->setVisible(enable);
    m_keyframeview->setVisible(enable);
    setFixedHeight(m_addedHeight + (enable ? m_baseHeight : 0));
}

void KeyframeWidget::slotCopyKeyframes()
{
    QJsonDocument effectDoc = m_model->toJson(false);
    if (effectDoc.isEmpty()) {
        return;
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(QString(effectDoc.toJson()));
}

void KeyframeWidget::slotImportKeyframes()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString values = clipboard->text();
    QList<QPersistentModelIndex> indexes;
    for (const auto &w : m_parameters) {
        indexes << w.first;
    }
    QPointer<KeyframeImport> import = new KeyframeImport(values, m_model, indexes, m_model->data(m_index, AssetParameterModel::ParentInRole).toInt(), m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(), this);
    if (import->exec() != QDialog::Accepted) {
        delete import;
        return;
    }
    import->importSelectedData();

    /*m_model->getKeyframeModel()->getKeyModel()->dataChanged(QModelIndex(), QModelIndex());*/
    /*m_model->modelChanged();
    qDebug()<<"//// UPDATING KEYFRAMES CORE---------";
    pCore->updateItemKeyframes(m_model->getOwnerId());*/
    delete import;
}

void KeyframeWidget::slotRemoveNextKeyframes()
{
    int pos = m_time->getValue() + m_model->data(m_index, AssetParameterModel::ParentInRole).toInt();
    m_keyframes->removeNextKeyframes(GenTime(pos, pCore->getCurrentFps()));
}


void KeyframeWidget::slotSeekToKeyframe(int ix)
{
    int pos = m_keyframes->getPosAtIndex(ix).frames(pCore->getCurrentFps());
    slotSetPosition(pos, true);
}
