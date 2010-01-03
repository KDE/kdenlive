/***************************************************************************
 *   Copyright (C) 2006 by Peter Penz                                      *
 *   peter.penz@gmx.at                                                     *
 *   Code borrowed from Dolphin, adapted (2008) to Kdenlive by             *
 *   Jean-Baptiste Mardelle, jb@kdenlive.org                               *
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

#include "statusbarmessagelabel.h"

#include <kcolorscheme.h>
#include <kiconloader.h>
#include <kicon.h>
#include <klocale.h>
#include <KNotification>

#include <QFontMetrics>
#include <QPainter>
#include <QKeyEvent>
#include <QPushButton>
#include <QPixmap>


StatusBarMessageLabel::StatusBarMessageLabel(QWidget* parent) :
        QWidget(parent),
        m_type(DefaultMessage),
        m_state(Default),
        m_illumination(-64),
        m_minTextHeight(-1),
        m_closeButton(0)
{
    setMinimumHeight(KIconLoader::SizeSmall);

    QPalette palette;
    palette.setColor(QPalette::Background, Qt::transparent);
    setPalette(palette);

    connect(&m_timer, SIGNAL(timeout()), this, SLOT(timerDone()));

    m_closeButton = new QPushButton(i18nc("@action:button", "Close"), this);
    m_closeButton->hide();
    connect(m_closeButton, SIGNAL(clicked()), this, SLOT(closeErrorMessage()));
}

StatusBarMessageLabel::~StatusBarMessageLabel()
{
}

void StatusBarMessageLabel::setMessage(const QString& text,
                                       MessageType type)
{
    if ((text == m_text) && (type == m_type)) {
        if (type == ErrorMessage) KNotification::event("ErrorMessage", m_text);
        return;
    }

    /*if (m_type == ErrorMessage) {
        if (type == ErrorMessage) {
            m_pendingMessages.insert(0, m_text);
        } else if ((m_state != Default) || !m_pendingMessages.isEmpty()) {
            // a non-error message should not be shown, as there
            // are other pending error messages in the queue
            return;
        }
    }*/

    m_text = text;
    m_type = type;

    m_illumination = -64;
    m_state = Default;
    m_timer.stop();

    const char* iconName = 0;
    QPixmap pixmap;
    switch (type) {
    case OperationCompletedMessage:
        iconName = "dialog-ok";
        // "ok" icon should probably be "dialog-success", but we don't have that icon in KDE 4.0
        m_closeButton->hide();
        break;

    case InformationMessage:
        iconName = "dialog-information";
        m_closeButton->hide();
        break;

    case ErrorMessage:
        iconName = "dialog-warning";
        m_timer.start(100);
        m_state = Illuminate;
        m_closeButton->hide();
        KNotification::event("ErrorMessage", m_text);
        break;

    case MltError:
        iconName = "dialog-close";
        m_timer.start(100);
        m_state = Illuminate;
        updateCloseButtonPosition();
        m_closeButton->show();
        break;

    case DefaultMessage:
    default:
        m_closeButton->hide();
        break;
    }

    m_pixmap = (iconName == 0) ? QPixmap() : SmallIcon(iconName);

    /*QFontMetrics fontMetrics(font());
    setMaximumWidth(fontMetrics.boundingRect(m_text).width() + m_pixmap.width() + (BorderGap * 4));
    updateGeometry();*/

    //QTimer::singleShot(GeometryTimeout, this, SLOT(assureVisibleText()));
    update();
}

void StatusBarMessageLabel::setMinimumTextHeight(int min)
{
    if (min != m_minTextHeight) {
        m_minTextHeight = min;
        setMinimumHeight(min);
        if (m_closeButton->height() > min) {
            m_closeButton->setFixedHeight(min);
        }
    }
}

void StatusBarMessageLabel::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);

    // draw background
    QColor backgroundColor;
    if (m_state == Default || m_illumination < 0) backgroundColor = palette().window().color();
    else {
        KColorScheme scheme(palette().currentColorGroup(), KColorScheme::Window);
        backgroundColor = scheme.background(KColorScheme::NegativeBackground).color();
    }
    if (m_state == Desaturate && m_illumination > 0) {
        backgroundColor.setAlpha(m_illumination * 2);
    }
    painter.fillRect(0, 0, width(), height(), backgroundColor);

    // draw pixmap
    int x = BorderGap;
    int y = (height() - m_pixmap.height()) / 2;

    if (!m_pixmap.isNull()) {
        painter.drawPixmap(x, y, m_pixmap);
        x += m_pixmap.width() + BorderGap * 2;
    }

    // draw text
    painter.setPen(palette().windowText().color());
    int flags = Qt::AlignVCenter;
    if (height() > m_minTextHeight) {
        flags = flags | Qt::TextWordWrap;
    }
    painter.drawText(QRect(x, 0, availableTextWidth(), height()), flags, m_text);
    painter.end();
}

void StatusBarMessageLabel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateCloseButtonPosition();
    //QTimer::singleShot(GeometryTimeout, this, SLOT(assureVisibleText()));
}

void StatusBarMessageLabel::timerDone()
{
    switch (m_state) {
    case Illuminate: {
        // increase the illumination
        const int illumination_max = 128;
        if (m_illumination < illumination_max) {
            m_illumination += 32;
            if (m_illumination > illumination_max) {
                m_illumination = illumination_max;
            }
            update();
        } else {
            m_state = Illuminated;
            m_timer.start(1500);
        }
        break;
    }

    case Illuminated: {
        // start desaturation
        if (m_type != MltError) {
            m_state = Desaturate;
            m_timer.start(80);
        }
        break;
    }

    case Desaturate: {
        // desaturate
        if (m_illumination < -128) {
            m_illumination = 0;
            m_state = Default;
            m_timer.stop();
            setMessage(QString(), DefaultMessage);
        } else {
            m_illumination -= 5;
            update();
        }
        break;
    }

    default:
        break;
    }
}

void StatusBarMessageLabel::assureVisibleText()
{
    if (m_text.isEmpty()) {
        return;
    }

    int requiredHeight = m_minTextHeight;
    if (m_type != DefaultMessage) {
        // Calculate the required height of the widget thats
        // needed for having a fully visible text. Note that for the default
        // statusbar type (e. g. hover information) increasing the text height
        // is not wanted, as this might rearrange the layout of items.

        QFontMetrics fontMetrics(font());
        const QRect bounds(fontMetrics.boundingRect(0, 0, availableTextWidth(), height(),
                           Qt::AlignVCenter | Qt::TextWordWrap, m_text));
        requiredHeight = bounds.height();
        if (requiredHeight < m_minTextHeight) {
            requiredHeight = m_minTextHeight;
        }
    }

    // Increase/decrease the current height of the widget to the
    // required height. The increasing/decreasing is done in several
    // steps to have an animation if the height is modified
    // (see StatusBarMessageLabel::resizeEvent())
    const int gap = m_minTextHeight / 2;
    int minHeight = minimumHeight();
    if (minHeight < requiredHeight) {
        minHeight += gap;
        if (minHeight > requiredHeight) {
            minHeight = requiredHeight;
        }
        setMinimumHeight(minHeight);
        updateGeometry();
    } else if (minHeight > requiredHeight) {
        minHeight -= gap;
        if (minHeight < requiredHeight) {
            minHeight = requiredHeight;
        }
        setMinimumHeight(minHeight);
        updateGeometry();
    }

    updateCloseButtonPosition();
}

int StatusBarMessageLabel::availableTextWidth() const
{
    const int buttonWidth = 0; /*(m_type == ErrorMessage) ?
                            m_closeButton->width() + BorderGap : 0;*/
    return width() - m_pixmap.width() - (BorderGap * 4) - buttonWidth;
}

void StatusBarMessageLabel::updateCloseButtonPosition()
{
    const int x = width() - m_closeButton->width() - BorderGap;
    const int y = (height() - m_closeButton->height()) / 2;
    m_closeButton->move(x, y);
}

void StatusBarMessageLabel::closeErrorMessage()
{
    if (!showPendingMessage()) {
        setMessage(QString(), DefaultMessage);
    }
}

bool StatusBarMessageLabel::showPendingMessage()
{
    if (!m_pendingMessages.isEmpty()) {
        setMessage(m_pendingMessages.takeFirst(), ErrorMessage);
        return true;
    }
    return false;
}


#include "statusbarmessagelabel.moc"
