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
#include <QDoubleSpinBox>
#include <QLabel>

#include "assets/model/assetparametermodel.hpp"
#include "definitions.h"
#include "timecode.h"

class PositionWidget;
class QComboBox;
class QCheckBox;
class QSpinBox;

namespace Mlt {
}

class KeyframeImport : public QDialog
{
    Q_OBJECT
public:
    explicit KeyframeImport(const QString &animData, std::shared_ptr<AssetParameterModel> model, QList<QPersistentModelIndex> indexes, int parentIn, int parentDuration, QWidget *parent = nullptr);
    ~KeyframeImport() override;
    QString selectedData() const;
    void importSelectedData();
    QString selectedTarget() const;
    int getImportType() const;

private:
    std::shared_ptr<AssetParameterModel> m_model;
    QList<QPersistentModelIndex> m_indexes;
    bool m_supportsAnim;
    QComboBox *m_dataCombo;
    QLabel *m_previewLabel;
    PositionWidget *m_inPoint;
    PositionWidget *m_outPoint;
    PositionWidget *m_offsetPoint;
    QCheckBox *m_limitRange;
    QCheckBox *m_limitKeyframes;
    QSpinBox *m_limitNumber;
    QComboBox *m_sourceCombo;
    QComboBox *m_targetCombo;
    QComboBox *m_alignCombo;
    QLabel *m_sourceRangeLabel;
    QList<QPoint> m_maximas;
    QDoubleSpinBox m_destMin;
    QDoubleSpinBox m_destMax;
    /** @brief Contains the 4 dimensional (x,y,w,h) target parameter names / tag **/
    QMap<QString, QModelIndex> m_geometryTargets;
    /** @brief Contains the 1 dimensional target parameter names / tag **/
    QMap<QString, QModelIndex> m_simpleTargets;
    bool m_isReady;
    void drawKeyFrameChannels(QPixmap &pix, int in, int out, int limitKeyframes, const QColor &textColor);

protected:
    enum ImportRoles {
        SimpleValue,
        FullGeometry,
        Position,
        XOnly,
        YOnly,
        WidthOnly,
        HeightOnly
    };
    mutable QReadWriteLock m_lock; // This is a lock that ensures safety in case of concurrent access
    void resizeEvent(QResizeEvent *ev) override;

private slots:
    void updateDataDisplay();
    void updateDisplay();
    void updateRange();
    void updateDestinationRange();
};

#endif
