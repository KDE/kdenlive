/***************************************************************************
 *   Copyright (C) 2018 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "slidewidget.hpp"
#include "assets/model/assetparametermodel.hpp"

SlideWidget::SlideWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    // setup the comment
    setupUi(this);
    QString comment = m_model->data(m_index, AssetParameterModel::CommentRole).toString();

    slotRefresh();
    connect(end_up, &QAbstractButton::clicked, this, &SlideWidget::updateValue);
    connect(end_down, &QAbstractButton::clicked, this, &SlideWidget::updateValue);
    connect(end_left, &QAbstractButton::clicked, this, &SlideWidget::updateValue);
    connect(end_right, &QAbstractButton::clicked, this, &SlideWidget::updateValue);
    connect(end_center, &QAbstractButton::clicked, this, &SlideWidget::updateValue);
    connect(start_up, &QAbstractButton::clicked, this, &SlideWidget::updateValue);
    connect(start_down, &QAbstractButton::clicked, this, &SlideWidget::updateValue);
    connect(start_left, &QAbstractButton::clicked, this, &SlideWidget::updateValue);
    connect(start_right, &QAbstractButton::clicked, this, &SlideWidget::updateValue);
    connect(start_center, &QAbstractButton::clicked, this, &SlideWidget::updateValue);
    connect(start_transp, &QAbstractSlider::valueChanged, this, &SlideWidget::updateValue);
    connect(end_transp, &QAbstractSlider::valueChanged, this, &SlideWidget::updateValue);

    // emit the signal of the base class when appropriate
    connect(this, &SlideWidget::modified, [this](const QString &val) { emit valueChanged(m_index, val, true); });

    // setup comment
    setToolTip(comment);
}

void SlideWidget::slotShowComment(bool) {}

void SlideWidget::slotRefresh()
{
    QString value = m_model->data(m_index, AssetParameterModel::ValueRole).toString();
    QColor bg = QPalette().highlight().color();
    setStyleSheet(QStringLiteral("QPushButton:checked {background-color:rgb(%1,%2,%3);}").arg(bg.red()).arg(bg.green()).arg(bg.blue()));
    wipeInfo w = getWipeInfo(value);
    switch (w.start) {
    case UP:
        start_up->setChecked(true);
        break;
    case DOWN:
        start_down->setChecked(true);
        break;
    case RIGHT:
        start_right->setChecked(true);
        break;
    case LEFT:
        start_left->setChecked(true);
        break;
    default:
        start_center->setChecked(true);
        break;
    }
    switch (w.end) {
    case UP:
        end_up->setChecked(true);
        break;
    case DOWN:
        end_down->setChecked(true);
        break;
    case RIGHT:
        end_right->setChecked(true);
        break;
    case LEFT:
        end_left->setChecked(true);
        break;
    default:
        end_center->setChecked(true);
        break;
    }
    start_transp->setValue(w.startTransparency);
    end_transp->setValue(w.endTransparency);
}

void SlideWidget::updateValue()
{
    wipeInfo info;
    if (start_left->isChecked()) {
        info.start = LEFT;
    } else if (start_right->isChecked()) {
        info.start = RIGHT;
    } else if (start_up->isChecked()) {
        info.start = UP;
    } else if (start_down->isChecked()) {
        info.start = DOWN;
    } else if (start_center->isChecked()) {
        info.start = CENTER;
    } else {
        info.start = LEFT;
    }
    info.startTransparency = start_transp->value();

    if (end_left->isChecked()) {
        info.end = LEFT;
    } else if (end_right->isChecked()) {
        info.end = RIGHT;
    } else if (end_up->isChecked()) {
        info.end = UP;
    } else if (end_down->isChecked()) {
        info.end = DOWN;
    } else if (end_center->isChecked()) {
        info.end = CENTER;
    } else {
        info.end = RIGHT;
    }
    info.endTransparency = end_transp->value();
    emit modified(getWipeString(info));
}

SlideWidget::wipeInfo SlideWidget::getWipeInfo(QString value)
{
    wipeInfo info;
    // Convert old geometry values that used a comma as separator
    if (value.contains(QLatin1Char(','))) {
        value.replace(',', '/');
    }
    QString start = value.section(QLatin1Char(';'), 0, 0);
    QString end = value.section(QLatin1Char(';'), 1, 1).section(QLatin1Char('='), 1, 1);
    if (start.startsWith(QLatin1String("-100%/0"))) {
        info.start = LEFT;
    } else if (start.startsWith(QLatin1String("100%/0"))) {
        info.start = RIGHT;
    } else if (start.startsWith(QLatin1String("0%/100%"))) {
        info.start = DOWN;
    } else if (start.startsWith(QLatin1String("0%/-100%"))) {
        info.start = UP;
    } else {
        info.start = CENTER;
    }

    if (start.count(':') == 2) {
        info.startTransparency = start.section(QLatin1Char(':'), -1).toInt();
    } else {
        info.startTransparency = 100;
    }

    if (end.startsWith(QLatin1String("-100%/0"))) {
        info.end = LEFT;
    } else if (end.startsWith(QLatin1String("100%/0"))) {
        info.end = RIGHT;
    } else if (end.startsWith(QLatin1String("0%/100%"))) {
        info.end = DOWN;
    } else if (end.startsWith(QLatin1String("0%/-100%"))) {
        info.end = UP;
    } else {
        info.end = CENTER;
    }

    if (end.count(':') == 2) {
        info.endTransparency = end.section(QLatin1Char(':'), -1).toInt();
    } else {
        info.endTransparency = 100;
    }

    return info;
}

const QString SlideWidget::getWipeString(wipeInfo info)
{

    QString start;
    QString end;
    switch (info.start) {
    case LEFT:
        start = QStringLiteral("-100%/0%:100%x100%");
        break;
    case RIGHT:
        start = QStringLiteral("100%/0%:100%x100%");
        break;
    case DOWN:
        start = QStringLiteral("0%/100%:100%x100%");
        break;
    case UP:
        start = QStringLiteral("0%/-100%:100%x100%");
        break;
    default:
        start = QStringLiteral("0%/0%:100%x100%");
        break;
    }
    start.append(':' + QString::number(info.startTransparency));

    switch (info.end) {
    case LEFT:
        end = QStringLiteral("-100%/0%:100%x100%");
        break;
    case RIGHT:
        end = QStringLiteral("100%/0%:100%x100%");
        break;
    case DOWN:
        end = QStringLiteral("0%/100%:100%x100%");
        break;
    case UP:
        end = QStringLiteral("0%/-100%:100%x100%");
        break;
    default:
        end = QStringLiteral("0%/0%:100%x100%");
        break;
    }
    end.append(':' + QString::number(info.endTransparency));
    return QString(start + QStringLiteral(";-1=") + end);
}
