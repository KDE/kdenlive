
#include <QMouseEvent>
#include <QStylePainter>

#include <KDebug>
#include <KLocale>

#include "gentime.h"
#include "monitor.h"

Monitor::Monitor(QString name, QWidget *parent)
    : QWidget(parent)
{
  ui.setupUi(this);
  if (name == "project") {
  render = new Render(name, this);
  render->createVideoXWindow(ui.video_frame->winId(), -1);
  connect(ui.button_play, SIGNAL(clicked()), this, SLOT(slotOpen()));
  connect(ui.button_rew, SIGNAL(clicked()), this, SLOT(slotRewind()));
  connect(ui.button_play_2, SIGNAL(clicked()), this, SLOT(slotPlay()));
  }
}

void Monitor::slotOpen()
{

render->mltInsertClip(2, GenTime(1, 25), QString("<westley><producer mlt_service=\"colour\" colour=\"red\" in=\"1\" out=\"30\" /></westley>"));
render->mltInsertClip(2, GenTime(0, 25), QString("<westley><producer mlt_service=\"avformat\" resource=\"/home/one/.vids/clip3e.mpg\" in=\"1\" out=\"300\" /></westley>"));
}

void Monitor::slotRewind()
{
  render->seek(GenTime(0));

}

void Monitor::slotPlay()
{
  render->switchPlay();
}

void Monitor::slotSetXml(const QDomElement &e)
{
    QDomDocument doc;
    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);
    westley.appendChild(e);
    render->setSceneList(doc, 0);
}


void Monitor::slotOpenFile(const QString &file)
{
    QDomDocument doc;
    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);
    QDomElement prod = doc.createElement("producer");
    westley.appendChild(prod);
    prod.setAttribute("mlt_service", "avformat");
    prod.setAttribute("resource", file);
    render->setSceneList(doc, 0);
}



#include "monitor.moc"
