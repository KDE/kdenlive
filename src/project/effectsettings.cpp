/*
SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
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
    checkAll->setCheckState(Qt::CheckState(state));
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
