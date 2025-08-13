/*
    SPDX-FileCopyrightText: 2018 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "slidewidget.hpp"
#include "assets/model/assetparametermodel.hpp"

SlideWidget::SlideWidget(std::shared_ptr<AssetParameterModel> model, QModelIndex index, QWidget *parent)
    : AbstractParamWidget(std::move(model), index, parent)
{
    // setup the comment
    setupUi(this);

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

    // Q_EMIT the signal of the base class when appropriate
    connect(this, &SlideWidget::modified, [this](const QString &val) { Q_EMIT valueChanged(m_index, val, true); });
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
    Q_EMIT modified(getWipeString(info));
}

SlideWidget::wipeInfo SlideWidget::getWipeInfo(QString value)
{
    wipeInfo info;
    // Convert old geometry values that used a comma as separator
    if (value.contains(QLatin1Char(','))) {
        value.replace(',', '/');
    }
    QString start = value.section(QLatin1Char(';'), 0, 0).section(QLatin1Char('='), 1);
    QString end = value.section(QLatin1Char(';'), 1, 1).section(QLatin1Char('='), 1, 1);
    if (start.startsWith(QLatin1String("-100% 0"))) {
        info.start = LEFT;
    } else if (start.startsWith(QLatin1String("100% 0"))) {
        info.start = RIGHT;
    } else if (start.startsWith(QLatin1String("0% 100%"))) {
        info.start = DOWN;
    } else if (start.startsWith(QLatin1String("0% -100%"))) {
        info.start = UP;
    } else {
        info.start = CENTER;
    }

    if (start.split(QLatin1Char(' ')).count() == 5) {
        qDebug() << "== READING START TR: " << start.section(QLatin1Char(' '), -1);
        info.startTransparency = start.section(QLatin1Char(' '), -1).section(QLatin1Char('%'), 0, 0).toInt();
    } else {
        info.startTransparency = 100;
    }

    if (end.startsWith(QLatin1String("-100% 0"))) {
        info.end = LEFT;
    } else if (end.startsWith(QLatin1String("100% 0"))) {
        info.end = RIGHT;
    } else if (end.startsWith(QLatin1String("0% 100%"))) {
        info.end = DOWN;
    } else if (end.startsWith(QLatin1String("0% -100%"))) {
        info.end = UP;
    } else {
        info.end = CENTER;
    }

    if (end.split(QLatin1Char(' ')).count() == 5) {
        info.endTransparency = end.section(QLatin1Char(' '), -1).section(QLatin1Char('%'), 0, 0).toInt();
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
        start = QStringLiteral("-100% 0% 100% 100%");
        break;
    case RIGHT:
        start = QStringLiteral("100% 0% 100% 100%");
        break;
    case DOWN:
        start = QStringLiteral("0% 100% 100% 100%");
        break;
    case UP:
        start = QStringLiteral("0% -100% 100% 100%");
        break;
    default:
        start = QStringLiteral("0% 0% 100% 100%");
        break;
    }
    start.append(QStringLiteral(" %1%").arg(info.startTransparency));

    switch (info.end) {
    case LEFT:
        end = QStringLiteral("-100% 0% 100% 100%");
        break;
    case RIGHT:
        end = QStringLiteral("100% 0% 100% 100%");
        break;
    case DOWN:
        end = QStringLiteral("0% 100% 100% 100%");
        break;
    case UP:
        end = QStringLiteral("0% -100% 100% 100%");
        break;
    default:
        end = QStringLiteral("0% 0% 100% 100%");
        break;
    }
    end.append(QStringLiteral(" %1%").arg(info.endTransparency));
    return QStringLiteral("0=%1;-1=%2").arg(start, end);
}
