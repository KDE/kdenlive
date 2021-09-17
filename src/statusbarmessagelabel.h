/*
    SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2008 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2012 Simon A. Eugster <simon.eu@gmail.com>

    Some code borrowed from Dolphin, adapted (2008) to Kdenlive

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/


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
class StatusBarMessageLabel : public QWidget
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
    /** @brief Display a key binding info in status bar */
    void setKeyMap(const QString &text);
    /** @brief Display a temporary key binding info in status bar, revert to default one if text is empty */
    void setTmpKeyMap(const QString &text);

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
    FlashLabel *m_container;
    QLabel *m_pixmap;
    QLabel *m_label;
    QLabel *m_keyMap;
    QString m_keymapText;
    QProgressBar *m_progress;
    QTimerWithTime m_queueTimer;
    QSemaphore m_queueSemaphore;
    QList<StatusBarMessageItem> m_messageQueue;
    StatusBarMessageItem m_currentMessage;
};

#endif
