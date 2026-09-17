/*
    SPDX-FileCopyrightText: 2011 Till Theato <root@ttill.de>
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "keyframecontainer.hpp"
#include "assets/keyframes/model/corners/cornershelper.hpp"
#include "assets/keyframes/model/dopesheetmodel.hpp"
#include "assets/keyframes/model/keyframemodel.hpp"
#include "assets/keyframes/model/keyframemodellist.hpp"
#include "assets/keyframes/model/rect/recthelper.hpp"
#include "assets/keyframes/model/rect/rotatedrecthelper.hpp"
#include "assets/keyframes/model/rotoscoping/rotohelper.hpp"
#include "assets/keyframes/view/keyframeview.hpp"
#include "assets/model/assetcommand.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "assets/view/widgets/keyframeimport.h"
#include "assets/view/widgets/pointparamwidget.hpp"
#include "core.h"
#include "kdenlivesettings.h"
#include "lumaliftgainparam.hpp"
#include "macros.hpp"
#include "monitor/monitor.h"
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
#include <QHBoxLayout>
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

struct
{
    mlt_keyframe_type t;
    const QChar s;
} keyframe_type_map[] = {
    // Map keyframe type to any single character except numeric values.
    {mlt_keyframe_discrete, QChar('|')},
    {mlt_keyframe_discrete, QChar('!')},
    {mlt_keyframe_linear, QChar()},
    {mlt_keyframe_smooth, QChar('~')},
    {mlt_keyframe_smooth_loose, QChar('~')},
    {mlt_keyframe_smooth_natural, QChar('$')},
    {mlt_keyframe_smooth_tight, QChar('-')},
    {mlt_keyframe_sinusoidal_in, QChar('a')},
    {mlt_keyframe_sinusoidal_out, QChar('b')},
    {mlt_keyframe_sinusoidal_in_out, QChar('c')},
    {mlt_keyframe_quadratic_in, QChar('d')},
    {mlt_keyframe_quadratic_out, QChar('e')},
    {mlt_keyframe_quadratic_in_out, QChar('f')},
    {mlt_keyframe_cubic_in, QChar('g')},
    {mlt_keyframe_cubic_out, QChar('h')},
    {mlt_keyframe_cubic_in_out, QChar('i')},
    {mlt_keyframe_quartic_in, QChar('j')},
    {mlt_keyframe_quartic_out, QChar('k')},
    {mlt_keyframe_quartic_in_out, QChar('l')},
    {mlt_keyframe_quintic_in, QChar('m')},
    {mlt_keyframe_quintic_out, QChar('n')},
    {mlt_keyframe_quintic_in_out, QChar('o')},
    {mlt_keyframe_exponential_in, QChar('p')},
    {mlt_keyframe_exponential_out, QChar('q')},
    {mlt_keyframe_exponential_in_out, QChar('r')},
    {mlt_keyframe_circular_in, QChar('s')},
    {mlt_keyframe_circular_out, QChar('t')},
    {mlt_keyframe_circular_in_out, QChar('u')},
    {mlt_keyframe_back_in, QChar('v')},
    {mlt_keyframe_back_out, QChar('w')},
    {mlt_keyframe_back_in_out, QChar('x')},
    {mlt_keyframe_elastic_in, QChar('y')},
    {mlt_keyframe_elastic_out, QChar('z')},
    {mlt_keyframe_elastic_in_out, QChar('A')},
    {mlt_keyframe_bounce_in, QChar('B')},
    {mlt_keyframe_bounce_out, QChar('C')},
    {mlt_keyframe_bounce_in_out, QChar('D')},
};

static mlt_keyframe_type str_to_keyframe_type(const QChar s)
{
    int map_count = sizeof(keyframe_type_map) / sizeof(*keyframe_type_map);
    for (int i = 0; i < map_count; i++) {
        if (s == keyframe_type_map[i].s) {
            return keyframe_type_map[i].t;
        }
    }
    return mlt_keyframe_linear;
}

KeyframeContainer::KeyframeContainer(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QSize frameSize, QWidget *parent, QFormLayout *layout)
    : QObject(parent)
    , m_model(model)
    , m_index(index)
    , m_parent(parent)
    , m_neededScene(SceneType::MonitorSceneDefault)
    , m_sourceFrameSize(frameSize.isValid() && !frameSize.isNull() ? frameSize : pCore->getCurrentFrameSize())
    , m_layout(layout)
{
    connect(pCore->dopeSheetModel().get(), &DopeSheetModel::matchingKeyframes, this, &KeyframeContainer::updatedPosition);
    connect(pCore.get(), &Core::connectEffectStack, this, &KeyframeContainer::connectEffectStack, Qt::DirectConnection);
    connect(pCore.get(), &Core::disconnectEffectStack, this, &KeyframeContainer::disconnectEffectStack, Qt::DirectConnection);

    m_model->prepareKeyframes();
    m_keyframes = m_model->getKeyframeModel();

    bool isColorWheel = m_model->data(index, AssetParameterModel::TypeRole).value<ParamType>() == ParamType::ColorWheel;
    if (isColorWheel) {
        addParameter(index);
    }
}

KeyframeContainer::~KeyframeContainer() {}

void KeyframeContainer::disconnectEffectStack()
{
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    disconnect(monitor, &Monitor::seekPosition, this, &KeyframeContainer::monitorSeek);
}

void KeyframeContainer::connectEffectStack()
{
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    connect(monitor, &Monitor::seekPosition, this, &KeyframeContainer::monitorSeek, Qt::DirectConnection);
}

void KeyframeContainer::monitorSeek(int pos)
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
    if (isInRange) {
        int framePos = qBound(in, pos, out) - in;
        slotSetPosition(framePos, false);
    }
}

void KeyframeContainer::slotRefreshParams()
{
    int pos = getPosition();
    Q_EMIT updateAnimCheckBox();
    for (const auto &w : m_parameters) {
        auto type = m_model->data(w.first, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::AnimatedFakePoint || type == ParamType::AnimatedPoint) {
            const QString val = m_keyframes->getInterpolatedValue(pos, w.first).toString();
            const QStringList vals = val.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            QPointF point;
            if (vals.size() > 1) {
                point = QPointF(vals.at(0).toDouble(), vals.at(1).toDouble());
            }
            (static_cast<PointParamWidget *>(w.second))->setValue(point);
        } else if (type == ParamType::KeyframeParam) {
            (static_cast<DoubleWidget *>(w.second))->setValue(m_keyframes->getInterpolatedValue(pos, w.first).toDouble());
        } else if (type == ParamType::AnimatedRect || type == ParamType::AnimatedFakeRect) {
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
            if (m_geom) {
                m_geom->setValue(rect, opacity, pos);
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
    qDebug() << ":::: REFRESHING PARAMNS....";
    if (m_monitorHelper && m_model->isActive() /*&& m_curveeditorcontainer->isEnabled()*/) {
        qDebug() << ":::: REFRESHING MONITORHELPER PARAMNS....";
        m_monitorHelper->refreshParams(pos);
    }
}
void KeyframeContainer::slotSetPosition(int pos, bool update)
{
    slotRefreshParams();
}

int KeyframeContainer::getPosition() const
{
    int pos = pCore->getMonitorPosition(m_model->monitorId);
    int itemPos = pCore->getItemPosition(m_keyframes->getOwnerId());
    return pos - itemPos + (m_isRelative ? 0 : pCore->getItemIn(m_model->getOwnerId()));
}

void KeyframeContainer::updatedPosition(QList<QPersistentModelIndex> matchingIndexes, QList<QPersistentModelIndex> notMatchingIndexes)
{
    int pos = pCore->getMonitorPosition(m_model->monitorId);
    bool inside = pCore->itemContainsPos(m_keyframes->getOwnerId(), pos);
    for (auto &index : matchingIndexes) {
        if (!m_parameters.contains(index)) {
            qDebug() << "Dope parameter with index not found: " << index;
            continue;
        }
        auto w = m_parameters[index];
        if (w) {
            auto tb = m_keyframeActions[index];
            tb->setActive(inside);
            auto abstractParam = qobject_cast<AbstractParamWidget *>(w);
            if (abstractParam) {
                abstractParam->setParamState(inside, m_keyframes->keyframesCount(index) == 1);
            } else {
                auto doubleParam = qobject_cast<DoubleWidget *>(w);
                if (doubleParam) {
                    Q_EMIT doubleParam->setParamState(inside, m_keyframes->keyframesCount(index) == 1);
                } else {
                    qDebug() << "::: COULD NOT CONVERT PARAM TO ABSTRACT ON: " << m_model->getAssetId();
                    w->setEnabled(inside);
                }
            }
        } else {
            qDebug() << "::: MISSING WIDGET FOR IX: " << index << " / GEOM: " << m_geometryIndex;
        }
    }
    for (auto &index : notMatchingIndexes) {
        if (!m_parameters.contains(index)) {
            qDebug() << "Dope parameter with index not found: " << index;
            continue;
        }
        auto w = m_parameters[index];
        if (w) {
            auto tb = m_keyframeActions[index];
            tb->setActive(false);
            auto abstractParam = qobject_cast<AbstractParamWidget *>(w);
            if (abstractParam) {
                abstractParam->setParamState(false, m_keyframes->keyframesCount(index) == 1);
            } else {
                auto doubleParam = qobject_cast<DoubleWidget *>(w);
                if (doubleParam) {
                    Q_EMIT doubleParam->setParamState(false, m_keyframes->keyframesCount(index) == 1);
                } else {
                    qDebug() << "::: COULD NOT CONVERT PARAM TO ABSTRACT ON: " << m_model->getAssetId();
                    w->setEnabled(false);
                }
            }
        } else {
            qDebug() << "::: MISSING WIDGET FOR IX: " << index << " / GEOM: " << m_geometryIndex;
        }
    }
    if (m_geom) {
        m_geom->setEnabled(inside && matchingIndexes.contains(m_geometryIndex));
    }
}

void KeyframeContainer::positionUpdated(int relativePos)
{
    int pos = pCore->getMonitorPosition(m_model->monitorId);
    bool outside = !pCore->itemContainsPos(m_keyframes->getOwnerId(), pos);
    bool enableParameter;
    if (m_geom) {
        enableParameter = outside ? false : m_keyframes->enableParameter(m_geometryIndex, relativePos);
        m_geom->setEnabled(enableParameter);
    }
    for (const auto &w : m_parameters) {
        if (w.second) {
            enableParameter = outside ? false : m_keyframes->enableParameter(w.first, relativePos);
            w.second->setEnabled(enableParameter);
        }
    }
}

void KeyframeContainer::slotRefresh()
{
    // update duration
    slotRefreshParams();
}

void KeyframeContainer::setDuration(int duration)
{
    int pos = pCore->getMonitorPosition(m_model->monitorId) - pCore->getItemPosition(m_keyframes->getOwnerId()) +
              (m_isRelative ? 0 : pCore->getItemIn(m_keyframes->getOwnerId()));
    positionUpdated(pos);
    // Unselect keyframes that are outside range if any
    QVector<int> toDelete;
    int kfrIx = 0;
    int offset = m_isRelative ? 0 : pCore->getItemIn(m_keyframes->getOwnerId());
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
}

void KeyframeContainer::resetKeyframes()
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
    slotRefreshParams();
}

void KeyframeContainer::initNeededSceneAndHelper()
{
    // Loop over all parameters to determine the needed scene and helper
    m_monitorHelper.reset();
    m_neededScene = SceneType::MonitorSceneDefault;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        QModelIndex index = m_model->index(i, 0);
        auto type = m_model->data(index, AssetParameterModel::TypeRole).value<ParamType>();
        const QString assetId = m_model->getAssetId();
        if (assetId == QLatin1String("qtblend")) {
            m_neededScene = SceneType::MonitorSceneRotatedGeometry;
            m_monitorHelper.reset(new RotatedRectHelper(pCore->getMonitor(m_model->monitorId), m_model, m_parent));
            break;
        } else if (type == ParamType::Roto_spline) {
            m_neededScene = SceneType::MonitorSceneRoto;
            m_monitorHelper.reset(new RotoHelper(pCore->getMonitor(m_model->monitorId), m_model, m_parent));
            break;
        } else if (type == ParamType::AnimatedRect || type == ParamType::AnimatedFakeRect) {
            m_neededScene = SceneType::MonitorSceneGeometry;
            m_monitorHelper.reset(new KeyframeMonitorHelper(pCore->getMonitor(m_model->monitorId), m_model, m_neededScene, m_parent));
            break;
        } else if (assetId == QLatin1String("frei0r.c0rners")) {
            m_neededScene = SceneType::MonitorSceneCorners;
            m_monitorHelper.reset(new CornersHelper(pCore->getMonitor(m_model->monitorId), m_model, m_parent));
            break;
        } else if (assetId == QLatin1String("frei0r.alpha0ps_alphaspot") || assetId.contains(QLatin1String("frei0r.alphaspot"))) {
            m_neededScene = SceneType::MonitorSceneGeometry;
            m_monitorHelper.reset(new RectHelper(pCore->getMonitor(m_model->monitorId), m_model, m_parent));
            break;
        }
    }
    if (m_monitorHelper) {
        connect(this, &KeyframeContainer::addIndex, m_monitorHelper.get(), &KeyframeMonitorHelper::addIndex);
    }
}

void KeyframeContainer::addParameter(const QPersistentModelIndex &index)
{
    // Retrieve parameters from the model
    QString name = m_model->data(index, Qt::DisplayRole).toString();
    QString comment = m_model->data(index, AssetParameterModel::CommentRole).toString();
    QString suffix = m_model->data(index, AssetParameterModel::SuffixRole).toString();
    auto type = m_model->data(index, AssetParameterModel::TypeRole).value<ParamType>();

    // Construct object
    QLabel *labelWidget = nullptr;
    QWidget *paramWidget = nullptr;
    QString paramName = m_model->data(index, AssetParameterModel::NameRole).toString();
    if (type == ParamType::AnimatedPoint || type == ParamType::AnimatedFakePoint) {
        int inPos = m_model->data(index, AssetParameterModel::ParentInRole).toInt();
        QPair<int, int> range(inPos, inPos + m_model->data(index, AssetParameterModel::ParentDurationRole).toInt());
        QPointF point = QPoint();
        if (m_keyframes->hasKeyframes(index) > 0) {
            const QString value = m_keyframes->getInterpolatedValue(getPosition(), index).toString();
            QStringList vals = value.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (vals.count() > 1) {
                point = QPointF(vals.at(0).toDouble(), vals.at(1).toDouble());
            }
        }
        Q_EMIT addIndex(index);
        labelWidget = new QLabel(name, m_parent);
        auto pointWidget = new PointParamWidget(m_model, index, m_parent, point);
        connect(pointWidget, &PointParamWidget::valueChanged, this, [this](QModelIndex ix, QString v, bool) {
            Q_EMIT activateEffect();
            m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(v), -1, ix);
        });
        paramWidget = pointWidget;
    } else if (type == ParamType::AnimatedRect || type == ParamType::AnimatedFakeRect) {
        int inPos = m_model->data(index, AssetParameterModel::ParentInRole).toInt();
        QPair<int, int> range(inPos, inPos + m_model->data(index, AssetParameterModel::ParentDurationRole).toInt());
        const QString value = m_keyframes->getInterpolatedValue(getPosition(), index).toString();
        QRect rect;
        double opacity = 0;
        QStringList vals = value.split(QLatin1Char(' '));
        if (vals.count() > 3) {
            rect = QRect(vals.at(0).toInt(), vals.at(1).toInt(), vals.at(2).toInt(), vals.at(3).toInt());
            if (vals.count() > 4) {
                opacity = vals.at(4).toDouble();
            }
        }
        Q_EMIT addIndex(index);

        // Build add/seek keyframe widget
        QHBoxLayout *kfrLayout = buildKeyframeLayout(m_parent, index);
        // qtblend uses an opacity value in the (0-1) range, while older geometry effects use (0-100)
        m_geom.reset(new GeometryWidget(pCore->getMonitor(m_model->monitorId), range, rect, true, opacity, m_sourceFrameSize, false,
                                        m_model->data(m_index, AssetParameterModel::OpacityRole).toBool(), m_parent, m_layout, kfrLayout));
        m_geometryIndex = index;
        if (m_neededScene == SceneType::MonitorSceneRotatedGeometry) {
            m_geom->setRotatable(true);
        }
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
        auto colorWheelWidget = new LumaLiftGainParam(m_model, index, m_parent);
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
        connect(colorWheelWidget, &LumaLiftGainParam::updateHeight, this, [&](int h) { QTimer::singleShot(100, this, &KeyframeContainer::updateHeight); });
        paramWidget = colorWheelWidget;
    } else if (type == ParamType::Roto_spline) {
        Q_EMIT addIndex(index);
    } else if (type == ParamType::Color) {
        QString value = m_keyframes->getInterpolatedValue(getPosition(), index).toString();
        bool alphaEnabled = m_model->data(index, AssetParameterModel::AlphaRole).toBool();
        labelWidget = new QLabel(name, m_parent);
        auto colorWidget = new ChooseColorWidget(m_parent, QColorUtils::stringToColor(value), alphaEnabled);
        colorWidget->setToolTip(comment);
        connect(colorWidget, &ChooseColorWidget::disableCurrentFilter, this, &KeyframeContainer::disableCurrentFilter);
        connect(colorWidget, &ChooseColorWidget::modified, this, [this, index, alphaEnabled](const QColor &color) {
            Q_EMIT activateEffect();
            m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(QColorUtils::colorToString(color, alphaEnabled)), -1, index);
        });
        paramWidget = colorWidget;
    } else {
        if (m_model->getAssetId() == QLatin1String("frei0r.c0rners")) {
            if (type == ParamType::KeyframeParam) {
                // We're only interested in the first 8 parameters of the corners effect representing x/y coordinates of the corners.
                // frei0r.c0rners named these params as integers, starting from 0 so we can convert then name to an int and do a int comparison.
                int paramNameAsInt = paramName.toInt();
                if (paramNameAsInt < 8) {
                    Q_EMIT addIndex(index);
                }
            }
        }
        if (m_model->getAssetId().contains(QLatin1String("frei0r.alphaspot")) || m_model->getAssetId() == QLatin1String("frei0r.alpha0ps_alphaspot")) {
            if (type == ParamType::KeyframeParam) {
                if (paramName.contains(QLatin1String("Position X")) || paramName.contains(QLatin1String("Position Y")) ||
                    paramName.contains(QLatin1String("Size X")) || paramName.contains(QLatin1String("Size Y"))) {
                    Q_EMIT addIndex(index);
                }
            }
        }
        if (m_model->getAssetId() == QLatin1String("qtblend")) {
            if (paramName == QLatin1String("rotation")) {
                Q_EMIT addIndex(index);
            }
        }

        double value = m_keyframes->getInterpolatedValue(getPosition(), index).toDouble();
        double min = m_model->data(index, AssetParameterModel::MinRole).toDouble();
        double max = m_model->data(index, AssetParameterModel::MaxRole).toDouble();
        double defaultValue = m_model->data(index, AssetParameterModel::DefaultRole).toDouble();
        int decimals = m_model->data(index, AssetParameterModel::DecimalsRole).toInt();
        double factor = m_model->data(index, AssetParameterModel::FactorRole).toDouble();
        factor = qFuzzyIsNull(factor) ? 1 : factor;
        QWidget *container = new QWidget(m_parent);
        auto lay = new QHBoxLayout(container);
        lay->setSpacing(0);
        lay->setContentsMargins(0, 0, 0, 0);
        auto doubleWidget = new DoubleWidget(name, value, min, max, factor, defaultValue, comment, -1, suffix, decimals,
                                             m_model->data(index, AssetParameterModel::OddRole).toBool(),
                                             m_model->data(index, AssetParameterModel::CompactRole).toBool(), m_parent);
        QToolButton *keyframable = new QToolButton(m_parent);
        keyframable->setCheckable(true);
        keyframable->setIcon(QIcon::fromTheme(QStringLiteral("smallclock")));
        keyframable->setToolTip(i18n("Enable keyframes"));
        keyframable->setChecked(!m_model->data(index, AssetParameterModel::BlockedKeyframesRole)
                                     .toStringList()
                                     .contains(m_model->data(index, AssetParameterModel::NameRole).toString()));
        // Update status on change
        connect(this, &KeyframeContainer::updateAnimCheckBox, this, [this, keyframable, index]() {
            QSignalBlocker bk(keyframable);
            keyframable->setChecked(!m_model->data(index, AssetParameterModel::BlockedKeyframesRole)
                                         .toStringList()
                                         .contains(m_model->data(index, AssetParameterModel::NameRole).toString()));
        });

        // React on user toggle
        connect(keyframable, &QToolButton::toggled, this, [this, index](bool checked) {
            if (checked) {
                // Re-enable keyframes, we only need to discard the blocking flag
                QStringList currentBlocked = m_model->data(index, AssetParameterModel::BlockedKeyframesRole).toStringList();
                qDebug() << ":::: CHECKING FOR BLOCKED PARAM FOR IX: " << index << ", NAME: " << m_model->data(index, AssetParameterModel::NameRole).toString();
                currentBlocked.removeAll(m_model->data(index, AssetParameterModel::NameRole).toString());
                currentBlocked.removeDuplicates();

                const QVector<QPair<QString, QVariant>> values = {
                    {QStringLiteral("kdenlive:block_keyframes"), QVariant(currentBlocked.join(QLatin1Char(';')))}};
                auto *command = new AssetUpdateCommand(m_model, values);
                pCore->pushUndo(command);
            } else {
                // Remove all keyframes and flag
                const QStringList initialBlockState = m_model->data(index, AssetParameterModel::BlockedKeyframesRole).toStringList();
                QStringList updatedBlockState = initialBlockState;
                const QString paramName = m_model->data(index, AssetParameterModel::NameRole).toString();
                updatedBlockState.append(paramName);
                updatedBlockState.removeDuplicates();
                Fun undo = []() { return true; };
                Fun redo = []() { return true; };
                // Get current parameter value
                Fun local_undo = [this, initialBlockState, index]() {
                    const QVector<QPair<QString, QVariant>> values = {
                        {QStringLiteral("kdenlive:block_keyframes"), QVariant(initialBlockState.join(QLatin1Char(';')))}};
                    m_model->setParameters(values);
                    return true;
                };
                Fun local_redo = [this, updatedBlockState, index]() {
                    const QVector<QPair<QString, QVariant>> values = {
                        {QStringLiteral("kdenlive:block_keyframes"), QVariant(updatedBlockState.join(QLatin1Char(';')))}};
                    m_model->setParameters(values);
                    return true;
                };
                local_redo();
                auto km = m_keyframes->getKeyModel(index);
                if (km) {
                    km->removeAllKeyframes(undo, redo);
                }
                PUSH_LAMBDA(local_redo, redo);
                PUSH_LAMBDA(local_undo, undo);
                pCore->pushUndo(undo, redo, i18n("Remove and Disable Keyframes"));
            }
        });
        lay->addWidget(keyframable);
        lay->addWidget(doubleWidget);
        QHBoxLayout *kfrLayout = buildKeyframeLayout(m_parent, index);
        lay->addLayout(kfrLayout);

        connect(doubleWidget, &DoubleWidget::valueChanged, this, [this, index](double v) {
            Q_EMIT activateEffect();
            m_keyframes->updateKeyframe(GenTime(getPosition(), pCore->getCurrentFps()), QVariant(v), -1, index);
        });
        if (m_geom) {
            connect(doubleWidget, &DoubleWidget::valueChanged, this, [this](double v) {
                if (m_geom) {
                    m_geom->slotUpdateRotation(v);
                }
            });
        }
        doubleWidget->setDragObjectName(QString::number(index.row()));
        m_parameters[index] = doubleWidget;
        labelWidget = doubleWidget->createLabel();
        m_layout->addRow(labelWidget, container);
        return;
    }
    if (paramWidget) {
        m_parameters[index] = paramWidget;
        QWidget *container = new QWidget(m_parent);
        auto lay = new QHBoxLayout(container);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->addWidget(paramWidget);
        QHBoxLayout *kfrLayout = buildKeyframeLayout(m_parent, index);
        lay->addLayout(kfrLayout);
        if (labelWidget) {
            m_layout->addRow(labelWidget, container);
        } else {
            m_layout->addRow(container);
        }
    } else {
        m_parameters[index] = nullptr;
    }
}

QHBoxLayout *KeyframeContainer::buildKeyframeLayout(QWidget *parent, QPersistentModelIndex index)
{
    QHBoxLayout *kfrLayout = new QHBoxLayout(parent);
    kfrLayout->setSpacing(0);
    QToolButton *goPrev = new QToolButton(parent);
    goPrev->setIcon(QIcon::fromTheme("arrow-left"));
    goPrev->setToolTip(i18n("Go to Previous Keyframe"));
    goPrev->setAutoRaise(true);
    goPrev->setMaximumWidth(goPrev->height() * 0.6);
    kfrLayout->addWidget(goPrev);
    connect(goPrev, &QToolButton::clicked, this, [this, index]() { Q_EMIT activateEffectParamAndSeek(index.row(), false); });
    KDualAction *kfAction = new KDualAction(parent);
    kfAction->setActiveIcon(QIcon::fromTheme(QStringLiteral("task-process-4")));
    kfAction->setActiveText(i18n("Remove Keyframe"));
    kfAction->setInactiveIcon(QIcon::fromTheme(QStringLiteral("task-process-0")));
    kfAction->setInactiveText(i18n("Add Keyframe"));
    QToolButton *tb = new QToolButton(parent);
    tb->setAutoRaise(true);
    tb->setDefaultAction(kfAction);
    kfrLayout->addWidget(tb);
    QToolButton *goNext = new QToolButton(parent);
    goNext->setIcon(QIcon::fromTheme("arrow-right"));
    goNext->setAutoRaise(true);
    goNext->setMaximumWidth(goNext->height() * 0.6);
    goNext->setToolTip(i18n("Go to Next Keyframe"));
    connect(goNext, &QToolButton::clicked, this, [this, index]() { Q_EMIT activateEffectParamAndSeek(index.row(), true); });
    kfrLayout->addWidget(goNext);

    connect(kfAction, &KDualAction::activeChangedByUser, this, [this, index](bool activated) {
        auto km = m_keyframes->getKeyModel(index);
        if (activated) {
            Q_EMIT activateEffectParam(index.row());
            km->addKeyframe(getPosition());
        } else {
            Q_EMIT activateEffectParam(index.row());
            km->removeKeyframe(getPosition());
        }
    });
    m_keyframeActions[index] = kfAction;
    return kfrLayout;
}

void KeyframeContainer::slotInitMonitor(bool active, bool)
{
    connectMonitor(active);
    if (m_monitorHelper) {
        Monitor *monitor = pCore->getMonitor(m_model->monitorId);
        int framePos = monitor->position() - pCore->getItemKeyframeOffset(m_model->getOwnerId());
        m_monitorHelper->refreshParamsWhenReady(framePos);
    }
}

void KeyframeContainer::connectMonitor(bool active)
{
    if (m_monitorHelper) {
        if (m_model->isActive()) {
            if (m_monitorHelper->connectMonitor(active)) {
                connect(m_monitorHelper.get(), &KeyframeMonitorHelper::updateKeyframeData, this, &KeyframeContainer::slotUpdateKeyframesFromMonitor,
                        Qt::UniqueConnection);
                slotRefreshParams();
            }
        } else {
            if (m_monitorHelper->connectMonitor(false)) {
                disconnect(m_monitorHelper.get(), &KeyframeMonitorHelper::updateKeyframeData, this, &KeyframeContainer::slotUpdateKeyframesFromMonitor);
            }
        }
    }

    if (m_monitorActive == active) {
        return;
    }
    Monitor *monitor = pCore->getMonitor(m_model->monitorId);
    if (active) {
        // TODO: reconnect to dopesheet
        //  connect(monitor, &Monitor::addRemoveKeyframe, this, &KeyframeContainer::slotAddRemove, Qt::UniqueConnection);
        //  connect(monitor, &Monitor::seekToKeyframe, this, &KeyframeContainer::slotSeekToKeyframe, Qt::UniqueConnection);
    } else {
        // TODO: reconnect to dopesheet
        //  disconnect(monitor, &Monitor::addRemoveKeyframe, this, &KeyframeContainer::slotAddRemove);
        //  disconnect(monitor, &Monitor::seekToKeyframe, this, &KeyframeContainer::slotSeekToKeyframe);
    }
    m_monitorActive = active;
    if (m_geom) {
        m_geom->connectMonitor(active, m_keyframes->singleKeyframe());
    }
}

void KeyframeContainer::slotUpdateKeyframesFromMonitor(const QPersistentModelIndex &index, const QVariant &res)
{
    Q_EMIT activateEffect();
    QVariant result = res;
    auto monitor = pCore->getMonitor(m_model->monitorId);
    int framePos = monitor->position() - pCore->getItemKeyframeOffset(m_model->getOwnerId());
    if (m_keyframes->isEmpty()) {
        QStringList updated = res.toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (updated.count() == 4) {
            // Check if we need to add opacity
            const QString currentValue = m_model->getKeyframeModel()->getInterpolatedValue(0, index).toString();
            const QStringList parts = currentValue.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.count() == 5) {
                // Add missing opacity
                updated << parts.at(4);
                result = updated.join(QLatin1Char(' '));
            }
        }

        // TODO: GET CURRENT KEYFRAME POSITION
        GenTime pos(((m_isRelative ? 0 : pCore->getItemIn(m_model->getOwnerId()))) /*+ m_time->getValue()*/, pCore->getCurrentFps());
        if (framePos > 0) {
            // First add keyframe at start of the clip
            GenTime pos0(m_isRelative ? 0 : pCore->getItemIn(m_model->getOwnerId()), pCore->getCurrentFps());
            m_keyframes->addKeyframe(pos0, KeyframeType::Linear, index);
            m_keyframes->updateKeyframe(pos0, result, -1, index);
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
        m_keyframes->addKeyframe(pos, KeyframeType::Linear, index);
        m_keyframes->updateKeyframe(pos, result, -1, index);
        return;
    }
    GenTime pos(framePos, pCore->getCurrentFps());
    if (KdenliveSettings::autoKeyframe() && m_neededScene != SceneType::MonitorSceneDefault) {
        if (!m_keyframes->hasKeyframe(framePos)) {
            // Auto add keyframe
            m_keyframes->addKeyframe(pos, KeyframeType::Linear, index);
        } else if (m_monitorHelper && m_monitorHelper->isPlaying()) {
            // Don't try to modify a keyframe when playing in monitor
            return;
        }
    }
    if (m_keyframes->hasKeyframe(framePos) || m_keyframes->singleKeyframe()) {
        QStringList updated = res.toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (updated.count() == 4) {
            // Check if we need to add opacity
            const QString currentValue = m_model->getKeyframeModel()->getInterpolatedValue(framePos, index).toString();
            const QStringList parts = currentValue.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.count() == 5) {
                // Add missing opacity
                updated << parts.at(4);
                result = updated.join(QLatin1Char(' '));
            }
        }
        m_keyframes->updateKeyframe(pos, result, -1, index);
    } else {
        qDebug() << "==== NO KFR AT: " << framePos;
    }
}

SceneType::MonitorSceneType KeyframeContainer::requiredScene() const
{
    qDebug() << "// // // RESULTING REQUIRED SCENE: " << m_neededScene;
    return m_neededScene;
}

void KeyframeContainer::slotCopyKeyframes()
{
    /*QJsonDocument effectDoc = m_model->toJson({}, false);
    if (effectDoc.isEmpty()) {
        return;
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(QString(effectDoc.toJson()));
    pCore->displayMessage(i18n("Keyframes copied"), InformationMessage);*/
}

void KeyframeContainer::slotPasteKeyframeFromClipBoard()
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
    QMap<QString, QMap<std::pair<int, QChar>, QVariant>> storedValues;
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
            QMap<std::pair<int, QChar>, QVariant> values;
            if (kfrType == ParamType::Roto_spline) {
                auto value = entryObj.value(QLatin1String("value"));
                if (value.isObject()) {
                    QJsonObject obj = value.toObject();
                    QStringList keys = obj.keys();
                    for (auto &k : keys) {
                        values.insert({k.toInt(), QChar()}, obj.value(k));
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
                            values.insert({k.toInt(), QChar()}, obj.value(k));
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
                    QChar separator;
                    QString timeVal = val.section(QLatin1Char('='), 0, 0);
                    if (!timeVal.isEmpty() && !timeVal.back().isDigit()) {
                        separator = timeVal.back();
                        timeVal.chop(1);
                    }
                    int position = m_model->time_to_frames(timeVal);
                    values.insert({position, separator}, val.section(QLatin1Char('='), 1));
                }
            }
            storedValues.insert(entryObj[QLatin1String("name")].toString(), values);
        } else {
            const QString value = entryObj.value(QLatin1String("value")).toString();
            QMap<std::pair<int, QChar>, QVariant> values;
            values.insert({0, QChar()}, value);
            storedValues.insert(entryObj[QLatin1String("name")].toString(), values);
        }
    }
    int destPos = getPosition();

    std::vector<QPersistentModelIndex> indexes = m_keyframes->getIndexes();
    for (const auto &ix : indexes) {
        auto paramName = m_model->data(ix, AssetParameterModel::NameRole).toString();
        if (storedValues.contains(paramName)) {
            auto km = m_keyframes->getKeyModel(ix);
            const QMap<std::pair<int, QChar>, QVariant> values = storedValues.value(paramName);
            int offset = values.firstKey().first;
            QMapIterator<std::pair<int, QChar>, QVariant> i(values);
            while (i.hasNext()) {
                i.next();
                mlt_keyframe_type type = str_to_keyframe_type(i.key().second);
                km->addKeyframe(GenTime(destPos + i.key().first - offset, pCore->getCurrentFps()), KeyframeModel::convertFromMltType(type), i.value(), true,
                                undo, redo);
            }
        } else {
            qDebug() << "::: NOT FOUND PARAM: " << paramName << " in list: " << storedValues.keys();
        }
    }
    pCore->pushUndo(undo, redo, i18n("Paste keyframe"));
}

void KeyframeContainer::slotCopySelectedKeyframes()
{
    /*const QVector<int> results = m_keyframeview->selectedKeyframesIndexes();
    QJsonDocument effectDoc = m_model->toJson(results, false);
    if (effectDoc.isEmpty()) {
        pCore->displayMessage(i18n("Cannot copy current parameter values"), InformationMessage);
        return;
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(QString(effectDoc.toJson()));
    pCore->displayMessage(i18n("Current values copied"), InformationMessage);*/
}

void KeyframeContainer::slotCopyValueAtCursorPos()
{
    QJsonDocument effectDoc = m_model->valueAsJson(getPosition(), false);
    if (effectDoc.isEmpty()) {
        return;
    }
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(QString(effectDoc.toJson()));
    pCore->displayMessage(i18n("Current values copied"), InformationMessage);
}

void KeyframeContainer::slotImportKeyframes()
{
    QClipboard *clipboard = QApplication::clipboard();
    const QString values = clipboard->text();
    // Check if there is some valid data in clipboard
    auto json = QJsonDocument::fromJson(values.toUtf8());
    if (!json.isArray()) {
        if (!values.contains(QLatin1Char('=')) || !values.contains(QLatin1Char(';'))) {
            // No valid keyframe data
            KMessageBox::information(m_parent, i18n("No keyframe data in clipboard"));
            return;
        }
    }
    QList<QPersistentModelIndex> indexes;
    for (const auto &w : m_parameters) {
        indexes << w.first;
    }
    if (m_neededScene == SceneType::MonitorSceneRoto) {
        indexes << m_monitorHelper->getIndexes();
    }
    QPointer<KeyframeImport> import = new KeyframeImport(values, m_model, indexes, m_model->data(m_index, AssetParameterModel::ParentInRole).toInt(),
                                                         m_model->data(m_index, AssetParameterModel::ParentDurationRole).toInt(), m_parent);
    connect(import, &KeyframeImport::updateQmlView, this, [this](QPersistentModelIndex ix, const QString animData) {
        auto kfModel = m_keyframes->getKeyModel(ix);
        if (kfModel) {
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            kfModel->parseAnimProperty(animData, -1, -1, undo, redo);
        }
    });
    import->updateView();
    import->show();
}

void KeyframeContainer::slotAddRemove(bool addOnly)
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
bool KeyframeContainer::slotAddKeyframe(int pos)
{
    if (pos < 0) {
        pos = getPosition();
    }
    return m_keyframes->addKeyframe(GenTime(pos, pCore->getCurrentFps()), KeyframeType::KeyframeEnum(KdenliveSettings::defaultkeyframeinterp()));
}
void KeyframeContainer::slotRemoveKeyframe(const QVector<int> &positions)
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

/*void KeyframeContainer::slotSeekToPos(int pos)
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
    positionUpdated(pos + (m_isRelative ? 0 : pCore->getItemIn(m_keyframes->getOwnerId())));
    m_addDeleteAction->setEnabled(pos > 0);
    slotRefreshParams();

    Q_EMIT seekToPos(pos + (canHaveZone ? in : 0));
}*/
