/*
    SPDX-FileCopyrightText: 2011 Till Theato <root@ttill.de>
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractparamwidget.hpp"
#include "curves/keyframe/keyframecurveeditor.h"
#include "definitions.h"
#include <QPersistentModelIndex>
#include <memory>
#include <unordered_map>

class AssetParameterModel;
class DoubleWidget;
class KeyframeView;
class KeyframeCurveEditor;
class KeyframeModelList;
class QVBoxLayout;
class QToolButton;
class QToolBar;
class TimecodeDisplay;
class KSelectAction;
class KeyframeMonitorHelper;
class RotatedRectHelper;
class KDualAction;
class GeometryWidget;
class QStackedWidget;
class QTabWidget;

class KeyframeContainer : public QObject
{
    Q_OBJECT

public:
    explicit KeyframeContainer(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QSize frameSize, QWidget *parent, QFormLayout *layout);
    ~KeyframeContainer() override;

    /** @brief Add a new parameter to be managed using the same keyframe viewer. Also handles creation of KeyframeCurveEditor objects */
    void addParameter(const QPersistentModelIndex &index);
    int getPosition() const;
    /** @brief Returns the monitor scene required for this asset
     */
    SceneType::MonitorSceneType requiredScene() const;
    void resetKeyframes();
    /** @brief Initialize the needed scene and monitor helper for the effect/asset. Should be called before addParameter when setting up the widget. */
    void initNeededSceneAndHelper();

public Q_SLOTS:
    void slotRefresh();
    /** @brief initialize qml overlay
     */
    void slotInitMonitor(bool active, bool);
    void slotAddRemove(bool addOnly);
    void slotSetPosition(int pos = -1, bool update = true);
    /** @brief remove the keyframe at given position
       If pos is negative, we remove keyframe at current position
     */
    void slotRemoveKeyframe(const QVector<int> &positions);
    /** @brief Add a keyframe with given parameter value at given pos.
       If pos is negative, then keyframe is added at current position
    */
    bool slotAddKeyframe(int pos = -1);
    /** @brief Process monitor seek event */
    void connectEffectStack();
    void disconnectEffectStack();

private Q_SLOTS:
    /** @brief Update the value of the widgets to reflect keyframe change */
    void slotRefreshParams();
    void slotUpdateKeyframesFromMonitor(const QPersistentModelIndex &index, const QVariant &res);
    /** @brief Paste a keyframe from clipboard */
    void slotPasteKeyframeFromClipBoard();
    void slotCopySelectedKeyframes();
    void slotCopyKeyframes();
    void slotCopyValueAtCursorPos();
    void slotImportKeyframes();

    // void slotSeekToPos(int pos);
    void monitorSeek(int pos);
    void positionUpdated(int pos);
    void updatedPosition(QList<QPersistentModelIndex> matchingIndexes, QList<QPersistentModelIndex> notMatchingIndexes);

private:
    std::shared_ptr<AssetParameterModel> m_model;
    QModelIndex m_index;
    QWidget *m_parent;
    QToolBar *m_toolbar;
    QToolButton *m_viewswitch;
    std::shared_ptr<KeyframeModelList> m_keyframes;
    std::unique_ptr<KeyframeMonitorHelper> m_monitorHelper;
    int m_lastKeyframePos{-1};
    SceneType::MonitorSceneType m_neededScene;
    bool m_monitorActive{false};
    bool m_isRelative{false};
    QSize m_sourceFrameSize;
    void connectMonitor(bool active);
    void setDuration(int duration);
    std::unordered_map<QPersistentModelIndex, QWidget *> m_parameters;
    std::unordered_map<QPersistentModelIndex, KDualAction *> m_keyframeActions;
    QFormLayout *m_layout;
    std::unique_ptr<GeometryWidget> m_geom;
    QPersistentModelIndex m_geometryIndex;

Q_SIGNALS:
    void addIndex(QPersistentModelIndex ix);
    void setKeyframes(const QString &);
    void updateEffectKeyframe(bool atkeyframe, bool outside);
    void addRemove(bool addOnly = false);
    void onCurveEditorView();
    void onKeyframeView();
    void seekToPos(int);
    void activateEffect();
    void activateEffectParam(int row);
    void activateEffectParamAndSeek(int row, bool forwards);
    void updateHeight();
    void updateAnimCheckBox();
    void disableCurrentFilter(bool);
};
