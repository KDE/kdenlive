/*
SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include <QPersistentModelIndex>

#include <QObject>
#include <QVariant>

#include <memory>

class Monitor;
class AssetParameterModel;

/** @class KeyframeMonitorHelper
    @brief This class helps manage effects that receive data from the monitor's qml overlay to translate
    the data and pass it to the model
    */
class KeyframeMonitorHelper : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Construct a keyframe monitor helper for the given effect/asset.
     *
     * The helper is initialized with an empty list of managed parameter indexes.
     * You must call addIndex() for every parameter that should be managed by this helper.
     * This allows the same helper instance to manage one or multiple parameters as needed.
     *
     * @param monitor The Monitor instance this helper will interact with (for effect overlays, etc).
     * @param model The asset parameter model this helper will manage keyframes for.
     * @param sceneType The type of monitor scene this helper is associated with (geometry, corners, etc).
     * @param parent Optional QObject parent.
     */
    explicit KeyframeMonitorHelper(Monitor *monitor, std::shared_ptr<AssetParameterModel> model, MonitorSceneType sceneType, QObject *parent = nullptr);
    /** @brief Send signals to the monitor to update the qml overlay.
       @param returns : true if the monitor's connection was changed to active.
    */
    virtual bool connectMonitor(bool activate);
    /** @brief Send data update to the monitor
     */
    virtual void refreshParams(int pos);
    /** @brief Wait until monitor scene is active to send data
     */
    void refreshParamsWhenReady(int pos);

    /** @brief Returns true if the monitor is playing
     */
    bool isPlaying() const;

    QList<QPersistentModelIndex> getIndexes();

protected:
    Monitor *m_monitor;
    std::shared_ptr<AssetParameterModel> m_model;
    /** @brief List of indexes managed by this class
     */
    QList<QPersistentModelIndex> m_indexes;
    bool m_active;
    MonitorSceneType m_requestedSceneType{MonitorSceneNone};

private Q_SLOTS:
    virtual void slotUpdateFromMonitorData(const QVariantList &v);

public Q_SLOTS:
    /**
     * @brief Register a parameter index to be managed by this helper.
     *
     * This should be called for every parameter that needs monitor interaction and keyframe management.
     * The index is added to the internal list of managed parameters, enabling the helper to update and synchronize
     * keyframe data between the monitor overlay and the parameter model.
     */
    void addIndex(const QPersistentModelIndex &index);

Q_SIGNALS:
    /** @brief Send updated keyframe data to the parameter \@index
     */
    void updateKeyframeData(QPersistentModelIndex index, const QVariant &v);
};
