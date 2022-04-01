/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ui_gradientedit_ui.h"

#include <QDialog>

/** @class GradientWidget
 *  @brief Title creation dialog
 *  Instances of TitleWidget classes are instantiated by KdenliveDoc::slotCreateTextClip ()
*/
class GradientWidget : public QDialog, public Ui::GradientEdit_UI
{
    Q_OBJECT

public:
    /** @brief Draws the dialog and loads a title document (if any).
     * @param url title document to load
     * @param tc timecode of the project
     * @param projectPath default path to save to or load from title documents
     * @param render project renderer
     * @param parent (optional) parent widget */
    explicit GradientWidget(const QMap<QString, QString> &gradients = QMap<QString, QString>(), int ix = 0, QWidget *parent = nullptr);
    void resizeEvent(QResizeEvent *event) override;
    QString gradientToString() const;
    static QLinearGradient gradientFromString(const QString &str, int width, int height);
    QMap<QString, QString> gradients() const;
    QList<QIcon> icons() const;
    int selectedGradient() const;

private:
    QLinearGradient m_gradient;
    int m_height;
    QStringList getNames() const;
    void loadGradients(QMap<QString, QString> gradients = QMap<QString, QString>());

private slots:
    void updatePreview();
    void saveGradient(const QString &name = QString());
    void loadGradient();
    void deleteGradient();
};
