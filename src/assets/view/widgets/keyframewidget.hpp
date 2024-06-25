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
class KDualAction;
class QStackedWidget;
class QTabWidget;

class KeyframeWidget : public AbstractParamWidget
{
    Q_OBJECT

public:
    explicit KeyframeWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QSize frameSize, QWidget *parent = nullptr);
    ~KeyframeWidget() override;

    /** @brief Add a new parameter to be managed using the same keyframe viewer. Also handles creation of KeyframeCurveEditor objects */
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
    int getCurrentView();

public Q_SLOTS:
    void slotRefresh() override;
    /** @brief initialize qml overlay
     */
    void slotInitMonitor(bool active, bool) override;
    /** @brief Activate a standard action passed from the mainwindow, like copy or paste */
    void sendStandardCommand(int command);
    void slotAddRemove();
    void slotGoToNext();
    void slotGoToPrev();
    void slotSetPosition(int pos = -1, bool update = true);
    /** @brief remove the keyframe at given position
       If pos is negative, we remove keyframe at current position
     */
    void slotRemoveKeyframe(const QVector<int> &positions);
    /** @brief Add a keyframe with given parameter value at given pos.
       If pos is negative, then keyframe is added at current position
    */
    bool slotAddKeyframe(int pos = -1);

private Q_SLOTS:
    /** @brief Update the value of the widgets to reflect keyframe change */
    void slotRefreshParams();
    void slotAtKeyframe(bool atKeyframe, bool singleKeyframe);
    void slotEditKeyframeType(QAction *action);
    void slotUpdateKeyframesFromMonitor(const QPersistentModelIndex &index, const QVariant &res);
    /** @brief Paste a keyframe from clipboard */
    void slotPasteKeyframeFromClipBoard();
    void slotCopySelectedKeyframes();
    void slotCopyKeyframes();
    void slotCopyValueAtCursorPos();
    void slotImportKeyframes();
    void slotRemoveNextKeyframes();
    void slotSeekToKeyframe(int ix);
    void slotSeekToPos(int pos);
    void slotToggleView();
    void monitorSeek(int pos);
    void disconnectEffectStack();

private:
    QVBoxLayout *m_lay;
    QToolBar *m_toolbar;
    QToolButton *m_viewswitch;
    std::shared_ptr<KeyframeModelList> m_keyframes;
    KeyframeView *m_keyframeview;
    KeyframeMonitorHelper *m_monitorHelper;
    QTabWidget *m_curveeditorcontainer;
    QVector<KeyframeCurveEditor *> m_curveeditorview;
    QStackedWidget *m_editorviewcontainer;
    KDualAction *m_addDeleteAction;
    KDualAction *m_toggleViewAction;
    QAction *m_centerAction;
    QAction *m_copyAction;
    QAction *m_pasteAction;
    QAction *m_previousKFAction;
    QAction *m_nextKFAction;
    QAction *m_applyAction;
    KSelectAction *m_selectType;
    TimecodeDisplay *m_time;
    MonitorSceneType m_neededScene;
    bool m_monitorActive{false};
    QSize m_sourceFrameSize;
    void connectMonitor(bool active);
    void setDuration(int duration);
    void addCurveEditor(const QPersistentModelIndex &index, QString name = "", int rectindex = -1);
    std::unordered_map<QPersistentModelIndex, QWidget *> m_parameters;
    int m_baseHeight;
    int m_addedHeight;
    int m_curveContainerHeight = 0;

Q_SIGNALS:
    void addIndex(QPersistentModelIndex ix);
    void setKeyframes(const QString &);
    void updateEffectKeyframe(bool atkeyframe, bool outside);
    void goToNext();
    void goToPrevious();
    void addRemove();
    void onCurveEditorView();
    void onKeyframeView();
};
