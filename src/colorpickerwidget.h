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


#ifndef COLORPICKERWIDGET_H
#define COLORPICKERWIDGET_H

#include <QtCore>
#include <QWidget>

#ifdef Q_WS_X11
class KCDPickerFilter;
#endif

/**
 * @class ColorPickerWidget
 * @brief A widget to pick a color anywhere on the screen.
 * @author Till Theato
 *
 * The code is based on the color picker in KColorDialog. 
 */

class ColorPickerWidget : public QWidget
{
    Q_OBJECT

public:
    /** @brief Sets up the widget. */
    ColorPickerWidget(QWidget *parent = 0);
    /** @brief Makes sure the event filter is removed. */
    virtual ~ColorPickerWidget();

protected:
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);

private:
    /** @brief Closes the event filter and makes mouse and keyboard work again on other widgets/windows. */
    void closeEventFilter();
    bool m_filterActive;
#ifdef Q_WS_X11
    KCDPickerFilter *m_filter;
#endif 

private slots:
    /** @brief Sets up an event filter for picking a color. */
    void slotSetupEventFilter();

signals:
    void colorPicked(QColor);
};

#endif
