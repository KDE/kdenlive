#ifndef TRACKVIEW_H
#define TRACKVIEW_H

#include <QScrollArea>
#include <QVBoxLayout>
#include <KRuler>

#include "ui_timeline_ui.h"
#include "customruler.h"
#include "kdenlivedoc.h"

class TrackView : public QWidget
{
  Q_OBJECT
  
  public:
    TrackView(KdenliveDoc *doc, QWidget *parent=0);

  public slots:
    KdenliveDoc *document();

  private:
    Ui::TimeLine_UI *view;
    CustomRuler *m_ruler;
    KdenliveDoc *m_doc;
    QVBoxLayout *m_tracksLayout;
    QVBoxLayout *m_headersLayout;
    QScrollArea *m_scrollArea;
    QVBoxLayout *m_tracksAreaLayout;
    void parseDocument(QDomDocument doc);

  private slots:
    void slotChangeZoom(int factor);
    void slotAddTrack(int ix);
};

#endif
