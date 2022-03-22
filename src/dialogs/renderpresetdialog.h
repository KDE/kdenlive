/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <jk.kdedev@smartlab.uber.space>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef RENDERPRESETDIALOG_H
#define RENDERPRESETDIALOG_H

#include "ui_editrenderpreset_ui.h"

class RenderPresetModel;
class Monitor;

class RenderPresetDialog : public QDialog, Ui::EditRenderPreset_UI
{
    Q_OBJECT
public:
    enum Mode {
        New = 0,
        Edit,
        SaveAs
    };
    explicit RenderPresetDialog(QWidget *parent, RenderPresetModel *preset = nullptr, Mode mode = Mode::New);
    /** @returns the name that was finally under which the preset has been saved */
    ~RenderPresetDialog();
    QString saveName();

private:
    QString m_saveName;
    QStringList m_uiParams;
    Monitor *m_monitor;
    double m_fixedResRatio;

    void setPixelAspectRatio(int num, int den);
    void updateDisplayAspectRatio();

private slots:
    void slotUpdateParams();
};

#endif // RENDERPRESETDIALOG_H
