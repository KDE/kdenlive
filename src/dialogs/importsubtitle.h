/*
    SPDX-FileCopyrightText: 2023 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ui_import_subtitle_ui.h"

#include "definitions.h"
#include "utils/timecode.h"
#include <QTimer>

/**
 * @class ImportSubtitle
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */
class ImportSubtitle : public QDialog, public Ui::ImportSub_UI
{
    Q_OBJECT

public:
    explicit ImportSubtitle(const QString &path, QWidget *parent = nullptr);
    ~ImportSubtitle() override;

private:
    QTimer m_parseTimer;
};
