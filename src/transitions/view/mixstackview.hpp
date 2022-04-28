/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "assets/view/assetparameterview.hpp"
#include "definitions.h"

class QComboBox;
class QToolButton;
class TimecodeDisplay;
class PositionWidget;
class QHBoxLayout;

class MixStackView : public AssetParameterView
{
    Q_OBJECT

public:
    MixStackView(QWidget *parent = nullptr);
    void setModel(const std::shared_ptr<AssetParameterModel> &model, QSize frameSize, bool addSpacer = false) override;
    void unsetModel();
    ObjectId stackOwner() const;

signals:
    void seekToTransPos(int pos);

private slots:
    void durationChanged(const QModelIndex &, const QModelIndex &, const QVector<int> &roles);
    void updateDuration();
    void updatePosition();
    void slotAlignLeft();
    void slotAlignRight();
    void slotAlignCenter();

private:
    //QHBoxLayout *m_positionLayout;
    PositionWidget *m_position;
    QHBoxLayout *m_durationLayout;
    TimecodeDisplay *m_duration;
    QToolButton *m_alignLeft;
    QToolButton *m_alignCenter;
    QToolButton *m_alignRight;
    MixAlignment alignment() const;
    void checkAlignment();
};
