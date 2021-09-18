/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef TRANSITIONSTACKVIEW_H
#define TRANSITIONSTACKVIEW_H

#include "assets/view/assetparameterview.hpp"
#include "definitions.h"

class QComboBox;

class TransitionStackView : public AssetParameterView
{
    Q_OBJECT

public:
    TransitionStackView(QWidget *parent = nullptr);
    void setModel(const std::shared_ptr<AssetParameterModel> &model, QSize frameSize, bool addSpacer = false) override;
    void unsetModel();
    ObjectId stackOwner() const;
    void refreshTracks();

private slots:
    void updateTrack(int newTrack);
    void checkCompoTrack();

signals:
    void seekToTransPos(int pos);

private:
    QComboBox *m_trackBox{nullptr};
};

#endif
