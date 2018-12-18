/*
Copyright (C) 2016  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef GRADIENTWIDGET_H
#define GRADIENTWIDGET_H

#include "ui_gradientedit_ui.h"

#include <QDialog>

/*! \class GradientWidget
    \brief Title creation dialog
    Instances of TitleWidget classes are instantiated by KdenliveDoc::slotCreateTextClip ()
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

#endif
