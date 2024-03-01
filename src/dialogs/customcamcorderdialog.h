/*
    SPDX-FileCopyrightText: 2024 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ui_customcamcorderproxy_ui.h"

class ProjectClip;

namespace Mlt {
}

/**
 * @class CustomCamcorderDialog
 * @brief A dialog for editing camcorder proxy profiles
 * @author Jean-Baptiste Mardelle
 */
class CustomCamcorderDialog : public QDialog, public Ui::CustomCamcorderProxy_UI
{
    Q_OBJECT

public:
    explicit CustomCamcorderDialog(QWidget *parent = nullptr);
    ~CustomCamcorderDialog() override;

private:
    void loadEntries();

private Q_SLOTS:
    void slotAddProfile();
    void slotRemoveProfile();
    void slotResetProfiles();
    void slotSaveProfiles();
    void validateInput();
    void updateParams(int row);
};
