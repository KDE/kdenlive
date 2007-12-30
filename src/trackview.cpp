
#include <QMouseEvent>
#include <QStylePainter>

#include <KDebug>


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

  connect(view->horizontalSlider, SIGNAL(valueChanged ( int )), this, SLOT(slotChangeZoom( int )));
}

void TrackView::slotChangeZoom(int factor)
{
  m_ruler->setPixelPerMark(factor);
}

KdenliveDoc *TrackView::document()
{
  return m_doc;
}

#include "trackview.moc"
