/*
    SPDX-FileCopyrightText: 2011 Till Theato <root@ttill.de>
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstractparamwidget.hpp"

#include "definitions.h"
#include <QPersistentModelIndex>
#include <memory>
#include <unordered_map>

class AssetParameterModel;
class DoubleWidget;
class KeyframeView;
class KeyframeModelList;
class QVBoxLayout;
class QToolButton;
class QToolBar;
class TimecodeDisplay;
class KSelectAction;
class KeyframeMonitorHelper;
class KDualAction;

class KeyframeWidget : public AbstractParamWidget
{
    Q_OBJECT

public:
    explicit KeyframeWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QSize frameSize, QWidget *parent = nullptr);
    ~KeyframeWidget() override;

    /** @brief Add a new parameter to be managed using the same keyframe viewer */
    void addParameter(const QPersistentModelIndex &index);
    int getPosition() const;
    /** @brief Returns the monitor scene required for this asset
     */
    MonitorSceneType requiredScene() const;
    /** @brief Show / hide keyframe related widgets
     */
    void showKeyframes(bool enable);
    /** @brief Returns true if keyframes options are visible
     */
    bool keyframesVisible() const;
    void resetKeyframes();

public slots:
    void slotRefresh() override;
    /** @brief initialize qml overlay
     */
    void slotInitMonitor(bool active) override;

public slots:
    void slotSetPosition(int pos = -1, bool update = true);

private slots:
    /** @brief Update the value of the widgets to reflect keyframe change */
    void slotRefreshParams();
    void slotAtKeyframe(bool atKeyframe, bool singleKeyframe);
    void slotEditKeyframeType(QAction *action);
    void slotUpdateKeyframesFromMonitor(const QPersistentModelIndex &index, const QVariant &res);
    void slotCopyKeyframes();
    void slotCopyValueAtCursorPos();
    void slotImportKeyframes();
    void slotRemoveNextKeyframes();
    void slotSeekToKeyframe(int ix);
    void monitorSeek(int pos);
    void disconnectEffectStack();

private:
    QVBoxLayout *m_lay;
    QToolBar *m_toolbar;
    std::shared_ptr<KeyframeModelList> m_keyframes;
    KeyframeView *m_keyframeview;
    KeyframeMonitorHelper *m_monitorHelper;
    KDualAction *m_addDeleteAction;
    QAction *m_centerAction;
    QAction *m_copyAction;
    KSelectAction *m_selectType;
    TimecodeDisplay *m_time;
    MonitorSceneType m_neededScene;
    QSize m_sourceFrameSize;
    void connectMonitor(bool active);
    std::unordered_map<QPersistentModelIndex, QWidget *> m_parameters;
    int m_baseHeight;
    int m_addedHeight;

signals:
    void addIndex(QPersistentModelIndex ix);
    void setKeyframes(const QString &);
    void updateEffectKeyframe(bool);
    void goToNext();
    void goToPrevious();
    void addRemove();
};
