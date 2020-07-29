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

#ifndef KEYFRAMEWIDGET_H
#define KEYFRAMEWIDGET_H

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

class KeyframeWidget : public AbstractParamWidget
{
    Q_OBJECT

public:
    explicit KeyframeWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QSize frameSize, QWidget *parent = nullptr);
    ~KeyframeWidget() override;

    /* @brief Add a new parameter to be managed using the same keyframe viewer */
    void addParameter(const QPersistentModelIndex &index);
    int getPosition() const;
    void addKeyframe(int pos = -1);
    /** @brief Returns the monitor scene required for this asset
     */
    MonitorSceneType requiredScene() const;
    void updateTimecodeFormat();
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
    /* brief Update the value of the widgets to reflect keyframe change */
    void slotRefreshParams();
    void slotAtKeyframe(bool atKeyframe, bool singleKeyframe);
    void monitorSeek(int pos);
    void slotEditKeyframeType(QAction *action);
    void slotUpdateKeyframesFromMonitor(const QPersistentModelIndex &index, const QVariant &res);
    void slotCopyKeyframes();
    void slotImportKeyframes();
    void slotRemoveNextKeyframes();
    void slotSeekToKeyframe(int ix);

private:
    QVBoxLayout *m_lay;
    QToolBar *m_toolbar;
    std::shared_ptr<KeyframeModelList> m_keyframes;
    KeyframeView *m_keyframeview;
    KeyframeMonitorHelper *m_monitorHelper;
    QToolButton *m_buttonAddDelete;
    QToolButton *m_buttonPrevious;
    QToolButton *m_buttonNext;
    KSelectAction *m_selectType;
    TimecodeDisplay *m_time;
    MonitorSceneType m_neededScene;
    QSize m_sourceFrameSize;
    void connectMonitor(bool active);
    std::unordered_map<QPersistentModelIndex, QWidget *> m_parameters;
    int m_baseHeight;
    int m_addedHeight;
    bool m_effectIsSelected;

signals:
    void addIndex(QPersistentModelIndex ix);
    void setKeyframes(const QString &);
};

#endif
