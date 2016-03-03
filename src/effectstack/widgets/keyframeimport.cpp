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

#include <QDebug>
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
#include "timeline/keyframeview.h"

KeyframeImport::KeyframeImport(GraphicsRectItem type, ItemInfo info, QMap<QString, QString> data, QWidget *parent) :
    QDialog(parent)
{
    QVBoxLayout *lay = new QVBoxLayout(this);
    QHBoxLayout *l1 = new QHBoxLayout;
    QLabel *lab = new QLabel(i18n("Data to import: "), this);
    l1->addWidget(lab);

    m_dataCombo = new QComboBox(this);
    l1->addWidget(m_dataCombo);
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
    l1 = new QHBoxLayout;
    m_position = new QCheckBox(i18n("Position"), this);
    m_size = new QCheckBox(i18n("Size"), this);
    m_position->setChecked(true);
    l1->addWidget(m_position);
    l1->addWidget(m_size);
    lay->addLayout(l1);
    l1 = new QHBoxLayout;
    m_limit = new QCheckBox(i18n("Limit keyframe number"), this);
    m_limitNumber = new QSpinBox(this);
    m_limitNumber->setMinimum(1);
    m_limitNumber->setValue(20);
    l1->addWidget(m_limit);
    l1->addWidget(m_limitNumber);
    lay->addLayout(l1);
    m_limitNumber->setEnabled(false);
    connect(m_limit, &QCheckBox::toggled, m_limitNumber, &QSpinBox::setEnabled);
    connect(m_dataCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateDataDisplay()));
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    lay->addWidget(buttonBox);
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
    updateDisplay();
}

void KeyframeImport::updateDisplay()
{
    QPixmap pix(m_previewLabel->width(), m_previewLabel->height());
    pix.fill(Qt::transparent);
    QPainter *painter = new QPainter(&pix);
    m_keyframeView->drawKeyFrameChannels(pix.rect(), m_keyframeView->duration, painter, m_maximas, palette().text().color());
    painter->end();
    m_previewLabel->setPixmap(pix);
}

int KeyframeImport::limited() const
{
    return m_limit->isChecked() ? m_limitNumber->value() : -1;
}

bool KeyframeImport::importPosition() const
{
    return m_position->isChecked();
}

bool KeyframeImport::importSize() const
{
    return m_size->isChecked();
}

QString KeyframeImport::selectedData() const
{
    return m_dataCombo->currentData().toString();
}
