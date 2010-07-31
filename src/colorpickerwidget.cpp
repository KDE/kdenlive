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
#include <KIcon>
#include <KLocalizedString>

#ifdef Q_WS_X11
#include <X11/Xutil.h>
#include <fixx11h.h>

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
    m_image = 0;
#endif

    QHBoxLayout *layout = new QHBoxLayout(this);

    QPushButton *button = new QPushButton(this);
    button->setIcon(KIcon("color-picker"));
    button->setToolTip(i18n("Pick a color on the screen"));
    connect(button, SIGNAL(clicked()), this, SLOT(slotSetupEventFilter()));

    m_size = new QSpinBox(this);
    m_size->setMinimum(1);
    // Use qMax here, to make it possible to get the average for the whole screen
    m_size->setMaximum(qMax(qApp->desktop()->geometry().width(), qApp->desktop()->geometry().height()));
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

    /*
     Only getting the XImage once for the whole rect,
     results in a vast speed improvement.
    */
#ifdef Q_WS_X11
    Window root = RootWindow(QX11Info::display(), QX11Info::appScreen());
    m_image = XGetImage(QX11Info::display(), root, x0, y0, x1 - x0, y1 - y0, -1, ZPixmap);
#endif

    for (int i = x0; i < x1; ++i) {
        for (int j = y0; j < y1; ++j) {
            QColor color;
#ifdef Q_WS_X11
            color = grabColor(QPoint(i - x0, j - y0), false);
#else
            color = grabColor(QPoint(i, j));
#endif
            sumR += color.red();
            sumG += color.green();
            sumB += color.blue();
        }
    }

#ifdef Q_WS_X11
    XDestroyImage(m_image);
    m_image = 0;
#endif

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
            emit colorPicked(grabColor(event->globalPos()));
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

QColor ColorPickerWidget::grabColor(const QPoint &p, bool destroyImage)
{
#ifdef Q_WS_X11
    /*
     we use the X11 API directly in this case as we are not getting back a valid
     return from QPixmap::grabWindow in the case where the application is using
     an argb visual
    */
    if( !qApp->desktop()->geometry().contains( p ))
        return QColor();
    unsigned long xpixel;
    if (m_image == 0) {
        Window root = RootWindow(QX11Info::display(), QX11Info::appScreen());
        m_image = XGetImage(QX11Info::display(), root, p.x(), p.y(), 1, 1, -1, ZPixmap);
        xpixel = XGetPixel(m_image, 0, 0);
    } else {
        xpixel = XGetPixel(m_image, p.x(), p.y());
    }
    if (destroyImage) {
        XDestroyImage(m_image);
        m_image = 0;
    }
    XColor xcol;
    xcol.pixel = xpixel;
    xcol.flags = DoRed | DoGreen | DoBlue;
    XQueryColor(QX11Info::display(),
                DefaultColormap(QX11Info::display(), QX11Info::appScreen()),
                &xcol);
    return QColor::fromRgbF(xcol.red / 65535.0, xcol.green / 65535.0, xcol.blue / 65535.0);
#else
    QWidget *desktop = QApplication::desktop();
    QPixmap pm = QPixmap::grabWindow(desktop->winId(), p.x(), p.y(), 1, 1);
    QImage i = pm.toImage();
    return i.pixel(0, 0);
#endif
}

#include "colorpickerwidget.moc"
