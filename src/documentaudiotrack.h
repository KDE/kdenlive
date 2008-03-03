#ifndef DOCUMENTAUDIOTRACK_H
#define DOCUMENTAUDIOTRACK_H

#include "documenttrack.h"
#include "trackview.h"

class DocumentAudioTrack : public DocumentTrack {
    Q_OBJECT

public:
    DocumentAudioTrack(QDomElement xml, TrackView * view, QWidget *parent = 0);

protected:
    //virtual void paintEvent(QPaintEvent * /*e*/);

private:
    TrackView *m_trackView;
public slots:

};

#endif
