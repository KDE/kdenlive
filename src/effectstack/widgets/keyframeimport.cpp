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

#include "utils/KoIconUtils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPainter>

#include "klocalizedstring.h"

#include "keyframeimport.h"
#include "effectstack/widgets/positionwidget.h"
#include "timeline/keyframeview.h"

KeyframeImport::KeyframeImport(const ItemInfo &srcInfo, const ItemInfo &dstInfo, const QMap<QString, QString> &data, const Timecode &tc, const QDomElement &xml, const ProfileInfo &profile, QWidget *parent) :
    QDialog(parent)
    , m_xml(xml)
    , m_profile(profile)
    , m_supportsAnim(false)
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    QHBoxLayout *l1 = new QHBoxLayout;
    QLabel *lab = new QLabel(i18n("Data to import: "), this);
    l1->addWidget(lab);

    m_dataCombo = new QComboBox(this);
    l1->addWidget(m_dataCombo);
    l1->addStretch(10);
    lay->addLayout(l1);
    // Set  up data
    int ix = 0;
    QMap<QString, QString>::const_iterator i = data.constBegin();
    while (i != data.constEnd()) {
        m_dataCombo->insertItem(ix, i.key());
        m_dataCombo->setItemData(ix, i.value(), Qt::UserRole);
        ++i;
        ix++;
    }
    m_previewLabel = new QLabel(this);
    m_previewLabel->setMinimumSize(100, 150);
    m_previewLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    m_previewLabel->setScaledContents(true);
    lay->addWidget(m_previewLabel);
    m_keyframeView = new KeyframeView(0, this);
    // Zone in / out
    ItemInfo reference;
    if (srcInfo.isValid()) {
        reference = srcInfo;
    } else {
        reference = dstInfo;
    }
    m_inPoint = new PositionWidget(i18n("In"), reference.cropStart.frames(tc.fps()),
                                   reference.cropStart.frames(tc.fps()),
                                   (reference.cropStart + reference.cropDuration).frames(tc.fps()),
                                   tc, "", this);
    connect(m_inPoint, &PositionWidget::valueChanged, this, &KeyframeImport::updateDisplay);
    lay->addWidget(m_inPoint);
    m_outPoint = new PositionWidget(i18n("Out"),
                                    (reference.cropStart + reference.cropDuration).frames(tc.fps()),
                                    reference.cropStart.frames(tc.fps()), (reference.cropStart + reference.cropDuration).frames(tc.fps()),
                                    tc, "", this);
    connect(m_outPoint, &PositionWidget::valueChanged, this, &KeyframeImport::updateDisplay);
    lay->addWidget(m_outPoint);

    // Check what kind of parameters are in our target
    QDomNodeList params = xml.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < params.count(); i++) {
        QDomElement e = params.at(i).toElement();
        QString pType = e.attribute(QStringLiteral("type"));
        if (pType == QLatin1String("animatedrect") || pType == QLatin1String("geometry")) {
            if (pType == QLatin1String("animatedrect")) {
                m_supportsAnim = true;
            }
            QDomElement na = e.firstChildElement(QStringLiteral("name"));
            QString paramName = na.isNull() ? e.attribute(QStringLiteral("name")) : i18n(na.text().toUtf8().data());
            m_geometryTargets.insert(paramName, e.attribute(QStringLiteral("name")));
        } else if (pType == QLatin1String("animated")) {
            QDomElement na = e.firstChildElement(QStringLiteral("name"));
            QString paramName = na.isNull() ? e.attribute(QStringLiteral("name")) : i18n(na.text().toUtf8().data());
            m_simpleTargets.insert(paramName, e.attribute(QStringLiteral("name")));
        }
    }
    l1 = new QHBoxLayout;
    m_targetCombo = new QComboBox(this);
    m_sourceCombo = new QComboBox(this);
    ix = 0;
    if (!m_geometryTargets.isEmpty()) {
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
    }
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
    m_offsetPoint = new PositionWidget(i18n("Offset"), 0, 0, dstInfo.cropDuration.frames(tc.fps()), tc, "", this);
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
    QString data = m_dataCombo->currentData().toString();
    m_maximas = m_keyframeView->loadKeyframes(data);
    double wDist = m_maximas.at(2).y() - m_maximas.at(2).x();
    double hDist = m_maximas.at(3).y() - m_maximas.at(3).x();
    if (wDist == 0 && hDist == 0) {
        // Source data has only x/y pos, no width/height so disable geometry import
        int pos = m_sourceCombo->currentData().toInt();
        int ix = m_sourceCombo->findData(10);
        if (ix > -1) {
            m_sourceCombo->removeItem(ix);
        }
        if (pos == 10) {
            ix = m_sourceCombo->findData(11);
            if (ix > -1) {
                m_sourceCombo->setCurrentIndex(ix);
            }
        }
    }
    updateRange();
    if (!m_inPoint->isValid()) {
        m_inPoint->blockSignals(true);
        m_outPoint->blockSignals(true);
        m_inPoint->setRange(0, m_keyframeView->duration);
        m_outPoint->setRange(0, m_keyframeView->duration);
        m_inPoint->setPosition(0);
        m_outPoint->setPosition(m_keyframeView->duration);
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
        int profileWidth = m_profile.profileSize.width();
        int profileHeight = m_profile.profileSize.height();
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
            rangeText = i18n("Source range: (%1-%2), (%3-%4)", qMin(0, m_maximas.at(0).x()), qMax(profileWidth, m_maximas.at(0).y()), qMin(0, m_maximas.at(1).x()), qMax(profileHeight, m_maximas.at(1).y()));
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
        }
    } else {
        //TODO
        m_destMin.setRange(0, m_profile.profileSize.width());
        m_destMax.setRange(0, m_profile.profileSize.width());
        m_destMin.setEnabled(false);
        m_destMax.setEnabled(false);
        m_limitRange->setEnabled(false);
    }
}

void KeyframeImport::updateDisplay()
{
    QPixmap pix(m_previewLabel->width(), m_previewLabel->height());
    pix.fill(Qt::transparent);
    QPainter *painter = new QPainter(&pix);
    QList<QPoint> maximas;
    int selectedtarget = m_sourceCombo->currentData().toInt();
    int profileWidth = m_profile.profileSize.width();
    int profileHeight = m_profile.profileSize.height();
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
    m_keyframeView->drawKeyFrameChannels(pix.rect(), m_inPoint->getPosition(), m_outPoint->getPosition(), painter, maximas, m_limitKeyframes->isChecked() ? m_limitNumber->value() : 0, palette().text().color());
    painter->end();
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
            maximas = QPoint(qMin(m_maximas.at(ix).x(), 0), qMax(m_maximas.at(ix).y(), m_profile.profileSize.width()));
        } else {
            // Height maximas
            maximas = QPoint(qMin(m_maximas.at(ix).x(), 0), qMax(m_maximas.at(ix).y(), m_profile.profileSize.height()));
        }
        return m_keyframeView->getSingleAnimation(ix, m_inPoint->getPosition(), m_outPoint->getPosition(), m_offsetPoint->getPosition(), m_limitKeyframes->isChecked() ? m_limitNumber->value() : 0, maximas, m_destMin.value(), m_destMax.value());
    }
    // Geometry target
    int pos = m_sourceCombo->currentData().toInt();
    QPoint rectOffset;
    int ix = m_alignCombo->currentIndex();
    switch (ix) {
    case 1:
        rectOffset = QPoint(m_profile.profileSize.width() / 2, m_profile.profileSize.height() / 2);
        break;
    case 2:
        rectOffset = QPoint(m_profile.profileSize.width(), m_profile.profileSize.height());
        break;
    default:
        break;
    }
    return m_keyframeView->getOffsetAnimation(m_inPoint->getPosition(), m_outPoint->getPosition(), m_offsetPoint->getPosition(), m_limitKeyframes->isChecked() ? m_limitNumber->value() : 0, m_profile, m_supportsAnim, pos == 11, rectOffset);
}

QString KeyframeImport::selectedTarget() const
{
    return m_targetCombo->currentData().toString();
}
