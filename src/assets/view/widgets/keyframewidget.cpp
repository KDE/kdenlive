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
#include <KMessageBox>
#include <KSelectAction>
#include <KStandardAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMenu>
#include <QPointer>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

KeyframeWidget::KeyframeWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QSize frameSize, QWidget *parent, QFormLayout *layout)
    : AbstractParamWidget(std::move(model), index, parent)
    , m_monitorHelper(nullptr)
    , m_neededScene(MonitorSceneType::MonitorSceneDefault)
    , m_sourceFrameSize(frameSize.isValid() && !frameSize.isNull() ? frameSize : pCore->getCurrentFrameSize())
    , m_baseHeight(0)
    , m_addedHeight(0)
    , m_layout(layout)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_editorviewcontainer = new QStackedWidget(this);
    m_curveeditorcontainer = new QTabWidget(this);
    m_curveeditorcontainer->setTabBarAutoHide(true);

    bool ok = false;
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    Q_ASSERT(ok);
    m_model->prepareKeyframes();
    m_keyframes = m_model->getKeyframeModel();
    m_keyframeview = new KeyframeView(m_keyframes, duration, this);
    m_toggleViewAction = new KDualAction(this);
    m_toggleViewAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("measure")));
    m_toggleViewAction->setActiveText(i18n("Switch to timeline view"));
    m_toggleViewAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("tool_curve")));
    m_toggleViewAction->setInactiveText(i18n("Switch to curve editor view"));
    m_toggleViewAction->setEnabled(false);
    // use these two icons for now

    connect(m_toggleViewAction, &KDualAction::triggered, this, &KeyframeWidget::slotToggleView);

    m_viewswitch = new QToolButton(this);
    m_viewswitch->setToolButtonStyle(Qt::ToolButtonIconOnly);
    int size = style()->pixelMetric(QStyle::PM_SmallIconSize);
    m_viewswitch->setIconSize(QSize(size, size));
    m_viewswitch->setDefaultAction(m_toggleViewAction);

    m_addDeleteAction = new KDualAction(this);
    m_addDeleteAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("keyframe-add")));
    m_addDeleteAction->setActiveText(i18n("Add keyframe"));
    m_addDeleteAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("keyframe-remove")));
    m_addDeleteAction->setInactiveText(i18n("Delete keyframe"));

    connect(m_addDeleteAction, &KDualAction::triggered, this, &KeyframeWidget::slotAddRemove);
    connect(this, &KeyframeWidget::addRemove, this, &KeyframeWidget::slotAddRemove);

    m_previousKFAction = new QAction(QIcon::fromTheme(QStringLiteral("keyframe-previous")), i18n("Go to previous keyframe"), this);
    connect(m_previousKFAction, &QAction::triggered, this, &KeyframeWidget::slotGoToPrev);
    connect(this, &KeyframeWidget::goToPrevious, this, &KeyframeWidget::slotGoToPrev);

    m_nextKFAction = new QAction(QIcon::fromTheme(QStringLiteral("keyframe-next")), i18n("Go to next keyframe"), this);
    connect(m_nextKFAction, &QAction::triggered, this, &KeyframeWidget::slotGoToNext);
    connect(this, &KeyframeWidget::goToNext, this, &KeyframeWidget::slotGoToNext);

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

    m_applyAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-paste")), i18n("Apply current position value to selected keyframes"), this);

    // Keyframe type widget
    m_selectType = new KSelectAction(QIcon::fromTheme(QStringLiteral("linear")), i18n("Keyframe interpolation"), this);
    QMap<KeyframeType::KeyframeEnum, QAction *> kfTypeHandles;
    const auto cmap = KeyframeModel::getKeyframeTypes();
    for (auto it = cmap.cbegin(); it != cmap.cend(); it++) { // Order is fixed due to the nature of <map>
        QAction *tmp = new QAction(QIcon::fromTheme(KeyframeModel::getIconByKeyframeType(it.key())), it.value(), this);
        tmp->setData(int(it.key()));
        tmp->setCheckable(true);
        kfTypeHandles.insert(it.key(), tmp);
        m_selectType->addAction(kfTypeHandles[it.key()]);
    }
    m_selectType->setCurrentAction(kfTypeHandles[KeyframeType::Linear]);
    connect(m_selectType, &KSelectAction::actionTriggered, this, &KeyframeWidget::slotEditKeyframeType);
    m_selectType->setToolBarMode(KSelectAction::MenuMode);
    m_selectType->setToolTip(i18n("Keyframe interpolation"));
    m_selectType->setWhatsThis(xi18nc("@info:whatsthis", "Keyframe interpolation. This defines which interpolation will be used for the current keyframe."));

    m_toolbar = new QToolBar(this);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setIconSize(QSize(size, size));

    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    connect(monitor, &Monitor::seekPosition, this, &KeyframeWidget::monitorSeek, Qt::DirectConnection);
    connect(pCore.get(), &Core::disconnectEffectStack, this, &KeyframeWidget::disconnectEffectStack);

    m_time = new TimecodeDisplay(this);
    m_time->setRange(0, duration - 1);

    m_toolbar->addAction(m_previousKFAction);
    m_toolbar->addAction(m_addDeleteAction);
    m_toolbar->addAction(m_nextKFAction);
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
    QAction *curve2 = new QAction(QIcon::fromTheme(KeyframeModel::getIconByKeyframeType(KeyframeType::CurveSmooth)),
                                  KeyframeModel::getKeyframeTypes().value(KeyframeType::CurveSmooth), this);
    curve2->setData(int(KeyframeType::CurveSmooth));
    curve2->setCheckable(true);
    kfType->addAction(curve2);
    switch (KdenliveSettings::defaultkeyframeinterp()) {
    case int(KeyframeType::Discrete):
        kfType->setCurrentAction(discrete2);
        break;
    case int(KeyframeType::Curve):
    case int(KeyframeType::CurveSmooth):
        kfType->setCurrentAction(curve2);
        break;
    default:
        kfType->setCurrentAction(linear2);
        break;
    }
    connect(kfType, &KSelectAction::actionTriggered, this, [&](QAction *ac) { KdenliveSettings::setDefaultkeyframeinterp(ac->data().toInt()); });

    // rotoscoping only supports linear keyframes
    if (m_model->getAssetId() == QLatin1String("rotoscoping")) {
        m_selectType->setVisible(false);
        m_selectType->setCurrentAction(kfTypeHandles[KeyframeType::Linear]);
        kfType->setVisible(false);
        kfType->setCurrentAction(linear2);
    }

    // Auto keyframe limit
    QAction *autoLimit = new QAction(QIcon::fromTheme(QStringLiteral("keyframe-duplicate")), i18n("Limit automatic keyframes"), this);
    autoLimit->setCheckable(true);
    autoLimit->setChecked(KdenliveSettings::limitAutoKeyframes() > 0);
    connect(autoLimit, &QAction::toggled, this, [this](bool toggled) {
        if (toggled) {
            KdenliveSettings::setLimitAutoKeyframes(KdenliveSettings::limitAutoKeyframesInterval());
        } else {
            KdenliveSettings::setLimitAutoKeyframes(0);
        }
    });

    // Menu toolbutton
    auto *menuAction = new KActionMenu(QIcon::fromTheme(QStringLiteral("application-menu")), i18n("Options"), this);
    menuAction->setWhatsThis(
        xi18nc("@info:whatsthis", "Opens a list of further actions for managing keyframes (for example: copy to and pasting keyframes from clipboard)."));
    menuAction->setPopupMode(QToolButton::InstantPopup);
    menuAction->addAction(seekKeyframe);
    menuAction->addAction(copy);
    menuAction->addAction(paste);
    menuAction->addAction(m_applyAction);
    menuAction->addSeparator();
    menuAction->addAction(kfType);
    menuAction->addAction(removeNext);
    menuAction->addAction(autoLimit);
    m_toolbar->addAction(menuAction);

    m_editorviewcontainer->addWidget(m_keyframeview);
    m_editorviewcontainer->addWidget(m_curveeditorcontainer);
    // Show standard keyframe editor by default
    m_editorviewcontainer->setCurrentIndex(0);
    m_keyframeview->slotOnFocus();
    m_layout->addRow(m_editorviewcontainer);
    auto *hlay = new QHBoxLayout;
    hlay->addWidget(m_toolbar);
    hlay->addWidget(m_time);
    hlay->addStretch();
    hlay->addWidget(m_viewswitch);
    m_layout->addRow(hlay);

    connect(m_time, &TimecodeDisplay::timeCodeEditingFinished, this, [&]() { slotSetPosition(-1, true); });
    connect(m_keyframeview, &KeyframeView::seekToPos, this, &KeyframeWidget::slotSeekToPos);
    connect(m_keyframeview, &KeyframeView::atKeyframe, this, &KeyframeWidget::slotAtKeyframe);
    connect(m_keyframeview, &KeyframeView::modified, this, &KeyframeWidget::slotRefreshParams);
    connect(m_keyframeview, &KeyframeView::activateEffect, this, &KeyframeWidget::activateEffect);
    connect(m_keyframeview, &KeyframeView::goToNext, this, &KeyframeWidget::slotGoToNext);
    connect(m_keyframeview, &KeyframeView::goToPrevious, this, &KeyframeWidget::slotGoToPrev);
    connect(this, &KeyframeWidget::onKeyframeView, m_keyframeview, &KeyframeView::slotOnFocus);
    connect(this, &KeyframeWidget::onCurveEditorView, m_keyframeview, &KeyframeView::slotLoseFocus);

    connect(m_centerAction, &QAction::triggered, m_keyframeview, &KeyframeView::slotCenterKeyframe);
    connect(m_applyAction, &QAction::triggered, this, [this]() {
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
        QMultiMapIterator<QPersistentModelIndex, QString> i(paramList);
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
        for (auto c : std::as_const(cbs)) {
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

    QMargins mrg = m_layout->contentsMargins();
    m_editorviewcontainer->setFixedHeight(m_editorviewcontainer->currentWidget()->height());
    // m_baseHeight = m_editorviewcontainer->height() + m_toolbar->sizeHint().height();
    m_addedHeight = mrg.top() + mrg.bottom();
    addParameter(index);
    setFixedHeight(m_baseHeight + m_addedHeight);
    Q_EMIT updateHeight();
}

KeyframeWidget::~KeyframeWidget() {}

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
    qDebug() << "===============0\nKFRWIDGET REFRESH!!!!!!!!!!!!!!!!";
    int pos = getPosition();
    KeyframeType::KeyframeEnum keyType = m_keyframes->keyframeType(GenTime(pos, pCore->getCurrentFps()));
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
            qDebug() << "=== QUERY REFRESH FOR: " << val;
            const QStringList vals = val.split(QLatin1Char(' '));
            QRect rect;
            double opacity = -1;
            if (vals.count() >= 4) {
                rect = QRect(vals.at(0).toInt(), vals.at(1).toInt(), vals.at(2).toInt(), vals.at(3).toInt());
                if (vals.count() > 4) {
                    opacity = vals.at(4).toDouble();
                }
            }
            if (m_geom) {
                qDebug() << "=== QUERY REFRESH DONE FOR: " << val;
                m_geom->setValue(rect, opacity, getPosition());
            } else {
                qDebug() << "=== QUERY REFRESH FAILED!!!: " << val;
            }
        } else if (type == ParamType::ColorWheel) {
            (static_cast<LumaLiftGainParam *>(w.second)->slotRefresh(pos));
        } else if (type == ParamType::Color) {
            const QString value = m_keyframes->getInterpolatedValue(pos, w.first).toString();
            (static_cast<ChooseColorWidget *>(w.second)->slotColorModified(QColorUtils::stringToColor(value)));
        }
    }
    if (m_monitorHelper && m_model->isActive() && isEnabled()) {
        m_monitorHelper->refreshParams(pos);
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
    for (auto &i : std::as_const(m_curveeditorview)) {
        i->slotSetPosition(pos, true);
    }
    slotAtKeyframe(m_keyframes->hasKeyframe(pos + pCore->getItemIn(m_keyframes->getOwnerId())), m_keyframes->singleKeyframe());
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
    m_centerAction->setEnabled(!atKeyframe && getCurrentView() == 0);
    Q_EMIT updateEffectKeyframe(atKeyframe || singleKeyframe, !m_monitorActive);
    bool enableWidgets = (m_monitorActive && atKeyframe) || singleKeyframe;
    m_selectType->setEnabled(enableWidgets);
    if (m_geom) {
        m_geom->setEnabled(enableWidgets);
    }
    for (const auto &w : m_parameters) {
        if (w.second) {
            w.second->setEnabled(enableWidgets);
        }
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
    setDuration(duration);
    m_time->setRange(0, duration - 1);
    if (m_model->monitorId == Kdenlive::ProjectMonitor) {
        monitorSeek(pCore->getMonitorPosition());
    } else {
        int pos = m_time->getValue();
        bool isInRange = pos >= in && pos < out;
        qDebug() << "::: MONITOR SEEK FROM KFRW2: " << (isInRange && m_model->isActive());
        connectMonitor(isInRange && m_model->isActive());
        m_addDeleteAction->setEnabled(isInRange && pos > in);
        int framePos = qBound(in, pos, out) - in;
        if (isInRange && framePos != m_time->getValue()) {
            slotSetPosition(framePos, false);
        }
    }
    slotRefreshParams();
}

void KeyframeWidget::setDuration(int duration)
{
    slotAtKeyframe(m_keyframes->hasKeyframe(getPosition()), m_keyframes->singleKeyframe());
    // Unselect keyframes that are outside range if any
    QVector<int> toDelete;
    int kfrIx = 0;
    int offset = pCore->getItemIn(m_keyframes->getOwnerId());
    for (auto &p : m_keyframes->selectedKeyframes()) {
        int kfPos = m_keyframes->getPosAtIndex(p).frames(pCore->getCurrentFps());
        if (kfPos < offset || kfPos >= offset + duration) {
            toDelete << kfrIx;
        }
        kfrIx++;
    }
    for (auto &p : toDelete) {
        m_keyframes->removeFromSelected(p);
    }
    m_keyframeview->setDuration(duration);
    for (auto &i : m_curveeditorview) {
        i->setDuration(duration);
    }
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
    setDuration(duration);
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

    qDebug() << "::::::PARAM ADDED:" << name << static_cast<int>(type) << comment << suffix;
    // create KeyframeCurveEditor(s) which controls the current parameter
    if (type == ParamType::AnimatedRect) {
        QVector<QString> tabname = QVector<QString>() << i18n("X position") << i18n("Y position") << i18n("Width") << i18n("Height");
        if (m_model->data(index, AssetParameterModel::OpacityRole).toBool()) {
            tabname.append(i18n("Opacity"));
        }
        for (int i = 0; i < tabname.size(); i++) {
            addCurveEditor(index, tabname[i], i);
        }
    } else if (type == ParamType::KeyframeParam) { // other types which support curve editors
        addCurveEditor(index);
    }

    // Construct object
    QLabel *labelWidget = nullptr;
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
        m_geom.reset(new GeometryWidget(pCore->getMonitor(m_model->monitorId), range, rect, opacity, m_sourceFrameSize, false,
                                        m_model->data(m_index, AssetParameterModel::OpacityRole).toBool(), this, m_layout));
        connect(m_geom.get(), &GeometryWidget::valueChanged, this, [this, index](const QString &v, int ix, int frame) {
            Q_EMIT activateEffect();
            m_keyframes->updateKeyframe(GenTime(frame, pCore->getCurrentFps()), QVariant(v), ix, index);
        });
        connect(m_geom.get(), &GeometryWidget::updateMonitorGeometry, this, [this](const QRect r) {
            if (m_model->isActive()) {
                pCore->getMonitor(m_model->monitorId)->setUpEffectGeometry(r);
            }
        });
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
                                             m_model->data(index, AssetParameterModel::OddRole).toBool(),
                                             m_model->data(index, AssetParameterModel::CompactRole).toBool(), this);
        connect(doubleWidget, &DoubleWidget::valueChanged, this, [this, index](double v) {
            Q_EMIT activateEffect();
            m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(v), -1, index);
        });
        doubleWidget->setDragObjectName(QString::number(index.row()));
        paramWidget = doubleWidget;
        labelWidget = doubleWidget->createLabel();
    }
    if (paramWidget) {
        m_parameters[index] = paramWidget;
        if (labelWidget) {
            m_layout->addRow(labelWidget, paramWidget);
        } else {
            m_layout->addRow(paramWidget);
        }
        m_addedHeight += paramWidget->minimumHeight();
        setFixedHeight(m_baseHeight + m_addedHeight);
    } else {
        m_parameters[index] = nullptr;
    }
}

void KeyframeWidget::slotInitMonitor(bool active, bool)
{
    connectMonitor(active);
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    if (m_keyframeview) {
        m_keyframeview->initKeyframePos();
        connect(monitor, &Monitor::updateScene, m_keyframeview, &KeyframeView::slotModelChanged, Qt::UniqueConnection);
    }
    for (auto &i : std::as_const(m_curveeditorview)) {
        connect(monitor, &Monitor::updateScene, i, &KeyframeCurveEditor::slotModelChanged, Qt::UniqueConnection);
    }
    if (m_monitorHelper) {
        m_monitorHelper->refreshParams(getPosition());
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

    if (m_monitorActive != active) {
        Monitor *monitor = pCore->getMonitor(m_model->monitorId);
        if (active) {
            connect(monitor, &Monitor::addRemoveKeyframe, this, &KeyframeWidget::slotAddRemove, Qt::UniqueConnection);
            connect(monitor, &Monitor::seekToKeyframe, this, &KeyframeWidget::slotSeekToKeyframe, Qt::UniqueConnection);
            connect(this, &KeyframeWidget::updateEffectKeyframe, monitor, &Monitor::setEffectKeyframe, Qt::DirectConnection);
        } else {
            disconnect(monitor, &Monitor::addRemoveKeyframe, this, &KeyframeWidget::slotAddRemove);
            disconnect(monitor, &Monitor::seekToKeyframe, this, &KeyframeWidget::slotSeekToKeyframe);
        }
    }
    m_monitorActive = active;
    if (m_geom) {
        m_geom->connectMonitor(active, m_keyframes->singleKeyframe());
        if (active) {
            m_keyframeview->initKeyframePos();
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
                if (m_model->monitorId == Kdenlive::ClipMonitor) {
                    // Clip monitor does not always refresh on first keyframe for some reason
                    pCore->getMonitor(m_model->monitorId)->forceMonitorRefresh();
                }
                return;
            }
        }
        // Next add keyframe at playhead position
        m_keyframes->addKeyframe(pos, KeyframeType::Linear);
        m_keyframes->updateKeyframe(pos, res, -1, index);
        return;
    }
    int framePos = getPosition();
    GenTime pos(framePos, pCore->getCurrentFps());
    if (!m_keyframes->singleKeyframe() && KdenliveSettings::autoKeyframe() && m_neededScene == MonitorSceneType::MonitorSceneRoto) {
        if (!m_keyframes->hasKeyframe(framePos)) {
            // Auto add keyframe only if there already is more than 1 keyframe
            m_keyframes->addKeyframe(pos, KeyframeType::Linear);
        } else if (m_monitorHelper && m_monitorHelper->isPlaying()) {
            // Don't try to modify a keyframe when playing in roto monitor
            return;
        }
    }
    if (m_keyframes->hasKeyframe(framePos) || m_keyframes->singleKeyframe()) {
        m_keyframes->updateKeyframe(pos, res, -1, index);
    } else {
        qDebug() << "==== NO KFR AT: " << framePos;
    }
}

MonitorSceneType KeyframeWidget::requiredScene() const
{
    qDebug() << "// // // RESULTING REQUIRED SCENE: " << m_neededScene;
    return m_neededScene;
}

bool KeyframeWidget::keyframesVisible() const
{
    return m_editorviewcontainer->isVisible();
}

void KeyframeWidget::showKeyframes(bool enable)
{
    if (enable && m_toolbar->isVisible()) {
        return;
    }
    m_toolbar->setVisible(enable);
    m_editorviewcontainer->setVisible(enable);
    m_time->setVisible(enable);
    m_viewswitch->setVisible(enable);
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
    for (const auto &entry : std::as_const(list)) {
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
                    for (const auto &entry : std::as_const(list)) {
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
            const QMap<int, QVariant> values = storedValues.value(paramName);
            int offset = values.firstKey();
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
    const QString values = clipboard->text();
    // Check if there is some valid data in clipboard
    auto json = QJsonDocument::fromJson(values.toUtf8());
    if (!json.isArray()) {
        if (!values.contains(QLatin1Char('=')) || !values.contains(QLatin1Char(';'))) {
            // No valid keyframe data
            KMessageBox::information(this, i18n("No keyframe data in clipboard"));
            return;
        }
    }
    QList<QPersistentModelIndex> indexes;
    for (const auto &w : m_parameters) {
        indexes << w.first;
    }
    if (m_neededScene == MonitorSceneRoto) {
        indexes << m_monitorHelper->getIndexes();
    }
    QPointer<KeyframeImport> import = new KeyframeImport(values, m_model, indexes, m_model->data(m_index, AssetParameterModel::ParentInRole).toInt(),
                                                         m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(), this);
    connect(import, &KeyframeImport::updateQmlView, this, [this](QPersistentModelIndex ix, const QString animData) {
        auto kfModel = m_keyframes->getKeyModel(ix);
        if (kfModel) {
            kfModel->parseAnimProperty(animData);
        }
    });
    import->updateView();
    import->show();
}

void KeyframeWidget::slotAddRemove(bool addOnly)
{
    Q_EMIT activateEffect();
    int position = getPosition();
    if (m_keyframes->hasKeyframe(position)) {
        if (addOnly) {
            // Do nothing
            return;
        }
        QVector<int> selectedPositions;
        for (auto &kf : m_keyframes->selectedKeyframes()) {
            if (kf > 0) {
                selectedPositions << m_keyframes->getPosAtIndex(kf).frames(pCore->getCurrentFps());
            }
        }
        if (selectedPositions.contains(position)) {
            // Delete all selected keyframes
            slotRemoveKeyframe(selectedPositions);
        } else {
            slotRemoveKeyframe({position});
        }
    } else {
        // when playing, limit the keyframe interval
        bool addOnPlay = m_monitorHelper && m_monitorHelper->isPlaying();
        if (addOnPlay && KdenliveSettings::limitAutoKeyframes() > 0) {
            if (m_lastKeyframePos < 0) {
                m_lastKeyframePos = position;
            } else if (position < m_lastKeyframePos) {
                m_lastKeyframePos = -1;
            } else if (position - m_lastKeyframePos < KdenliveSettings::limitAutoKeyframes()) {
                // Abort keyframe
                return;
            } else {
                // Proceed
                m_lastKeyframePos = position;
            }
        }
        if (slotAddKeyframe(position) && !addOnPlay) {
            GenTime pos(position, pCore->getCurrentFps());
            int currentIx = m_keyframes->getIndexForPos(pos);
            if (currentIx > -1) {
                m_keyframes->setSelectedKeyframes({currentIx});
                m_keyframes->setActiveKeyframe(currentIx);
            }
        }
    }
}
bool KeyframeWidget::slotAddKeyframe(int pos)
{
    if (pos < 0) {
        pos = getPosition();
    }
    return m_keyframes->addKeyframe(GenTime(pos, pCore->getCurrentFps()), KeyframeType::KeyframeEnum(KdenliveSettings::defaultkeyframeinterp()));
}
void KeyframeWidget::slotRemoveKeyframe(const QVector<int> &positions)
{
    if (m_keyframes->singleKeyframe()) {
        // Don't allow zero keyframe
        pCore->displayMessage(i18n("Cannot remove the last keyframe"), MessageType::ErrorMessage, 500);
        return;
    }
    Fun undo = []() { return true; };
    Fun redo = []() { return true; };
    for (int pos : positions) {
        if (pos == 0) {
            // Don't allow moving first keyframe
            continue;
        }
        m_keyframes->removeKeyframeWithUndo(GenTime(pos, pCore->getCurrentFps()), undo, redo);
    }
    pCore->pushUndo(undo, redo, i18np("Remove keyframe", "Remove keyframes", positions.size()));
}

void KeyframeWidget::slotGoToPrev()
{
    Q_EMIT activateEffect();
    bool ok;
    int position = getPosition();
    if (position == 0 || m_time->getValue() == 0) {
        // No keyframe before
        return;
    }

    int offset = pCore->getItemIn(m_keyframes->getOwnerId());
    auto prev = m_keyframes->getPrevKeyframe(GenTime(position, pCore->getCurrentFps()), &ok);

    if (ok) {
        slotSeekToPos(qMax(0, int(prev.first.frames(pCore->getCurrentFps())) - offset));
    } else {
        // Seek to start
        slotSeekToPos(0);
    }
}
void KeyframeWidget::slotGoToNext()
{
    Q_EMIT activateEffect();
    bool ok;
    int duration = m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(&ok);
    if (m_time->getValue() == duration - 1) {
        // Already at end
        return;
    }

    int position = getPosition();
    int offset = pCore->getItemIn(m_keyframes->getOwnerId());
    auto next = m_keyframes->getNextKeyframe(GenTime(position, pCore->getCurrentFps()), &ok);

    if (ok) {
        slotSeekToPos(qMin(int(next.first.frames(pCore->getCurrentFps())) - offset, duration - 1));
    } else {
        // Seek to end
        slotSeekToPos(duration - 1);
    }
}

void KeyframeWidget::slotRemoveNextKeyframes()
{
    int pos = m_time->getValue() + m_model->data(m_index, AssetParameterModel::ParentInRole).toInt();
    m_keyframes->removeNextKeyframes(GenTime(pos, pCore->getCurrentFps()));
}

void KeyframeWidget::slotSeekToKeyframe(int ix, int offset)
{
    if (offset > 0) {
        slotGoToNext();
        return;
    }
    if (offset < 0) {
        slotGoToPrev();
        return;
    }
    int pos = m_keyframes->getPosAtIndex(ix).frames(pCore->getCurrentFps()) - m_model->data(m_index, AssetParameterModel::ParentInRole).toInt();
    slotSetPosition(pos, true);
}
void KeyframeWidget::slotSeekToPos(int pos)
{
    int in = m_model->data(m_index, AssetParameterModel::InRole).toInt();
    bool canHaveZone = m_model->getOwnerId().type == KdenliveObjectType::Master || m_model->getOwnerId().type == KdenliveObjectType::TimelineTrack;
    if (pos < 0) {
        m_time->setValue(0);
        m_keyframeview->slotSetPosition(0, true);
    } else {
        m_time->setValue(qMax(0, pos - in));
        m_keyframeview->slotSetPosition(pos, true);
        for (auto &i : std::as_const(m_curveeditorview)) {
            i->slotSetPosition(pos, true);
        }
    }
    slotAtKeyframe(m_keyframes->hasKeyframe(pos + pCore->getItemIn(m_keyframes->getOwnerId())), m_keyframes->singleKeyframe());
    m_addDeleteAction->setEnabled(pos > 0);
    slotRefreshParams();

    Q_EMIT seekToPos(pos + (canHaveZone ? in : 0));
}

int KeyframeWidget::getCurrentView()
{
    // 0 for KeyframeView, 1 for KeyframeCurveEditor
    return m_editorviewcontainer->currentIndex();
}

void KeyframeWidget::slotToggleView()
{
    int cur = m_editorviewcontainer->currentIndex();
    int height = m_editorviewcontainer->height();
    switch (cur) {
    case 0:
        m_editorviewcontainer->setCurrentIndex(1);
        m_curveeditorcontainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        // m_keyframeview->setSint offset = pCore->getItemIn(m_keyframes->getOwnerId());lotaddizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        if (m_curveContainerHeight <= 0) { // initialize curve editor widget base height
            m_curveContainerHeight = m_curveeditorview.last()->height() + m_curveeditorcontainer->height();
        }
        height = m_curveContainerHeight;
        m_curveeditorcontainer->setFixedHeight(height);
        m_centerAction->setEnabled(false);
        Q_EMIT onCurveEditorView();
        break;
    case 1:
        m_editorviewcontainer->setCurrentIndex(0);
        m_keyframeview->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        m_curveeditorcontainer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        height = m_keyframeview->height();
        m_centerAction->setEnabled(!m_keyframes->hasKeyframe(getPosition()));
        Q_EMIT onKeyframeView();
        break;
    case -1:
    default:
        qDebug() << ":::: VIEW WIDGET NOT INITIALIZED CORRECTLY";
        break;
    }
    m_editorviewcontainer->setFixedHeight(height);
    m_baseHeight = height + m_toolbar->sizeHint().height();
    qDebug() << "::::::HEIGHT" << m_editorviewcontainer->currentWidget()->height() << m_editorviewcontainer->height();
    setFixedHeight(m_addedHeight + m_baseHeight);
    Q_EMIT updateHeight();
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

void KeyframeWidget::addCurveEditor(const QPersistentModelIndex &index, QString name, int rectindex)
{
    if (!m_toggleViewAction->isEnabled()) {
        m_toggleViewAction->setEnabled(true);
    }
    if (name.isEmpty()) {
        name = m_model->data(index, Qt::DisplayRole).toString();
    }
    int duration = m_model->data(index, AssetParameterModel::ParentDurationRole).toInt();
    double min = -99000.0, max = 99000.0, factor = 1.0;
    if (rectindex == -1) {
        factor = m_model->data(index, AssetParameterModel::FactorRole).toDouble();
        factor = qFuzzyIsNull(factor) ? 1.0 : factor;
        min = m_model->data(index, AssetParameterModel::MinRole).toDouble();
        max = m_model->data(index, AssetParameterModel::MaxRole).toDouble();
    } else if (rectindex == 2 || rectindex == 3) { // width and height
        min = 1;
    } else if (rectindex == 4) { // opacity
        min = 0;
        max = 1;
    }
    KeyframeCurveEditor *tmpkce = new KeyframeCurveEditor(m_keyframes, duration, min, max, factor, index, rectindex, this);
    m_curveeditorview.append(tmpkce);
    connect(this, &KeyframeWidget::onCurveEditorView, m_curveeditorview.last(), &KeyframeCurveEditor::slotOnFocus);
    connect(this, &KeyframeWidget::onKeyframeView, m_curveeditorview.last(), &KeyframeCurveEditor::slotLoseFocus);
    connect(m_curveeditorview.last(), &KeyframeCurveEditor::atKeyframe, this, &KeyframeWidget::slotAtKeyframe);
    connect(m_curveeditorview.last(), &KeyframeCurveEditor::modified, this, &KeyframeWidget::slotRefreshParams);
    connect(m_curveeditorview.last(), &KeyframeCurveEditor::activateEffect, this, &KeyframeWidget::activateEffect);
    connect(m_curveeditorview.last(), &KeyframeCurveEditor::seekToPos, this, &KeyframeWidget::slotSeekToPos);
    // NO slotCenterKeyframe
    m_curveeditorcontainer->addTab(m_curveeditorview.last(), name);
}
