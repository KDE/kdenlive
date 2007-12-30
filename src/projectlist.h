#ifndef PRJECTLIST_H
#define PRJECTLIST_H

#include <QDomNodeList>
#include <QToolBar>

#include <QTreeWidget>
#include <KTreeWidgetSearchLine>

#include "docclipbase.h"
#include "kdenlivedoc.h"
#include "renderer.h"
#include "timecode.h"

class ProjectList : public QWidget
{
  Q_OBJECT
  
  public:
    ProjectList(QWidget *parent=0);

    QDomElement producersList();
    void setRenderer(Render *projectRender);

  public slots:
    void setDocument(KdenliveDoc *doc);
    void addProducer(QDomElement producer);
    void slotReplyGetImage(const KUrl &url, int pos, const QPixmap &pix, int w, int h);
    void slotReplyGetFileProperties(const QMap < QString, QString > &properties, const QMap < QString, QString > &metadata);


  private:
    QTreeWidget *listView;
    KTreeWidgetSearchLine *searchView;
    Render *m_render;
    Timecode m_timecode;
    double m_fps;
    QToolBar *m_toolbar;

  private slots:
    void slotAddClip();
    void slotRemoveClip();
    void slotEditClip();
    void slotClipSelected();
    void slotAddColorClip();

  signals:
    void clipSelected(const QDomElement &);
    void getFileProperties(const KUrl &, uint);
};

#endif
