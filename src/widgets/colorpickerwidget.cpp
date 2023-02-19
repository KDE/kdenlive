/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "colorpickerwidget.h"
#include "core.h"
#include "mainwindow.h"

#include <KLocalizedString>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDebug>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QScreen>
#include <QStyleOptionFocusRect>
#include <QStylePainter>
#include <QTimer>
#include <QToolButton>

#ifdef Q_WS_X11
#include <X11/Xutil.h>
#include <fixx11h.h>
#endif

QDBusArgument &operator<<(QDBusArgument &arg, const QColor &color)
{
    arg.beginStructure();
    arg << color.redF() << color.greenF() << color.blueF();
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, QColor &color)
{
    double red, green, blue;
    arg.beginStructure();
    arg >> red >> green >> blue;
    color.setRedF(red);
    color.setGreenF(green);
    color.setBlueF(blue);
    arg.endStructure();

    return arg;
}

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
    , m_mouseColor(Qt::transparent)

{
#ifdef Q_WS_X11
    m_image = nullptr;
#endif

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Check wether grabWindow() works. On some systems like with Wayland it does.
    // We fallback to the Freedesktop portal with DBus which has less features than
    // our custom implementation (eg. preview and avarage color are missing)
    if (pCore) {
        QPoint p(pCore->window()->geometry().center());
        foreach (QScreen *screen, QGuiApplication::screens()) {
            QRect screenRect = screen->geometry();
            if (screenRect.contains(p)) {
                QPixmap pm = screen->grabWindow(pCore->window()->winId(), p.x(), p.y(), 1, 1);
                qDebug() << "got pixmap that is not null";
                m_useDBus = pm.isNull();
                break;
            }
        }
    }

    auto *button = new QToolButton(this);
    button->setIcon(QIcon::fromTheme(QStringLiteral("color-picker")));
    button->setToolTip(i18n("Pick a color on the screen."));
    button->setAutoRaise(true);
    if (!m_useDBus) {
        button->setWhatsThis(xi18nc("@info:whatsthis", "Pick a color on the screen. By pressing the mouse button and then moving your mouse you can select a "
                                                       "section of the screen from which to get an average color."));
        connect(button, &QAbstractButton::clicked, this, &ColorPickerWidget::slotSetupEventFilter);
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
    } else {
        qDBusRegisterMetaType<QColor>();
        connect(button, &QAbstractButton::clicked, this, &ColorPickerWidget::grabColorDBus);
    }

    layout->addWidget(button);
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

void ColorPickerWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QStylePainter painter(this);

    QStyleOptionComplex option;
    option.initFrom(this);
    if (m_filterActive) {
        QRect r = option.rect;
        int margin = r.height() / 8;
        r.adjust(margin, 4 * margin, -margin, -margin);
        painter.fillRect(r, m_mouseColor);
    }
    painter.drawComplexControl(QStyle::CC_ToolButton, option);
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
    foreach (QScreen *screen, QGuiApplication::screens()) {
        QRect screenRect = screen->geometry();
        if (screenRect.contains(m_grabRect.topLeft())) {
            m_image =
                screen->grabWindow(0, m_grabRect.x() - screenRect.x(), m_grabRect.y() - screenRect.y(), m_grabRect.width(), m_grabRect.height()).toImage();
            break;
        }
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
#else
    m_image = QImage();
#endif

    Q_EMIT colorPicked(QColor(sumR / numPixel, sumG / numPixel, sumB / numPixel));
    Q_EMIT disableCurrentFilter(false);
}

void ColorPickerWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        closeEventFilter();
        Q_EMIT disableCurrentFilter(false);
        event->accept();
        return;
    }

    if (m_filterActive) {
        m_clickPoint = event->globalPos();
        m_grabRect = QRect(m_clickPoint, QSize(1, 1));
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
        m_clickPoint = QPoint();

        if (m_grabRect.width() * m_grabRect.height() == 0) {
            Q_EMIT colorPicked(m_mouseColor);
            Q_EMIT disableCurrentFilter(false);
        } else {
            // delay because m_grabRectFrame does not hide immediately
            connect(m_grabRectFrame, SIGNAL(getColor()), this, SLOT(slotGetAverageColor()));
            m_grabRectFrame->hide();
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void ColorPickerWidget::mouseMoveEvent(QMouseEvent *event)
{
    // Draw live rectangle of current color under mouse
    m_mouseColor = grabColor(QCursor::pos(), true);
    update();
    if (m_filterActive && !m_clickPoint.isNull()) {
        m_grabRect.setWidth(event->globalX() - m_grabRect.x());
        m_grabRect.setHeight(event->globalY() - m_grabRect.y());
        m_grabRectFrame->setGeometry(m_grabRect.normalized());
    }
}

void ColorPickerWidget::slotSetupEventFilter()
{
    Q_EMIT disableCurrentFilter(true);
    m_filterActive = true;
    setFocus();
    installEventFilter(this);
    grabMouse(QCursor(QIcon::fromTheme(QStringLiteral("color-picker")).pixmap(32, 32), 4, 28));
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
        Q_EMIT disableCurrentFilter(false);
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
        foreach (QScreen *screen, QGuiApplication::screens()) {
            QRect screenRect = screen->geometry();
            if (screenRect.contains(p)) {
                QPixmap pm = screen->grabWindow(0, p.x() - screenRect.x(), p.y() - screenRect.y(), 1, 1);
                QImage i = pm.toImage();
                return i.pixel(0, 0);
            }
        }
        return qRgb(0, 0, 0);
    }
    return m_image.pixel(p.x(), p.y());

#endif
}

void ColorPickerWidget::grabColorDBus()
{
    QDBusMessage message = QDBusMessage::createMethodCall(QLatin1String("org.freedesktop.portal.Desktop"), QLatin1String("/org/freedesktop/portal/desktop"),
                                                          QLatin1String("org.freedesktop.portal.Screenshot"), QLatin1String("PickColor"));
    message << QLatin1String("x11:") << QVariantMap{};
    QDBusPendingCall pendingCall = QDBusConnection::sessionBus().asyncCall(message);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pendingCall);
    Q_EMIT disableCurrentFilter(true);
    connect(watcher, &QDBusPendingCallWatcher::finished, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        Q_EMIT disableCurrentFilter(false);
        if (reply.isError()) {
            qWarning() << "Couldn't get reply";
            qWarning() << "Error: " << reply.error().message();
        } else {
            QDBusConnection::sessionBus().connect(QString(), reply.value().path(), QLatin1String("org.freedesktop.portal.Request"), QLatin1String("Response"),
                                                  this, SLOT(gotColorResponse(uint, QVariantMap)));
        }
    });
}

void ColorPickerWidget::gotColorResponse(uint response, const QVariantMap &results)
{
    if (!response) {
        if (results.contains(QLatin1String("color"))) {
            const QColor color = qdbus_cast<QColor>(results.value(QLatin1String("color")));
            qDebug() << "picked" << color;
            m_mouseColor = color;
            Q_EMIT colorPicked(m_mouseColor);
        }
    } else {
        qWarning() << "Failed to take screenshot" << response << results;
    }
}
