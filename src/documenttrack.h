#ifndef DOCUMENTTRACK_H
#define DOCUMENTTRACK_H

#include <QDomElement>
#include <QList>
#include <QWidget>
#include <QMap>
#include <QStringList>

#include "definitions.h"
#include "gentime.h"


class TrackPanelFunction;
class TrackView;


class DocumentTrack : public QWidget {
    Q_OBJECT

public:
    DocumentTrack(QDomElement xml, TrackView * view, QWidget *parent = 0);

    QList <TrackViewClip> clipList();
    int duration();
    int documentTrackIndex();
    TrackViewClip *getClipAt(GenTime pos);
    void addFunctionDecorator(const QString & mode, const QString & function);
    QStringList applicableFunctions(const QString & mode);

protected:
    //virtual void paintEvent(QPaintEvent * /*e*/);

private:
    QDomElement m_xml;
    QList <TrackViewClip> m_clipList;
    void parseXml();
    int m_trackDuration;
    /** A map of lists of track panel functions. */
    QMap < QString, QStringList > m_trackPanelFunctions;


public slots:

};

#endif
