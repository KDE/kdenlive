#ifndef TRACKVIEW_H
#define TRACKVIEW_H

#include <QScrollArea>
#include <QVBoxLayout>
#include <KRuler>
#include <QGroupBox>

#define FRAME_SIZE 90

#include "ui_timeline_ui.h"
#include "customruler.h"
#include "kdenlivedoc.h"
#include "documenttrack.h"

class TrackView : public QWidget
{
  Q_OBJECT
  
  public:
    TrackView(KdenliveDoc *doc, QWidget *parent=0);

    const double zoomFactor() const;

  public slots:
    KdenliveDoc *document();

  private:
    Ui::TimeLine_UI *view;
    CustomRuler *m_ruler;
    double m_scale;
    QList <DocumentTrack*> documentTracks;
    int m_projectDuration;

    KdenliveDoc *m_doc;
    QVBoxLayout *m_tracksLayout;
    QVBoxLayout *m_headersLayout;
    QScrollArea *m_scrollArea;
    QFrame *m_scrollBox;
    QVBoxLayout *m_tracksAreaLayout;
    void parseDocument(QDomDocument doc);
    int slotAddAudioTrack(int ix, QDomElement xml);
    int slotAddVideoTrack(int ix, QDomElement xml);


  private slots:
    void slotChangeZoom(int factor);
};

#endif
