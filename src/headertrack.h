#ifndef HEADERTRACK_H
#define HEADERTRACK_H

#include "definitions.h"
#include "ui_trackheader_ui.h"

class HeaderTrack : public QWidget {
    Q_OBJECT

public:
    HeaderTrack(int index, TrackInfo info, QWidget *parent = 0);

protected:
    //virtual void paintEvent(QPaintEvent * /*e*/);

private:
    int m_index;
    TRACKTYPE m_type;
    Ui::TrackHeader_UI view;

private slots:
    void switchAudio();
    void switchVideo();

signals:
    void switchTrackAudio(int);
    void switchTrackVideo(int);
};

#endif
