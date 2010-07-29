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
#include <QPushButton>

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

    QPushButton *button = new QPushButton(this);
    button->setIcon(KIcon("color-picker"));
    button->setToolTip(i18n("Pick a color on the screen"));
    connect(button, SIGNAL(clicked()), this, SLOT(slotSetupEventFilter()));
}

ColorPickerWidget::~ColorPickerWidget()
{
#ifdef Q_WS_X11
    if (m_filterActive && kapp)
        kapp->removeX11EventFilter(m_filter);
#endif
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
        // does not work this way
        //if (event->button() == Qt::LeftButton)
        emit colorPicked(KColorDialog::grabColor(event->globalPos()));
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
