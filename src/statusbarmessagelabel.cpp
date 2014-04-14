/***************************************************************************
 *   Copyright (C) 2006 by Peter Penz                                      *
 *                 2012    Simon A. Eugster <simon.eu@gmail.com>           *
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
#include "kdenlivesettings.h"

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
    m_state(Default),
    m_illumination(-64),
    m_minTextHeight(-1),
    m_queueSemaphore(1),
    m_closeButton(0)
{
    setMinimumHeight(KIconLoader::SizeSmall);
    QPalette palette;
    palette.setColor(QPalette::Background, Qt::transparent);
    setPalette(palette);

    m_closeButton = new QPushButton(i18nc("@action:button", "Confirm"), this);
    m_closeButton->hide();

    m_queueTimer.setSingleShot(true);

    bool b = true;
    b &= connect(&m_queueTimer, SIGNAL(timeout()), this, SLOT(slotMessageTimeout()));

    b &= connect(m_closeButton, SIGNAL(clicked()), this, SLOT(confirmErrorMessage()));
    b &= connect(&m_timer, SIGNAL(timeout()), this, SLOT(timerDone()));
    Q_ASSERT(b);
}

StatusBarMessageLabel::~StatusBarMessageLabel()
{
}

void StatusBarMessageLabel::setMessage(const QString& text,
                                       MessageType type, int timeoutMS)
{
    StatusBarMessageItem item(text, type, timeoutMS);

    if (item.type == ErrorMessage || item.type == MltError) {
        KNotification::event("ErrorMessage", item.text);
    }

    m_queueSemaphore.acquire();
    if (!m_messageQueue.contains(item)) {
        if (item.type == ErrorMessage || item.type == MltError || item.type == ProcessingJobMessage) {
            qDebug() << item.text;

            // Put the new errror message at first place and immediately show it
            if (item.timeoutMillis < 2000) {
                item.timeoutMillis = 2000;
            }
            m_messageQueue.push_front(item);

            // In case we are already displaying an error message, add a little delay
            int delay = 800 * (m_currentMessage.type == ErrorMessage || m_currentMessage.type == MltError);
            m_queueTimer.start(delay);

        } else {

            // Message with normal priority
            m_messageQueue.push_back(item);
            if (m_queueTimer.elapsed() >= m_currentMessage.timeoutMillis) {
                m_queueTimer.start(0);
            }

        }
    }

    m_queueSemaphore.release();
}

bool StatusBarMessageLabel::slotMessageTimeout()
{
    m_queueSemaphore.acquire();

    bool newMessage = false;

    // Get the next message from the queue, unless the current one needs to be confirmed
    if (m_currentMessage.type == ProcessingJobMessage) {
        // Check if we have a job completed message to cancel this one
        StatusBarMessageItem item;
        while (!m_messageQueue.isEmpty()) {
            item = m_messageQueue.at(0);
            m_messageQueue.removeFirst();
            if (item.type == OperationCompletedMessage || item.type == ErrorMessage || item.type == MltError) {
                m_currentMessage = item;
                newMessage = true;
                break;
            }
        }
    }   
    else if (!m_messageQueue.isEmpty()) {
        if (!m_currentMessage.needsConfirmation()) {
            m_currentMessage = m_messageQueue.at(0);
            m_messageQueue.removeFirst();
            newMessage = true;

        }
    }

    // If the queue is empty, add a default (empty) message
    if (m_messageQueue.isEmpty() && m_currentMessage.type != DefaultMessage) {
        m_messageQueue.push_back(StatusBarMessageItem());
    }

    // Start a new timer, unless the current message still needs to be confirmed
    if (!m_messageQueue.isEmpty()) {

        if (!m_currentMessage.needsConfirmation()) {

            // If we only have the default message left to show in the queue,
            // keep the current one for a little longer.
            m_queueTimer.start(m_currentMessage.timeoutMillis + 4000*(m_messageQueue.at(0).type == DefaultMessage));

        }
    }


    m_illumination = -64;
    m_state = Default;
    m_timer.stop();

    const char* iconName = 0;
    switch (m_currentMessage.type) {
    case ProcessingJobMessage:
        iconName = "chronometer";
        m_closeButton->hide();
        break;
    case OperationCompletedMessage:
        iconName = "dialog-ok";
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

    m_queueSemaphore.release();

    update();
    return newMessage;
}

void StatusBarMessageLabel::confirmErrorMessage()
{
    m_currentMessage.confirmed = true;
    m_queueTimer.start(0);
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

void StatusBarMessageLabel::paintEvent(QPaintEvent*)
{
    QPainter painter(this);

    // draw background
    QColor backgroundColor;
    if (m_state == Default || m_illumination < 0) backgroundColor = palette().window().color();
    else {
        backgroundColor = KStatefulBrush(KColorScheme::Window, KColorScheme::NegativeBackground, KSharedConfig::openConfig(KdenliveSettings::colortheme())).brush(this).color();
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
    painter.drawText(QRect(x, 0, availableTextWidth(), height()), flags, m_currentMessage.text);
    painter.end();
}

void StatusBarMessageLabel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateCloseButtonPosition();
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
        if (m_currentMessage.type != MltError) {
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


#include "statusbarmessagelabel.moc"
