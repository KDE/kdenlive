/*
    SPDX-FileCopyrightText: 2011 Till Theato <root@ttill.de>
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "keyframewidget.hpp"
#include "assets/keyframes/model/corners/cornershelper.hpp"
#include "assets/keyframes/model/keyframemodel.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/keyframes/model/rect/recthelper.hpp"
#include "assets/keyframes/model/rotoscoping/rotohelper.hpp"
#include "assets/keyframes/view/keyframeview.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "assets/view/widgets/keyframeimport.h"
#include "core.h"
#include "effects/effectsrepository.hpp"
#include "kdenlivesettings.h"
#include "lumaliftgainparam.hpp"
#include "monitor/monitor.h"
#include "utils/timecode.h"
#include "widgets/choosecolorwidget.h"
#include "widgets/doublewidget.h"
#include "widgets/geometrywidget.h"
#include "widgets/timecodedisplay.h"

#include <KActionCategory>
#include <KActionMenu>
#include <KDualAction>
#include <KLocalizedString>
#include <KSelectAction>
#include <KStandardAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMenu>
#include <QPointer>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <kwidgetsaddons_version.h>
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
    m_model->prepareKeyframes();
    m_keyframes = m_model->getKeyframeModel();
    m_keyframeview = new KeyframeView(m_keyframes, duration, this);

    m_addDeleteAction = new KDualAction(this);
    m_addDeleteAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("keyframe-add")));
    m_addDeleteAction->setActiveText(i18n("Add keyframe"));
    m_addDeleteAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("keyframe-remove")));
    m_addDeleteAction->setInactiveText(i18n("Delete keyframe"));

    connect(m_addDeleteAction, &KDualAction::triggered, m_keyframeview, &KeyframeView::slotAddRemove);
    connect(this, &KeyframeWidget::addRemove, m_keyframeview, &KeyframeView::slotAddRemove);

    auto *previousKFAction = new QAction(QIcon::fromTheme(QStringLiteral("keyframe-previous")), i18n("Go to previous keyframe"), this);
    connect(previousKFAction, &QAction::triggered, m_keyframeview, &KeyframeView::slotGoToPrev);
    connect(this, &KeyframeWidget::goToPrevious, m_keyframeview, &KeyframeView::slotGoToPrev);

    auto *nextKFAction = new QAction(QIcon::fromTheme(QStringLiteral("keyframe-next")), i18n("Go to next keyframe"), this);
    connect(nextKFAction, &QAction::triggered, m_keyframeview, &KeyframeView::slotGoToNext);
    connect(this, &KeyframeWidget::goToNext, m_keyframeview, &KeyframeView::slotGoToNext);

    // Move keyframe to cursor
    m_centerAction = new QAction(QIcon::fromTheme(QStringLiteral("align-horizontal-center")), i18n("Move selected keyframe to cursor"), this);

    // Apply current value to selected keyframes
    m_copyAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18n("Copy keyframes"), this);
    connect(m_copyAction, &QAction::triggered, this, &KeyframeWidget::slotCopySelectedKeyframes);
    m_copyAction->setToolTip(i18n("Copy keyframes"));
    m_copyAction->setWhatsThis(
        xi18nc("@info:whatsthis", "Copy keyframes. Copy the selected keyframes, or current parameters values if no keyframe is selected."));

    m_pasteAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-paste")), i18n("Paste keyframe"), this);
    connect(m_pasteAction, &QAction::triggered, this, &KeyframeWidget::slotPasteKeyframeFromClipBoard);
    m_pasteAction->setToolTip(i18n("Paste keyframes"));
    m_pasteAction->setWhatsThis(xi18nc("@info:whatsthis", "Paste keyframes. Paste clipboard data as keyframes at current position."));

    auto *applyAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-paste")), i18n("Apply current position value to selected keyframes"), this);

    // Keyframe type widget
    m_selectType = new KSelectAction(QIcon::fromTheme(QStringLiteral("linear")), i18n("Keyframe interpolation"), this);
    QMap<KeyframeType, QAction *> kfTypeHandles;
    for (auto it = KeyframeModel::getKeyframeTypes().cbegin(); it != KeyframeModel::getKeyframeTypes().cend();
         it++) { // Order is fixed due to the nature of <map>
        QAction *tmp = new QAction(QIcon::fromTheme(KeyframeModel::getIconByKeyframeType(it.key())), it.value(), this);
        tmp->setData(int(it.key()));
        tmp->setCheckable(true);
        kfTypeHandles.insert(it.key(), tmp);
        m_selectType->addAction(kfTypeHandles[it.key()]);
    }
    m_selectType->setCurrentAction(kfTypeHandles[KeyframeType::Linear]);
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 240, 0)
    connect(m_selectType, &KSelectAction::actionTriggered, this, &KeyframeWidget::slotEditKeyframeType);
#else
    connect(m_selectType, static_cast<void (KSelectAction::*)(QAction *)>(&KSelectAction::triggered), this, &KeyframeWidget::slotEditKeyframeType);
#endif
    m_selectType->setToolBarMode(KSelectAction::MenuMode);
    m_selectType->setToolTip(i18n("Keyframe interpolation"));
    m_selectType->setWhatsThis(xi18nc("@info:whatsthis", "Keyframe interpolation. This defines which interpolation will be used for the current keyframe."));

    m_toolbar = new QToolBar(this);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    m_toolbar->setIconSize(QSize(size, size));

    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    connect(monitor, &Monitor::seekPosition, this, &KeyframeWidget::monitorSeek, Qt::UniqueConnection);
    connect(pCore.get(), &Core::disconnectEffectStack, this, &KeyframeWidget::disconnectEffectStack);

    m_time = new TimecodeDisplay(this);
    m_time->setRange(0, duration - 1);

    m_toolbar->addAction(previousKFAction);
    m_toolbar->addAction(m_addDeleteAction);
    m_toolbar->addAction(nextKFAction);
    m_toolbar->addAction(m_centerAction);
    m_toolbar->addAction(m_copyAction);
    m_toolbar->addAction(m_pasteAction);
    m_toolbar->addAction(m_selectType);

    QAction *seekKeyframe = new QAction(i18n("Seek to Keyframe on Select"), this);
    seekKeyframe->setCheckable(true);
    seekKeyframe->setChecked(KdenliveSettings::keyframeseek());
    connect(seekKeyframe, &QAction::triggered, [&](bool selected) { KdenliveSettings::setKeyframeseek(selected); });
    // copy/paste keyframes from clipboard
    QAction *copy = new QAction(i18n("Copy All Keyframes to Clipboard"), this);
    connect(copy, &QAction::triggered, this, &KeyframeWidget::slotCopyKeyframes);
    QAction *paste = new QAction(i18n("Import Keyframes from Clipboard…"), this);
    connect(paste, &QAction::triggered, this, &KeyframeWidget::slotImportKeyframes);
    if (m_model->data(index, AssetParameterModel::TypeRole).value<ParamType>() == ParamType::ColorWheel) {
        // TODO color wheel doesn't support keyframe import/export yet
        copy->setVisible(false);
        paste->setVisible(false);
    }
    // Remove keyframes
    QAction *removeNext = new QAction(i18n("Remove all Keyframes After Cursor"), this);
    connect(removeNext, &QAction::triggered, this, &KeyframeWidget::slotRemoveNextKeyframes);

    // Default kf interpolation
    KSelectAction *kfType = new KSelectAction(i18n("Default Keyframe Type"), this);
    QAction *discrete2 = new QAction(QIcon::fromTheme(KeyframeModel::getIconByKeyframeType(KeyframeType::Discrete)),
                                     KeyframeModel::getKeyframeTypes().value(KeyframeType::Discrete), this);
    discrete2->setData(int(KeyframeType::Discrete));
    discrete2->setCheckable(true);
    kfType->addAction(discrete2);
    QAction *linear2 = new QAction(QIcon::fromTheme(KeyframeModel::getIconByKeyframeType(KeyframeType::Linear)),
                                   KeyframeModel::getKeyframeTypes().value(KeyframeType::Linear), this);
    linear2->setData(int(KeyframeType::Linear));
    linear2->setCheckable(true);
    kfType->addAction(linear2);
    QAction *curve2 = new QAction(QIcon::fromTheme(KeyframeModel::getIconByKeyframeType(KeyframeType::Curve)),
                                  KeyframeModel::getKeyframeTypes().value(KeyframeType::Curve), this);
    curve2->setData(int(KeyframeType::Curve));
    curve2->setCheckable(true);
    kfType->addAction(curve2);
    switch (KdenliveSettings::defaultkeyframeinterp()) {
    case int(KeyframeType::Discrete):
        kfType->setCurrentAction(discrete2);
        break;
    case int(KeyframeType::Curve):
        kfType->setCurrentAction(curve2);
        break;
    default:
        kfType->setCurrentAction(linear2);
        break;
    }
#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 240, 0)
    connect(kfType, &KSelectAction::actionTriggered, this, [&](QAction *ac) { KdenliveSettings::setDefaultkeyframeinterp(ac->data().toInt()); });
#else
    connect(kfType, static_cast<void (KSelectAction::*)(QAction *)>(&KSelectAction::triggered), this,
            [&](QAction *ac) { KdenliveSettings::setDefaultkeyframeinterp(ac->data().toInt()); });
#endif

    // rotoscoping only supports linear keyframes
    if (m_model->getAssetId() == QLatin1String("rotoscoping")) {
        m_selectType->setVisible(false);
        m_selectType->setCurrentAction(kfTypeHandles[KeyframeType::Linear]);
        kfType->setVisible(false);
        kfType->setCurrentAction(linear2);
    }

    // Menu toolbutton
    auto *menuAction = new KActionMenu(QIcon::fromTheme(QStringLiteral("application-menu")), i18n("Options"), this);
    menuAction->setWhatsThis(
        xi18nc("@info:whatsthis", "Opens a list of further actions for managing keyframes (for example: copy to and pasting keyframes from clipboard)."));
    menuAction->setPopupMode(QToolButton::InstantPopup);
    menuAction->addAction(seekKeyframe);
    menuAction->addAction(copy);
    menuAction->addAction(paste);
    menuAction->addAction(applyAction);
    menuAction->addSeparator();
    menuAction->addAction(kfType);
    menuAction->addAction(removeNext);
    m_toolbar->addAction(menuAction);

    m_lay->addWidget(m_keyframeview);
    auto *hlay = new QHBoxLayout;
    hlay->addWidget(m_toolbar);
    hlay->addWidget(m_time);
    hlay->addStretch();
    m_lay->addLayout(hlay);

    connect(m_time, &TimecodeDisplay::timeCodeEditingFinished, this, [&]() { slotSetPosition(-1, true); });
    connect(m_keyframeview, &KeyframeView::seekToPos, this, [&](int pos) {
        int in = m_model->data(m_index, AssetParameterModel::InRole).toInt();
        bool canHaveZone = m_model->getOwnerId().type == KdenliveObjectType::Master || m_model->getOwnerId().type == KdenliveObjectType::TimelineTrack;
        if (pos < 0) {
            m_time->setValue(0);
            m_keyframeview->slotSetPosition(0, true);
        } else {
            m_time->setValue(qMax(0, pos - in));
            m_keyframeview->slotSetPosition(pos, true);
        }
        m_addDeleteAction->setEnabled(pos > 0);
        slotRefreshParams();

        Q_EMIT seekToPos(pos + (canHaveZone ? in : 0));
    });
    connect(m_keyframeview, &KeyframeView::atKeyframe, this, &KeyframeWidget::slotAtKeyframe);
    connect(m_keyframeview, &KeyframeView::modified, this, &KeyframeWidget::slotRefreshParams);
    connect(m_keyframeview, &KeyframeView::activateEffect, this, &KeyframeWidget::activateEffect);

    connect(m_centerAction, &QAction::triggered, m_keyframeview, &KeyframeView::slotCenterKeyframe);
    connect(applyAction, &QAction::triggered, this, [this]() {
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
            qDebug() << "=== No parameter to copy, aborting";
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QMapIterator<QPersistentModelIndex, QString> i(paramList);
#else
        QMultiMapIterator<QPersistentModelIndex, QString> i(paramList);
#endif
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
        QMap<QPersistentModelIndex, QStringList> params;
        for (auto c : qAsConst(cbs)) {
            // qDebug()<<"=== FOUND CBS: "<<KLocalizedString::removeAcceleratorMarker(c->text());
            if (c->isChecked()) {
                QPersistentModelIndex ix = c->property("index").toModelIndex();
                if (rectParams.contains(ix)) {
                    // Check param name
                    QString cbName = KLocalizedString::removeAcceleratorMarker(c->text());
                    if (cbName == i18n("Opacity")) {
                        if (params.contains(ix)) {
                            params[ix] << QStringLiteral("spinO");
                        } else {
                            params.insert(ix, {QStringLiteral("spinO")});
                        }
                    } else if (cbName == i18n("Height")) {
                        if (params.contains(ix)) {
                            params[ix] << QStringLiteral("spinH");
                        } else {
                            params.insert(ix, {QStringLiteral("spinH")});
                        }
                    } else if (cbName == i18n("Width")) {
                        if (params.contains(ix)) {
                            params[ix] << QStringLiteral("spinW");
                        } else {
                            params.insert(ix, {QStringLiteral("spinW")});
                        }
                    } else if (cbName == i18n("X position")) {
                        if (params.contains(ix)) {
                            params[ix] << QStringLiteral("spinX");
                        } else {
                            params.insert(ix, {QStringLiteral("spinX")});
                        }
                    } else if (cbName == i18n("Y position")) {
                        if (params.contains(ix)) {
                            params[ix] << QStringLiteral("spinY");
                        } else {
                            params.insert(ix, {QStringLiteral("spinY")});
                        }
                    }
                    if (!params.contains(ix)) {
                        params.insert(ix, {});
                    }
                } else {
                    params.insert(ix, {});
                }
            }
        }
        QMapIterator<QPersistentModelIndex, QStringList> p(params);
        while (p.hasNext()) {
            p.next();
            m_keyframeview->copyCurrentValue(p.key(), p.value().join(QLatin1Char(' ')));
        }
        return;
    });
    // m_baseHeight = m_keyframeview->height() + m_selectType->defaultWidget()->sizeHint().height();
    QMargins mrg = m_lay->contentsMargins();
    m_baseHeight = m_keyframeview->height() + m_toolbar->sizeHint().height();
    m_addedHeight = mrg.top() + mrg.bottom();
    setFixedHeight(m_baseHeight + m_addedHeight);
    addParameter(index);
}

KeyframeWidget::~KeyframeWidget()
{
    delete m_keyframeview;
    delete m_time;
}

void KeyframeWidget::disconnectEffectStack()
{
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    disconnect(monitor, &Monitor::seekPosition, this, &KeyframeWidget::monitorSeek);
}

void KeyframeWidget::monitorSeek(int pos)
{
    int in = 0;
    int out = 0;
    bool canHaveZone = m_model->getOwnerId().type == KdenliveObjectType::Master || m_model->getOwnerId().type == KdenliveObjectType::TimelineTrack;
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
    m_addDeleteAction->setEnabled(isInRange && pos > in);
    int framePos = qBound(in, pos, out) - in;
    if (isInRange && framePos != m_time->getValue()) {
        slotSetPosition(framePos, false);
    }
}

void KeyframeWidget::slotEditKeyframeType(QAction *action)
{
    int type = action->data().toInt();
    m_keyframeview->slotEditType(type, m_index);
    m_selectType->setIcon(action->icon());
    Q_EMIT activateEffect();
}

void KeyframeWidget::slotRefreshParams()
{
    int pos = getPosition();
    KeyframeType keyType = m_keyframes->keyframeType(GenTime(pos, pCore->getCurrentFps()));
    int i = 0;
    while (auto ac = m_selectType->action(i)) {
        if (ac->data().toInt() == int(keyType)) {
            m_selectType->setCurrentItem(i);
            m_selectType->setIcon(ac->icon());
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
        } else if (type == ParamType::ColorWheel) {
            (static_cast<LumaLiftGainParam *>(w.second)->slotRefresh(pos));
        } else if (type == ParamType::Color) {
            const QString value = m_keyframes->getInterpolatedValue(pos, w.first).toString();
            (static_cast<ChooseColorWidget *>(w.second)->slotColorModified(QColorUtils::stringToColor(value)));
        }
    }
    if (m_monitorHelper && m_model->isActive()) {
        m_monitorHelper->refreshParams(pos);
        return;
    }
}
void KeyframeWidget::slotSetPosition(int pos, bool update)
{
    bool canHaveZone = m_model->getOwnerId().type == KdenliveObjectType::Master || m_model->getOwnerId().type == KdenliveObjectType::TimelineTrack;
    int offset = 0;
    if (pos < 0) {
        if (canHaveZone) {
            offset = m_model->data(m_index, AssetParameterModel::InRole).toInt();
        }
        pos = m_time->getValue();
    } else {
        m_time->setValue(pos);
    }
    m_keyframeview->slotSetPosition(pos, true);
    m_addDeleteAction->setEnabled(pos > 0);
    slotRefreshParams();

    if (update) {
        Q_EMIT seekToPos(pos + offset);
    }
}

int KeyframeWidget::getPosition() const
{
    return m_time->getValue() + pCore->getItemIn(m_model->getOwnerId());
}

void KeyframeWidget::slotAtKeyframe(bool atKeyframe, bool singleKeyframe)
{
    m_addDeleteAction->setActive(!atKeyframe);
    m_centerAction->setEnabled(!atKeyframe);
    Q_EMIT updateEffectKeyframe(atKeyframe || singleKeyframe);
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
    Q_ASSERT(ok);
    int out = in + duration;

    m_keyframeview->setDuration(duration);
    m_time->setRange(0, duration - 1);
    if (m_model->monitorId == Kdenlive::ProjectMonitor) {
        monitorSeek(pCore->getMonitorPosition());
    } else {
        int pos = m_time->getValue();
        bool isInRange = pos >= in && pos < out;
        connectMonitor(isInRange && m_model->isActive());
        m_addDeleteAction->setEnabled(isInRange && pos > in);
        int framePos = qBound(in, pos, out) - in;
        if (isInRange && framePos != m_time->getValue()) {
            slotSetPosition(framePos, false);
        }
    }
    slotRefreshParams();
}

void KeyframeWidget::resetKeyframes()
{
    // update duration
    bool ok = false;
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    Q_ASSERT(ok);
    m_model->data(m_index, AssetParameterModel::InRole).toInt(&ok);
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
    QWidget *labelWidget = nullptr;
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
        GeometryWidget *geomWidget = new GeometryWidget(pCore->getMonitor(m_model->monitorId), range, rect, opacity, m_sourceFrameSize, false,
                                                        m_model->data(m_index, AssetParameterModel::OpacityRole).toBool(), this);
        connect(geomWidget, &GeometryWidget::valueChanged, this, [this, index](const QString &v, int ix) {
            Q_EMIT activateEffect();
            m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(v), ix, index);
        });
        connect(geomWidget, &GeometryWidget::updateMonitorGeometry, this, [this](const QRect r) {
            if (m_model->isActive()) {
                pCore->getMonitor(m_model->monitorId)->setUpEffectGeometry(r);
            }
        });
        paramWidget = geomWidget;
    } else if (type == ParamType::ColorWheel) {
        auto colorWheelWidget = new LumaLiftGainParam(m_model, index, this);
        connect(colorWheelWidget, &LumaLiftGainParam::valuesChanged, this,
                [this, index](const QList<QModelIndex> &indexes, const QStringList &sourceList, const QStringList &list, bool createUndo) {
                    Q_EMIT activateEffect();
                    if (createUndo) {
                        m_keyframes->updateMultiKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), sourceList, list, indexes);
                    } else {
                        // Execute without creating an undo/redo entry
                        auto *parentCommand = new QUndoCommand();
                        m_keyframes->updateMultiKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), sourceList, list, indexes, parentCommand);
                        parentCommand->redo();
                        delete parentCommand;
                    }
                });
        connect(colorWheelWidget, &LumaLiftGainParam::updateHeight, this, [&](int h) {
            setFixedHeight(m_baseHeight + m_addedHeight + h);
            Q_EMIT updateHeight();
        });
        paramWidget = colorWheelWidget;
    } else if (type == ParamType::Roto_spline) {
        m_monitorHelper = new RotoHelper(pCore->getMonitor(m_model->monitorId), m_model, index, this);
        m_neededScene = MonitorSceneType::MonitorSceneRoto;
    } else if (type == ParamType::Color) {
        QString value = m_keyframes->getInterpolatedValue(getPosition(), index).toString();
        bool alphaEnabled = m_model->data(index, AssetParameterModel::AlphaRole).toBool();
        labelWidget = new QLabel(name, this);
        auto colorWidget = new ChooseColorWidget(this, QColorUtils::stringToColor(value), alphaEnabled);
        colorWidget->setToolTip(comment);
        connect(colorWidget, &ChooseColorWidget::modified, this, [this, index, alphaEnabled](const QColor &color) {
            Q_EMIT activateEffect();
            m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(QColorUtils::colorToString(color, alphaEnabled)), -1, index);
        });
        paramWidget = colorWidget;

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
                        Q_EMIT addIndex(index);
                    }
                }
            }
        }
        if (m_model->getAssetId().contains(QLatin1String("frei0r.alphaspot"))) {
            if (m_neededScene == MonitorSceneDefault && !m_monitorHelper) {
                m_neededScene = MonitorSceneType::MonitorSceneGeometry;
                m_monitorHelper = new RectHelper(pCore->getMonitor(m_model->monitorId), m_model, index, this);
                connect(this, &KeyframeWidget::addIndex, m_monitorHelper, &RectHelper::addIndex);
            } else {
                if (type == ParamType::KeyframeParam) {
                    QString paramName = m_model->data(index, AssetParameterModel::NameRole).toString();
                    if (paramName.contains(QLatin1String("Position X")) || paramName.contains(QLatin1String("Position Y")) ||
                        paramName.contains(QLatin1String("Size X")) || paramName.contains(QLatin1String("Size Y"))) {
                        Q_EMIT addIndex(index);
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
        auto doubleWidget = new DoubleWidget(name, value, min, max, factor, defaultValue, comment, -1, suffix, decimals,
                                             m_model->data(index, AssetParameterModel::OddRole).toBool(), this);
        connect(doubleWidget, &DoubleWidget::valueChanged, this, [this, index](double v) {
            Q_EMIT activateEffect();
            m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(v), -1, index);
        });
        doubleWidget->setDragObjectName(QString::number(index.row()));
        paramWidget = doubleWidget;
    }
    if (paramWidget) {
        m_parameters[index] = paramWidget;
        if (labelWidget) {
            auto *hbox = new QHBoxLayout(this);
            hbox->setContentsMargins(0, 0, 0, 0);
            hbox->setSpacing(0);
            hbox->addWidget(labelWidget, 1);
            hbox->addWidget(paramWidget, 1);
            m_lay->addLayout(hbox);
        } else {
            m_lay->addWidget(paramWidget);
        }
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
    Q_EMIT activateEffect();
    if (m_keyframes->isEmpty()) {
        GenTime pos(pCore->getItemIn(m_model->getOwnerId()) + m_time->getValue(), pCore->getCurrentFps());
        if (m_time->getValue() > 0) {
            // First add keyframe at start of the clip
            GenTime pos0(pCore->getItemIn(m_model->getOwnerId()), pCore->getCurrentFps());
            m_keyframes->addKeyframe(pos0, KeyframeType::Linear);
            m_keyframes->updateKeyframe(pos0, res, -1, index);
            // For rotoscoping, don't add a second keyframe at cursor pos
            auto type = m_model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
            if (type == ParamType::Roto_spline) {
                return;
            }
        }
        // Next add keyframe at playhead position
        m_keyframes->addKeyframe(pos, KeyframeType::Linear);
        m_keyframes->updateKeyframe(pos, res, -1, index);
    } else if (m_keyframes->hasKeyframe(getPosition()) || m_keyframes->singleKeyframe()) {
        GenTime pos(getPosition(), pCore->getCurrentFps());
        // Auto add keyframe only if there already is more than 1 keyframe
        if (!m_keyframes->singleKeyframe() && KdenliveSettings::autoKeyframe() && m_neededScene == MonitorSceneType::MonitorSceneRoto) {
            m_keyframes->addKeyframe(pos, KeyframeType::Linear);
        }
        m_keyframes->updateKeyframe(pos, res, -1, index);
    } else {
        qDebug() << "==== NO KFR AT: " << getPosition();
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
    m_time->setVisible(enable);
    setFixedHeight(m_addedHeight + (enable ? m_baseHeight : 0));
}

void KeyframeWidget::slotCopyKeyframes()
{
    QJsonDocument effectDoc = m_model->toJson({}, false);
    if (effectDoc.isEmpty()) {
        return;
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(QString(effectDoc.toJson()));
    pCore->displayMessage(i18n("Keyframes copied"), InformationMessage);
}

void KeyframeWidget::slotPasteKeyframeFromClipBoard()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString values = clipboard->text();
    auto json = QJsonDocument::fromJson(values.toUtf8());
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    if (!json.isArray()) {
        pCore->displayMessage(i18n("No valid keyframe data in clipboard"), InformationMessage);
        return;
    }
    auto list = json.array();
    QMap<QString, QMap<int, QVariant>> storedValues;
    for (const auto &entry : qAsConst(list)) {
        if (!entry.isObject()) {
            qDebug() << "Warning : Skipping invalid marker data";
            continue;
        }
        auto entryObj = entry.toObject();
        if (!entryObj.contains(QLatin1String("name"))) {
            qDebug() << "Warning : Skipping invalid marker data (does not contain name)";
            continue;
        }

        ParamType kfrType = entryObj[QLatin1String("type")].toVariant().value<ParamType>();
        if (m_model->isAnimated(kfrType)) {
            QMap<int, QVariant> values;
            if (kfrType == ParamType::Roto_spline) {
                auto value = entryObj.value(QLatin1String("value"));
                if (value.isObject()) {
                    QJsonObject obj = value.toObject();
                    QStringList keys = obj.keys();
                    for (auto &k : keys) {
                        values.insert(k.toInt(), obj.value(k));
                    }
                } else if (value.isArray()) {
                    auto list = value.toArray();
                    for (const auto &entry : qAsConst(list)) {
                        if (!entry.isObject()) {
                            qDebug() << "Warning : Skipping invalid category data";
                            continue;
                        }
                        QJsonObject obj = entry.toObject();
                        QStringList keys = obj.keys();
                        for (auto &k : keys) {
                            values.insert(k.toInt(), obj.value(k));
                        }
                    }
                } else {
                    pCore->displayMessage(i18n("No valid keyframe data in clipboard"), InformationMessage);
                    qDebug() << "::: Invalid ROTO VALUE, ABORTING PASTE\n" << value;
                    return;
                }
            } else {
                const QString value = entryObj.value(QLatin1String("value")).toString();
                if (value.isEmpty()) {
                    pCore->displayMessage(i18n("No valid keyframe data in clipboard"), InformationMessage);
                    qDebug() << "::: Invalid KFR VALUE, ABORTING PASTE\n" << value;
                    return;
                }
                const QStringList stringVals = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
                for (auto &val : stringVals) {
                    int position = m_model->time_to_frames(val.section(QLatin1Char('='), 0, 0));
                    values.insert(position, val.section(QLatin1Char('='), 1));
                }
            }
            storedValues.insert(entryObj[QLatin1String("name")].toString(), values);
        } else {
            const QString value = entryObj.value(QLatin1String("value")).toString();
            QMap<int, QVariant> values;
            values.insert(0, value);
            storedValues.insert(entryObj[QLatin1String("name")].toString(), values);
        }
    }
    int destPos = getPosition();

    std::vector<QPersistentModelIndex> indexes = m_keyframes->getIndexes();
    for (const auto &ix : indexes) {
        auto paramName = m_model->data(ix, AssetParameterModel::NameRole).toString();
        if (storedValues.contains(paramName)) {
            KeyframeModel *km = m_keyframes->getKeyModel(ix);
            QMap<int, QVariant> values = storedValues.value(paramName);
            int offset = values.keys().first();
            QMapIterator<int, QVariant> i(values);
            while (i.hasNext()) {
                i.next();
                km->addKeyframe(GenTime(destPos + i.key() - offset, pCore->getCurrentFps()), KeyframeType::Linear, i.value(), true, undo, redo);
            }
        } else {
            qDebug() << "::: NOT FOUND PARAM: " << paramName << " in list: " << storedValues.keys();
        }
    }
    pCore->pushUndo(undo, redo, i18n("Paste keyframe"));
}

void KeyframeWidget::slotCopySelectedKeyframes()
{
    const QVector<int> results = m_keyframeview->selectedKeyframesIndexes();
    QJsonDocument effectDoc = m_model->toJson(results, false);
    if (effectDoc.isEmpty()) {
        pCore->displayMessage(i18n("Cannot copy current parameter values"), InformationMessage);
        return;
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(QString(effectDoc.toJson()));
    pCore->displayMessage(i18n("Current values copied"), InformationMessage);
}

void KeyframeWidget::slotCopyValueAtCursorPos()
{
    QJsonDocument effectDoc = m_model->valueAsJson(getPosition(), false);
    if (effectDoc.isEmpty()) {
        return;
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(QString(effectDoc.toJson()));
    pCore->displayMessage(i18n("Current values copied"), InformationMessage);
}

void KeyframeWidget::slotImportKeyframes()
{
    QClipboard *clipboard = QApplication::clipboard();
    QString values = clipboard->text();
    QList<QPersistentModelIndex> indexes;
    for (const auto &w : m_parameters) {
        indexes << w.first;
    }
    if (m_neededScene == MonitorSceneRoto) {
        indexes << m_monitorHelper->getIndexes();
    }
    QPointer<KeyframeImport> import = new KeyframeImport(values, m_model, indexes, m_model->data(m_index, AssetParameterModel::ParentInRole).toInt(),
                                                         m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(), this);
    import->show();
    connect(import, &KeyframeImport::updateQmlView, this, &KeyframeWidget::slotRefreshParams);
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

void KeyframeWidget::sendStandardCommand(int command)
{
    switch (command) {
    case KStandardAction::Copy:
        m_copyAction->trigger();
        break;
    case KStandardAction::Paste:
        m_pasteAction->trigger();
        break;
    default:
        qDebug() << ":::: UNKNOWN COMMAND: " << command;
        break;
    }
}
