#ifndef TRACKVIEW_H
#define TRACKVIEW_H

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

  private slots:
    void slotChangeZoom(int factor);
};

#endif
