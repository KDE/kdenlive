/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#include "clipspeed.h"
#include "kdenlivesettings.h"

#include <QFontDatabase>
#include <QMenu>
#include <QStandardPaths>
#include <klocalizedstring.h>

ClipSpeed::ClipSpeed(const QUrl &destination, bool isDirectory, QWidget *parent)
    : QDialog(parent)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    m_view.setupUi(this);
    setWindowTitle(i18n("Create clip with speed"));
    if (isDirectory) {
        m_view.kurlrequester->setMode(KFile::Directory);
    } else {
        m_view.kurlrequester->setMode(KFile::File);
    }
    m_view.kurlrequester->setUrl(destination);
    m_view.toolButton->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-menu")));
    auto *settingsMenu = new QMenu(this);
    m_view.toolButton->setMenu(settingsMenu);
    QAction *a = settingsMenu->addAction(i18n("Reverse clip"));
    a->setData(-100);
    a = settingsMenu->addAction(i18n("25%"));
    a->setData(25);
    a = settingsMenu->addAction(i18n("50%"));
    a->setData(50);
    a = settingsMenu->addAction(i18n("200%"));
    a->setData(200);
    a = settingsMenu->addAction(i18n("400%"));
    a->setData(400);
    connect(settingsMenu, &QMenu::triggered, this, &ClipSpeed::adjustSpeed);
    connect(m_view.speedSlider, &QSlider::valueChanged, this, &ClipSpeed::slotUpdateSpeed);
    connect(m_view.speedSpin, SIGNAL(valueChanged(double)), this, SLOT(slotUpdateSlider(double)));
}

void ClipSpeed::slotUpdateSlider(double speed)
{
    m_view.speedSlider->blockSignals(true);
    m_view.speedSlider->setValue((int)speed);
    m_view.speedSlider->blockSignals(false);
    m_view.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!qFuzzyIsNull(speed));
}

void ClipSpeed::slotUpdateSpeed(int speed)
{
    m_view.speedSpin->blockSignals(true);
    m_view.speedSpin->setValue((double)speed);
    m_view.speedSpin->blockSignals(false);
}

QUrl ClipSpeed::selectedUrl() const
{
    return m_view.kurlrequester->url();
}

void ClipSpeed::adjustSpeed(QAction *a)
{
    if (!a) {
        return;
    }
    int speed = a->data().toInt();
    m_view.speedSlider->blockSignals(true);
    m_view.speedSpin->blockSignals(true);
    m_view.speedSpin->setValue((double)speed);
    m_view.speedSlider->setValue(speed);
    m_view.speedSlider->blockSignals(false);
    m_view.speedSpin->blockSignals(false);
}

double ClipSpeed::speed() const
{
    return m_view.speedSpin->value();
}
