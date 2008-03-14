#ifndef HEADERTRACK_H
#define HEADERTRACK_H



class HeaderTrack : public QWidget {
    Q_OBJECT

public:
    HeaderTrack(int index, QWidget *parent = 0);

protected:
    virtual void paintEvent(QPaintEvent * /*e*/);

private:
    int m_index;
    QString m_label;

public slots:

};

#endif
