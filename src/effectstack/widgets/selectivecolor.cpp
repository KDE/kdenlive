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

#include "effectstack/widgets/selectivecolor.h"
#include "utils/KoIconUtils.h"

#include <KLocalizedString>

SelectiveColor::SelectiveColor(const QDomElement &effect, QWidget *parent) :
    QWidget(parent)
{
    setupUi(this);
    QDomNodeList namenode = effect.elementsByTagName(QStringLiteral("parameter"));
    for (int i = 0; i < namenode.count(); ++i) {
        QDomElement pa = namenode.item(i).toElement();
        QDomElement na = pa.firstChildElement(QStringLiteral("name"));
        QString type = pa.attribute(QStringLiteral("type"));
        QString paramName = na.isNull() ? pa.attribute(QStringLiteral("name")) : i18n(na.text().toUtf8().data());
        if (type == QLatin1String("cmyk")) {
            addParam(pa, paramName);
        } else if (pa.attribute(QStringLiteral("name")) == QLatin1String("av.correction_method")) {
            if (pa.attribute(QStringLiteral("value")).toInt() == 1) {
                relative->setChecked(true);
            } else {
                absolute->setChecked(true);
            }
        }
    }
    connect(range, SIGNAL(currentIndexChanged(int)), this, SLOT(updateValues()));
    connect(slider_black, &QAbstractSlider::valueChanged, this, &SelectiveColor::effectChanged);
    connect(slider_yell, &QAbstractSlider::valueChanged, this, &SelectiveColor::effectChanged);
    connect(slider_mag, &QAbstractSlider::valueChanged, this, &SelectiveColor::effectChanged);
    connect(slider_cyan, &QAbstractSlider::valueChanged, this, &SelectiveColor::effectChanged);
    connect(relative, &QAbstractButton::toggled, this, &SelectiveColor::effectChanged);
    updateValues();
}

SelectiveColor::~SelectiveColor()
{
}

void SelectiveColor::addParam(QDomElement &effect, QString name)
{
    QString tag = effect.attribute(QStringLiteral("name"));
    if (name.isEmpty()) {
        name = tag;
    }
    QString value = effect.attribute(QStringLiteral("value"));
    if (value.isEmpty()) {
        value = effect.attribute(QStringLiteral("default"));
    }
    QIcon icon;
    if (!value.isEmpty()) {
        icon = KoIconUtils::themedIcon(QStringLiteral("dialog-ok-apply"));
    }
    range->addItem(icon, name, QStringList() << effect.attribute(QStringLiteral("name")) << value);
}

void SelectiveColor::updateValues()
{
    QStringList values = range->currentData().toStringList();
    if (values.count() < 2) {
        // Something is wrong, abort
        return;
    }
    blockSignals(true);
    spin_black->setValue(0);
    spin_yell->setValue(0);
    spin_mag->setValue(0);
    spin_cyan->setValue(0);
    QStringList vals = values.at(1).split(QLatin1Char(' '));
    switch (vals.count()) {
    case 4 :
        spin_black->setValue(vals.at(3).toDouble() * 100);
    case 3:
        spin_yell->setValue(vals.at(2).toDouble() * 100);
    case 2:
        spin_mag->setValue(vals.at(1).toDouble() * 100);
    case 1:
        spin_cyan->setValue(vals.at(0).toDouble() * 100);
        break;
    default:
        break;
    }
    blockSignals(false);
}

void SelectiveColor::effectChanged()
{
    int vBlack = spin_black->value();
    int vYell = spin_yell->value();
    int vMag = spin_mag->value();
    int vCyan = spin_cyan->value();
    QString result;
    if (vBlack == 0 && vYell == 0 && vMag == 0 && vCyan == 0) {
        // default empty val
    } else {
        result = QStringLiteral("%1 %2 %3 %4").arg(vCyan / 100.0).arg(vMag / 100.0).arg(vYell / 100.0).arg(vBlack / 100.0);
    }
    QStringList values = range->currentData().toStringList();
    if (values.isEmpty()) {
        // Something is wrong, abort
        return;
    }
    if (values.at(1).isEmpty() && !result.isEmpty()) {
        range->setItemIcon(range->currentIndex(), KoIconUtils::themedIcon(QStringLiteral("dialog-ok-apply")));
    } else if (!values.at(1).isEmpty() && result.isEmpty()) {
        range->setItemIcon(range->currentIndex(), QIcon());
    }
    QStringList newData = QStringList() << values.at(0) << result;
    range->setItemData(range->currentIndex(), newData);
    emit valueChanged();
}

void SelectiveColor::updateEffect(QDomElement &effect)
{
    QMap<QString, QString> values;
    for (int i = 0; i < range->count(); i++) {
        QStringList vals = range->itemData(i).toStringList();
        values.insert(vals.at(0), vals.at(1));
    }
    QDomNodeList namenode = effect.childNodes();
    for (int i = 0; i < namenode.count(); ++i) {
        QDomElement pa = namenode.item(i).toElement();
        if (pa.tagName() != QLatin1String("parameter")) {
            continue;
        }
        QString paramName = pa.attribute(QStringLiteral("name"));
        const QString val = values.value(paramName);
        if (!val.isEmpty()) {
            pa.setAttribute(QStringLiteral("value"), val);
        } else if (paramName == QLatin1String("av.correction_method")) {
            pa.setAttribute(QStringLiteral("value"), relative->isChecked() ? QStringLiteral("1") : QStringLiteral("0"));
        }
    }
}

