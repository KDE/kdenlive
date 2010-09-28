/***************************************************************************
 *   Copyright (C) 2010 by Marco Gittler (g.marco@freenet.de)              *
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

#ifndef AUDIOSIGNAL_H
#define AUDIOSIGNAL_H

#include <QByteArray>
#include <QList>
#include <QColor>
class QLabel;

#include  <QWidget>
class AudioSignal : public QWidget
{
    Q_OBJECT
public:
    AudioSignal(QWidget *parent = 0);
private:
    QLabel* label;
    QByteArray channels;
    QList<QColor> col;
protected:
    void paintEvent(QPaintEvent*);
public slots:
    void showAudio(const QByteArray);


};

#endif
