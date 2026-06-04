/*
    SPDX-FileCopyrightText: 2026
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "autotrackcreationdialog.h"
#include "ui_autotrackcreationdialog_ui.h"

#include <KLocalizedString>

AutoTrackCreationDialog::AutoTrackCreationDialog(QWidget *parent, int missingTracks, int missingTracksBeforeMirror, int streamCount)
    : QDialog(parent)
    , ui(new Ui::AutoTrackCreationDialog_UI)
    , m_missingTracks(missingTracks)
{
    ui->setupUi(this);
    if (missingTracksBeforeMirror > 0) {
        ui->mirrorInfoLabel->setText(i18np("The video track needs %1 additional track before creating mirror track.",
                                           "The video track needs %1 additional tracks before creating mirror track.", missingTracksBeforeMirror));
        int height = sizeHint().height() + ui->mirrorInfoLabel->sizeHint().height();
        resize(width(), height);
    } else {
        ui->mirrorInfoLabel->hide();
    }
    ui->infoLabel->setText(i18np("The selected clip needs %1 additional audio track for %2 audio stream.",
                                 "The selected clip needs %1 additional audio tracks for %2 audio streams.", missingTracks, streamCount));
    ui->createAllRadio->setText(i18n("Create all required tracks (%1)", missingTracks));
    if (streamCount > missingTracks) {
        ui->tracksSpinBox->setRange(0, qMax(1, missingTracks));
        ui->tracksSpinBox->setValue(0);
    }
    else {
        ui->tracksSpinBox->setRange(1, qMax(1, missingTracks));
        ui->tracksSpinBox->setValue(1);
    }
    
    ui->tracksSpinBox->setEnabled(false);
    connect(ui->createCustomRadio, &QRadioButton::toggled, this, [this](bool checked) {
        ui->tracksSpinBox->setEnabled(checked);
    });
}

AutoTrackCreationDialog::~AutoTrackCreationDialog()
{
    delete ui;
}

int AutoTrackCreationDialog::tracksToCreate() const
{
    if (ui->createCustomRadio->isChecked()) {
        return ui->tracksSpinBox->value();
    }
    return m_missingTracks;
}

int AutoTrackCreationDialog::getTracksToCreate(QWidget *parent, int missingTracks, int missingTracksBeforeMirror, int streamCount)
{
    if (missingTracks <= 0) {
        return 0;
    }
    AutoTrackCreationDialog dialog(parent, missingTracks, missingTracksBeforeMirror, streamCount);
    if (dialog.exec() != QDialog::Accepted) {
        return -1;
    }
    return dialog.tracksToCreate() + missingTracksBeforeMirror;
}
