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
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/keyframes/model/rotoscoping/rotohelper.hpp"
#include "assets/keyframes/model/corners/cornershelper.hpp"
#include "assets/keyframes/view/keyframeview.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "assets/model/assetcommand.hpp"
#include "assets/view/widgets/keyframeimport.h"
#include "core.h"
#include "monitor/monitor.h"
#include "timecode.h"
#include "timecodedisplay.h"

#include "widgets/doublewidget.h"
#include "widgets/geometrywidget.h"

#include <KSelectAction>
#include <QToolButton>
#include <QApplication>
#include <QClipboard>
#include <QVBoxLayout>
#include <QMenu>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <klocalizedstring.h>

KeyframeWidget::KeyframeWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(model, index, parent)
    , m_keyframes(model->getKeyframeModel())
    , m_monitorHelper(nullptr)
    , m_neededScene(MonitorSceneType::MonitorSceneDefault)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_lay = new QVBoxLayout(this);
    m_lay->setContentsMargins(2, 2, 2, 0);
    m_lay->setSpacing(0);

    bool ok = false;
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    Q_ASSERT(ok);
    m_keyframeview = new KeyframeView(m_keyframes, this);
    m_keyframeview->setDuration(duration);

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
    m_time = new TimecodeDisplay(monitor->timecode(), this);
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
    auto *container = new QMenu(this);
    container->addAction(copy);
    container->addAction(paste);

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
    // slotSetPosition(0, false);
    monitorSeek(monitor->position());

    connect(m_time, &TimecodeDisplay::timeCodeEditingFinished, [&]() { slotSetPosition(-1, true); });
    connect(m_keyframeview, &KeyframeView::seekToPos, [&](int p) { slotSetPosition(p, true); });
    connect(m_keyframeview, &KeyframeView::atKeyframe, this, &KeyframeWidget::slotAtKeyframe);
    connect(m_keyframeview, &KeyframeView::modified, this, &KeyframeWidget::slotRefreshParams);

    connect(m_buttonAddDelete, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotAddRemove);
    connect(m_buttonPrevious, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotGoToPrev);
    connect(m_buttonNext, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotGoToNext);
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
    m_buttonAddDelete->setEnabled(isInRange);
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
}

void KeyframeWidget::slotRefreshParams()
{
    int pos = getPosition();
    KeyframeType keyType = m_keyframes->keyframeType(GenTime(pos, pCore->getCurrentFps()));
    m_selectType->setCurrentItem((int)keyType);
    for (const auto &w : m_parameters) {
        ParamType type = m_model->data(w.first, AssetParameterModel::TypeRole).value<ParamType>();
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
                    QLocale locale;
                    opacity = locale.toDouble(vals.at(4));
                }
            }
            ((GeometryWidget *)w.second)->setValue(rect, opacity);
        }
    }
    if (m_monitorHelper) {
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
    // refresh keyframes
    m_keyframes->refresh();
    //m_model->dataChanged(QModelIndex(), QModelIndex());
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
    m_keyframes->reset();
    //m_model->dataChanged(QModelIndex(), QModelIndex());
    m_keyframeview->setDuration(duration);
    m_time->setRange(0, duration - 1);
    slotRefreshParams();
}

void KeyframeWidget::addParameter(const QPersistentModelIndex &index)
{
    QLocale locale;
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    // Retrieve parameters from the model
    QString name = m_model->data(index, Qt::DisplayRole).toString();
    QString comment = m_model->data(index, AssetParameterModel::CommentRole).toString();
    QString suffix = m_model->data(index, AssetParameterModel::SuffixRole).toString();

    ParamType type = m_model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
    // Construct object
    QWidget *paramWidget = nullptr;
    if (type == ParamType::AnimatedRect) {
        m_neededScene = MonitorSceneType::MonitorSceneGeometry;
        int inPos = m_model->data(index, AssetParameterModel::ParentInRole).toInt();
        QPair<int, int> range(inPos, inPos + m_model->data(index, AssetParameterModel::ParentDurationRole).toInt());
        QSize frameSize = pCore->getCurrentFrameSize();
        const QString value = m_keyframes->getInterpolatedValue(getPosition(), index).toString();
        QRect rect;
        double opacity = 0;
        QStringList vals = value.split(QLatin1Char(' '));
        if (vals.count() >= 4) {
            rect = QRect(vals.at(0).toInt(), vals.at(1).toInt(), vals.at(2).toInt(), vals.at(3).toInt());
            if (vals.count() > 4) {
                opacity = locale.toDouble(vals.at(4));
            }
        }
        // qtblend uses an opacity value in the (0-1) range, while older geometry effects use (0-100)
        bool integerOpacity = m_model->getAssetId() != QLatin1String("qtblend");
        GeometryWidget *geomWidget = new GeometryWidget(pCore->getMonitor(m_model->monitorId), range, rect, opacity, frameSize, false,
                                                        m_model->data(m_index, AssetParameterModel::OpacityRole).toBool(), integerOpacity, this);
        connect(geomWidget, &GeometryWidget::valueChanged,
                [this, index](const QString v) { m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(v), index); });
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
                connect(m_monitorHelper, &KeyframeMonitorHelper::updateKeyframeData, this, &KeyframeWidget::slotUpdateKeyframesFromMonitor, Qt::UniqueConnection);
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
        double min = locale.toDouble(m_model->data(index, AssetParameterModel::MinRole).toString());
        double max = locale.toDouble(m_model->data(index, AssetParameterModel::MaxRole).toString());
        double defaultValue = m_model->data(index, AssetParameterModel::DefaultRole).toDouble();
        int decimals = m_model->data(index, AssetParameterModel::DecimalsRole).toInt();
        double factor = locale.toDouble(m_model->data(index, AssetParameterModel::FactorRole).toString());
        factor = qFuzzyIsNull(factor) ? 1 : factor;
        auto doubleWidget = new DoubleWidget(name, value * factor, min, max, factor, defaultValue, comment, -1, suffix, decimals, this);
        connect(doubleWidget, &DoubleWidget::valueChanged,
                [this, index](double v) { m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(v), index); });
        paramWidget = doubleWidget;
    }
    if (paramWidget) {
        m_parameters[index] = paramWidget;
        m_lay->addWidget(paramWidget);
    }
}

void KeyframeWidget::slotInitMonitor(bool active)
{
    if (m_keyframeview) {
        m_keyframeview->initKeyframePos();
    }
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    connectMonitor(active);
    if (active) {
        connect(monitor, &Monitor::seekPosition, this, &KeyframeWidget::monitorSeek, Qt::UniqueConnection);
    } else {
        disconnect(monitor, &Monitor::seekPosition, this, &KeyframeWidget::monitorSeek);
    }
}

void KeyframeWidget::connectMonitor(bool active)
{
    if (m_monitorHelper) {
        if (m_monitorHelper->connectMonitor(active)) {
            slotRefreshParams();
        }
    }
    for (const auto &w : m_parameters) {
        ParamType type = m_model->data(w.first, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::AnimatedRect) {
            ((GeometryWidget *)w.second)->connectMonitor(active);
            break;
        }
    }
}

void KeyframeWidget::slotUpdateKeyframesFromMonitor(QPersistentModelIndex index, const QVariant &res)
{
    if (m_keyframes->isEmpty()) {
        m_keyframes->addKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), KeyframeType::Linear);
        m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), res, index);
    } else if (m_keyframes->hasKeyframe(getPosition()) || m_keyframes->singleKeyframe()) {
        m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), res, index);
    }
}

MonitorSceneType KeyframeWidget::requiredScene() const
{
    qDebug()<<"// // // RESULTING REQUIRED SCENE: "<<m_neededScene;
    return m_neededScene;
}

bool KeyframeWidget::keyframesVisible() const
{
    return m_keyframeview->isVisible();
}

void KeyframeWidget::showKeyframes(bool enable)
{
    m_toolbar->setVisible(enable);
    m_keyframeview->setVisible(enable);
}

void KeyframeWidget::slotCopyKeyframes()
{
    QJsonArray list;
    for (const auto &w : m_parameters) {
        int type = m_model->data(w.first, AssetParameterModel::TypeRole).toInt();
        QString name = m_model->data(w.first, Qt::DisplayRole).toString();
        QString value = m_model->data(w.first, AssetParameterModel::ValueRole).toString();
        double min = m_model->data(w.first, AssetParameterModel::MinRole).toDouble();
        double max = m_model->data(w.first, AssetParameterModel::MaxRole).toDouble();
        double factor = m_model->data(w.first, AssetParameterModel::FactorRole).toDouble();
        if (factor > 0) {
            min /= factor;
            max /= factor;
        }
        QJsonObject currentParam;
        currentParam.insert(QLatin1String("name"), QJsonValue(name));
        currentParam.insert(QLatin1String("value"), QJsonValue(value));
        currentParam.insert(QLatin1String("type"), QJsonValue(type));
        currentParam.insert(QLatin1String("min"), QJsonValue(min));
        currentParam.insert(QLatin1String("max"), QJsonValue(max));
        list.push_back(currentParam);
    }
    if (list.isEmpty()) {
        return;
    }
    QClipboard *clipboard = QApplication::clipboard();
    QJsonDocument json(list);
    clipboard->setText(QString(json.toJson()));
}

void KeyframeWidget::slotImportKeyframes()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString values = clipboard->text();

    int inPos = m_model->data(m_index, AssetParameterModel::ParentInRole).toInt();
    int outPos = inPos + m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt();
    QList <QPersistentModelIndex>indexes;
    for (const auto &w : m_parameters) {
        indexes << w.first;
    }
    QPointer<KeyframeImport>import = new KeyframeImport(inPos, outPos, values, m_model, indexes, this);
    if (import->exec() != QDialog::Accepted) {
        delete import;
        return;
    }
    QString keyframeData = import->selectedData();
    QString tag = import->selectedTarget();
    qDebug()<<"// CHECKING FOR TARGET PARAM: "<<tag;
    //m_model->setParameter(tag, keyframeData, true);
    /*for (const auto &w : m_parameters) {
        qDebug()<<"// GOT PARAM: "<<m_model->data(w.first, AssetParameterModel::NameRole).toString();
        if (tag == m_model->data(w.first, AssetParameterModel::NameRole).toString()) {
            qDebug()<<"// PASSING DTAT: "<<keyframeData;
            m_model->getKeyframeModel()->getKeyModel()->parseAnimProperty(keyframeData);
            m_model->getKeyframeModel()->getKeyModel()->modelChanged();
            break;
        }
    }*/
    AssetCommand *command = new AssetCommand(m_model, m_index, keyframeData);
    pCore->pushUndo(command);
    /*m_model->getKeyframeModel()->getKeyModel()->dataChanged(QModelIndex(), QModelIndex());
    m_model->modelChanged();
    qDebug()<<"//// UPDATING KEYFRAMES CORE---------";
    pCore->updateItemKeyframes(m_model->getOwnerId());*/
    qDebug()<<"//// UPDATING KEYFRAMES CORE . ..  .DONE ---------";
    //emit importKeyframes(type, tag, keyframeData);
    delete import;
}
