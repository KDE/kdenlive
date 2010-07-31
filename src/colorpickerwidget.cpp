/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
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


#include "colorpickerwidget.h"

#include <QMouseEvent>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QDesktopWidget>

#include <KApplication>
#include <KColorDialog>
#include <KIcon>
#include <KLocalizedString>
#include <KDebug>

#ifdef Q_WS_X11
#include <X11/Xlib.h>

class KCDPickerFilter: public QWidget
{
public:
    KCDPickerFilter(QWidget* parent): QWidget(parent) {}

    virtual bool x11Event(XEvent* event) {
        if (event->type == ButtonRelease) {
            QMouseEvent e(QEvent::MouseButtonRelease, QPoint(),
            QPoint(event->xmotion.x_root, event->xmotion.y_root) , Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(parentWidget(), &e);
            return true;
        }
        return false;
    }
};
#endif 


ColorPickerWidget::ColorPickerWidget(QWidget *parent) :
        QWidget(parent),
        m_filterActive(false)
{
#ifdef Q_WS_X11
    m_filter = 0;
#endif

    QHBoxLayout *layout = new QHBoxLayout(this);

    QPushButton *button = new QPushButton(this);
    button->setIcon(KIcon("color-picker"));
    button->setToolTip(i18n("Pick a color on the screen"));
    connect(button, SIGNAL(clicked()), this, SLOT(slotSetupEventFilter()));

    m_size = new QSpinBox(this);
    m_size->setMinimum(1);
    //m_size->setMaximum(qMin(qApp->desktop()->geometry().width(), qApp->desktop()->geometry().height()));
    m_size->setMaximum(100);
    m_size->setValue(1);

    layout->addWidget(button);
    layout->addStretch(1);
    layout->addWidget(new QLabel(i18n("Width of square to pick color from:")));
    layout->addWidget(m_size);
}

ColorPickerWidget::~ColorPickerWidget()
{
#ifdef Q_WS_X11
    if (m_filterActive && kapp)
        kapp->removeX11EventFilter(m_filter);
#endif
}

QColor ColorPickerWidget::averagePickedColor(const QPoint pos)
{
    int size = m_size->value();
    int x0 = qMax(0, pos.x() - size / 2); 
    int y0 = qMax(0, pos.y() - size / 2);
    int x1 = qMin(qApp->desktop()->geometry().width(), pos.x() + size / 2);
    int y1 = qMin(qApp->desktop()->geometry().height(), pos.y() + size / 2);

    int sumR = 0;
    int sumG = 0;
    int sumB = 0;

    for (int i = x0; i < x1; ++i) {
        for (int j = y0; j < y1; ++j) {
            QColor color = KColorDialog::grabColor(QPoint(i, j));
            sumR += color.red();
            sumG += color.green();
            sumB += color.blue();
        }
    }

    int numPixel = (x1 - x0) * (y1 - y0);
    return QColor(sumR / numPixel, sumG / numPixel, sumB / numPixel);
}

void ColorPickerWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        closeEventFilter();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ColorPickerWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_filterActive) {
        closeEventFilter();

        if (m_size->value() == 1)
            emit colorPicked(KColorDialog::grabColor(event->globalPos()));
        else
            emit colorPicked(averagePickedColor(event->globalPos()));

        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ColorPickerWidget::keyPressEvent(QKeyEvent *event)
{
    if (m_filterActive) {
        // "special keys" (non letter, numeral) do not work, so close for every key
        //if (event->key() == Qt::Key_Escape)
        closeEventFilter();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ColorPickerWidget::slotSetupEventFilter()
{
    m_filterActive = true;
#ifdef Q_WS_X11
    m_filter = new KCDPickerFilter(this);
    kapp->installX11EventFilter(m_filter);
#endif
    if (m_size->value() == 1)
        grabMouse(QCursor(KIcon("color-picker").pixmap(22, 22), 0, 21));
    else
        grabMouse(Qt::CrossCursor);
    grabKeyboard();
}

void ColorPickerWidget::closeEventFilter()
{
    m_filterActive = false;
#ifdef Q_WS_X11
    kapp->removeX11EventFilter(m_filter);
    delete m_filter;
    m_filter = 0;
#endif
    releaseMouse();
    releaseKeyboard();
}

#include "colorpickerwidget.moc"
