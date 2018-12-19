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



#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#include "klocalizedstring.h"

#include "widgets/positionwidget.h"
#include "keyframeimport.h"
#include "assets/keyframes/view/keyframeview.hpp"
#include "core.h"
#include "doc/kdenlivedoc.h"
#include "profiles/profilemodel.hpp"

#include "mlt++/MltProperties.h"
#include "mlt++/MltAnimation.h"

KeyframeImport::KeyframeImport(int in, int out, const QString &animData, std::shared_ptr<AssetParameterModel> model, QList <QPersistentModelIndex>indexes, QWidget *parent)
    : QDialog(parent)
    , m_model(model)
    , m_supportsAnim(false)
{
    auto *lay = new QVBoxLayout(this);
    auto *l1 = new QHBoxLayout;
    QLabel *lab = new QLabel(i18n("Data to import: "), this);
    l1->addWidget(lab);

    m_dataCombo = new QComboBox(this);
    l1->addWidget(m_dataCombo);
    l1->addStretch(10);
    lay->addLayout(l1);
    // Set  up data
    auto json = QJsonDocument::fromJson(animData.toUtf8());
    if (!json.isArray()) {
        qDebug() << "Error : Json file should be an array";
        return;
    }
    auto list = json.array();
    int ix = 0;
    for (const auto &entry : list) {
        if (!entry.isObject()) {
            qDebug() << "Warning : Skipping invalid marker data";
            continue;
        }
        auto entryObj = entry.toObject();
        if (!entryObj.contains(QLatin1String("name"))) {
            qDebug() << "Warning : Skipping invalid marker data (does not contain name)";
            continue;
        }
        QString name = entryObj[QLatin1String("name")].toString();
        QString value = entryObj[QLatin1String("value")].toString();
        int type = entryObj[QLatin1String("type")].toInt(0);
        double min = entryObj[QLatin1String("min")].toDouble(0);
        double max = entryObj[QLatin1String("max")].toDouble(0);
        m_dataCombo->insertItem(ix, name);
        m_dataCombo->setItemData(ix, value, Qt::UserRole);
        m_dataCombo->setItemData(ix, type, Qt::UserRole + 1);
        m_dataCombo->setItemData(ix, min, Qt::UserRole + 2);
        m_dataCombo->setItemData(ix, max, Qt::UserRole + 3);
        ix++;
    }

    m_previewLabel = new QLabel(this);
    m_previewLabel->setMinimumSize(100, 150);
    m_previewLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_previewLabel->setScaledContents(true);
    lay->addWidget(m_previewLabel);
    // Zone in / out
    m_inPoint = new PositionWidget(i18n("In"), in, in, out, pCore->currentDoc()->timecode(), QString(), this);
    connect(m_inPoint, &PositionWidget::valueChanged, this, &KeyframeImport::updateDisplay);
    lay->addWidget(m_inPoint);
    m_outPoint = new PositionWidget(i18n("Out"), out, in, out, pCore->currentDoc()->timecode(), QString(), this);
    connect(m_outPoint, &PositionWidget::valueChanged, this, &KeyframeImport::updateDisplay);
    lay->addWidget(m_outPoint);

    // Check what kind of parameters are in our target
    for (const QPersistentModelIndex &ix : indexes) {
        ParamType type = m_model->data(ix, AssetParameterModel::TypeRole).value<ParamType>();
        if (type == ParamType::KeyframeParam) {
            m_simpleTargets.insert(m_model->data(ix, Qt::DisplayRole).toString(), m_model->data(ix, AssetParameterModel::NameRole).toString());
        } else if (type == ParamType::AnimatedRect) {
            m_geometryTargets.insert(m_model->data(ix, Qt::DisplayRole).toString(), m_model->data(ix, AssetParameterModel::NameRole).toString());
        }
    }

    l1 = new QHBoxLayout;
    m_targetCombo = new QComboBox(this);
    m_sourceCombo = new QComboBox(this);
    ix = 0;
    /*if (!m_geometryTargets.isEmpty()) {
        m_sourceCombo->insertItem(ix, i18n("Geometry"));
        m_sourceCombo->setItemData(ix, QString::number(10), Qt::UserRole);
        ix++;
        m_sourceCombo->insertItem(ix, i18n("Position"));
        m_sourceCombo->setItemData(ix, QString::number(11), Qt::UserRole);
        ix++;
    }
    if (!m_simpleTargets.isEmpty()) {
        m_sourceCombo->insertItem(ix, i18n("X"));
        m_sourceCombo->setItemData(ix, QString::number(0), Qt::UserRole);
        ix++;
        m_sourceCombo->insertItem(ix, i18n("Y"));
        m_sourceCombo->setItemData(ix, QString::number(1), Qt::UserRole);
        ix++;
        m_sourceCombo->insertItem(ix, i18n("Width"));
        m_sourceCombo->setItemData(ix, QString::number(2), Qt::UserRole);
        ix++;
        m_sourceCombo->insertItem(ix, i18n("Height"));
        m_sourceCombo->setItemData(ix, QString::number(3), Qt::UserRole);
        ix++;
    }*/
    connect(m_sourceCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateRange()));
    m_alignCombo = new QComboBox(this);
    m_alignCombo->addItems(QStringList() << i18n("Align top left") << i18n("Align center") << i18n("Align bottom right"));
    lab = new QLabel(i18n("Map "), this);
    QLabel *lab2 = new QLabel(i18n(" to "), this);
    l1->addWidget(lab);
    l1->addWidget(m_sourceCombo);
    l1->addWidget(lab2);
    l1->addWidget(m_targetCombo);
    l1->addWidget(m_alignCombo);
    l1->addStretch(10);
    ix = 0;
    QMap<QString, QString>::const_iterator j = m_geometryTargets.constBegin();
    while (j != m_geometryTargets.constEnd()) {
        m_targetCombo->insertItem(ix, j.key());
        m_targetCombo->setItemData(ix, j.value(), Qt::UserRole);
        ++j;
        ix++;
    }
    ix = 0;
    j = m_simpleTargets.constBegin();
    while (j != m_simpleTargets.constEnd()) {
        m_targetCombo->insertItem(ix, j.key());
        m_targetCombo->setItemData(ix, j.value(), Qt::UserRole);
        ++j;
        ix++;
    }
    if (m_simpleTargets.count() + m_geometryTargets.count() > 1) {
        // Target contains several animatable parameters, propose choice
    }
    lay->addLayout(l1);

    // Output offset
    m_offsetPoint = new PositionWidget(i18n("Offset"), 0, 0, out, pCore->currentDoc()->timecode(), "", this);
    lay->addWidget(m_offsetPoint);

    // Source range
    m_sourceRangeLabel = new QLabel(i18n("Source range %1 to %2", 0, 100), this);
    lay->addWidget(m_sourceRangeLabel);

    // update range info
    connect(m_targetCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateDestinationRange()));

    // Destination range
    l1 = new QHBoxLayout;
    lab = new QLabel(i18n("Destination range"), this);

    l1->addWidget(lab);
    l1->addWidget(&m_destMin);
    l1->addWidget(&m_destMax);
    lay->addLayout(l1);

    l1 = new QHBoxLayout;
    m_limitRange = new QCheckBox(i18n("Actual range only"), this);
    connect(m_limitRange, &QAbstractButton::toggled, this, &KeyframeImport::updateRange);
    connect(m_limitRange, &QAbstractButton::toggled, this, &KeyframeImport::updateDisplay);
    l1->addWidget(m_limitRange);
    l1->addStretch(10);
    lay->addLayout(l1);
    l1 = new QHBoxLayout;
    m_limitKeyframes = new QCheckBox(i18n("Limit keyframe number"), this);
    m_limitKeyframes->setChecked(true);
    m_limitNumber = new QSpinBox(this);
    m_limitNumber->setMinimum(1);
    m_limitNumber->setValue(20);
    l1->addWidget(m_limitKeyframes);
    l1->addWidget(m_limitNumber);
    l1->addStretch(10);
    lay->addLayout(l1);
    connect(m_limitKeyframes, &QCheckBox::toggled, m_limitNumber, &QSpinBox::setEnabled);
    connect(m_limitKeyframes, &QAbstractButton::toggled, this, &KeyframeImport::updateDisplay);
    connect(m_limitNumber, SIGNAL(valueChanged(int)), this, SLOT(updateDisplay()));
    connect(m_dataCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateDataDisplay()));
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(buttonBox);
    updateDestinationRange();
    updateDataDisplay();
}

KeyframeImport::~KeyframeImport()
{
}

void KeyframeImport::resizeEvent(QResizeEvent *ev)
{
    QWidget::resizeEvent(ev);
    updateDisplay();
}

void KeyframeImport::updateDataDisplay()
{
    QString comboData = m_dataCombo->currentData().toString();
    ParamType type = m_dataCombo->currentData(Qt::UserRole + 1).value<ParamType>();
    m_maximas = KeyframeModel::getRanges(comboData);
    m_sourceCombo->clear();
    if (type == ParamType::KeyframeParam) {
        // 1 dimensional param.
        m_sourceCombo->addItem(m_dataCombo->currentText());
        updateRange();
        return;
    }
    qDebug()<<"DATA: "<<comboData<<"\nRESULT: "<<m_maximas;
    double wDist = m_maximas.at(2).y() - m_maximas.at(2).x();
    double hDist = m_maximas.at(3).y() - m_maximas.at(3).x();
    m_sourceCombo->addItem(i18n("Geometry"), 10);
    m_sourceCombo->addItem(i18n("Position"), 11);
    m_sourceCombo->addItem(i18n("X"), 0);
    m_sourceCombo->addItem(i18n("Y"), 1);
    if (wDist > 0) {
        m_sourceCombo->addItem(i18n("Width"), 2);
    }
    if (hDist > 0) {
        m_sourceCombo->addItem(i18n("Height"), 3);
    }
    updateRange();
    if (!m_inPoint->isValid()) {
        m_inPoint->blockSignals(true);
        m_outPoint->blockSignals(true);
        //m_inPoint->setRange(0, m_keyframeView->duration);
        m_inPoint->setPosition(0);
        //m_outPoint->setPosition(m_keyframeView->duration);
        m_inPoint->blockSignals(false);
        m_outPoint->blockSignals(false);
    }
}

void KeyframeImport::updateRange()
{
    int pos = m_sourceCombo->currentData().toInt();
    m_alignCombo->setEnabled(pos == 11);
    QString rangeText;
    if (m_limitRange->isChecked()) {
        switch (pos) {
        case 0:
            rangeText = i18n("Source range %1 to %2", m_maximas.at(0).x(), m_maximas.at(0).y());
            break;
        case 1:
            rangeText = i18n("Source range %1 to %2", m_maximas.at(1).x(), m_maximas.at(1).y());
            break;
        case 2:
            rangeText = i18n("Source range %1 to %2", m_maximas.at(2).x(), m_maximas.at(2).y());
            break;
        case 3:
            rangeText = i18n("Source range %1 to %2", m_maximas.at(3).x(), m_maximas.at(3).y());
            break;
        default:
            rangeText = i18n("Source range: (%1-%2), (%3-%4)", m_maximas.at(0).x(), m_maximas.at(0).y(), m_maximas.at(1).x(), m_maximas.at(1).y());
            break;
        }
    } else {
        int profileWidth = pCore->getCurrentProfile()->width();
        int profileHeight = pCore->getCurrentProfile()->height();
        switch (pos) {
        case 0:
            rangeText = i18n("Source range %1 to %2", qMin(0, m_maximas.at(0).x()), qMax(profileWidth, m_maximas.at(0).y()));
            break;
        case 1:
            rangeText = i18n("Source range %1 to %2", qMin(0, m_maximas.at(1).x()), qMax(profileHeight, m_maximas.at(1).y()));
            break;
        case 2:
            rangeText = i18n("Source range %1 to %2", qMin(0, m_maximas.at(2).x()), qMax(profileWidth, m_maximas.at(2).y()));
            break;
        case 3:
            rangeText = i18n("Source range %1 to %2", qMin(0, m_maximas.at(3).x()), qMax(profileHeight, m_maximas.at(3).y()));
            break;
        default:
            rangeText = i18n("Source range: (%1-%2), (%3-%4)", qMin(0, m_maximas.at(0).x()), qMax(profileWidth, m_maximas.at(0).y()),
                             qMin(0, m_maximas.at(1).x()), qMax(profileHeight, m_maximas.at(1).y()));
            break;
        }
    }
    m_sourceRangeLabel->setText(rangeText);
    updateDisplay();
}

void KeyframeImport::updateDestinationRange()
{
    if (m_simpleTargets.contains(m_targetCombo->currentText())) {
        // 1 dimension target
        m_destMin.setEnabled(true);
        m_destMax.setEnabled(true);
        m_limitRange->setEnabled(true);
        QString tag = m_targetCombo->currentData().toString();
        /*
        QDomNodeList params = m_xml.elementsByTagName(QStringLiteral("parameter"));
        for (int i = 0; i < params.count(); i++) {
            QDomElement e = params.at(i).toElement();
            if (e.attribute(QStringLiteral("name")) == tag) {
                double factor = e.attribute(QStringLiteral("factor")).toDouble();
                if (factor == 0) {
                    factor = 1;
                }
                double min = e.attribute(QStringLiteral("min")).toDouble() / factor;
                double max = e.attribute(QStringLiteral("max")).toDouble() / factor;
                m_destMin.setRange(min, max);
                m_destMax.setRange(min, max);
                m_destMin.setValue(min);
                m_destMax.setValue(max);
                break;
            }
        }*/
    } else {
        // TODO
        int profileWidth = pCore->getCurrentProfile()->width();
        m_destMin.setRange(0, profileWidth);
        m_destMax.setRange(0, profileWidth);
        m_destMin.setEnabled(false);
        m_destMax.setEnabled(false);
        m_limitRange->setEnabled(false);
    }
}

void KeyframeImport::updateDisplay()
{
    QPixmap pix(m_previewLabel->width(), m_previewLabel->height());
    pix.fill(Qt::transparent);
    QList<QPoint> maximas;
    int selectedtarget = m_sourceCombo->currentData().toInt();
    int profileWidth = pCore->getCurrentProfile()->width();
    int profileHeight = pCore->getCurrentProfile()->height();
    if (!m_maximas.isEmpty()) {
        if (m_maximas.at(0).x() == m_maximas.at(0).y() || (selectedtarget < 10 && selectedtarget != 0)) {
            maximas << QPoint();
        } else {
            if (m_limitRange->isChecked()) {
                maximas << m_maximas.at(0);
            } else {
                QPoint p1(qMin(0, m_maximas.at(0).x()), qMax(profileWidth, m_maximas.at(0).y()));
                maximas << p1;
            }
        }
    }
    if (m_maximas.count() > 1) {
        if (m_maximas.at(1).x() == m_maximas.at(1).y() || (selectedtarget < 10 && selectedtarget != 1)) {
            maximas << QPoint();
        } else {
            if (m_limitRange->isChecked()) {
                maximas << m_maximas.at(1);
            } else {
                QPoint p2(qMin(0, m_maximas.at(1).x()), qMax(profileHeight, m_maximas.at(1).y()));
                maximas << p2;
            }
        }
    }
    if (m_maximas.count() > 2) {
        if (m_maximas.at(2).x() == m_maximas.at(2).y() || (selectedtarget < 10 && selectedtarget != 2)) {
            maximas << QPoint();
        } else {
            if (m_limitRange->isChecked()) {
                maximas << m_maximas.at(2);
            } else {
                QPoint p3(qMin(0, m_maximas.at(2).x()), qMax(profileWidth, m_maximas.at(2).y()));
                maximas << p3;
            }
        }
    }
    if (m_maximas.count() > 3) {
        if (m_maximas.at(3).x() == m_maximas.at(3).y() || (selectedtarget < 10 && selectedtarget != 3)) {
            maximas << QPoint();
        } else {
            if (m_limitRange->isChecked()) {
                maximas << m_maximas.at(3);
            } else {
                QPoint p4(qMin(0, m_maximas.at(3).x()), qMax(profileHeight, m_maximas.at(3).y()));
                maximas << p4;
            }
        }
    }
    drawKeyFrameChannels(pix, m_inPoint->getPosition(), m_outPoint->getPosition(), m_limitKeyframes->isChecked() ? m_limitNumber->value() : 0, palette().text().color());
    m_previewLabel->setPixmap(pix);
}

QString KeyframeImport::selectedData() const
{
    // return serialized keyframes
    if (m_simpleTargets.contains(m_targetCombo->currentText())) {
        // Exporting a 1 dimension animation
        int ix = m_sourceCombo->currentData().toInt();
        QPoint maximas;
        if (m_limitRange->isChecked()) {
            maximas = m_maximas.at(ix);
        } else if (ix == 0 || ix == 2) {
            // Width maximas
            maximas = QPoint(qMin(m_maximas.at(ix).x(), 0), qMax(m_maximas.at(ix).y(), pCore->getCurrentProfile()->width()));
        } else {
            // Height maximas
            maximas = QPoint(qMin(m_maximas.at(ix).x(), 0), qMax(m_maximas.at(ix).y(), pCore->getCurrentProfile()->height()));
        }
        std::shared_ptr<Mlt::Properties> animData = KeyframeModel::getAnimation(m_dataCombo->currentData().toString());
        std::shared_ptr<Mlt::Animation> anim(new Mlt::Animation(animData->get_animation("key")));
        animData->anim_get_double("key", m_inPoint->getPosition(), m_outPoint->getPosition());
        return anim->serialize_cut();
        //m_keyframeView->getSingleAnimation(ix, m_inPoint->getPosition(), m_outPoint->getPosition(), m_offsetPoint->getPosition(), m_limitKeyframes->isChecked() ? m_limitNumber->value() : 0, maximas, m_destMin.value(), m_destMax.value());
    }
    // Geometry target
    int pos = m_sourceCombo->currentData().toInt();
    QPoint rectOffset;
    int ix = m_alignCombo->currentIndex();
    switch (ix) {
    case 1:
        rectOffset = QPoint(pCore->getCurrentProfile()->width() / 2, pCore->getCurrentProfile()->height() / 2);
        break;
    case 2:
        rectOffset = QPoint(pCore->getCurrentProfile()->width(), pCore->getCurrentProfile()->height());
        break;
    default:
        break;
    }
    return QString();
    //m_keyframeView->getOffsetAnimation(m_inPoint->getPosition(), m_outPoint->getPosition(), m_offsetPoint->getPosition(), m_limitKeyframes->isChecked() ? m_limitNumber->value() : 0, m_supportsAnim, pos == 11, rectOffset);
}

QString KeyframeImport::selectedTarget() const
{
    return m_targetCombo->currentData().toString();
}

void KeyframeImport::drawKeyFrameChannels(QPixmap &pix, int in, int out, int limitKeyframes, const QColor &textColor)
{
    std::shared_ptr<Mlt::Properties> animData = KeyframeModel::getAnimation(m_dataCombo->currentData().toString());
    QRect br(0, 0, pix.width(), pix.height());
    double frameFactor = (double)(out - in) / br.width();
    int offset = 1;
    if (limitKeyframes > 0) {
        offset = (out - in) / limitKeyframes / frameFactor;
    }
    double min = m_dataCombo->currentData(Qt::UserRole + 2).toDouble();
    double max = m_dataCombo->currentData(Qt::UserRole + 3).toDouble();
    double xDist;
    if (max > min) {
        xDist = max - min;
    } else {
        xDist = m_maximas.at(0).y() - m_maximas.at(0).x();
    }
    double yDist = m_maximas.at(1).y() - m_maximas.at(1).x();
    double wDist = m_maximas.at(2).y() - m_maximas.at(2).x();
    double hDist = m_maximas.at(3).y() - m_maximas.at(3).x();
    double xOffset = m_maximas.at(0).x();
    double yOffset = m_maximas.at(1).x();
    double wOffset = m_maximas.at(2).x();
    double hOffset = m_maximas.at(3).x();
    QColor cX(255, 0, 0, 100);
    QColor cY(0, 255, 0, 100);
    QColor cW(0, 0, 255, 100);
    QColor cH(255, 255, 0, 100);
    // Draw curves labels
    QPainter painter;
    painter.begin(&pix);
    QRectF txtRect = painter.boundingRect(br, QStringLiteral("t"));
    txtRect.setX(2);
    txtRect.setWidth(br.width() - 4);
    txtRect.moveTop(br.height() - txtRect.height());
    QRectF drawnText;
    int maxHeight = br.height() - txtRect.height() - 2;
    painter.setPen(textColor);
    int rectSize = txtRect.height() / 2;
    if (xDist > 0) {
        painter.fillRect(txtRect.x(), txtRect.top() + rectSize / 2, rectSize, rectSize, cX);
        txtRect.setX(txtRect.x() + rectSize * 2);
        painter.drawText(txtRect, 0, i18nc("X as in x coordinate", "X") + QStringLiteral(" (%1-%2)").arg(m_maximas.at(0).x()).arg(m_maximas.at(0).y()), &drawnText);
    }
    if (yDist > 0) {
        if (drawnText.isValid()) {
            txtRect.setX(drawnText.right() + rectSize);
        }
        painter.fillRect(txtRect.x(), txtRect.top() + rectSize / 2, rectSize, rectSize, cY);
        txtRect.setX(txtRect.x() + rectSize * 2);
        painter.drawText(txtRect, 0, i18nc("Y as in y coordinate", "Y") + QStringLiteral(" (%1-%2)").arg(m_maximas.at(1).x()).arg(m_maximas.at(1).y()), &drawnText);
    }
    if (wDist > 0) {
        if (drawnText.isValid()) {
            txtRect.setX(drawnText.right() + rectSize);
        }
        painter.fillRect(txtRect.x(), txtRect.top() + rectSize / 2, rectSize, rectSize, cW);
        txtRect.setX(txtRect.x() + rectSize * 2);
        painter.drawText(txtRect, 0, i18n("Width") + QStringLiteral(" (%1-%2)").arg(m_maximas.at(2).x()).arg(m_maximas.at(2).y()), &drawnText);
    }
    if (hDist > 0) {
        if (drawnText.isValid()) {
            txtRect.setX(drawnText.right() + rectSize);
        }
        painter.fillRect(txtRect.x(), txtRect.top() + rectSize / 2, rectSize, rectSize, cH);
        txtRect.setX(txtRect.x() + rectSize * 2);
        painter.drawText(txtRect, 0, i18n("Height") + QStringLiteral(" (%1-%2)").arg(m_maximas.at(3).x()).arg(m_maximas.at(3).y()), &drawnText);
    }

    // Draw curves
    for (int i = 0; i < br.width(); i++) {
        mlt_rect rect = animData->anim_get_rect("key", (int)(i * frameFactor) + in);
        qDebug()<<"// DRAWINC CURVE IWDTH: "<<rect.w<<", WDIST: "<<wDist;
        if (xDist > 0) {
            painter.setPen(cX);
            int val = (rect.x - xOffset) * maxHeight / xDist;
            painter.drawLine(i, maxHeight - val, i, maxHeight);
        }
        if (yDist > 0) {
            painter.setPen(cY);
            int val = (rect.y - yOffset) * maxHeight / yDist;
            painter.drawLine(i, maxHeight - val, i, maxHeight);
        }
        if (wDist > 0) {
            painter.setPen(cW);
            int val = (rect.w - wOffset) * maxHeight / wDist;
            qDebug()<<"// OFFSET: "<<wOffset<<", maxH: "<<maxHeight<<", wDIst:"<<wDist<<" = "<<val;
            painter.drawLine(i, maxHeight - val, i, maxHeight);
        }
        if (hDist > 0) {
            painter.setPen(cH);
            int val = (rect.h - hOffset) * maxHeight / hDist;
            painter.drawLine(i, maxHeight - val, i, maxHeight);
        }
    }
    if (offset > 1) {
        // Overlay limited keyframes curve
        cX.setAlpha(255);
        cY.setAlpha(255);
        cW.setAlpha(255);
        cH.setAlpha(255);
        mlt_rect rect1 = animData->anim_get_rect("key", in);
        int prevPos = 0;
        for (int i = offset; i < br.width(); i += offset) {
            mlt_rect rect2 = animData->anim_get_rect("key", (int)(i * frameFactor) + in);
            if (xDist > 0) {
                painter.setPen(cX);
                int val1 = (rect1.x - xOffset) * maxHeight / xDist;
                int val2 = (rect2.x - xOffset) * maxHeight / xDist;
                painter.drawLine(prevPos, maxHeight - val1, i, maxHeight - val2);
            }
            if (yDist > 0) {
                painter.setPen(cY);
                int val1 = (rect1.y - yOffset) * maxHeight / yDist;
                int val2 = (rect2.y - yOffset) * maxHeight / yDist;
                painter.drawLine(prevPos, maxHeight - val1, i, maxHeight - val2);
            }
            if (wDist > 0) {
                painter.setPen(cW);
                int val1 = (rect1.w - wOffset) * maxHeight / wDist;
                int val2 = (rect2.w - wOffset) * maxHeight / wDist;
                painter.drawLine(prevPos, maxHeight - val1, i, maxHeight - val2);
            }
            if (hDist > 0) {
                painter.setPen(cH);
                int val1 = (rect1.h - hOffset) * maxHeight / hDist;
                int val2 = (rect2.h - hOffset) * maxHeight / hDist;
                painter.drawLine(prevPos, maxHeight - val1, i, maxHeight - val2);
            }
            rect1 = rect2;
            prevPos = i;
        }
    }
}
