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
    col << Qt::green <<  Qt::green << Qt::green << Qt::green << Qt::green << Qt::green << Qt::green << Qt::green << Qt::green << Qt::green ;
    col << Qt::yellow <<  Qt::yellow << Qt::yellow << Qt::yellow << Qt::yellow  ;
    col << Qt::darkYellow << Qt::darkYellow << Qt::darkYellow;
    col << Qt::red << Qt::red;
}


void AudioSignal::showAudio(const QByteArray arr)
{
    channels = arr;
    update();
}
void AudioSignal::paintEvent(QPaintEvent* /*e*/)
{
    QPainter p(this);
    //p.begin();
    //p.fillRect(0,0,(unsigned char)channels[0]*width()/255,height()/2,QBrush(Qt::SolidPattern));
    //p.fillRect(0,height()/2,(unsigned char)channels[1]*width()/255,height()/2,QBrush(Qt::SolidPattern));
    int numchan = channels.size();
    for (int i = 0; i < numchan; i++) {
        int maxx = (unsigned char)channels[i] * width() / 255;
        int xdelta = width() / 20;
        int y1 = height() * i / numchan;
        int _h = height() / numchan - 1;
        for (int x = 0; x < 20; x++) {
            if (maxx > 0) {
                p.fillRect(x * xdelta, y1, maxx > xdelta ? xdelta - 1 : maxx - 1, _h, QBrush(col.at(x), Qt::SolidPattern));
                maxx -= xdelta;
            }
        }
    }
    p.end();
}
#include "audiosignal.moc"
