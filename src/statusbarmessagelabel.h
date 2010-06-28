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

#ifndef STATUSBARMESSAGELABEL_H
#define STATUSBARMESSAGELABEL_H


#include <QList>
#include <QPixmap>
#include <QWidget>
#include <QTimer>

#include <definitions.h>

class QPaintEvent;
class QResizeEvent;
class QPushButton;

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

    MessageType type() const;

    const QString& text() const;

    // TODO: maybe a better approach is possible with the size hint
    void setMinimumTextHeight(int min);
    int minimumTextHeight() const;

protected:
    /** @see QWidget::paintEvent() */
    virtual void paintEvent(QPaintEvent* event);

    /** @see QWidget::resizeEvent() */
    virtual void resizeEvent(QResizeEvent* event);

public slots:
    void setMessage(const QString& text, MessageType type);

private slots:
    void timerDone();

    /**
     * Increases the height of the message label so that
     * the given text fits into given area.
     */
    void assureVisibleText();

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
    void closeErrorMessage();

private:
    /**
     * Shows the next pending error message. If no pending message
     * was in the queue, false is returned.
     */
    bool showPendingMessage();

    /**
     * Resets the message label properties. This is useful when the
     * result of invoking StatusBarMessageLabel::setMessage() should
     * not rely on previous states.
     */
    void reset();

private:
    enum State {
        Default,
        Illuminate,
        Illuminated,
        Desaturate
    };

    enum { GeometryTimeout = 100 };
    enum { BorderGap = 2 };

    MessageType m_type;
    State m_state;
    int m_illumination;
    int m_minTextHeight;
    QTimer m_timer;
    QTimer m_hidetimer;
    QString m_text;
    QList<QString> m_pendingMessages;
    QPixmap m_pixmap;
    QPushButton* m_closeButton;
};

inline MessageType StatusBarMessageLabel::type() const
{
    return m_type;
}

inline const QString& StatusBarMessageLabel::text() const
{
    return m_text;
}

inline int StatusBarMessageLabel::minimumTextHeight() const
{
    return m_minTextHeight;
}

#endif
