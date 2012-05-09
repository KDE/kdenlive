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


#include <QList>
#include <QPixmap>
#include <QWidget>
#include <QTimer>
#include <QSemaphore>

#include <definitions.h>

#include "nolib/qtimerWithTime.h"

class QPaintEvent;
class QResizeEvent;
class QPushButton;


/**
  Queue-able message item holding all important information
  */
struct StatusBarMessageItem {

    QString text;
    MessageType type;
    int timeoutMillis;
    bool confirmed; ///< MLT errors need to be confirmed.

    /// \return true if the error still needs to be confirmed
    bool needsConfirmation() const
    {
        return type == MltError && !confirmed;
    }

    StatusBarMessageItem(const QString& text = QString(), MessageType type = DefaultMessage, int timeoutMS = 0) :
        text(text), type(type), timeoutMillis(timeoutMS), confirmed(false) {}

    bool operator ==(const StatusBarMessageItem &other)
    {
        return type == other.type && text == other.text;
    }
};

/**
 * @brief Represents a message text label as part of the status bar.
 *
 * Dependent from the given type automatically a corresponding icon
 * is shown in front of the text. For message texts having the type
 * DolphinStatusBar::Error a dynamic color blending is done to get the
 * attention from the user.
 */
class StatusBarMessageLabel : public QWidget
{
    Q_OBJECT

public:
    explicit StatusBarMessageLabel(QWidget* parent);
    virtual ~StatusBarMessageLabel();

    // TODO: maybe a better approach is possible with the size hint
    void setMinimumTextHeight(int min);
    int minimumTextHeight() const;

protected:
    /** @see QWidget::paintEvent() */
    virtual void paintEvent(QPaintEvent* event);

    /** @see QWidget::resizeEvent() */
    virtual void resizeEvent(QResizeEvent* event);

public slots:
    void setMessage(const QString& text, MessageType type, int timeoutMS = 0);

private slots:
    void timerDone();

    /**
     * Returns the available width in pixels for the text.
     */
    int availableTextWidth() const;

    /**
     * Moves the close button to the upper right corner
     * of the message label.
     */
    void updateCloseButtonPosition();

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

private:
    enum State {
        Default,
        Illuminate,
        Illuminated,
        Desaturate
    };

    enum { GeometryTimeout = 100 };
    enum { BorderGap = 2 };

    State m_state;
    int m_illumination;
    int m_minTextHeight;
    QTimer m_timer;

    QTimerWithTime m_queueTimer;
    QSemaphore m_queueSemaphore;
    QList<StatusBarMessageItem> m_messageQueue;
    StatusBarMessageItem m_currentMessage;

    QPixmap m_pixmap;
    QPushButton* m_closeButton;
};

inline int StatusBarMessageLabel::minimumTextHeight() const
{
    return m_minTextHeight;
}


#endif
