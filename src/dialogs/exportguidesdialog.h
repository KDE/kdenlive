/*
    SPDX-FileCopyrightText: 2022 Gary Wang <wzc782970009@gmail.com>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ui_exportguidesdialog_ui.h"

#include "definitions.h"
#include "utils/timecode.h"
#include "widgets/timecodedisplay.h"

namespace Mlt {
}

/**
 * @class ExportGuidesDialog
 * @brief A dialog for export guides as plain text.
 * @author Gary Wang
 */
class MarkerListModel;
class ExportGuidesDialog : public QDialog, public Ui::ExportGuidesDialog_UI
{
    Q_OBJECT

public:
    explicit ExportGuidesDialog(const MarkerListModel *model, const GenTime duration, QWidget *parent = nullptr);
    ~ExportGuidesDialog() override;

private:
    double offsetTimeMs() const;
    void updateContentByModel() const;
    void updateContentByModel(const QString &format, int markerIndex, double offset) const;

    const MarkerListModel * m_markerListModel;
    const GenTime m_projectDuration;
};
