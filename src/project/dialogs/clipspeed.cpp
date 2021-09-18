/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle (jb@kdenlive.org)

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

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
    m_view.speedSlider->setValue(int(speed));
    m_view.speedSlider->blockSignals(false);
    m_view.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!qFuzzyIsNull(speed));
}

void ClipSpeed::slotUpdateSpeed(int speed)
{
    m_view.speedSpin->blockSignals(true);
    m_view.speedSpin->setValue(speed);
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
    m_view.speedSpin->setValue(speed);
    m_view.speedSlider->setValue(speed);
    m_view.speedSlider->blockSignals(false);
    m_view.speedSpin->blockSignals(false);
}

double ClipSpeed::speed() const
{
    return m_view.speedSpin->value();
}
