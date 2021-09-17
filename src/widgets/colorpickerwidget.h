/*
    SPDX-FileCopyrightText: 2010 Till Theato <root@ttill.de>

SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef COLORPICKERWIDGET_H
#define COLORPICKERWIDGET_H

#include <QFrame>
#include <QWidget>

class QFrame;
#ifdef Q_WS_X11
#include <X11/Xlib.h>
#endif

class MyFrame : public QFrame
{
    Q_OBJECT
public:
    explicit MyFrame(QWidget *parent = nullptr);

protected:
    void hideEvent(QHideEvent *event) override;

signals:
    void getColor();
};

/** @class ColorPickerWidget
    @brief A widget to pick a color anywhere on the screen.
    The code is partially based on the color picker in KColorDialog.
    @author Till Theato
 */
class ColorPickerWidget : public QWidget
{
    Q_OBJECT

public:
    /** @brief Sets up the widget. */
    explicit ColorPickerWidget(QWidget *parent = nullptr);
    /** @brief Makes sure the event filter is removed. */
    ~ColorPickerWidget() override;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    /** @brief Closes the event filter and makes mouse and keyboard work again on other widgets/windows. */
    void closeEventFilter();

    /** @brief Color of the screen at point @param p.
    * @param p Position of color requested
    * @param destroyImage (optional) Whether or not to keep the XImage in m_image
                          (needed for fast processing of rects) */
    QColor grabColor(const QPoint &p, bool destroyImage = true);

    bool m_filterActive{false};
    QRect m_grabRect;
    QFrame *m_grabRectFrame;
    QColor m_mouseColor;
#ifdef Q_WS_X11
    XImage *m_image;
#else
    QImage m_image;
#endif

private slots:
    /** @brief Sets up an event filter for picking a color. */
    void slotSetupEventFilter();

    /** @brief Calculates the average color for the pixels in the rect m_grabRect and emits colorPicked. */
    void slotGetAverageColor();

signals:
    /** @brief Signal fired when a new color has been picked
     */
    void colorPicked(const QColor &);
    /** @brief When user wants to pick a color, it's better to disable filter so we get proper color values. */
    void disableCurrentFilter(bool);
};

#endif
