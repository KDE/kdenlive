/*
    SPDX-FileCopyrightText: 2026
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QDialog>

namespace Ui {
class AutoTrackCreationDialog_UI;
}

class AutoTrackCreationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AutoTrackCreationDialog(QWidget *parent, int missingTracks, int missingTracksBeforeMirror, int streamCount);
    ~AutoTrackCreationDialog() override;

    int tracksToCreate() const;
    static int getTracksToCreate(QWidget *parent, int missingTracks, int missingTracksBeforeMirror, int streamCount);

private:
    Ui::AutoTrackCreationDialog_UI *ui;
    int m_missingTracks;
};
