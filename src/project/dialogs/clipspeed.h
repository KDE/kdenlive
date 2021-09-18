/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef CLIPSPEED_H
#define CLIPSPEED_H

#include "ui_clipspeed_ui.h"
#include <QDialog>

/** @class ClipSpeed
    @brief A dialog allowing to set a destination and a new speed to create an MLT file from a clip.
 */
class ClipSpeed : public QDialog
{
    Q_OBJECT

public:
    explicit ClipSpeed(const QUrl &destination, bool isDirectory, QWidget *parent = nullptr);
    QUrl selectedUrl() const;
    double speed() const;

private slots:
    void slotUpdateSlider(double);
    void slotUpdateSpeed(int);
    void adjustSpeed(QAction *a);

private:
    Ui::ClipSpeed_UI m_view;
};

#endif
