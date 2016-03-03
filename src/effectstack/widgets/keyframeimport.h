/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef KEYFRAMEIMPORT_H
#define KEYFRAMEIMPORT_H

#include <QDialog>
#include <QPixmap>
#include <QLabel>

#include "definitions.h"

class QCheckBox;
class QSpinBox;
class KeyframeView;

class KeyframeImport : public QDialog
{
    Q_OBJECT
public:
    explicit KeyframeImport(GraphicsRectItem type, ItemInfo info, QMap<QString, QString> data, QWidget *parent = 0);
    virtual ~KeyframeImport();
    int limited() const;
    bool importPosition() const;
    bool importSize() const;
    QString selectedData() const;

private:
    QComboBox *m_dataCombo;
    QLabel *m_previewLabel;
    KeyframeView *m_keyframeView;
    QCheckBox *m_position;
    QCheckBox *m_size;
    QCheckBox *m_limit;
    QSpinBox *m_limitNumber;
    QList <QPoint> m_maximas;
    void updateDisplay();

protected:
    void resizeEvent(QResizeEvent *ev);

private slots:
    void updateDataDisplay();
};

#endif
