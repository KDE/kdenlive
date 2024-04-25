/*
    SPDX-FileCopyrightText: 2011 Till Theato <root@ttill.de>
    SPDX-FileCopyrightText: 2011 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "dragvalue.h"

#include "kdenlivesettings.h"

#include <cmath>

#include <QAction>
#include <QApplication>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QWheelEvent>

#include <KLocalizedString>
#include <QFontDatabase>
#include <QStyle>
#include <kwidgetsaddons_version.h>

DragValue::DragValue(const QString &label, double defaultValue, int decimals, double min, double max, int id, const QString &suffix, bool showSlider,
                     bool oddOnly, QWidget *parent)
    : QWidget(parent)
    , m_maximum(max)
    , m_minimum(min)
    , m_decimals(decimals)
    , m_default(defaultValue)
    , m_id(id)
    , m_intEdit(nullptr)
    , m_doubleEdit(nullptr)
{
    if (showSlider) {
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    } else {
        setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
    }
    setFocusPolicy(Qt::StrongFocus);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setFocusPolicy(Qt::StrongFocus);

    auto *l = new QHBoxLayout;
    l->setSpacing(0);
    l->setContentsMargins(0, 0, 0, 0);
    m_label = new CustomLabel(label, showSlider, m_maximum - m_minimum, this);
    m_label->setObjectName("draggLabel");
    l->addWidget(m_label);
    setMinimumHeight(m_label->sizeHint().height());
    if (decimals == 0) {
        m_label->setMaximum(max - min);
        m_label->setStep(oddOnly ? 2 : 1);
        m_intEdit = new QSpinBox(this);
        m_intEdit->setObjectName(QStringLiteral("dragBox"));
        m_intEdit->setFocusPolicy(Qt::StrongFocus);
        if (!suffix.isEmpty()) {
            m_intEdit->setSuffix(QLatin1Char(' ') + suffix);
        }
        m_intEdit->setKeyboardTracking(false);
        m_intEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
        m_intEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_intEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        m_intEdit->setRange((int)m_minimum, (int)m_maximum);
        m_intEdit->setValue((int)m_default);
        if (oddOnly) {
            m_intEdit->setSingleStep(2);
        }
        l->addWidget(m_intEdit);
        connect(m_intEdit, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this,
                static_cast<void (DragValue::*)(int)>(&DragValue::slotSetValue));
        connect(m_intEdit, &QAbstractSpinBox::editingFinished, this, &DragValue::slotEditingFinished);
        m_intEdit->installEventFilter(this);
    } else {
        m_doubleEdit = new QDoubleSpinBox(this);
        m_doubleEdit->setDecimals(decimals);
        m_doubleEdit->setFocusPolicy(Qt::StrongFocus);
        m_doubleEdit->setObjectName(QStringLiteral("dragBox"));
        if (!suffix.isEmpty()) {
            m_doubleEdit->setSuffix(QLatin1Char(' ') + suffix);
        }
        m_doubleEdit->setKeyboardTracking(false);
        m_doubleEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
        m_doubleEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_doubleEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        m_doubleEdit->setRange(m_minimum, m_maximum);
        double factor = 100;
        if (m_maximum - m_minimum > 10000) {
            factor = 1000;
        }
        double steps = (m_maximum - m_minimum) / factor;
        m_doubleEdit->setSingleStep(steps);
        m_label->setStep(steps);
        // m_label->setStep(1);
        l->addWidget(m_doubleEdit);
        m_doubleEdit->setValue(m_default);
        m_doubleEdit->installEventFilter(this);
        connect(m_doubleEdit, SIGNAL(valueChanged(double)), this, SLOT(slotSetValue(double)));
        connect(m_doubleEdit, &QAbstractSpinBox::editingFinished, this, &DragValue::slotEditingFinished);
    }
    connect(m_label, &CustomLabel::valueChanged, this, &DragValue::setValueFromProgress);
    connect(m_label, &CustomLabel::resetValue, this, &DragValue::slotReset);
    setLayout(l);
    if (m_intEdit) {
        m_label->setMaximumHeight(m_intEdit->sizeHint().height());
    } else {
        m_label->setMaximumHeight(m_doubleEdit->sizeHint().height());
    }

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
        connect(m_label, &CustomLabel::setInTimeline, this, &DragValue::slotSetInTimeline);
        m_menu->addAction(timeline);
    }
    connect(this, &QWidget::customContextMenuRequested, this, &DragValue::slotShowContextMenu);
    connect(m_scale, &KSelectAction::indexTriggered, this, &DragValue::slotSetScaleMode);
    connect(m_directUpdate, &QAction::triggered, this, &DragValue::slotSetDirectUpdate);
}

DragValue::~DragValue()
{
    delete m_intEdit;
    delete m_doubleEdit;
    delete m_menu;
    delete m_label;
    // delete m_scale;
    // delete m_directUpdate;
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

        auto *we = static_cast<QWheelEvent *>(event);
        if (we->angleDelta().y() > 0) {
            m_label->slotValueInc();
        } else {
            m_label->slotValueDec();
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
    m_label->setMaximum(max - min);
}

void DragValue::setStep(qreal step)
{
    if (m_intEdit) {
        m_intEdit->setSingleStep(step);
    } else {
        m_doubleEdit->setSingleStep(step);
    }
    m_label->setStep(step);
}

void DragValue::slotReset()
{
    if (m_intEdit) {
        m_intEdit->blockSignals(true);
        m_intEdit->setValue(m_default);
        m_intEdit->blockSignals(false);
        Q_EMIT valueChanged((int)m_default, true);
    } else {
        m_doubleEdit->blockSignals(true);
        m_doubleEdit->setValue(m_default);
        m_doubleEdit->blockSignals(false);
        Q_EMIT valueChanged(m_default, true);
    }
    m_label->setProgressValue((m_default - m_minimum) / (m_maximum - m_minimum) * m_label->maximum());
}

void DragValue::slotSetValue(int value)
{
    setValue(value, true);
}

void DragValue::slotSetValue(double value)
{
    setValue(value, true);
}

void DragValue::setValueFromProgress(double value, bool final, bool createUndoEntry)
{
    value = m_minimum + value * (m_maximum - m_minimum) / m_label->maximum();
    if (m_decimals == 0) {
        setValue(qRound(value), final, createUndoEntry);
    } else {
        setValue(value, final, createUndoEntry);
    }
}

void DragValue::setValue(double value, bool final, bool createUndoEntry)
{
    value = qBound(m_minimum, value, m_maximum);
    if (m_intEdit && m_intEdit->singleStep() > 1) {
        int div = (value - m_minimum) / m_intEdit->singleStep();
        value = m_minimum + (div * m_intEdit->singleStep());
    }
    m_label->setProgressValue((value - m_minimum) / (m_maximum - m_minimum) * m_label->maximum());
    if (m_intEdit) {
        m_intEdit->blockSignals(true);
        m_intEdit->setValue((int)value);
        m_intEdit->blockSignals(false);
        Q_EMIT valueChanged((int)value, final, createUndoEntry);
    } else {
        m_doubleEdit->blockSignals(true);
        m_doubleEdit->setValue(value);
        m_doubleEdit->blockSignals(false);
        Q_EMIT valueChanged(value, final, createUndoEntry);
    }
}

void DragValue::slotEditingFinished()
{
    qDebug() << "::: EDITING FINISHED...";
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
            Q_EMIT valueChanged((double)newValue, true);
        }
    } else {
        double value = m_doubleEdit->value();
        m_doubleEdit->blockSignals(true);
        m_doubleEdit->clearFocus();
        m_doubleEdit->blockSignals(false);
        if (!KdenliveSettings::dragvalue_directupdate()) {
            Q_EMIT valueChanged(value, true);
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

CustomLabel::CustomLabel(const QString &label, bool showSlider, int range, QWidget *parent)
    : QProgressBar(parent)
    , m_dragMode(false)
    , m_showSlider(showSlider)
    , m_step(10.0)
    , m_value(0.)
// m_precision(pow(10, precision)),
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setFormat(QLatin1Char(' ') + label);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
                int diff = e->pos().x() - m_dragLastPosition.x();
#else
                int diff = e->position().x() - m_dragLastPosition.x();
#endif
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
        QProgressBar::mouseMoveEvent(e);
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
        setNewValue(m_clickValue, true, false);
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
    qDebug()<<":::: GOT WHEEL DELTA: "<<e->angleDelta().y();
    if (e->angleDelta().y() > 0) {
        if (e->modifiers() == Qt::ControlModifier) {
            slotValueInc(10);
        } else if (e->modifiers() == Qt::AltModifier) {
            slotValueInc(0.1);
        } else {
            slotValueInc();
        }
    } else {
        if (e->modifiers() == Qt::ControlModifier) {
            slotValueDec(10);
        } else if (e->modifiers() == Qt::AltModifier) {
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
    Q_EMIT valueChanged(value, update, createUndoEntry);
}

void CustomLabel::setStep(double step)
{
    m_step = step;
}
