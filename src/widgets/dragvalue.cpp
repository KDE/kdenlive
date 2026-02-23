/*
    SPDX-FileCopyrightText: 2011 Till Theato <root@ttill.de>
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "dragvalue.h"

#include "kdenlivesettings.h"

#include <cmath>

#include <KColorScheme>
#include <KColorUtils>
#include <KLocalizedString>
#include <QAction>
#include <QApplication>
#include <QCache>
#include <QChar>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QStack>
#include <QStyle>
#include <QWheelEvent>

namespace {
int getOperatorPriority(const QChar op)
{
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    return 0;
}

QValidator::State calculate(QStack<double> &nums, QStack<QChar> &ops)
{

    if (nums.length() < 2 || ops.isEmpty()) return QValidator::Intermediate;
    double b = nums.pop();
    double a = nums.pop();
    QChar op = ops.pop();

    double result;
    if (op == '+')
        result = a + b;
    else if (op == '-')
        result = a - b;
    else if (op == '*')
        result = a * b;
    else if (op == '/' && b != 0)
        result = a / b;
    else
        return QValidator::Invalid;

    nums.push(result);
    return QValidator::Acceptable;
}

bool isOperator(const QChar op)
{
    return op == '+' || op == '-' || op == '*' || op == '/' || op == '(' || op == ')';
}

double evaluateMathExpression(const QString expr, QValidator::State &state)
{
    state = QValidator::Acceptable;
    QStack<double> nums;
    QStack<QChar> ops;
    QChar prevValidChar = ' ';
    QChar nowValidChar = ' ';
    bool prevIsValue = false;
    QLocale locale = QLocale::system();
    int i = 0;
    while (i < expr.length()) {
        QChar c = expr[i];
        if (c.isSpace()) {
            i++;
            continue;
        }
        prevValidChar = nowValidChar;
        nowValidChar = c;

        // numbers
        //  check for normal number || check for negative number
        if (c.isDigit() || c == locale.decimalPoint() || (c == '-' && (prevValidChar == ' ' || isOperator(prevValidChar)))) {
            if (prevIsValue) {
                state = QValidator::Invalid;
                return 0;
            }

            QString str;
            do {
                str.append(expr[i]);
                i++;
            } while (i < expr.length() && (expr[i].isDigit() || expr[i] == locale.decimalPoint()));

            bool ok;
            double result = locale.toDouble(str, &ok);
            if (!ok) {
                state = QValidator::Intermediate;
                return 0;
            }
            nums.push(result);
            prevIsValue = true;
            continue;
        }

        // operators
        if (!prevIsValue && prevValidChar != '(' && prevValidChar != ')' && c != '(') {
            state = QValidator::Invalid;
            return 0;
        }
        prevIsValue = c == ')';

        if (c == '(') {
            ops.push(c);
            i++;
            continue;
        }
        if (c == ')') {
            while (!ops.isEmpty() && ops.top() != '(') {
                state = calculate(nums, ops);
                if (state != QValidator::Acceptable) return 0;
            }
            if (ops.isEmpty()) {
                state = QValidator::Intermediate;
                return 0;
            }
            ops.pop();

            i++;
            continue;
        }

        while (!ops.isEmpty() && getOperatorPriority(ops.top()) >= getOperatorPriority(c)) {
            state = calculate(nums, ops);
            if (state != QValidator::Acceptable) return 0;
        }
        ops.push(c);
        i++;
    }

    while (!ops.isEmpty()) {
        state = calculate(nums, ops);
        if (state != QValidator::Acceptable) return 0;
    }

    if (nums.length() != 1) {
        state = QValidator::Intermediate;
        return 0;
    }
    double result = nums.pop();

    return result;
}

// cache for saving QRegularExpression, used for matching params
// if just use QString::replace(QChar before, QChar after) directly
// it might catch things that you dont wanted
// for example: matched %x in %xy
QCache<QString, QRegularExpression> cache(100);

QValidator::State replaceParameters(QString &text, const QWidget *spinBox)
{
    DragValue *parent = qobject_cast<DragValue *>(spinBox->parent());
    if (!parent || !parent->hasDataProviderCallback()) {
        return QValidator::Invalid;
    }

    QMap<int, QPair<QString, double>> map = parent->runDataProviderCallback();
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        const QString param = it.value().first;
        const double value = it.value().second;

        if (!cache.contains(param) || cache[param] == nullptr) {
            QRegularExpression *re = new QRegularExpression(QString("(%1(?![a-zA-Z0-9_]))").arg(param));
            re->optimize();
            cache.insert(param, re);
        }

        if (!text.contains(param)) {
            continue;
        }
        text.replace(*cache[param], QString::number(value));
    }

    return QValidator::Acceptable;
}

// Modified from QAbstractSpinBoxPrivate::stripped
QString stripped(const QString &t, const QString specialValueText, const QString prefix, const QString suffix)
{
    QStringView text(t);
    if (specialValueText.size() == 0 || text != specialValueText) {
        int from = 0;
        int size = text.size();
        bool changed = false;
        if (prefix.size() && text.startsWith(prefix)) {
            from += prefix.size();
            size -= from;
            changed = true;
        }
        if (suffix.size() && text.endsWith(suffix)) {
            size -= suffix.size();
            changed = true;
        }
        if (changed) text = text.mid(from, size);
    }

    text = text.trimmed();
    return text.toString();
}

// Reimplementation of QSpinBoxPrivate::validateAndInterpret
double validateAndInterpret(const QString text, const QWidget *const spinBox, const QString specialValueText, const QString prefix, const QString suffix,
                            QValidator::State &state)
{
    state = QValidator::Acceptable;

    if (!spinBox) {
        state = QValidator::Invalid;
        return 0;
    }

    QString copy = stripped(text, specialValueText, prefix, suffix);

    static const QRegularExpression regexExpression(QRegularExpression::anchoredPattern(R"((?:[0-9.,+\-*/() ]*|%[a-zA-Z0-9_]*)*)"));
    if (!regexExpression.match(copy).hasMatch()) {
        state = QValidator::Invalid;
        return 0;
    }

    if (copy.contains('%')) {
        state = replaceParameters(copy, spinBox);
        if (state != QValidator::Acceptable) {
            return 0;
        }

        // allowing the user to finish their incomplete parameters by returning QValidator::Intermediate
        static const QRegularExpression regexMathExpr(QRegularExpression::anchoredPattern(R"([0-9.,+\-*/() ]*)"));
        if (!regexMathExpr.match(copy).hasMatch()) {
            state = QValidator::Intermediate;
            return 0;
        }
    }

    double result = evaluateMathExpression(copy, state);
    if (state != QValidator::Acceptable) return 0;
    return result;
}

void spinBoxSetToolTip(QWidget &self)
{
    DragValue *parent = qobject_cast<DragValue *>(self.parent());
    if (!parent || !parent->hasDataProviderCallback())
        self.setToolTip(i18n("Support basic math expression"));
    else {
        QMap<int, QPair<QString, double>> map = parent->runDataProviderCallback();
        QString str;
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            if (str != "") str += ", ";
            str += it.value().first;
        }
        self.setToolTip(i18nc("", "Support basic math expression\nuse \"%1\" to reference other spinbox's value", str));
    }
}
} // namespace

MySpinBox::MySpinBox(QWidget *parent)
    : QSpinBox(parent)
{
    installEventFilter(this);
    lineEdit()->installEventFilter(this);
    setMinimumHeight(lineEdit()->sizeHint().height() + 4);
}

QValidator::State MySpinBox::validate(QString &text, int &pos) const
{
    if (m_cachedText == text && !text.isEmpty()) return m_cachedState;

    QValidator::State state;
    m_cachedResult = validateAndInterpret(text, this, specialValueText(), prefix(), suffix(), state);
    m_cachedText = text;
    m_cachedState = state;
    return state;
}

int MySpinBox::valueFromText(const QString &text) const
{
    QValidator::State state;
    double result;
    if (m_cachedText == text && !text.isEmpty()) {
        state = m_cachedState;
        result = m_cachedResult;
    } else {
        result = validateAndInterpret(text, this, specialValueText(), prefix(), suffix(), state);
    }

    if (state != QValidator::Acceptable) return maximum() > 0 ? minimum() : maximum();
    return std::round(result);
}

bool MySpinBox::eventFilter(QObject *watched, QEvent *event)
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
            auto *me = static_cast<QContextMenuEvent *>(event);
            Q_EMIT showMenu(me->globalPos());
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
            if (me->buttons() & Qt::MiddleButton) {
                Q_EMIT resetValue();
                event->accept();
                if (!m_editing) {
                    lineEdit()->clearFocus();
                }
                return true;
            }
        }
        if (type == QEvent::Wheel) {
            if (blockWheel && !hasFocus()) {
                return false;
            }
            auto *we = static_cast<QWheelEvent *>(event);
            int mini = singleStep();
            int factor = qMax(mini, (maximum() - minimum()) / 200);
            factor = qMin(4, factor);
            if (we->modifiers() & Qt::ControlModifier) {
                factor *= 5;
            } else if (we->modifiers() & Qt::ShiftModifier) {
                factor = mini;
            }
            if (we->angleDelta().y() > 0) {
                setValue(value() + factor);
            } else if (we->angleDelta().y() < 0) {
                setValue(value() - factor);
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
                            int mini = singleStep();
                            int factor = qMax(mini, (maximum() - minimum()) / 200);
                            factor = qMin(4, factor);
                            if (me->modifiers() & Qt::ControlModifier) {
                                factor *= 5;
                            } else if (me->modifiers() & Qt::ShiftModifier) {
                                factor = mini;
                            }
                            setValue(value() + factor * delta);
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
        if (event->type() == QEvent::ToolTip && toolTip() == "") {
            spinBoxSetToolTip(*this);
        }
    }
    return QSpinBox::eventFilter(watched, event);
}

MyDoubleSpinBox::MyDoubleSpinBox(QWidget *parent)
    : QDoubleSpinBox(parent)
{
    installEventFilter(this);
    lineEdit()->installEventFilter(this);
    setMinimumHeight(lineEdit()->sizeHint().height() + 4);
}

QValidator::State MyDoubleSpinBox::validate(QString &text, int &pos) const
{
    if (m_cachedText == text && !text.isEmpty()) return m_cachedState;

    QValidator::State state;
    m_cachedResult = validateAndInterpret(text, this, specialValueText(), prefix(), suffix(), state);
    m_cachedText = text;
    m_cachedState = state;
    return state;
}

double MyDoubleSpinBox::valueFromText(const QString &text) const
{
    QValidator::State state;
    double result;
    if (m_cachedText == text && !text.isEmpty()) {
        state = m_cachedState;
        result = m_cachedResult;
    } else {
        result = validateAndInterpret(text, this, specialValueText(), prefix(), suffix(), state);
    }

    if (state != QValidator::Acceptable) return maximum() > 0 ? minimum() : maximum();
    return result;
}

bool MyDoubleSpinBox::eventFilter(QObject *watched, QEvent *event)
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
            auto *me = static_cast<QContextMenuEvent *>(event);
            Q_EMIT showMenu(me->globalPos());
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
                m_clickMouse = QCursor::pos();
                if (!lineEdit()->hasFocus()) {
                    event->accept();
                    return true;
                }
            }
            if (me->buttons() & Qt::MiddleButton) {
                Q_EMIT resetValue();
                event->accept();
                if (!m_editing) {
                    lineEdit()->clearFocus();
                }
                return true;
            }
        }
        if (type == QEvent::Wheel) {
            if (blockWheel && !hasFocus()) {
                return false;
            }
            auto *we = static_cast<QWheelEvent *>(event);
            double mini = singleStep();
            double factor = qMax(mini, (maximum() - minimum()) / 200);
            factor = qMin(4., factor);
            if (we->modifiers() & Qt::ControlModifier) {
                factor *= 5;
            } else if (we->modifiers() & Qt::ShiftModifier) {
                factor = mini;
            }
            if (we->angleDelta().y() > 0) {
                setValue(value() + factor);
            } else if (we->angleDelta().y() < 0) {
                setValue(value() - factor);
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
                        double delta = movePos.x() - m_clickPos.x();
                        if (delta != 0) {
                            m_clickPos = movePos;
                            double factor = singleStep();
                            factor = qMin(3., factor);
                            if (me->modifiers() & Qt::ControlModifier) {
                                factor *= 5;
                            } else if (me->modifiers() & Qt::ShiftModifier) {
                                factor *= 0.1;
                            }
                            setValue(value() + factor * delta);
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
            } else if (!m_editing) {
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
        if (event->type() == QEvent::ToolTip && toolTip() == "") {
            spinBoxSetToolTip(*this);
        }
    }
    return QDoubleSpinBox::eventFilter(watched, event);
}

DragValue::DragValue(const QString &label, double defaultValue, int decimals, double min, double max, int id, const QString &suffix, bool showSlider,
                     bool oddOnly, QWidget *parent, bool isInGroup)
    : QWidget(parent)
    , m_maximum(max)
    , m_minimum(min)
    , m_decimals(decimals)
    , m_default(defaultValue)
    , m_id(id)
    , m_labelText(label)
{
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setFocusPolicy(Qt::StrongFocus);

    auto *l = new QHBoxLayout;
    l->setSpacing(2);
    l->setContentsMargins(0, 0, 0, 0);
    if (showSlider && m_maximum != m_minimum) {
        m_label = new CustomLabel(label, showSlider, m_maximum - m_minimum, this);
        m_label->setObjectName("draggLabel");
        l->addWidget(m_label, 0, Qt::AlignVCenter);
        // setMinimumHeight(m_label->sizeHint().height());
        connect(m_label, &CustomLabel::customValueChanged, this, &DragValue::setValueFromProgress);
        connect(m_label, &CustomLabel::resetValue, this, &DragValue::slotReset);
    } else if (!isInGroup) {
        l->addStretch(10);
    }
    int charWidth = QFontMetrics(font()).averageCharWidth();

    if (decimals == 0) {
        if (m_label) {
            m_label->setMaximum(max - min);
            m_label->setStep(oddOnly ? 2 : 1);
        }
        m_intEdit = new MySpinBox(this);
        connect(m_intEdit, &MySpinBox::resetValue, this, &DragValue::slotReset);
        connect(m_intEdit, &MySpinBox::showMenu, this, [this](const QPoint &pos) { m_menu->popup(pos); });
        m_intEdit->setObjectName(QStringLiteral("dragBox"));
        m_intEdit->setFocusPolicy(Qt::StrongFocus);
        if (!suffix.isEmpty()) {
            m_intEdit->setSuffix(QLatin1Char(' ') + suffix);
        }
        m_intEdit->setKeyboardTracking(false);
        m_intEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
        m_intEdit->setAlignment(Qt::AlignCenter);
        m_intEdit->setRange((int)m_minimum, (int)m_maximum);
        m_intEdit->setValue((int)m_default);
        QPalette pal = m_intEdit->palette();
        pal.setColor(QPalette::Active, QPalette::Base, pal.mid().color());
        m_intEdit->setAutoFillBackground(true);
        m_intEdit->setPalette(pal);
        // Try to have all spin boxes of the same size
        m_intEdit->setMinimumWidth(charWidth * 9);
        setMinimumHeight(m_intEdit->minimumHeight());
        if (oddOnly) {
            m_intEdit->setSingleStep(2);
        }
        l->addWidget(m_intEdit);
        connect(m_intEdit, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
                static_cast<void (DragValue::*)(int)>(&DragValue::slotSetValue));
        connect(m_intEdit, &QAbstractSpinBox::editingFinished, this, &DragValue::slotEditingFinished);
        m_intEdit->installEventFilter(this);
    } else {
        m_doubleEdit = new MyDoubleSpinBox(this);
        connect(m_doubleEdit, &MyDoubleSpinBox::resetValue, this, &DragValue::slotReset);
        connect(m_doubleEdit, &MyDoubleSpinBox::showMenu, this, [this](const QPoint &pos) { m_menu->popup(pos); });
        m_doubleEdit->setDecimals(decimals);
        m_doubleEdit->setFocusPolicy(Qt::StrongFocus);
        m_doubleEdit->setObjectName(QStringLiteral("dragBox"));
        if (!suffix.isEmpty()) {
            m_doubleEdit->setSuffix(QLatin1Char(' ') + suffix);
        }
        m_doubleEdit->setKeyboardTracking(false);
        m_doubleEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
        QPalette pal = m_doubleEdit->palette();
        pal.setColor(QPalette::Active, QPalette::Base, pal.mid().color());
        m_doubleEdit->setAutoFillBackground(true);
        m_doubleEdit->setPalette(pal);
        m_doubleEdit->setAlignment(Qt::AlignCenter);
        m_doubleEdit->setRange(m_minimum, m_maximum);
        double factor = 100;
        if (m_maximum - m_minimum > 10000) {
            factor = 1000;
        }
        double steps = (m_maximum - m_minimum) / factor;
        m_doubleEdit->setSingleStep(steps);
        if (m_label) {
            m_label->setStep(steps);
        }
        l->addWidget(m_doubleEdit);
        m_doubleEdit->setValue(m_default);
        // Try to have all spin boxes of the same size
        m_doubleEdit->setMinimumWidth(charWidth * 9);
        setMinimumHeight(m_doubleEdit->minimumHeight());
        m_doubleEdit->installEventFilter(this);
        connect(m_doubleEdit, SIGNAL(valueChanged(double)), this, SLOT(slotSetValue(double)));
        connect(m_doubleEdit, &QAbstractSpinBox::editingFinished, this, &DragValue::slotEditingFinished);
    }
    if (!showSlider && !isInGroup) {
        l->addStretch(10);
    }
    setLayout(l);
    int minimumHeight = 0;
    if (m_intEdit) {
        minimumHeight = m_intEdit->minimumHeight();
    } else {
        minimumHeight = m_doubleEdit->minimumHeight();
    }
    if (m_label) {
        m_label->setFixedHeight(minimumHeight);
    }
    setFixedHeight(minimumHeight);
    m_menu = new QMenu(this);

    m_scale = new KSelectAction(i18n("Scaling"), this);
    m_scale->addAction(i18n("Normal scale"));
    m_scale->addAction(i18n("Pixel scale"));
    m_scale->addAction(i18n("Nonlinear scale"));
    m_scale->setCurrentItem(KdenliveSettings::dragvalue_mode());
    m_menu->addAction(m_scale);

    m_directUpdate = new QAction(i18n("Direct update"), this);
    m_directUpdate->setCheckable(true);
    m_directUpdate->setChecked(KdenliveSettings::dragvalue_directupdate());
    m_menu->addAction(m_directUpdate);

    QAction *reset = new QAction(QIcon::fromTheme(QStringLiteral("edit-undo")), i18n("Reset value"), this);
    connect(reset, &QAction::triggered, this, &DragValue::slotReset);
    m_menu->addAction(reset);

    if (m_id > -1) {
        QAction *timeline = new QAction(QIcon::fromTheme(QStringLiteral("go-jump")), i18n("Show %1 in timeline", label), this);
        connect(timeline, &QAction::triggered, this, &DragValue::slotSetInTimeline);
        if (m_label) {
            connect(m_label, &CustomLabel::setInTimeline, this, &DragValue::slotSetInTimeline);
        }
        m_menu->addAction(timeline);
    }

    connect(this, &QWidget::customContextMenuRequested, this, &DragValue::slotShowContextMenu);
    connect(m_scale, &KSelectAction::indexTriggered, this, &DragValue::slotSetScaleMode);
    connect(m_directUpdate, &QAction::triggered, this, &DragValue::slotSetDirectUpdate);
}

DragValue::~DragValue()
{
    // Ensure the cursor does not stay hidden if the widget is deleted
    if (QGuiApplication::overrideCursor()) {
        QGuiApplication::restoreOverrideCursor();
    }
    delete m_intEdit;
    delete m_doubleEdit;
    delete m_menu;
    delete m_label;
    // delete m_scale;
    // delete m_directUpdate;
}

QLabel *DragValue::createLabel()
{
    return new QLabel(m_labelText, this);
}

bool DragValue::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        // Check if we should ignore the event
        bool useEvent = false;
        if (m_intEdit) {
            useEvent = m_intEdit->hasFocus();
        } else if (m_doubleEdit) {
            useEvent = m_doubleEdit->hasFocus();
        }
        if (!useEvent) {
            return true;
        }

        if (m_label) {
            auto *we = static_cast<QWheelEvent *>(event);
            if (we->angleDelta().y() > 0) {
                if (we->modifiers() == Qt::ControlModifier) {
                    m_label->slotValueInc(10);
                } else if (we->modifiers() == Qt::ShiftModifier) {
                    m_label->slotValueInc(0.1);
                } else {
                    m_label->slotValueInc();
                }
            } else {
                if (we->modifiers() == Qt::ControlModifier) {
                    m_label->slotValueDec(10);
                } else if (we->modifiers() == Qt::ShiftModifier) {
                    m_label->slotValueDec(0.1);
                } else {
                    m_label->slotValueDec();
                }
            }
        }
        // Stop processing, event accepted
        event->accept();
        return true;
    }
    return QObject::eventFilter(watched, event);
}

bool DragValue::hasEditFocus() const
{
    QWidget *fWidget = QApplication::focusWidget();
    return ((fWidget != nullptr) && fWidget->parentWidget() == this);
}

int DragValue::spinSize()
{
    if (m_intEdit) {
        return m_intEdit->sizeHint().width();
    }
    return m_doubleEdit->sizeHint().width();
}

void DragValue::setSpinSize(int width)
{
    if (m_intEdit) {
        m_intEdit->setMinimumWidth(width);
    } else {
        m_doubleEdit->setMinimumWidth(width);
    }
}

void DragValue::slotSetInTimeline()
{
    Q_EMIT inTimeline(m_id);
}

int DragValue::precision() const
{
    return m_decimals;
}

qreal DragValue::maximum() const
{
    return m_maximum;
}

qreal DragValue::minimum() const
{
    return m_minimum;
}

qreal DragValue::value() const
{
    if (m_intEdit) {
        return m_intEdit->value();
    }
    return m_doubleEdit->value();
}

void DragValue::setMaximum(qreal max)
{
    if (!qFuzzyCompare(m_maximum, max)) {
        m_maximum = max;
        if (m_intEdit) {
            m_intEdit->setRange(m_minimum, m_maximum);
        } else {
            m_doubleEdit->setRange(m_minimum, m_maximum);
        }
    }
}

void DragValue::setMinimum(qreal min)
{
    if (!qFuzzyCompare(m_minimum, min)) {
        m_minimum = min;
        if (m_intEdit) {
            m_intEdit->setRange(m_minimum, m_maximum);
        } else {
            m_doubleEdit->setRange(m_minimum, m_maximum);
        }
    }
}

void DragValue::setRange(qreal min, qreal max)
{
    m_maximum = max;
    m_minimum = min;
    if (m_intEdit) {
        m_intEdit->setRange(m_minimum, m_maximum);
    } else {
        m_doubleEdit->setRange(m_minimum, m_maximum);
    }
    if (m_label) {
        m_label->setMaximum(max - min);
    }
}

void DragValue::setStep(qreal step)
{
    if (m_intEdit) {
        m_intEdit->setSingleStep(step);
    } else {
        m_doubleEdit->setSingleStep(step);
    }
    if (m_label) {
        m_label->setStep(step);
    }
}

void DragValue::slotReset()
{
    if (m_intEdit) {
        m_intEdit->blockSignals(true);
        m_intEdit->setValue(m_default);
        m_intEdit->blockSignals(false);
        Q_EMIT customValueChanged((int)m_default, true);
    } else {
        m_doubleEdit->blockSignals(true);
        m_doubleEdit->setValue(m_default);
        m_doubleEdit->blockSignals(false);
        Q_EMIT customValueChanged(m_default, true);
    }
    if (m_label) {
        m_label->setProgressValue((m_default - m_minimum) / (m_maximum - m_minimum) * m_label->maximum());
    }
}

void DragValue::slotSetValue(int value)
{
    setValue(value, true);
}

void DragValue::slotSetValue(double value)
{
    setValue(value, true);
}

void DragValue::setValueFromProgress(double value, bool final, bool createUndoEntry, bool updateWidget)
{
    value = m_minimum + value * (m_maximum - m_minimum) / m_label->maximum();
    if (m_decimals == 0) {
        setValue(qRound(value), final, createUndoEntry, updateWidget);
    } else {
        setValue(value, final, createUndoEntry, updateWidget);
    }
}

void DragValue::setValue(double value, bool final, bool createUndoEntry, bool updateWidget)
{
    value = qBound(m_minimum, value, m_maximum);
    if (m_intEdit && m_intEdit->singleStep() > 1) {
        int div = (value - m_minimum) / m_intEdit->singleStep();
        value = m_minimum + (div * m_intEdit->singleStep());
    }
    if (!updateWidget) {
        Q_EMIT customValueChanged(value, final, createUndoEntry);
        return;
    }
    if (m_label) {
        m_label->setProgressValue((value - m_minimum) / (m_maximum - m_minimum) * m_label->maximum());
    }
    if (m_intEdit) {
        int val = qRound(value);
        m_intEdit->blockSignals(true);
        m_intEdit->setValue(val);
        m_intEdit->blockSignals(false);
        Q_EMIT customValueChanged(double(val), final, createUndoEntry);
    } else {
        m_doubleEdit->blockSignals(true);
        m_doubleEdit->setValue(value);
        m_doubleEdit->blockSignals(false);
        Q_EMIT customValueChanged(value, final, createUndoEntry);
    }
}

void DragValue::slotEditingFinished()
{
    if (m_intEdit) {
        int newValue = m_intEdit->value();
        m_intEdit->blockSignals(true);
        if (m_intEdit->singleStep() > 1) {
            int div = (newValue - m_minimum) / m_intEdit->singleStep();
            newValue = m_minimum + (div * m_intEdit->singleStep());
            m_intEdit->setValue(newValue);
        }
        m_intEdit->clearFocus();
        m_intEdit->blockSignals(false);
        if (!KdenliveSettings::dragvalue_directupdate()) {
            Q_EMIT customValueChanged((double)newValue, true);
        }
    } else {
        double value = m_doubleEdit->value();
        m_doubleEdit->blockSignals(true);
        m_doubleEdit->clearFocus();
        m_doubleEdit->blockSignals(false);
        if (!KdenliveSettings::dragvalue_directupdate()) {
            Q_EMIT customValueChanged(value, true);
        }
    }
}

void DragValue::slotShowContextMenu(const QPoint &pos)
{
    // values might have been changed by another object of this class
    m_scale->setCurrentItem(KdenliveSettings::dragvalue_mode());
    m_directUpdate->setChecked(KdenliveSettings::dragvalue_directupdate());
    m_menu->exec(mapToGlobal(pos));
}

void DragValue::slotSetScaleMode(int mode)
{
    KdenliveSettings::setDragvalue_mode(mode);
}

void DragValue::slotSetDirectUpdate(bool directUpdate)
{
    KdenliveSettings::setDragvalue_directupdate(directUpdate);
}

void DragValue::blockWheel(bool block)
{
    if (m_intEdit) {
        m_intEdit->blockWheel = block;
    } else if (m_doubleEdit) {
        m_doubleEdit->blockWheel = block;
    }
}

void DragValue::setInTimelineProperty(bool intimeline)
{
    if (m_label->property("inTimeline").toBool() == intimeline) {
        return;
    }
    m_label->setProperty("inTimeline", intimeline);
    style()->unpolish(m_label);
    style()->polish(m_label);
    m_label->update();
    if (m_intEdit) {
        m_intEdit->setProperty("inTimeline", intimeline);
        style()->unpolish(m_intEdit);
        style()->polish(m_intEdit);
        m_intEdit->update();
    } else {
        m_doubleEdit->setProperty("inTimeline", intimeline);
        style()->unpolish(m_doubleEdit);
        style()->polish(m_doubleEdit);
        m_doubleEdit->update();
    }
}

void DragValue::setDataProviderCallback(std::function<QMap<int, QPair<QString, double>>()> callback)
{
    m_callback = callback;
}

bool DragValue::hasDataProviderCallback()
{
    return m_callback != nullptr;
}

QMap<int, QPair<QString, double>> DragValue::runDataProviderCallback()
{
    return m_callback();
}

CustomLabel::CustomLabel(const QString &label, bool showSlider, int range, QWidget *parent)
    : QSlider(Qt::Horizontal, parent)
    , m_dragMode(false)
    , m_showSlider(showSlider)
    , m_step(10.0)
    , m_value(0.)
// m_precision(pow(10, precision)),
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    // setFormat(QLatin1Char(' ') + label);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    if (m_showSlider) {
        setToolTip(xi18n("Shift + Drag to adjust value one by one."));
    }
    if (showSlider) {
        setRange(0, 1000);
    } else {
        setRange(0, range);
        QSize sh;
        const QFontMetrics &fm = fontMetrics();
        sh.setWidth(fm.horizontalAdvance(QLatin1Char(' ') + label + QLatin1Char(' ')));
        setMaximumWidth(sh.width());
        setObjectName(QStringLiteral("dragOnly"));
    }
    setValue(0);
}

void CustomLabel::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragStartPosition = m_dragLastPosition = e->pos();
        m_clickValue = m_value;
        e->accept();
    } else if (e->button() == Qt::MiddleButton) {
        Q_EMIT resetValue();
        m_dragStartPosition = QPoint(-1, -1);
    } else {
        QWidget::mousePressEvent(e);
    }
}

void CustomLabel::mouseMoveEvent(QMouseEvent *e)
{
    if ((e->buttons() & Qt::LeftButton) && m_dragStartPosition != QPoint(-1, -1)) {
        if (!m_dragMode && (e->pos() - m_dragStartPosition).manhattanLength() >= QApplication::startDragDistance()) {
            m_dragMode = true;
            m_dragLastPosition = e->pos();
            e->accept();
            return;
        }
        if (m_dragMode) {
            if (KdenliveSettings::dragvalue_mode() > 0 || !m_showSlider) {
                int diff = e->position().x() - m_dragLastPosition.x();
                if (qApp->isRightToLeft()) {
                    diff = 0 - diff;
                }

                if (e->modifiers() == Qt::ControlModifier) {
                    diff *= 2;
                } else if (e->modifiers() == Qt::ShiftModifier) {
                    diff /= 2;
                }
                if (KdenliveSettings::dragvalue_mode() == 2) {
                    diff = (diff > 0 ? 1 : -1) * pow(diff, 2);
                }
                double nv = m_value + diff * m_step;
                if (!qFuzzyCompare(nv, m_value)) {
                    setNewValue(nv, KdenliveSettings::dragvalue_directupdate(), false);
                }
            } else {
                double nv;
                if (qApp->isLeftToRight()) {
                    nv = minimum() + ((double)maximum() - minimum()) / width() * e->pos().x();
                } else {
                    nv = maximum() - ((double)maximum() - minimum()) / width() * e->pos().x();
                }
                if (!qFuzzyCompare(nv, value())) {
                    if (m_step > 1) {
                        int current = (int)value();
                        int diff = (nv - current) / m_step;
                        setNewValue(current + diff * m_step, true, false);
                    } else {
                        if (e->modifiers() == Qt::ShiftModifier) {
                            double current = value();
                            if (e->pos().x() > m_dragLastPosition.x()) {
                                nv = qMin(current + 1, (double)maximum());
                            } else {
                                nv = qMax((double)minimum(), current - 1);
                            }
                        }
                        setNewValue(nv, KdenliveSettings::dragvalue_directupdate(), false);
                    }
                }
            }
            m_dragLastPosition = e->pos();
            e->accept();
        }
    } else {
        QSlider::mouseMoveEvent(e);
    }
}

void CustomLabel::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton) {
        e->accept();
        return;
    }
    if (e->modifiers() == Qt::ControlModifier) {
        Q_EMIT setInTimeline();
        e->accept();
        return;
    }
    if (m_dragMode) {
        double newValue = m_value;
        // Simulate a return to initial click position to create undo entry
        Q_EMIT customValueChanged(m_clickValue, true, false, false);
        setNewValue(newValue, true, true);
        m_dragLastPosition = m_dragStartPosition;
        e->accept();
    } else if (m_showSlider) {
        int newVal = (double)maximum() * e->pos().x() / width();
        if (qApp->isRightToLeft()) {
            newVal = maximum() - newVal;
        }
        if (m_step > 1) {
            int current = (int)value();
            int diff = (newVal - current) / m_step;
            setNewValue(current + diff * m_step, true, true);
        } else {
            setNewValue(newVal, true, true);
        }
        m_dragLastPosition = m_dragStartPosition;
        e->accept();
    }
    m_dragMode = false;
}

void CustomLabel::wheelEvent(QWheelEvent *e)
{
    if (e->angleDelta().y() > 0) {
        if (e->modifiers() == Qt::ControlModifier) {
            slotValueInc(10);
        } else if (e->modifiers() == Qt::ShiftModifier) {
            slotValueInc(0.1);
        } else {
            slotValueInc();
        }
    } else if (e->angleDelta().y() < 0) {
        if (e->modifiers() == Qt::ControlModifier) {
            slotValueDec(10);
        } else if (e->modifiers() == Qt::ShiftModifier) {
            slotValueDec(0.1);
        } else {
            slotValueDec();
        }
    }
    e->accept();
}

void CustomLabel::slotValueInc(double factor)
{
    setNewValue(m_value + m_step * factor, true, true);
}

void CustomLabel::slotValueDec(double factor)
{
    setNewValue(m_value - m_step * factor, true, true);
}

void CustomLabel::setProgressValue(double value)
{
    m_value = value;
    setValue(qRound(value));
}

void CustomLabel::setNewValue(double value, bool update, bool createUndoEntry)
{
    m_value = value;
    setValue(qRound(value));
    Q_EMIT customValueChanged(value, update, createUndoEntry);
}

void CustomLabel::setStep(double step)
{
    m_step = step;
}
