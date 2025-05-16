/*
 * SPDX-FileCopyrightText: 2015 Jean-Baptiste Mardelle <jb@kdenlive.org>
 * SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "elidedfilelinklabel.h"

#include "core.h"

#include <QResizeEvent>
#include <kio/openfilemanagerwindowjob.h>

ElidedFileLinkLabel::ElidedFileLinkLabel(QWidget *parent)
    : QLabel(parent)
{
    connect(this, &QLabel::linkActivated, [](const QString &link) { pCore->highlightFileInExplorer({QUrl::fromLocalFile(link)}); });
}

void ElidedFileLinkLabel::setText(const QString &text)
{
    m_text = text;
    int width = currentWidth();
    updateLabel(width);
}

void ElidedFileLinkLabel::setLink(const QString &link)
{
    m_link = link;
    int width = currentWidth();
    updateLabel(width);
}

void ElidedFileLinkLabel::setTextElideMode(Qt::TextElideMode mode)
{
    m_textElideMode = mode;
    int width = currentWidth();
    updateLabel(width);
}

QString ElidedFileLinkLabel::link() const
{
    return m_link;
}

QString ElidedFileLinkLabel::text() const
{
    return m_text;
}

Qt::TextElideMode ElidedFileLinkLabel::textElideMode() const
{
    return m_textElideMode;
}

void ElidedFileLinkLabel::clear()
{
    m_link.clear();
    m_text.clear();
    int width = currentWidth();
    updateLabel(width);
}

void ElidedFileLinkLabel::updateLabel(int width)
{
    if (m_link.isEmpty()) {
        QLabel::setText(fontMetrics().elidedText(m_text, m_textElideMode, width));
    } else {
        QLabel::setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(m_link, fontMetrics().elidedText(m_text, m_textElideMode, width)));
    }
}

int ElidedFileLinkLabel::currentWidth() const
{
    int width = 0;
    if (isVisible()) {
        width = contentsRect().width();
    } else {
        QMargins mrg = contentsMargins();
        width = sizeHint().width() - mrg.left() - mrg.right();
    }
    return width;
}

void ElidedFileLinkLabel::resizeEvent(QResizeEvent *event)
{
    int diff = event->size().width() - event->oldSize().width();
    updateLabel(currentWidth() + diff);
    QLabel::resizeEvent(event);
}
