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
#include <QStyle>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <klocalizedstring.h>
#include <utility>

KeyframeWidget::KeyframeWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QSize frameSize, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_monitorHelper(nullptr)
    , m_neededScene(MonitorSceneType::MonitorSceneDefault)
    , m_sourceFrameSize(frameSize.isValid() && !frameSize.isNull() ? frameSize : pCore->getCurrentFrameSize())
    , m_baseHeight(0)
    , m_addedHeight(0)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_lay = new QVBoxLayout(this);
    m_lay->setSpacing(0);

    bool ok = false;
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    Q_ASSERT(ok);
    int in = m_model->data(m_index, AssetParameterModel::InRole).toInt(&ok);
    m_model->prepareKeyframes();
    m_keyframes = m_model->getKeyframeModel();
    m_keyframeview = new KeyframeView(m_keyframes, duration, in, this);

    m_buttonAddDelete = new QToolButton(this);
    m_buttonAddDelete->setAutoRaise(true);
    m_buttonAddDelete->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-add")));
    m_buttonAddDelete->setToolTip(i18n("Add keyframe"));

    m_buttonPrevious = new QToolButton(this);
    m_buttonPrevious->setAutoRaise(true);
    m_buttonPrevious->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-previous")));
    m_buttonPrevious->setToolTip(i18n("Go to previous keyframe"));

    m_buttonNext = new QToolButton(this);
    m_buttonNext->setAutoRaise(true);
    m_buttonNext->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-next")));
    m_buttonNext->setToolTip(i18n("Go to next keyframe"));
    
    // Move keyframe to cursor
    m_buttonCenter = new QToolButton(this);
    m_buttonCenter->setAutoRaise(true);
    m_buttonCenter->setIcon(QIcon::fromTheme(QStringLiteral("align-horizontal-center")));
    m_buttonCenter->setToolTip(i18n("Move selected keyframe to cursor"));
    
    // Duplicate selected keyframe at cursor pos
    m_buttonCopy = new QToolButton(this);
    m_buttonCopy->setAutoRaise(true);
    m_buttonCopy->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-duplicate")));
    m_buttonCopy->setToolTip(i18n("Duplicate selected keyframe"));
    
    // Apply current value to selected keyframes
    m_buttonApply = new QToolButton(this);
    m_buttonApply->setAutoRaise(true);
    m_buttonApply->setIcon(QIcon::fromTheme(QStringLiteral("edit-paste")));
    m_buttonApply->setToolTip(i18n("Apply value to selected keyframes"));
    m_buttonApply->setFocusPolicy(Qt::StrongFocus);
    
    // Keyframe type widget
    m_selectType = new KSelectAction(QIcon::fromTheme(QStringLiteral("keyframes")), i18n("Keyframe interpolation"), this);
    QAction *linear = new QAction(QIcon::fromTheme(QStringLiteral("linear")), i18n("Linear"), this);
    linear->setData(int(mlt_keyframe_linear));
    linear->setCheckable(true);
    m_selectType->addAction(linear);
    QAction *discrete = new QAction(QIcon::fromTheme(QStringLiteral("discrete")), i18n("Discrete"), this);
    discrete->setData(int(mlt_keyframe_discrete));
    discrete->setCheckable(true);
    m_selectType->addAction(discrete);
    QAction *curve = new QAction(QIcon::fromTheme(QStringLiteral("smooth")), i18n("Smooth"), this);
    curve->setData(int(mlt_keyframe_smooth));
    curve->setCheckable(true);
    m_selectType->addAction(curve);
    m_selectType->setCurrentAction(linear);
    connect(m_selectType, static_cast<void (KSelectAction::*)(QAction *)>(&KSelectAction::triggered), this, &KeyframeWidget::slotEditKeyframeType);
    m_selectType->setToolBarMode(KSelectAction::ComboBoxMode);
    m_toolbar = new QToolBar(this);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    m_toolbar->setIconSize(QSize(size, size));

    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    connect(monitor, &Monitor::seekPosition, this, &KeyframeWidget::monitorSeek, Qt::UniqueConnection);
    m_time = new TimecodeDisplay(pCore->timecode(), this);
    m_time->setRange(0, duration - 1);
    m_time->setOffset(in);

    m_toolbar->addWidget(m_buttonPrevious);
    m_toolbar->addWidget(m_buttonAddDelete);
    m_toolbar->addWidget(m_buttonNext);
    m_toolbar->addWidget(m_buttonCenter);
    m_toolbar->addWidget(m_buttonCopy);
    m_toolbar->addWidget(m_buttonApply);
    m_toolbar->addAction(m_selectType);

    QAction *seekKeyframe = new QAction(i18n("Seek to keyframe on select"), this);
    seekKeyframe->setCheckable(true);
    seekKeyframe->setChecked(KdenliveSettings::keyframeseek());
    connect(seekKeyframe, &QAction::triggered, [&](bool selected) {
        KdenliveSettings::setKeyframeseek(selected);
    });
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
    discrete2->setData(int(mlt_keyframe_discrete));
    discrete2->setCheckable(true);
    kfType->addAction(discrete2);
    QAction *linear2 = new QAction(QIcon::fromTheme(QStringLiteral("linear")), i18n("Linear"), this);
    linear2->setData(int(mlt_keyframe_linear));
    linear2->setCheckable(true);
    kfType->addAction(linear2);
    QAction *curve2 = new QAction(QIcon::fromTheme(QStringLiteral("smooth")), i18n("Smooth"), this);
    curve2->setData(int(mlt_keyframe_smooth));
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
    container->addAction(seekKeyframe);
    container->addAction(copy);
    container->addAction(paste);
    container->addSeparator();
    container->addAction(kfType);
    container->addAction(removeNext);

    // rotoscoping only supports linear keyframes
    if (m_model->getAssetId() == QLatin1String("rotoscoping")) {
        m_selectType->setVisible(false);
        m_selectType->setCurrentAction(linear);
        kfType->setVisible(false);
        kfType->setCurrentAction(linear2);
    }

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

    connect(m_time, &TimecodeDisplay::timeCodeEditingFinished, this, [&]() { slotSetPosition(-1, true); });
    connect(m_keyframeview, &KeyframeView::seekToPos, this, [&](int p) { slotSetPosition(p, true); });
    connect(m_keyframeview, &KeyframeView::atKeyframe, this, &KeyframeWidget::slotAtKeyframe);
    connect(m_keyframeview, &KeyframeView::modified, this, &KeyframeWidget::slotRefreshParams);
    connect(m_keyframeview, &KeyframeView::activateEffect, this, &KeyframeWidget::activateEffect);

    connect(m_buttonAddDelete, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotAddRemove);
    connect(m_buttonPrevious, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotGoToPrev);
    connect(m_buttonNext, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotGoToNext);
    connect(m_buttonCenter, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotCenterKeyframe);
    connect(m_buttonCopy, &QAbstractButton::pressed, m_keyframeview, &KeyframeView::slotDuplicateKeyframe);
    connect(m_buttonApply, &QAbstractButton::pressed, this, [this]() {
        QMultiMap<QPersistentModelIndex, QString> paramList;
        QList<QPersistentModelIndex> rectParams;
        for (const auto &w : m_parameters) {
            auto type = m_model->data(w.first, AssetParameterModel::TypeRole).value<ParamType>();
            if (type == ParamType::AnimatedRect) {
                if (m_model->data(w.first, AssetParameterModel::OpacityRole).toBool()) {
                    paramList.insert(w.first, i18n("Opacity"));
                }
                paramList.insert(w.first, i18n("Height"));
                paramList.insert(w.first, i18n("Width"));
                paramList.insert(w.first, i18n("Y position"));
                paramList.insert(w.first, i18n("X position"));
                rectParams << w.first;
            } else {
                paramList.insert(w.first, m_model->data(w.first, Qt::DisplayRole).toString());
            }
        }
        if (paramList.count() == 0) {
            qDebug()<<"=== No parameter to copy, aborting";
            return;
        }
        if (paramList.count() == 1) {
            m_keyframeview->copyCurrentValue(m_keyframes->getIndexAtRow(0), QString());
            return;
        }
        // More than one param
        QDialog d(this);
        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        auto *l = new QVBoxLayout;
        d.setLayout(l);
        l->addWidget(new QLabel(i18n("Select parameters to copy"), &d));
        QMapIterator<QPersistentModelIndex, QString> i(paramList);
        while (i.hasNext()) {
            i.next();
            auto *cb = new QCheckBox(i.value(), this);
            cb->setProperty("index", i.key());
            l->addWidget(cb);
        }
        l->addWidget(buttonBox);
        d.connect(buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::reject);
        d.connect(buttonBox, &QDialogButtonBox::accepted, &d, &QDialog::accept);
        if (d.exec() != QDialog::Accepted) {
            return;
        }
        paramList.clear();
        QList<QCheckBox *> cbs = d.findChildren<QCheckBox *>();
        for (auto c : qAsConst(cbs)) {
            //qDebug()<<"=== FOUND CBS: "<<KLocalizedString::removeAcceleratorMarker(c->text());
            if (c->isChecked()) {
                QPersistentModelIndex ix = c->property("index").toModelIndex();
                if (rectParams.contains(ix)) {
                    // Check param name
                    QString cbName = KLocalizedString::removeAcceleratorMarker(c->text());
                    QString paramName;
                    if (cbName == i18n("Opacity")) {
                        paramName = QStringLiteral("spinO");
                    } else if (cbName == i18n("Height")) {
                        paramName = QStringLiteral("spinH");
                    } else if (cbName == i18n("Width")) {
                        paramName = QStringLiteral("spinW");
                    } else if (cbName == i18n("X position")) {
                        paramName = QStringLiteral("spinX");
                    } else if (cbName == i18n("Y position")) {
                        paramName = QStringLiteral("spinY");
                    }
                    if (!paramName.isEmpty()) {
                        m_keyframeview->copyCurrentValue(ix, paramName);
                    }
                } else {
                    m_keyframeview->copyCurrentValue(ix, QString());
                }
            }
        }
        return;
    });
    //m_baseHeight = m_keyframeview->height() + m_selectType->defaultWidget()->sizeHint().height();
    QMargins mrg = m_lay->contentsMargins();
    m_baseHeight = m_keyframeview->height() + m_toolbar->sizeHint().height();
    m_addedHeight = mrg.top() + mrg.bottom();
    setFixedHeight(m_baseHeight + m_addedHeight);
    addParameter(index);
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
    int in = 0;
    int out = 0;
    bool canHaveZone = m_model->getOwnerId().first == ObjectType::Master || m_model->getOwnerId().first == ObjectType::TimelineTrack;
    if (canHaveZone) {
        bool ok = false;
        in = m_model->data(m_index, AssetParameterModel::InRole).toInt(&ok);
        out = m_model->data(m_index, AssetParameterModel::OutRole).toInt(&ok);
        Q_ASSERT(ok);
    }
    if (in == 0 && out == 0) {
        in = pCore->getItemPosition(m_model->getOwnerId());
        out = in + pCore->getItemDuration(m_model->getOwnerId());
    }
    bool isInRange = pos >= in && pos < out;
    connectMonitor(isInRange && m_model->isActive());
    m_buttonAddDelete->setEnabled(isInRange && pos > in);
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
        if (ac->data().toInt() == int(keyType)) {
            m_selectType->setCurrentItem(i);
            break;
        }
        i++;
    }
    for (const auto &w : m_parameters) {
        auto type = m_model->data(w.first, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::KeyframeParam) {
            (static_cast<DoubleWidget *>(w.second))->setValue(m_keyframes->getInterpolatedValue(pos, w.first).toDouble());
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
            (static_cast<GeometryWidget *>(w.second))->setValue(rect, opacity);
        }
    }
    if (m_monitorHelper && m_model->isActive()) {
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

void KeyframeWidget::updateTimecodeFormat()
{
    m_time->slotUpdateTimeCodeFormat();
}

void KeyframeWidget::slotAtKeyframe(bool atKeyframe, bool singleKeyframe)
{
    if (atKeyframe) {
        m_buttonAddDelete->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-remove")));
        m_buttonAddDelete->setToolTip(i18n("Delete keyframe"));
    } else {
        m_buttonAddDelete->setIcon(QIcon::fromTheme(QStringLiteral("keyframe-add")));
        m_buttonAddDelete->setToolTip(i18n("Add keyframe"));
    }
    m_buttonCenter->setEnabled(!atKeyframe);
    emit updateEffectKeyframe(atKeyframe || singleKeyframe);
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
    int in = m_model->data(m_index, AssetParameterModel::InRole).toInt(&ok);
    // m_model->dataChanged(QModelIndex(), QModelIndex());
    //->getKeyframeModel()->getKeyModel(m_index)->dataChanged(QModelIndex(), QModelIndex());
    m_keyframeview->setDuration(duration, in);
    m_time->setRange(0, duration - 1);
    m_time->setOffset(in);
    slotRefreshParams();
}

void KeyframeWidget::resetKeyframes()
{
    // update duration
    bool ok = false;
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    Q_ASSERT(ok);
    int in = m_model->data(m_index, AssetParameterModel::InRole).toInt(&ok);
    // reset keyframes
    m_keyframes->refresh();
    // m_model->dataChanged(QModelIndex(), QModelIndex());
    m_keyframeview->setDuration(duration, in);
    m_time->setRange(0, duration - 1);
    m_time->setOffset(in);
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
        connect(geomWidget, &GeometryWidget::updateMonitorGeometry, this, [this](const QRect r) {
                    if (m_model->isActive()) {
                        pCore->getMonitor(m_model->monitorId)->setUpEffectGeometry(r);
                    }
        });
        paramWidget = geomWidget;
    } else if (type == ParamType::Roto_spline) {
        m_monitorHelper = new RotoHelper(pCore->getMonitor(m_model->monitorId), m_model, index, this);
        m_neededScene = MonitorSceneType::MonitorSceneRoto;
    } else {
        if (m_model->getAssetId() == QLatin1String("frei0r.c0rners")) {
            if (m_neededScene == MonitorSceneDefault && !m_monitorHelper) {
                m_neededScene = MonitorSceneType::MonitorSceneCorners;
                m_monitorHelper = new CornersHelper(pCore->getMonitor(m_model->monitorId), m_model, index, this);
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
        doubleWidget->setDragObjectName(QString::number(index.row()));
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
    connectMonitor(active);
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    if (m_keyframeview) {
        m_keyframeview->initKeyframePos();
        connect(monitor, &Monitor::updateScene, m_keyframeview, &KeyframeView::slotModelChanged, Qt::UniqueConnection);
    }
}

void KeyframeWidget::connectMonitor(bool active)
{
    if (m_monitorHelper) {
        if (m_model->isActive()) {
            connect(m_monitorHelper, &KeyframeMonitorHelper::updateKeyframeData, this, &KeyframeWidget::slotUpdateKeyframesFromMonitor, Qt::UniqueConnection);
            if (m_monitorHelper->connectMonitor(active)) {
                slotRefreshParams();
            }
        } else {
            m_monitorHelper->connectMonitor(false);
            disconnect(m_monitorHelper, &KeyframeMonitorHelper::updateKeyframeData, this, &KeyframeWidget::slotUpdateKeyframesFromMonitor);
        }
    }
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    if (active) {
        connect(monitor, &Monitor::seekToNextKeyframe, m_keyframeview, &KeyframeView::slotGoToNext, Qt::UniqueConnection);
        connect(monitor, &Monitor::seekToPreviousKeyframe, m_keyframeview, &KeyframeView::slotGoToPrev, Qt::UniqueConnection);
        connect(monitor, &Monitor::addRemoveKeyframe, m_keyframeview, &KeyframeView::slotAddRemove, Qt::UniqueConnection);
        connect(this, &KeyframeWidget::updateEffectKeyframe, monitor, &Monitor::setEffectKeyframe, Qt::DirectConnection);
        connect(monitor, &Monitor::seekToKeyframe, this, &KeyframeWidget::slotSeekToKeyframe, Qt::UniqueConnection);
    } else {
        disconnect(monitor, &Monitor::seekToNextKeyframe, m_keyframeview, &KeyframeView::slotGoToNext);
        disconnect(monitor, &Monitor::seekToPreviousKeyframe, m_keyframeview, &KeyframeView::slotGoToPrev);
        disconnect(monitor, &Monitor::addRemoveKeyframe, m_keyframeview, &KeyframeView::slotAddRemove);
        disconnect(this, &KeyframeWidget::updateEffectKeyframe, monitor, &Monitor::setEffectKeyframe);
        disconnect(monitor, &Monitor::seekToKeyframe, this, &KeyframeWidget::slotSeekToKeyframe);
    }
    for (const auto &w : m_parameters) {
        auto type = m_model->data(w.first, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::AnimatedRect) {
            (static_cast<GeometryWidget *>(w.second))->connectMonitor(active);
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
