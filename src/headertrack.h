#ifndef HEADERTRACK_H
#define HEADERTRACK_H

#include "definitions.h"

class HeaderTrack : public QWidget {
    Q_OBJECT

public:
    HeaderTrack(int index, TRACKTYPE type, QWidget *parent = 0);

protected:
    virtual void paintEvent(QPaintEvent * /*e*/);

private:
    int m_index;
    QString m_label;
    TRACKTYPE m_type;

public slots:

};

#endif
