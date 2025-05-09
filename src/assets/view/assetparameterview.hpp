/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include <QModelIndex>
#include <QMutex>
#include <QVector>
#include <QWidget>
#include <memory>

class QVBoxLayout;
class QFormLayout;
class QMenu;
class QActionGroup;
class AbstractParamWidget;
class AssetParameterModel;
class KeyframeContainer;

/** @class AssetParameterView
    @brief This class is the view for a list of parameters.
 */
class AssetParameterView : public QWidget
{
    Q_OBJECT

public:
    AssetParameterView(QWidget *parent = nullptr);

    /** Sets the model to be displayed by current view */
    virtual void setModel(const std::shared_ptr<AssetParameterModel> &model, QSize frameSize, bool addSpacer = false);

    /** Set the widget to display no model (this yield ownership on the smart-ptr)*/
    void unsetModel();
    void disconnectKeyframeWidget();

    /** Returns the preferred widget height */
    int contentHeight() const;

    /** Returns the type of monitor overlay required by this effect */
    MonitorSceneType needsMonitorEffectScene() const;

    /** Returns true is the effect can use keyframes */
    bool keyframesAllowed() const;
    /** @brief Returns true is the model has more than one keyframe */
    bool hasMultipleKeyframes() const;
    /** Returns true is the keyframes should be hidden on first opening*/
    bool modelHideKeyframes() const;
    /** Returns the preset menu to be embedded in toolbars */
    QMenu *presetMenu();

public Q_SLOTS:
    void slotRefresh();
    void toggleKeyframes(bool enable);
    /** Reset all parameter values to default */
    void resetValues();
    /** Save all parameters to a preset */
    void slotSavePreset(QString presetName = QString());
    /** Save all parameters to a preset */
    void slotLoadPreset();
    void slotUpdatePreset();
    void slotDeleteCurrentPreset();
    void slotDeletePreset(const QString &presetName);

protected:
    /** @brief This is a handler for the dataChanged slot of the model.
        It basically instructs the widgets in the given range to be refreshed */
    void refresh(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);

    QFormLayout *m_lay;
    /** @brief Protect from concurrent operations
     **/
    QMutex m_lock;
    std::shared_ptr<AssetParameterModel> m_model;
    std::vector<AbstractParamWidget *> m_widgets;
    KeyframeContainer *m_mainKeyframeWidget{nullptr};
    QMenu *m_presetMenu;
    std::shared_ptr<QActionGroup> m_presetGroup;

private:
    QVector<QPair<QString, QVariant>> getDefaultValues() const;

private Q_SLOTS:
    /** @brief Apply a change of parameter sent by the view
       @param index is the index corresponding to the modified param
       @param value is the new value of the parameter
       @param storeUndo: if true, an undo object is created
    */
    void commitChanges(const QModelIndex &index, const QString &value, bool storeUndo);
    void commitMultipleChanges(const QList<QModelIndex> &indexes, const QStringList &values, bool storeUndo);
    void disableCurrentFilter(bool disable);

Q_SIGNALS:
    void seekToPos(int);
    void initKeyframeView(bool active, bool outside);
    /** @brief clear and refill the effect presets */
    void updatePresets(const QString &presetName = QString());
    void updateHeight();
    void activateEffect();
    void nextKeyframe();
    void previousKeyframe();
    void addRemoveKeyframe(bool addOnly = false);
    void saveEffect();
    /** @brief Used to pass a standard action like copy or paste to the effect stack widget */
    void sendStandardCommand(int command);
};
