
#include <QMouseEvent>
#include <QStylePainter>
#include <QScrollBar>

#include <KDebug>

#include "definitions.h"
#include "documentvideotrack.h"
#include "documentaudiotrack.h"
#include "headertrack.h"
#include "trackview.h"
#include "trackpanelclipmovefunction.h"

TrackView::TrackView(KdenliveDoc *doc, QWidget *parent)
    : QWidget(parent), m_doc(doc), m_scale(1.0), m_panelUnderMouse(NULL), m_function(NULL)
{
  setMouseTracking(true);
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
  m_scrollArea->setWidgetResizable( true );

  m_scrollBox = new QFrame(m_scrollArea);
  //m_scrollBox ->setFlat(false);
  m_scrollBox ->setFocusPolicy(Qt::WheelFocus);
  m_scrollBox->setSizePolicy( QSizePolicy::Preferred,QSizePolicy::Fixed );

  m_tracksAreaLayout = new QVBoxLayout(m_scrollBox);
  m_tracksAreaLayout->setContentsMargins (0, 0, 0, 0);
  m_tracksAreaLayout->setSpacing(0);
//MyScroll->setGeometry(...);
  m_scrollArea->setWidget(m_scrollBox);

  view->tracks_frame->setLayout(m_tracksLayout);

  m_headersLayout = new QVBoxLayout;
  m_headersLayout->setContentsMargins (0, 0, 0, 0);
  view->headers_frame->setLayout(m_headersLayout);

  parseDocument(doc->toXml());

  TrackPanelClipMoveFunction *m_moveFunction = new TrackPanelClipMoveFunction(this);
  registerFunction("move", m_moveFunction);
  setEditMode("move");

  connect(view->horizontalSlider, SIGNAL(valueChanged ( int )), this, SLOT(slotChangeZoom( int )));
}

void TrackView::registerFunction(const QString & name, TrackPanelFunction * function) 
{
  m_factory.registerFunction(name, function);
}

void TrackView::parseDocument(QDomDocument doc)
{
  QDomNodeList tracks = doc.elementsByTagName("playlist");
  m_projectDuration = 300;
  int duration;
  for (int i = 0; i < tracks.count(); i++)
  {
    if (tracks.item(i).toElement().attribute("hide", QString::null) == "video") {
      // this is an audio track
      duration = slotAddAudioTrack(i, tracks.item(i).toElement());
    }
    else duration = slotAddVideoTrack(i, tracks.item(i).toElement());
    if (duration > m_projectDuration) m_projectDuration = duration;
  }
  m_tracksAreaLayout->insertStretch (1000);
  //m_scrollBox->setGeometry(0, 0, 300 * zoomFactor(), m_scrollArea->height());
}

void TrackView::slotChangeZoom(int factor)
{
  m_ruler->setPixelPerMark(factor);
  m_scale = m_ruler->pixelPerMark();
  for (int i = 0; i < documentTracks.count(); i++) {
    kDebug()<<"------REPAINTING OBJECT";
    documentTracks.at(i)->update();
    //documentTracks.at(i)->setFixedWidth(300 * zoomFactor());
  }
  m_scrollBox->setFixedWidth(( m_projectDuration + 300) * zoomFactor());
  /*m_scrollArea->horizontalScrollBar()->setMaximum(300 * zoomFactor());
  m_scrollArea->horizontalScrollBar()->setPageStep(FRAME_SIZE * zoomFactor());*/
}

const double TrackView::zoomFactor() const
{
  return m_scale * FRAME_SIZE;
}

const int TrackView::mapLocalToValue(int x) const
{
  return (int) x * zoomFactor();
}

KdenliveDoc *TrackView::document()
{
  return m_doc;
}

int TrackView::slotAddAudioTrack(int ix, QDomElement xml)
{
  DocumentTrack *track = new DocumentAudioTrack(xml, this, m_scrollBox);
  HeaderTrack *header = new HeaderTrack();
  m_tracksAreaLayout->addWidget(track); //, ix, Qt::AlignTop);
  m_headersLayout->addWidget(header); //, ix, Qt::AlignTop);
  documentTracks.insert(ix, track);
  return track->duration();
  //track->show();
}

int TrackView::slotAddVideoTrack(int ix, QDomElement xml)
{
  DocumentTrack *track = new DocumentVideoTrack(xml, this, m_scrollBox);
  HeaderTrack *header = new HeaderTrack();
  m_tracksAreaLayout->addWidget(track); //, ix, Qt::AlignTop);
  m_headersLayout->addWidget(header); //, ix, Qt::AlignTop);
  documentTracks.insert(ix, track);
  return track->duration();
  //track->show();
}

DocumentTrack *TrackView::panelAt(int y)
{
  return NULL;
}

void TrackView::setEditMode(const QString & editMode)
{
  m_editMode = editMode;
}

const QString & TrackView::editMode() const
{
  return m_editMode;
}

/** This event occurs when the mouse has been moved. */
    void TrackView::mouseMoveEvent(QMouseEvent * event) {
    kDebug()<<"--------  TRACKVIEW MOUSE MOVE EVENT -----";
	if (m_panelUnderMouse) {
	    if (event->buttons() & Qt::LeftButton) {
		bool result = false;
		if (m_function)
		    result =
			m_function->mouseMoved(m_panelUnderMouse, event);
		if (!result) {
		    m_panelUnderMouse = 0;
		    m_function = 0;
		}
	    } else {
		if (m_function) {
		    m_function->mouseReleased(m_panelUnderMouse, event);
		    m_function = 0;
		}
		m_panelUnderMouse = 0;
	    }
	} else {
	    DocumentTrack *panel = panelAt(event->y());
	    if (panel) {
		QCursor result(Qt::ArrowCursor);

		TrackPanelFunction *function =
		    getApplicableFunction(panel, editMode(),
		    event);
		if (function)
		    result = function->getMouseCursor(panel, event);

		setCursor(result);
	    } else {
		setCursor(QCursor(Qt::ArrowCursor));
	    }
	}
    }

    TrackPanelFunction *TrackView::getApplicableFunction(DocumentTrack *
	panel, const QString & editMode, QMouseEvent * event) {
	TrackPanelFunction *function = 0;

	QStringList list = panel->applicableFunctions(editMode);
	QStringList::iterator itt = list.begin();

	while (itt != list.end()) {
	    TrackPanelFunction *testFunction = m_factory.function(*itt);
	    if (testFunction) {
		if (testFunction->mouseApplies(panel, event)) {
		    function = testFunction;
		    break;
		}
	    }

	    ++itt;
	}

	return function;
    }


#include "trackview.moc"
