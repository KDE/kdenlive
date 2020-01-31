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

#include "effectsettings.h"
#include "kdenlivesettings.h"

#include "klocalizedstring.h"
#include <QApplication>
#include <QResizeEvent>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

ElidedCheckBox::ElidedCheckBox(QWidget *parent)
    : QCheckBox(parent)
{
}

void ElidedCheckBox::setBoxText(const QString &text)
{
    m_text = text;
    int width = currentWidth();
    updateText(width);
}

void ElidedCheckBox::updateText(int width)
{
    setText(fontMetrics().elidedText(m_text, Qt::ElideRight, width));
}

int ElidedCheckBox::currentWidth() const
{
    int width = 0;
    if (isVisible()) {
        width = contentsRect().width() - (2 * iconSize().width());
    } else {
        QMargins mrg = contentsMargins();
        width = sizeHint().width() - mrg.left() - mrg.right() - (2 * iconSize().width());
    }
    return width;
}

void ElidedCheckBox::resizeEvent(QResizeEvent *event)
{
    int diff = event->size().width() - event->oldSize().width();
    updateText(currentWidth() + diff);
    QCheckBox::resizeEvent(event);
}

EffectSettings::EffectSettings(QWidget *parent)
    : QWidget(parent)
{
    auto *vbox1 = new QVBoxLayout(this);
    vbox1->setContentsMargins(0, 0, 0, 0);
    vbox1->setSpacing(0);
    checkAll = new ElidedCheckBox(this);
    checkAll->setToolTip(i18n("Enable/Disable all effects"));
    auto *hbox = new QHBoxLayout;
    hbox->addWidget(checkAll);
    effectCompare = new QToolButton(this);
    effectCompare->setIcon(QIcon::fromTheme(QStringLiteral("view-split-effect")));
    effectCompare->setToolTip(i18n("Split compare"));
    effectCompare->setCheckable(true);
    effectCompare->setChecked(false);
    hbox->addWidget(effectCompare);
    vbox1->addLayout(hbox);
    container = new QScrollArea;
    container->setFrameShape(QFrame::NoFrame);
    container->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred));
    container->setWidgetResizable(true);
    QPalette p = qApp->palette();
    p.setBrush(QPalette::Window, QBrush(Qt::transparent));
    container->setPalette(p);
    container->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    vbox1->addWidget(container);
    setLayout(vbox1);

    // m_ui.buttonShowComments->setIcon(QIcon::fromTheme(QStringLiteral("help-about")));
    // m_ui.buttonShowComments->setToolTip(i18n("Show additional information for the parameters"));
    // connect(m_ui.buttonShowComments, SIGNAL(clicked()), this, SLOT(slotShowComments()));
    // m_ui.labelComment->setHidden(true);
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum));
}

void EffectSettings::setLabel(const QString &label, const QString &tooltip)
{
    checkAll->setBoxText(label);
    checkAll->setToolTip(tooltip);
    checkAll->setEnabled(!label.isEmpty());
}

void EffectSettings::updateCheckState(int state)
{
    checkAll->blockSignals(true);
    checkAll->setCheckState((Qt::CheckState)state);
    checkAll->blockSignals(false);
}

void EffectSettings::updatePalette()
{
    // We need to reset current stylesheet if we want to change the palette!
    // m_effectEdit->updatePalette();
}

void EffectSettings::resizeEvent(QResizeEvent *event)
{
    int diff = event->size().width() - event->oldSize().width();
    checkAll->updateText(checkAll->currentWidth() + diff);
    QWidget::resizeEvent(event);
}
