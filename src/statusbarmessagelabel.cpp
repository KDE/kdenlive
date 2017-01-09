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
#include <KNotification>
#include <klocalizedstring.h>

#include <QPushButton>
#include <QPixmap>
#include <QLabel>
#include <QProgressBar>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextEdit>

FlashLabel::FlashLabel(QWidget *parent) : QWidget(parent)
{
    setAutoFillBackground(true);
}

FlashLabel::~FlashLabel()
{
}

void FlashLabel::setColor(const QColor &col)
{
    QPalette pal = palette();
    pal.setColor(QPalette::Window, col);
    setPalette(pal);
    update();
}

QColor FlashLabel::color() const
{
    return palette().window().color();
}

StatusBarMessageLabel::StatusBarMessageLabel(QWidget *parent) :
    FlashLabel(parent),
    m_minTextHeight(-1),
    m_queueSemaphore(1)
{
    setMinimumHeight(KIconLoader::SizeSmall);
    QHBoxLayout *lay = new QHBoxLayout(this);
    m_pixmap = new QLabel(this);
    m_pixmap->setAlignment(Qt::AlignCenter);
    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignLeft);
    m_progress = new QProgressBar(this);
    lay->addWidget(m_pixmap);
    lay->addWidget(m_label);
    lay->addWidget(m_progress);
    setLayout(lay);
    m_progress->setVisible(false);
    lay->setContentsMargins(BorderGap, 0, BorderGap, 0);
    m_animation.setTargetObject(this);
    m_animation.setPropertyName("color");
    m_queueTimer.setSingleShot(true);
    connect(&m_queueTimer, &QTimer::timeout, this, &StatusBarMessageLabel::slotMessageTimeout);
    connect(m_label, &QLabel::linkActivated, this, &StatusBarMessageLabel::slotShowJobLog);
}

StatusBarMessageLabel::~StatusBarMessageLabel()
{
}

void StatusBarMessageLabel::updatePalette()
{
    m_animation.setKeyValueAt(1, parentWidget()->palette().window().color());
    setColor(parentWidget()->palette().window().color());
}

void StatusBarMessageLabel::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    if (m_pixmap->rect().contains(event->localPos().toPoint()) && m_currentMessage.type == MltError) {
        confirmErrorMessage();
    }
}

void StatusBarMessageLabel::setProgressMessage(const QString &text, int progress, MessageType type, int timeoutMS)
{
    if (type == ProcessingJobMessage) {
        m_progress->setValue(progress);
        m_progress->setVisible(progress < 100);
    } else if (m_currentMessage.type != ProcessingJobMessage || type == OperationCompletedMessage) {
        m_progress->setVisible(progress < 100);
    }
    if (text == m_currentMessage.text) {
        return;
    }
    setMessage(text, type, timeoutMS);
}

void StatusBarMessageLabel::setMessage(const QString &text, MessageType type, int timeoutMS)
{
    StatusBarMessageItem item(text, type, timeoutMS);
    if (type == OperationCompletedMessage) {
        m_progress->setVisible(false);
    }
    if (item.type == ErrorMessage || item.type == MltError) {
        KNotification::event(QStringLiteral("ErrorMessage"), item.text);
    }

    m_queueSemaphore.acquire();
    if (!m_messageQueue.contains(item)) {
        if (item.type == ErrorMessage || item.type == MltError || item.type == ProcessingJobMessage) {
            qCDebug(KDENLIVE_LOG) << item.text;

            // Put the new error message at first place and immediately show it
            if (item.timeoutMillis < 3000) {
                item.timeoutMillis = 3000;
            }
            if (item.type == ProcessingJobMessage) {
                // This is a job progress info, discard previous ones
                QList<StatusBarMessageItem> cleanList;
                foreach (const StatusBarMessageItem &msg, m_messageQueue) {
                    if (msg.type != ProcessingJobMessage) {
                        cleanList << msg;
                    }
                }
                m_messageQueue = cleanList;
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
            if (item.type == OperationCompletedMessage || item.type == ErrorMessage || item.type == MltError || item.type == ProcessingJobMessage) {
                m_currentMessage = item;
                m_label->setText(m_currentMessage.text);
                newMessage = true;
                break;
            }
        }
    } else if (!m_messageQueue.isEmpty()) {
        if (!m_currentMessage.needsConfirmation()) {
            m_currentMessage = m_messageQueue.at(0);
            m_label->setText(m_currentMessage.text);
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
            m_queueTimer.start(m_currentMessage.timeoutMillis + 4000 * (m_messageQueue.at(0).type == DefaultMessage));

        }
    }

    QColor bgColor = KStatefulBrush(KColorScheme::Window, KColorScheme::NegativeBackground, KSharedConfig::openConfig(KdenliveSettings::colortheme())).brush(this).color();
    const char *iconName = nullptr;
    if (m_animation.state() == QAbstractAnimation::Running) {
        m_animation.stop();
    }
    setColor(parentWidget()->palette().window().color());
    switch (m_currentMessage.type) {
    case ProcessingJobMessage:
        iconName = "chronometer";
        m_pixmap->setCursor(Qt::ArrowCursor);
        break;
    case OperationCompletedMessage:
        iconName = "dialog-ok";
        m_pixmap->setCursor(Qt::ArrowCursor);
        break;

    case InformationMessage:
        iconName = "dialog-information";
        m_pixmap->setCursor(Qt::ArrowCursor);
        break;

    case ErrorMessage:
        iconName = "dialog-warning";
        m_pixmap->setCursor(Qt::ArrowCursor);
        m_animation.setKeyValueAt(0, bgColor);
        m_animation.setKeyValueAt(0.8, bgColor);
        m_animation.setKeyValueAt(1, parentWidget()->palette().window().color());
        m_animation.setEasingCurve(QEasingCurve::OutCubic);
        m_animation.setDuration(4000);
        m_animation.start();
        break;

    case MltError:
        iconName = "dialog-close";
        m_pixmap->setCursor(Qt::PointingHandCursor);
        m_animation.setKeyValueAt(0, bgColor);
        m_animation.setKeyValueAt(1, bgColor);
        m_animation.setDuration(1000);
        m_animation.start();
        break;

    case DefaultMessage:
        m_pixmap->setCursor(Qt::ArrowCursor);
    default:
        break;
    }

    if (iconName == nullptr) {
        m_pixmap->setVisible(false);
    } else {
        m_pixmap->setPixmap(SmallIcon(iconName));
        m_pixmap->setVisible(true);
    }
    m_queueSemaphore.release();

    return newMessage;
}

void StatusBarMessageLabel::confirmErrorMessage()
{
    m_currentMessage.confirmed = true;
    m_queueTimer.start(0);
}

void StatusBarMessageLabel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void StatusBarMessageLabel::slotShowJobLog(const QString &text)
{
    QDialog d(this);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *l = new QVBoxLayout;
    QTextEdit t(&d);
    t.insertPlainText(QUrl::fromPercentEncoding(text.toUtf8()));
    t.setReadOnly(true);
    l->addWidget(&t);
    mainWidget->setLayout(l);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    d.setLayout(mainLayout);
    mainLayout->addWidget(mainWidget);
    mainLayout->addWidget(buttonBox);
    d.connect(buttonBox, &QDialogButtonBox::rejected, &d, &QDialog::accept);
    d.exec();
    confirmErrorMessage();
}
