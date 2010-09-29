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

#include "audiosignal.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QDebug>
#include <QList>


AudioSignal::AudioSignal(QWidget *parent): QWidget(parent)
{
    //QVBoxLayout *vbox=new QVBoxLayout(this);
    //label=new QLabel();
    //vbox->addWidget(label);
    setMinimumHeight(10);
    setMinimumWidth(10);
    col << Qt::green <<  Qt::green << Qt::green << Qt::green << Qt::green << Qt::green << Qt::green << Qt::green << Qt::green << Qt::green ;
    col << Qt::yellow <<  Qt::yellow << Qt::yellow << Qt::yellow << Qt::yellow  ;
    col << Qt::darkYellow << Qt::darkYellow << Qt::darkYellow;
    col << Qt::red << Qt::red;
}


void AudioSignal::showAudio(const QByteArray arr)
{
    channels = arr;
    if (peeks.count()!=channels.count()){
        peeks=QByteArray(channels.count(),0);
        peekage=QByteArray(channels.count(),0);
    }
    for (int chan=0;chan<peeks.count();chan++)
    {
        peekage[chan]=peekage[chan]+1;
        if (  peeks.at(chan)<arr.at(chan) ||  peekage.at(chan)>50 )
        {
            peekage[chan]=0;
            peeks[chan]=arr[chan];
        }
    }
    update();
}
void AudioSignal::paintEvent(QPaintEvent* /*e*/)
{
    QPainter p(this);
    //p.begin();
    //p.fillRect(0,0,(unsigned char)channels[0]*width()/255,height()/2,QBrush(Qt::SolidPattern));
    //p.fillRect(0,height()/2,(unsigned char)channels[1]*width()/255,height()/2,QBrush(Qt::SolidPattern));
    int numchan = channels.size();
    bool horiz=width() > height();
    for (int i = 0; i < numchan; i++) {
        int maxx= (unsigned char)channels[i] * (horiz ? width() : height() ) / 127;
        int xdelta=(horiz ? width():height() )  /20 ;
        int _y2= (horiz ?  height() :width () ) / numchan - 1  ;
        int _y1=(horiz ? height():width() ) *i/numchan;
        int _x2=maxx >  xdelta ? xdelta - (horiz?1:3) : maxx - (horiz ?1 :3 );

        for (int x = 0; x < 20; x++) {
            int _x1= x *xdelta;
            if (maxx > 0) {
                //p.fillRect(x * xdelta, y1, maxx > xdelta ? xdelta - 1 : maxx - 1, _h, QBrush(col.at(x), Qt::SolidPattern));
                p.fillRect(horiz?_x1:_y1, horiz?_y1:height()-_x1, horiz?_x2:_y2,horiz? _y2:-_x2, QBrush(col.at(x), Qt::SolidPattern));
                maxx -= xdelta;
            }
        }
        int xp=peeks.at(i)*(horiz?width():height())/127-2;
        p.fillRect(horiz?xp:_y1,horiz?_y1:height()-xdelta-xp,horiz?3:_y2,horiz?_y2:3,QBrush(Qt::black,Qt::SolidPattern));
    }
    p.end();
}
#include "audiosignal.moc"
