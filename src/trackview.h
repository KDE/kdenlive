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
#include "trackpanelfunctionfactory.h"
#include "trackpanelfunction.h"

class TrackView : public QWidget
{
  Q_OBJECT
  
  public:
    TrackView(KdenliveDoc *doc, QWidget *parent=0);

	/** This event occurs when the mouse has been moved. */
    void mouseMoveEvent(QMouseEvent * event);

    const double zoomFactor() const;
    DocumentTrack *panelAt(int y);
    const int mapLocalToValue(int x) const;
    void setEditMode(const QString & editMode);
    const QString & editMode() const;

  public slots:
    KdenliveDoc *document();

  private:
    Ui::TimeLine_UI *view;
    CustomRuler *m_ruler;
    double m_scale;
    QList <DocumentTrack*> documentTracks;
    int m_projectDuration;
    TrackPanelFunctionFactory m_factory;
    DocumentTrack *m_panelUnderMouse;
	/** The currently applied function. This lasts from mousePressed until mouseRelease. */
    TrackPanelFunction *m_function;
    QString m_editMode;

    KdenliveDoc *m_doc;
    QVBoxLayout *m_tracksLayout;
    QVBoxLayout *m_headersLayout;
    QScrollArea *m_scrollArea;
    QFrame *m_scrollBox;
    QVBoxLayout *m_tracksAreaLayout;
    void parseDocument(QDomDocument doc);
    int slotAddAudioTrack(int ix, QDomElement xml);
    int slotAddVideoTrack(int ix, QDomElement xml);
    void registerFunction(const QString & name, TrackPanelFunction * function);
    TrackPanelFunction *getApplicableFunction(DocumentTrack * panel, const QString & editMode, QMouseEvent * event);

  private slots:
    void slotChangeZoom(int factor);
};

#endif
