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

#include <QApplication>
#include <QScreen>
#include <QFrame>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QTimer>
#include <QToolButton>
#include <klocalizedstring.h>

#ifdef Q_WS_X11
#include <X11/Xutil.h>
#include <fixx11h.h>
#endif

MyFrame::MyFrame(QWidget *parent)
    : QFrame(parent)
{
    setFrameStyle(QFrame::Box | QFrame::Plain);
    setWindowOpacity(0.5);
    setWindowFlags(Qt::FramelessWindowHint);
}

// virtual
void MyFrame::hideEvent(QHideEvent *event)
{
    QFrame::hideEvent(event);
    // We need a timer here since hiding the frame will trigger a monitor refresh timer that will
    // repaint the monitor after 70 ms.
    QTimer::singleShot(250, this, &MyFrame::getColor);
}

ColorPickerWidget::ColorPickerWidget(QWidget *parent)
    : QWidget(parent)

{
#ifdef Q_WS_X11
    m_image = nullptr;
#endif

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *button = new QToolButton(this);
    button->setIcon(QIcon::fromTheme(QStringLiteral("color-picker")));
    button->setToolTip(QStringLiteral("<p>") +
                       i18n("Pick a color on the screen. By pressing the mouse button and then moving your mouse you can select a "
                            "section of the screen from which to get an average color.") +
                       QStringLiteral("</p>"));
    button->setAutoRaise(true);
    connect(button, &QAbstractButton::clicked, this, &ColorPickerWidget::slotSetupEventFilter);

    layout->addWidget(button);
    setFocusPolicy(Qt::StrongFocus);

    m_grabRectFrame = new MyFrame();
    m_grabRectFrame->hide();
}

ColorPickerWidget::~ColorPickerWidget()
{
    delete m_grabRectFrame;
    if (m_filterActive) {
        removeEventFilter(this);
    }
}

void ColorPickerWidget::slotGetAverageColor()
{
    disconnect(m_grabRectFrame, SIGNAL(getColor()), this, SLOT(slotGetAverageColor()));
    m_grabRect = m_grabRect.normalized();

    int numPixel = m_grabRect.width() * m_grabRect.height();

    int sumR = 0;
    int sumG = 0;
    int sumB = 0;

/*
 Only getting the image once for the whole rect
 results in a vast speed improvement.
*/
#ifdef Q_WS_X11
    Window root = RootWindow(QX11Info::display(), QX11Info::appScreen());
    m_image = XGetImage(QX11Info::display(), root, m_grabRect.x(), m_grabRect.y(), m_grabRect.width(), m_grabRect.height(), -1, ZPixmap);
#else
    QScreen *currentScreen = QApplication::primaryScreen();
    if (currentScreen) {
        m_image = currentScreen->grabWindow(0, m_grabRect.x(), m_grabRect.y(), m_grabRect.width(), m_grabRect.height()).toImage();
    }
#endif

    for (int x = 0; x < m_grabRect.width(); ++x) {
        for (int y = 0; y < m_grabRect.height(); ++y) {
            QColor color = grabColor(QPoint(x, y), false);
            sumR += color.red();
            sumG += color.green();
            sumB += color.blue();
        }
    }

#ifdef Q_WS_X11
    XDestroyImage(m_image);
    m_image = nullptr;
#endif

    emit colorPicked(QColor(sumR / numPixel, sumG / numPixel, sumB / numPixel));
    emit disableCurrentFilter(false);
}

void ColorPickerWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        closeEventFilter();
        emit disableCurrentFilter(false);
        event->accept();
        return;
    }

    if (m_filterActive) {
        m_grabRect = QRect(event->globalPos(), QSize(1, 1));
        m_grabRectFrame->setGeometry(m_grabRect);
        m_grabRectFrame->show();
    }
}

void ColorPickerWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_filterActive) {
        closeEventFilter();

        m_grabRect.setWidth(event->globalX() - m_grabRect.x());
        m_grabRect.setHeight(event->globalY() - m_grabRect.y());
        m_grabRect = m_grabRect.normalized();

        if (m_grabRect.width() * m_grabRect.height() == 0) {
            m_grabRectFrame->hide();
            emit colorPicked(grabColor(event->globalPos()));
            emit disableCurrentFilter(false);
        } else {
            // delay because m_grabRectFrame does not hide immediately
            connect(m_grabRectFrame, SIGNAL(getColor()), this, SLOT(slotGetAverageColor()));
            m_grabRectFrame->hide();
        }
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ColorPickerWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_filterActive) {
        m_grabRect.setWidth(event->globalX() - m_grabRect.x());
        m_grabRect.setHeight(event->globalY() - m_grabRect.y());
        m_grabRectFrame->setGeometry(m_grabRect.normalized());
    }
}

void ColorPickerWidget::slotSetupEventFilter()
{
    emit disableCurrentFilter(true);
    m_filterActive = true;
    setFocus();
    installEventFilter(this);
    grabMouse(QCursor(QIcon::fromTheme(QStringLiteral("color-picker")).pixmap(32, 32), 16, 2));
    grabKeyboard();
}

void ColorPickerWidget::closeEventFilter()
{
    m_filterActive = false;
    releaseMouse();
    releaseKeyboard();
    removeEventFilter(this);
}

bool ColorPickerWidget::eventFilter(QObject *object, QEvent *event)
{
    // Close color picker on any key press
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
        closeEventFilter();
        emit disableCurrentFilter(false);
        event->setAccepted(true);
        return true;
    }
    return QObject::eventFilter(object, event);
}

QColor ColorPickerWidget::grabColor(const QPoint &p, bool destroyImage)
{
#ifdef Q_WS_X11
    /*
     we use the X11 API directly in this case as we are not getting back a valid
     return from QPixmap::grabWindow in the case where the application is using
     an argb visual
    */
    if (!QApplication::primaryScreen()->geometry().contains(p)) {
        return QColor();
    }
    unsigned long xpixel;
    if (m_image == nullptr) {
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
    XQueryColor(QX11Info::display(), DefaultColormap(QX11Info::display(), QX11Info::appScreen()), &xcol);
    return QColor::fromRgbF(xcol.red / 65535.0, xcol.green / 65535.0, xcol.blue / 65535.0);
#else
    Q_UNUSED(destroyImage)

    if (m_image.isNull()) {
        QScreen *currentScreen = QApplication::primaryScreen();
        if (currentScreen) {
            QPixmap pm = currentScreen->grabWindow(0, p.x(), p.y(), 1, 1);
            QImage i = pm.toImage();
            return i.pixel(0, 0);
        }
        return qRgb(0, 0, 0);
    }
    return m_image.pixel(p.x(), p.y());

#endif
}
