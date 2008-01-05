#ifndef PRJECTLIST_H
#define PRJECTLIST_H

#include <QDomNodeList>
#include <QToolBar>
#include <QTreeWidget>

#include <KUndoStack>
#include <KTreeWidgetSearchLine>

#include "docclipbase.h"
#include "kdenlivedoc.h"
#include "renderer.h"
#include "timecode.h"

class ProjectList : public QWidget
{
  Q_OBJECT
  
  public:
    ProjectList(KUndoStack *commandStack, QWidget *parent=0);

    QDomElement producersList();
    void setRenderer(Render *projectRender);

    void addClip(const QStringList &name, const QDomElement &elem, const int clipId, const KUrl &url = KUrl());
    void deleteClip(const int clipId);

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
    KUndoStack *m_commandStack;
    int m_clipIdCounter;

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
