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

#ifndef STATUSBARMESSAGELABEL_H
#define STATUSBARMESSAGELABEL_H

#include <QColor>
#include <QLabel>
#include <QList>
#include <QSemaphore>
#include <QTimer>
#include <QWidget>
#include <definitions.h>
#include <utility>

#include "lib/qtimerWithTime.h"

class QPaintEvent;
class QResizeEvent;
class QProgressBar;

class FlashLabel : public QWidget
{
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_OBJECT
public:
    explicit FlashLabel(QWidget *parent = nullptr);
    ~FlashLabel() override;
    QColor color() const;
    void setColor(const QColor &);
signals:
    void colorChanged();
};

/**
  Queue-able message item holding all important information
  */
struct StatusBarMessageItem
{

    QString text;
    MessageType type;
    int timeoutMillis;
    bool confirmed{false}; ///< MLT errors need to be confirmed.

    /// \return true if the error still needs to be confirmed
    bool needsConfirmation() const { return (type == MltError && !confirmed); }

    StatusBarMessageItem(QString messageText = QString(), MessageType messageType = DefaultMessage, int timeoutMS = 0)
        : text(std::move(messageText))
        , type(messageType)
        , timeoutMillis(timeoutMS)
    {
    }

    bool operator==(const StatusBarMessageItem &other) const { return type == other.type && text == other.text; }
};

/**
 * @brief Represents a message text label as part of the status bar.
 *
 * Dependent from the given type automatically a corresponding icon
 * is shown in front of the text. For message texts having the type
 * DolphinStatusBar::Error a dynamic color blending is done to get the
 * attention from the user.
 */
class StatusBarMessageLabel : public FlashLabel
{
    Q_OBJECT

public:
    explicit StatusBarMessageLabel(QWidget *parent);
    ~StatusBarMessageLabel() override;

protected:
    // void paintEvent(QPaintEvent* event);
    void mousePressEvent(QMouseEvent *) override;

    /** @see QWidget::resizeEvent() */
    void resizeEvent(QResizeEvent *event) override;

public slots:
    void setProgressMessage(const QString &text, MessageType type = ProcessingJobMessage, int progress = 100);
    void setMessage(const QString &text, MessageType type = DefaultMessage, int timeoutMS = 0);

private slots:

    /**
     * Closes the currently shown error message and replaces it
     * by the next pending message.
     */
    void confirmErrorMessage();

    /**
     * Shows the next pending error message. If no pending message
     * was in the queue, false is returned.
     */
    bool slotMessageTimeout();
    void slotShowJobLog(const QString &text);

private:
    enum { GeometryTimeout = 100 };
    enum { BorderGap = 2 };

    int m_minTextHeight;
    QLabel *m_pixmap;
    QLabel *m_label;
    QProgressBar *m_progress;
    QTimerWithTime m_queueTimer;
    QSemaphore m_queueSemaphore;
    QList<StatusBarMessageItem> m_messageQueue;
    StatusBarMessageItem m_currentMessage;
};

#endif
