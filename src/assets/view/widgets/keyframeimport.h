/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QDialog>
#include <QDoubleSpinBox>
#include <QLabel>

#include "assets/model/assetparametermodel.hpp"
#include "definitions.h"
#include "utils/timecode.h"

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
    explicit KeyframeImport(const QString &animData, std::shared_ptr<AssetParameterModel> model, const QList<QPersistentModelIndex> &indexes, int parentIn, int parentDuration, QWidget *parent = nullptr);
    ~KeyframeImport() override;
    QString selectedData() const;
    void importSelectedData();
    QString selectedTarget() const;
    int getImportType() const;

private:
    std::shared_ptr<AssetParameterModel> m_model;
    QMap <QPersistentModelIndex, QString> m_originalParams;
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
    QComboBox *m_alignSourceCombo;
    QComboBox *m_alignTargetCombo;
    QLabel *m_sourceRangeLabel;
    QList<QPoint> m_maximas;
    QDoubleSpinBox m_destMin;
    QDoubleSpinBox m_destMax;
    QSpinBox m_offsetX;
    QSpinBox m_offsetY;
    /** @brief Contains the 4 dimensional (x,y,w,h) target parameter names / tag **/
    QMap<QString, QModelIndex> m_geometryTargets;
    /** @brief Contains the 1 dimensional target parameter names / tag **/
    QMap<QString, QModelIndex> m_simpleTargets;
    bool m_isReady;
    void drawKeyFrameChannels(QPixmap &pix, int in, int out, int limitKeyframes, const QColor &textColor);

protected:
    enum ImportRoles {
        SimpleValue,
        RotoData,
        FullGeometry,
        Position,
        InvertedPosition,
        OffsetPosition,
        XOnly,
        YOnly,
        WidthOnly,
        HeightOnly
    };
    enum { ValueRole = Qt::UserRole, TypeRole, MinRole, MaxRole, OpacityRole };
    mutable QReadWriteLock m_lock; // This is a lock that ensures safety in case of concurrent access
    void resizeEvent(QResizeEvent *ev) override;

private slots:
    void updateDataDisplay();
    void updateDisplay();
    void updateRange();
    void updateDestinationRange();
    void updateView();

public slots:
    virtual void accept() override;
    virtual void reject() override;

signals:
    void updateQmlView();
};
