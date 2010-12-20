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
#include "math.h"

#include <KLocale>

#include <QVBoxLayout>
#include <QLabel>
#include <QAction>
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
    setContextMenuPolicy(Qt::ActionsContextMenu);
    m_aMonitoringEnabled = new QAction(i18n("Monitor audio signal"), this);
    m_aMonitoringEnabled->setCheckable(true);
    connect(m_aMonitoringEnabled, SIGNAL(toggled(bool)), this, SLOT(slotSwitchAudioMonitoring(bool)));
    addAction(m_aMonitoringEnabled);
}

AudioSignal::~AudioSignal()
{
    delete m_aMonitoringEnabled;
}

bool AudioSignal::monitoringEnabled() const
{
    return m_aMonitoringEnabled->isChecked();
}

void AudioSignal::slotReceiveAudio(const QVector<int16_t>& data, int, int num_channels, int samples){

    int num_samples = samples > 200 ? 200 : samples;

    QByteArray channels;
    for (int i = 0; i < num_channels; i++) {
        long val = 0;
        for (int s = 0; s < num_samples; s ++) {
            val += abs(data[i+s*num_channels] / 128);
        }
        channels.append(val / num_samples);
    }
    showAudio(channels);
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

double AudioSignal::valueToPixel(double in,bool db)
{
    if (db)
    {
        // ratio db(in)/db(0.01) (means: min db value = -40.0 )
        return 1.0- log10( in)/log10(0.01);
    }
    else
    {
        return in;
    }
}

void AudioSignal::paintEvent(QPaintEvent* /*e*/)
{
    if (!m_aMonitoringEnabled->isChecked()) {
        return;
    }
    QPainter p(this);
    //p.begin();
    //p.fillRect(0,0,(unsigned char)channels[0]*width()/255,height()/2,QBrush(Qt::SolidPattern));
    //p.fillRect(0,height()/2,(unsigned char)channels[1]*width()/255,height()/2,QBrush(Qt::SolidPattern));
    int numchan = channels.size();
    bool db=true; // show values in db(i)
    bool horiz=width() > height();
    for (int i = 0; i < numchan; i++) {
        //int maxx= (unsigned char)channels[i] * (horiz ? width() : height() ) / 127;
        int maxx= (horiz ? width() : height() ) * valueToPixel((double)(unsigned char)channels[i]/127.0,db);
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
        int xp=valueToPixel((double)peeks.at(i)/127.0,db)*(horiz?width():height())-2;
        p.fillRect(horiz?xp:_y1,horiz?_y1:height()-xdelta-xp,horiz?3:_y2,horiz?_y2:3,QBrush(Qt::black,Qt::SolidPattern));
    }
    p.end();
}

void AudioSignal::slotSwitchAudioMonitoring(bool)
{
    emit updateAudioMonitoring();
}

#include "audiosignal.moc"
