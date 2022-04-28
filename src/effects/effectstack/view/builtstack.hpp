/*
    SPDX-FileCopyrightText: 2017 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "definitions.h"
#include <QQuickWidget>
#include <memory>

class AssetPanel;
class EffectStackModel;

class BuiltStack : public QQuickWidget
{
    Q_OBJECT

public:
    BuiltStack(AssetPanel *parent);
    ~BuiltStack() override;
    void setModel(const std::shared_ptr<EffectStackModel> &model, ObjectId ownerId);

private:
    std::shared_ptr<EffectStackModel> m_model;
};
