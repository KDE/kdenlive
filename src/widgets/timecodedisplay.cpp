/* This file is part of the KDE project
    SPDX-FileCopyrightText: 2010 Jean-Baptiste Mardelle <jb@kdenlive.org>

    SPDX-License-Identifier: LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "timecodedisplay.h"
#include "core.h"
#include "kdenlivesettings.h"

#include <QApplication>
#include <QFontDatabase>
#include <QLineEdit>
#include <QMouseEvent>
#include <QStyle>

#include <KColorScheme>

TimecodeValidator::TimecodeValidator(QObject *parent)
    : QValidator(parent)
{
}

void TimecodeValidator::fixup(QString &str) const
{
    str.replace(QLatin1Char(' '), QLatin1Char('0'));
}

QValidator::State TimecodeValidator::validate(QString &str, int &) const
{
    if (str.contains(QLatin1Char(' '))) {
        fixup(str);
    }
    return QValidator::Acceptable;
}

TimecodeDisplay::TimecodeDisplay(QWidget *parent, bool autoAdjust)
    : TimecodeDisplay(parent, (pCore && autoAdjust) ? pCore->timecode() : Timecode())
{
    if (pCore && autoAdjust) {
        connect(pCore.get(), &Core::updateProjectTimecode, this, &TimecodeDisplay::refreshTimeCode);
    }
    installEventFilter(this);
    lineEdit()->installEventFilter(this);
}

TimecodeDisplay::TimecodeDisplay(QWidget *parent, const Timecode &t)
    : QAbstractSpinBox(parent)
    , m_timecode(t)
    , m_frametimecode(false)
    , m_minimum(0)
    , m_maximum(-1)
    , m_value(0)
    , m_offset(0)
{
    installEventFilter(this);
    lineEdit()->installEventFilter(this);
    /*const QFont ft = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    lineEdit()->setFont(ft);
    setFont(ft);
    lineEdit()->setAlignment(Qt::AlignRight | Qt::AlignVCenter);*/
    const QFont ft = font();
    QFontMetrics fm(ft);
    setFrame(false);
    QPalette palette;
    palette.setColor(QPalette::Base, Qt::transparent); // palette.window().color());
    setPalette(palette);
    setTimeCodeFormat(KdenliveSettings::frametimecode(), true);
    setValue(m_minimum);
    setMinimumWidth(fm.horizontalAdvance(QStringLiteral("88:88:88:88")) + contentsMargins().right() + contentsMargins().left() + frameSize().width() -
                    lineEdit()->contentsRect().width() + (int)QStyle::PM_SpinBoxFrameWidth + 6);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Maximum);
    setAccelerated(true);
    setMinimumHeight(lineEdit()->sizeHint().height());
    connect(lineEdit(), &QLineEdit::editingFinished, this, &TimecodeDisplay::slotEditingFinished, Qt::DirectConnection);
}

// virtual protected
QAbstractSpinBox::StepEnabled TimecodeDisplay::stepEnabled() const
{
    QAbstractSpinBox::StepEnabled result = QAbstractSpinBox::StepNone;
    if (m_value > m_minimum) {
        result |= QAbstractSpinBox::StepDownEnabled;
    }
    if (m_maximum == -1 || m_value < m_maximum) {
        result |= QAbstractSpinBox::StepUpEnabled;
    }
    return result;
}

// virtual
void TimecodeDisplay::stepBy(int steps)
{
    int val = m_value + steps;
    setValue(val);
}

void TimecodeDisplay::setTimeCodeFormat(bool frametimecode, bool init)
{
    if (!init && m_frametimecode == frametimecode) {
        return;
    }
    m_frametimecode = frametimecode;
    lineEdit()->clear();
    if (m_frametimecode) {
        auto *valid = new QIntValidator(lineEdit());
        valid->setBottom(0);
        lineEdit()->setValidator(valid);
        lineEdit()->setInputMask(QString());
    } else {
        lineEdit()->setInputMask(m_timecode.mask());
        auto *valid = new TimecodeValidator(lineEdit());
        lineEdit()->setValidator(valid);
    }
    setValue(m_value);
}

void TimecodeDisplay::updateTimeCode(const Timecode &t)
{
    m_timecode = t;
    setTimeCodeFormat(KdenliveSettings::frametimecode(), true);
}

void TimecodeDisplay::refreshTimeCode()
{
    updateTimeCode(pCore->timecode());
}

void TimecodeDisplay::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        e->setAccepted(true);
        clearFocus();
    } else {
        QAbstractSpinBox::keyPressEvent(e);
    }
}

void TimecodeDisplay::mouseReleaseEvent(QMouseEvent *e)
{
    QAbstractSpinBox::mouseReleaseEvent(e);
    if (!lineEdit()->underMouse()) {
        clearFocus();
    }
}

void TimecodeDisplay::wheelEvent(QWheelEvent *e)
{
    QAbstractSpinBox::wheelEvent(e);
    if (hasFocus()) {
        clearFocus();
    } else {
        slotEditingFinished();
    }
}

void TimecodeDisplay::enterEvent(QEnterEvent *e)
{
    QAbstractSpinBox::enterEvent(e);
    setFrame(true);
}

void TimecodeDisplay::leaveEvent(QEvent *e)
{
    QAbstractSpinBox::leaveEvent(e);
    setFrame(false);
}

int TimecodeDisplay::maximum() const
{
    return m_maximum;
}

int TimecodeDisplay::minimum() const
{
    return m_minimum;
}

int TimecodeDisplay::getValue() const
{
    return m_value;
}

GenTime TimecodeDisplay::gentime() const
{
    return {m_value, m_timecode.fps()};
}

Timecode TimecodeDisplay::timecode() const
{
    return m_timecode;
}

void TimecodeDisplay::setTimecode(const Timecode &t)
{
    m_timecode = t;
}

void TimecodeDisplay::setRange(int min, int max)
{
    m_minimum = min;
    m_maximum = max;
}

void TimecodeDisplay::setValue(const QString &value)
{
    setValue(m_timecode.getFrameCount(value));
}

void TimecodeDisplay::setValue(int value)
{
    if (m_maximum > 0) {
        value = qBound(m_minimum, value, m_maximum);
    } else {
        value = qMax(m_minimum, value);
    }

    if (m_frametimecode) {
        if (value == m_value && !lineEdit()->text().isEmpty()) {
            return;
        }
        m_value = value;
        lineEdit()->setText(QString::number(value - m_minimum));
    } else {
        if (value == m_value && lineEdit()->text() != QLatin1String(":::")) {
            return;
        }
        m_value = value;
        lineEdit()->setText(m_timecode.getTimecodeFromFrames(m_offset + value - m_minimum));
    }
    Q_EMIT timeCodeUpdated();
}

void TimecodeDisplay::setValue(const GenTime &value)
{
    setValue((int)value.frames(m_timecode.fps()));
}

void TimecodeDisplay::slotEditingFinished()
{
    lineEdit()->deselect();
    if (m_frametimecode) {
        setValue(lineEdit()->text().toInt() + m_minimum);
    } else {
        setValue(m_timecode.getFrameCount(lineEdit()->text()) + m_minimum - m_offset);
    }
    Q_EMIT timeCodeEditingFinished(m_value);
}

const QString TimecodeDisplay::displayText() const
{
    return lineEdit()->displayText();
}

void TimecodeDisplay::selectAll()
{
    lineEdit()->selectAll();
}

void TimecodeDisplay::setMsOffset(int offset)
{
    m_offset = GenTime(offset / 1000.).frames(m_timecode.fps());
    // Update timecode display
    if (!m_frametimecode) {
        lineEdit()->setText(m_timecode.getTimecodeFromFrames(m_offset + m_value - m_minimum));
        Q_EMIT timeCodeUpdated();
    }
}

void TimecodeDisplay::setFrameOffset(int offset)
{
    m_offset = offset;
    // Update timecode display
    if (!m_frametimecode) {
        lineEdit()->setText(m_timecode.getTimecodeFromFrames(m_offset + m_value - m_minimum));
        Q_EMIT timeCodeUpdated();
    }
}

void TimecodeDisplay::setBold(bool enable)
{
    QFont font = lineEdit()->font();
    font.setBold(enable);
    lineEdit()->setFont(font);
}

bool TimecodeDisplay::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == lineEdit()) {
        QEvent::Type type = event->type();
        QList<QEvent::Type> handledTypes = {
            QEvent::ContextMenu, QEvent::MouseButtonPress, QEvent::MouseButtonRelease, QEvent::MouseMove, QEvent::Wheel, QEvent::Enter, QEvent::Leave,
            QEvent::FocusIn,     QEvent::FocusOut};
        if (!isEnabled() && handledTypes.contains(type)) {
            // Widget is disabled
            event->accept();
            return true;
        }
        if (type == QEvent::ContextMenu) {
            // auto *me = static_cast<QContextMenuEvent *>(event);
            // Q_EMIT showMenu(me->globalPos());
            event->accept();
            return true;
        }
        if (type == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            m_dragging = false;
            if (!lineEdit()->hasFocus()) {
                m_editing = false;
            }
            if (me->buttons() & Qt::LeftButton) {
                m_clickPos = me->position();
                m_cursorClickPos = lineEdit()->cursorPositionAt(m_clickPos.toPoint());
                m_clickMouse = QCursor::pos();
                if (!lineEdit()->hasFocus()) {
                    event->accept();
                    return true;
                }
            }
        }
        if (type == QEvent::Wheel) {
            auto *we = static_cast<QWheelEvent *>(event);
            int mini = 1;
            int factor = qMax(mini, (maximum() - minimum()) / 200);
            factor = qMin(4, factor);
            if (we->modifiers() & Qt::ControlModifier) {
                factor *= 5;
            } else if (we->modifiers() & Qt::ShiftModifier) {
                factor = mini;
            }
            if (we->angleDelta().y() > 0) {
                setValue(m_value + factor);
                slotEditingFinished();
            } else if (we->angleDelta().y() < 0) {
                setValue(m_value - factor);
                slotEditingFinished();
            }
            event->accept();
            return true;
        }
        if (type == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->buttons() & Qt::LeftButton) {
                QPointF movePos = me->position();
                if (!m_editing) {
                    if (!m_dragging && (movePos - m_clickPos).manhattanLength() >= QApplication::startDragDistance()) {
                        QGuiApplication::setOverrideCursor(QCursor(Qt::BlankCursor));
                        m_dragging = true;
                        m_clickPos = movePos;
                        lineEdit()->clearFocus();
                    }
                    if (m_dragging) {
                        int delta = movePos.x() - m_clickPos.x();
                        if (delta != 0) {
                            m_clickPos = movePos;
                            int mini = 1;
                            int factor = qMax(mini, (maximum() - minimum()) / 200);
                            factor = qMin(4, factor);
                            if (me->modifiers() & Qt::ControlModifier) {
                                factor *= 5;
                            } else if (me->modifiers() & Qt::ShiftModifier) {
                                factor = mini;
                            }
                            setValue(m_value + factor * delta);
                            slotEditingFinished();
                        }
                    }
                    event->accept();
                    return true;
                }
            }
        }
        if (type == QEvent::MouseButtonRelease) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::MiddleButton) {
                event->accept();
                return true;
            }
            if (m_dragging) {
                QCursor::setPos(m_clickMouse);
                QGuiApplication::restoreOverrideCursor();
                m_dragging = false;
                event->accept();
                return true;
            } else {
                m_editing = true;
                lineEdit()->setFocus(Qt::MouseFocusReason);
            }
        }
        if (type == QEvent::Enter) {
            if (!m_editing) {
                event->accept();
                return true;
            }
        }
    } else {
        if (event->type() == QEvent::FocusOut) {
            m_editing = false;
        }
    }
    return QObject::eventFilter(watched, event);
}
