
#include <QMouseEvent>
#include <QStylePainter>

#include <KDebug>

#include "documenttrack.h"
#include "headertrack.h"
#include "trackview.h"

TrackView::TrackView(KdenliveDoc *doc, QWidget *parent)
    : QWidget(parent), m_doc(doc)
{
  view = new Ui::TimeLine_UI();
  view->setupUi(this);
  m_ruler = new CustomRuler(doc->timecode());
  QVBoxLayout *layout = new QVBoxLayout;
  layout->addWidget(m_ruler);
  view->ruler_frame->setLayout(layout);

  m_tracksLayout = new QVBoxLayout;
  m_tracksLayout->setContentsMargins (0, 0, 0, 0);
  m_scrollArea = new QScrollArea;

  m_tracksLayout->addWidget(m_scrollArea);
  m_scrollArea->setHorizontalScrollBarPolicy ( Qt::ScrollBarAlwaysOn);
  m_scrollArea->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
  m_tracksAreaLayout = new QVBoxLayout;
  m_tracksAreaLayout->setContentsMargins (0, 0, 0, 0);
  m_tracksAreaLayout->insertStretch (1000);
  m_scrollArea->setLayout(m_tracksAreaLayout);
  view->tracks_frame->setLayout(m_tracksLayout);

  m_headersLayout = new QVBoxLayout;
  m_headersLayout->setContentsMargins (0, 0, 0, 0);
  view->headers_frame->setLayout(m_headersLayout);

  parseDocument(doc->toXml());

  connect(view->horizontalSlider, SIGNAL(valueChanged ( int )), this, SLOT(slotChangeZoom( int )));
}

void TrackView::parseDocument(QDomDocument doc)
{
  QDomNodeList tracks = doc.elementsByTagName("kdenlivetrack");
  for (int i = 0; i < tracks.count(); i++)
  {
    slotAddTrack(i);
  }
}

void TrackView::slotChangeZoom(int factor)
{
  m_ruler->setPixelPerMark(factor);
}

KdenliveDoc *TrackView::document()
{
  return m_doc;
}

void TrackView::slotAddTrack(int ix)
{
  DocumentTrack *track = new DocumentTrack();
  HeaderTrack *header = new HeaderTrack();
  m_tracksAreaLayout->addWidget(track, ix, Qt::AlignTop);
  m_headersLayout->addWidget(header, ix, Qt::AlignTop);
  //track->show();
}

#include "trackview.moc"
